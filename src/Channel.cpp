/**
 * OddSockets C++ SDK - Channel Implementation
 */

#include "../include/oddsockets/Channel.hpp"
#include "../include/oddsockets/OddSockets.hpp"

namespace oddsockets {

Channel::Channel(const std::string& name, OddSockets* client)
    : name_(name)
    , client_(client)
    , subscribed_(false)
    , subscribing_(false) {
}

Channel::~Channel() {
    if (subscribed_) {
        try { unsubscribe().get(); } catch (...) {}
    }
}

namespace {

// Escape a value for embedding inside a JSON string literal.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

} // namespace

std::future<bool> Channel::subscribe(MessageCallback callback, const SubscribeOptions& options) {
    return std::async(std::launch::async, [this, callback, options]() -> bool {
        if (subscribed_ || subscribing_) return false;
        if (!client_ || client_->getState() != ConnectionState::Connected) return false;

        subscribing_ = true;
        messageCallback_ = callback;
        subscribeOptions_ = options;

        // Emit a Socket.IO "subscribe" event; options in camelCase as the worker
        // reads them. Joining the scoped room is required to receive broadcasts.
        std::string payload =
            "{\"channel\":\"" + jsonEscape(name_) + "\",\"options\":{" +
            "\"maxHistory\":" + std::to_string(options.maxHistory) +
            ",\"retainHistory\":" + (options.retainHistory ? "true" : "false") +
            ",\"enablePresence\":" + (options.enablePresence ? "true" : "false") + "}}";

        client_->emit("subscribe", payload);

        subscribed_ = true;
        subscribing_ = false;
        return true;
    });
}

std::future<bool> Channel::unsubscribe() {
    return std::async(std::launch::async, [this]() -> bool {
        if (!subscribed_) return false;

        std::string payload = "{\"channel\":\"" + jsonEscape(name_) + "\"}";
        if (client_) client_->emit("unsubscribe", payload);

        subscribed_ = false;
        messageCallback_ = nullptr;
        return true;
    });
}

std::future<PublishResult> Channel::publish(const std::string& message, const PublishOptions& options) {
    return std::async(std::launch::async, [this, message, options]() -> PublishResult {
        PublishResult result;

        if (!validateMessageSize(message)) {
            result.success = false;
            result.error = "Message exceeds 32KB limit";
            return result;
        }

        if (!client_ || client_->getState() != ConnectionState::Connected) {
            result.success = false;
            result.error = "Not connected";
            return result;
        }

        // Embed the message raw when it is already JSON (object/array), otherwise
        // as a JSON string. The worker delivers structured bodies verbatim.
        std::string trimmed = message;
        size_t firstNonWs = trimmed.find_first_not_of(" \t\n\r");
        bool raw = (firstNonWs != std::string::npos &&
                    (trimmed[firstNonWs] == '{' || trimmed[firstNonWs] == '['));

        std::string payload = "{\"channel\":\"" + jsonEscape(name_) + "\",\"message\":";
        payload += raw ? message : ("\"" + jsonEscape(message) + "\"");

        // Only attach options when set - the worker defaults them on undefined,
        // and a JSON null would crash the handler.
        if (options.ttlSeconds > 0 || !options.metadata.empty()) {
            payload += ",\"options\":{\"ttl\":" + std::to_string(options.ttlSeconds) +
                       ",\"storeInHistory\":" + (options.storeInHistory ? "true" : "false");
            if (!options.metadata.empty()) {
                payload += ",\"metadata\":\"" + jsonEscape(options.metadata) + "\"";
            }
            payload += "}";
        }
        payload += "}";

        client_->emit("publish", payload);

        result.success = true;
        result.timestamp = std::chrono::system_clock::now();
        return result;
    });
}

std::future<std::vector<std::string>> Channel::getHistory(const HistoryOptions& options) {
    auto promise = std::make_shared<std::promise<std::vector<std::string>>>();
    auto future = promise->get_future();

    if (!client_ || client_->getState() != ConnectionState::Connected) {
        promise->set_value({});
        return future;
    }

    // Register the waiter before emitting so the response can never race ahead.
    {
        std::lock_guard<std::mutex> lock(pendingHistoryMutex_);
        pendingHistory_ = promise;
    }

    std::string payload = "{\"channel\":\"" + jsonEscape(name_) + "\",\"count\":" +
                          std::to_string(options.count > 0 ? options.count : 50);
    if (options.startTime) payload += ",\"start\":\"" + jsonEscape(*options.startTime) + "\"";
    if (options.endTime)   payload += ",\"end\":\"" + jsonEscape(*options.endTime) + "\"";
    payload += "}";

    // Query the shared store over the real transport; the worker replies with a
    // query:true "history" event that handleHistory() correlates. Do NOT fall
    // back to local history - that only reflects locally observed messages.
    client_->emit("get_history", payload);

    // Bound the wait so a lost response can't hang the caller.
    return std::async(std::launch::async,
                      [this, fut = std::move(future), promise]() mutable -> std::vector<std::string> {
        if (fut.wait_for(std::chrono::seconds(10)) == std::future_status::ready) {
            return fut.get();
        }
        std::lock_guard<std::mutex> lock(pendingHistoryMutex_);
        if (pendingHistory_ == promise) pendingHistory_.reset();
        return {};
    });
}

void Channel::handleHistory(const json::Value& data) {
    // The worker emits "history" both as the explicit get_history RESPONSE
    // (query:true) and as a fire-and-forget on-join snapshot (~10 msgs, no query
    // flag). Only the query:true response may resolve a pending getHistory
    // waiter; ignore the snapshot so it can't return the wrong data.
    // BUG-2026-0727-0012.
    if (!(data.has("query") &&
          data.get("query").getType() == json::Value::Bool &&
          data.get("query").asBool())) {
        return;
    }

    std::vector<std::string> messages;
    if (data.has("messages")) {
        const json::Value& arr = data.get("messages");
        if (arr.getType() == json::Value::Array) {
            for (size_t i = 0; i < arr.size(); ++i) {
                messages.push_back(arr.at(i).toString());
            }
        }
    }

    std::shared_ptr<std::promise<std::vector<std::string>>> promise;
    {
        std::lock_guard<std::mutex> lock(pendingHistoryMutex_);
        promise = pendingHistory_;
        pendingHistory_.reset();
    }
    if (promise) {
        try { promise->set_value(std::move(messages)); } catch (...) {}
    }
}

std::future<PresenceInfo> Channel::getPresence() {
    return std::async(std::launch::async, [this]() -> PresenceInfo {
        if (client_ && client_->getState() == ConnectionState::Connected) {
            std::string payload = "{\"channel\":\"" + jsonEscape(name_) + "\"}";
            client_->emit("get_presence", payload);
        }
        std::lock_guard<std::mutex> lock(presenceMutex_);
        return presenceInfo_;
    });
}

std::future<bool> Channel::updateState(const std::string& state) {
    return std::async(std::launch::async, [this, state]() -> bool {
        if (!client_ || client_->getState() != ConnectionState::Connected) return false;

        // state is expected to be a JSON object string.
        std::string payload = "{\"channel\":\"" + jsonEscape(name_) + "\",\"state\":" +
                              (state.empty() ? std::string("{}") : state) + "}";
        client_->emit("update_state", payload);
        return true;
    });
}

} // namespace oddsockets
