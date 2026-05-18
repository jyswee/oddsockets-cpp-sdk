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

std::future<bool> Channel::subscribe(MessageCallback callback, const SubscribeOptions& options) {
    return std::async(std::launch::async, [this, callback, options]() -> bool {
        if (subscribed_ || subscribing_) return false;
        if (!client_ || client_->getState() != ConnectionState::Connected) return false;

        subscribing_ = true;
        messageCallback_ = callback;

        json::Value msg;
        msg.set("type", json::Value(std::string("subscribe")));
        msg.set("channel", json::Value(name_));

        json::Value opts;
        opts.set("maxHistory", json::Value(static_cast<double>(options.maxHistory)));
        opts.set("retainHistory", json::Value(options.retainHistory));
        opts.set("enablePresence", json::Value(options.enablePresence));
        msg.set("options", opts);

        if (client_->websocket_) {
            auto result = client_->websocket_->send(json::stringify(msg));
            if (!result.get()) {
                subscribing_ = false;
                return false;
            }
        }

        subscribed_ = true;
        subscribing_ = false;
        return true;
    });
}

std::future<bool> Channel::unsubscribe() {
    return std::async(std::launch::async, [this]() -> bool {
        if (!subscribed_) return false;

        json::Value msg;
        msg.set("type", json::Value(std::string("unsubscribe")));
        msg.set("channel", json::Value(name_));

        if (client_ && client_->websocket_) {
            client_->websocket_->send(json::stringify(msg));
        }

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

        json::Value msg;
        msg.set("type", json::Value(std::string("publish")));
        msg.set("channel", json::Value(name_));
        msg.set("message", json::Value(message));

        if (options.ttlSeconds > 0 || !options.metadata.empty()) {
            json::Value opts;
            opts.set("ttl", json::Value(static_cast<double>(options.ttlSeconds)));
            if (!options.metadata.empty()) {
                opts.set("metadata", json::Value(options.metadata));
            }
            msg.set("options", opts);
        }

        if (client_->websocket_) {
            result.success = client_->websocket_->send(json::stringify(msg)).get();
        }

        if (!result.success) result.error = "Failed to send";
        result.timestamp = std::chrono::system_clock::now();
        return result;
    });
}

std::future<std::vector<std::string>> Channel::getHistory(const HistoryOptions& options) {
    return std::async(std::launch::async, [this, options]() -> std::vector<std::string> {
        (void)options;
        return {};
    });
}

std::future<PresenceInfo> Channel::getPresence() {
    return std::async(std::launch::async, [this]() -> PresenceInfo {
        return PresenceInfo{};
    });
}

std::future<bool> Channel::updateState(const std::map<std::string, std::string>& state) {
    return std::async(std::launch::async, [this, state]() -> bool {
        if (!client_ || client_->getState() != ConnectionState::Connected) return false;

        json::Value stateObj;
        for (const auto& kv : state) {
            stateObj.set(kv.first, json::Value(kv.second));
        }

        json::Value msg;
        msg.set("type", json::Value(std::string("update_state")));
        msg.set("state", stateObj);

        if (client_->websocket_) {
            return client_->websocket_->send(json::stringify(msg)).get();
        }
        return false;
    });
}

} // namespace oddsockets
