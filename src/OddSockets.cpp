/**
 * OddSockets C++ SDK - Main Client Implementation
 */

#include "../include/oddsockets/OddSockets.hpp"
#include "../include/oddsockets/EnhancedFeatures.hpp"
#include <sstream>

namespace oddsockets {

// Extract the raw JSON value for `key` out of an object string. Handles balanced
// objects/arrays, quoted strings, and primitives - needed because the worker's
// message envelope carries the published body as a nested object under "message".
static bool extractRawValue(const std::string& obj, const std::string& key, std::string& out) {
    std::string needle = "\"" + key + "\"";
    auto pos = obj.find(needle);
    if (pos == std::string::npos) return false;
    const char* p = obj.c_str() + pos + needle.size();
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return false;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    const char* start = p;
    if (*p == '{' || *p == '[') {
        char open = *p, close = (open == '{') ? '}' : ']';
        int depth = 0;
        while (*p) {
            if (*p == open) depth++;
            else if (*p == close) { depth--; if (depth == 0) { p++; break; } }
            p++;
        }
    } else if (*p == '"') {
        p++;
        while (*p && *p != '"') { if (*p == '\\' && *(p + 1)) p++; p++; }
        if (*p == '"') p++;
    } else {
        while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    }
    out.assign(start, static_cast<size_t>(p - start));
    return true;
}

// Decode a Socket.IO EVENT frame ("42[\"event\",payload]"). Writes the event
// name and the raw payload argument (empty if none).
static bool parseSocketIoEventFrame(const std::string& frame, std::string& eventOut, std::string& payloadOut) {
    eventOut.clear();
    payloadOut.clear();
    auto bracket = frame.find('[');
    if (bracket == std::string::npos) return false;
    const char* p = frame.c_str() + bracket + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;
    std::string ev;
    while (*p && *p != '"') { if (*p == '\\' && *(p + 1)) p++; ev += *p++; }
    if (*p == '"') p++;
    eventOut = ev;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == ',') { p++; while (*p == ' ' || *p == '\t') p++; }
    if (*p && *p != ']') {
        // Payload runs to the closing ']' of the event array.
        std::string rest(p);
        auto lastBracket = rest.rfind(']');
        if (lastBracket != std::string::npos) rest = rest.substr(0, lastBracket);
        payloadOut = rest;
    }
    return true;
}

OddSockets::OddSockets(const Config& config)
    : config_(config)
    , state_(ConnectionState::Disconnected)
    , reconnectAttempts_(0)
    , eventThreadRunning_(false) {

    if (config_.apiKey.empty()) {
        throw Exception(ErrorCode::InvalidApiKey, "API key is required");
    }

    managerDiscovery_ = std::make_unique<ManagerDiscovery>();
    enhanced_ = std::make_unique<EnhancedFeatures>(this);
    generateClientIdentifier();

    if (config_.autoConnect) {
        connect();
    }
}

OddSockets::~OddSockets() {
    disconnect();
}

std::future<bool> OddSockets::connect() {
    return std::async(std::launch::async, [this]() -> bool {
        if (state_ == ConnectionState::Connected || state_ == ConnectionState::Connecting) {
            return true;
        }

        setState(ConnectionState::Connecting);
        intentionalClose_ = false;

        try {
            if (!getWorkerAssignment().get()) {
                setState(ConnectionState::Error);
                return false;
            }
            if (!connectToWorker().get()) {
                setState(ConnectionState::Error);
                return false;
            }
            // The raw WebSocket is up, but we are not a live Socket.IO client
            // until the Engine.IO OPEN -> CONNECT -> CONNECT-ack handshake
            // completes. onWebSocketMessage drives setState(Connected) when the
            // worker's "40" ack arrives; poll for it up to the timeout.
            auto deadline = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(config_.connectionTimeoutMs);
            while (state_.load() != ConnectionState::Connected &&
                   std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (state_.load() != ConnectionState::Connected) {
                handleError(ErrorCode::Timeout, "Socket.IO handshake timed out");
                setState(ConnectionState::Error);
                return false;
            }
            reconnectAttempts_ = 0;
            return true;
        } catch (const std::exception& e) {
            handleError(ErrorCode::ConnectionFailed, e.what());
            setState(ConnectionState::Error);
            if (reconnectAttempts_ < config_.reconnectAttempts) {
                scheduleReconnect();
            }
            return false;
        }
    });
}

void OddSockets::disconnect() {
    intentionalClose_ = true;
    eventThreadRunning_ = false;
    if (websocket_) {
        websocket_->disconnect();
        websocket_.reset();
    }
    workerUrl_.clear();
    workerId_.clear();
    setState(ConnectionState::Disconnected);
}

std::shared_ptr<Channel> OddSockets::channel(const std::string& channelName) {
    if (channelName.empty()) {
        throw Exception(ErrorCode::InvalidParameter, "Channel name required");
    }
    std::lock_guard<std::mutex> lock(channelsMutex_);
    auto it = channels_.find(channelName);
    if (it != channels_.end()) return it->second;
    auto ch = std::make_shared<Channel>(channelName, this);
    channels_[channelName] = ch;
    return ch;
}

std::optional<WorkerInfo> OddSockets::getWorkerInfo() const {
    if (workerId_.empty()) return std::nullopt;
    return WorkerInfo{workerId_, workerUrl_, sessionId_};
}

std::optional<SessionInfo> OddSockets::getSessionInfo() const {
    if (sessionId_.empty()) return std::nullopt;
    SessionInfo info;
    info.sessionId = sessionId_;
    info.clientIdentifier = clientIdentifier_;
    return info;
}

void OddSockets::processEvents() {
    if (websocket_) websocket_->processEvents();
}

std::future<std::vector<PublishResult>> OddSockets::publishBulk(const std::vector<BulkMessage>& messages) {
    return std::async(std::launch::async, [this, messages]() -> std::vector<PublishResult> {
        std::vector<PublishResult> results;
        for (const auto& msg : messages) {
            try {
                auto ch = channel(msg.channel);
                results.push_back(ch->publish(msg.message, msg.options).get());
            } catch (const std::exception& e) {
                PublishResult r;
                r.success = false;
                r.error = e.what();
                results.push_back(r);
            }
        }
        return results;
    });
}

std::future<bool> OddSockets::getWorkerAssignment() {
    return std::async(std::launch::async, [this]() -> bool {
        try {
            std::string managerUrl = managerDiscovery_->discoverManagerUrl(config_.apiKey).get();
            std::string url = managerUrl + "/api/cluster/select-worker?apiKey=" +
                config_.apiKey + "&userId=" +
                (config_.userId.empty() ? clientIdentifier_ : config_.userId) +
                "&clientIdentifier=" + clientIdentifier_;

            auto resp = http::get(url, {{"User-Agent", "OddSockets-CPP-SDK/1.0.0"}},
                                  config_.connectionTimeoutMs).get();
            if (!resp.success) {
                handleError(ErrorCode::ManagerOffline, "Manager request failed: " + resp.error);
                return false;
            }

            auto data = json::parse(resp.body);
            workerUrl_ = data.get("url").asString();
            workerId_ = data.get("workerId").asString();
            if (data.has("session") && data.get("session").has("id")) {
                sessionId_ = data.get("session").get("id").asString();
            }

            if (workerUrl_.empty() || workerId_.empty()) {
                handleError(ErrorCode::WorkerAssignmentFailed, "Invalid response");
                return false;
            }

            log(LogLevel::Info, "Assigned to worker: " + workerId_ + " (" + workerUrl_ + ")");
            return true;
        } catch (const std::exception& e) {
            handleError(ErrorCode::ManagerOffline, e.what());
            return false;
        }
    });
}

std::future<bool> OddSockets::connectToWorker() {
    return std::async(std::launch::async, [this]() -> bool {
        if (workerUrl_.empty()) return false;

        WebSocketClient::Config ws;
        ws.url = workerUrl_;
        ws.enableSsl = config_.enableSsl;
        ws.sslVerifyPeer = config_.sslVerifyPeer;
        ws.caCertPath = config_.caCertPath;
        ws.connectionTimeoutMs = config_.connectionTimeoutMs;
        ws.headers["Authorization"] = "Bearer " + config_.apiKey;
        ws.onConnected = [this]() { onWebSocketConnected(); };
        ws.onDisconnected = [this]() { onWebSocketDisconnected(); };
        ws.onMessage = [this](const std::string& m) { onWebSocketMessage(m); };
        ws.onError = [this](const std::string& e) { onWebSocketError(e); };

        websocket_ = WebSocketClient::create(ws);
        return websocket_->connect().get();
    });
}

void OddSockets::setState(ConnectionState s) {
    auto prev = state_.exchange(s);
    if (prev != s) {
        log(LogLevel::Info, "State: " + connectionStateToString(s));
        if (config_.connectionCallback) config_.connectionCallback(s);
    }
}

void OddSockets::handleError(ErrorCode code, const std::string& msg) {
    log(LogLevel::Error, Exception::errorCodeToString(code) + ": " + msg);
    if (config_.errorCallback) config_.errorCallback(code, msg);
}

void OddSockets::log(LogLevel level, const std::string& msg) {
    if (!config_.enableLogging || level > config_.logLevel) return;
    if (config_.logCallback) config_.logCallback(level, msg);
}

void OddSockets::generateClientIdentifier() {
    clientIdentifier_ = oddsockets::generateClientIdentifier(config_.apiKey, config_.userId);
}

void OddSockets::scheduleReconnect() {
    reconnectAttempts_++;
    setState(ConnectionState::Reconnecting);
    int delay = config_.reconnectDelayMs * (1 << (reconnectAttempts_.load() - 1));
    if (delay > 30000) delay = 30000;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    connect();
}

void OddSockets::setupWebSocketEventHandlers() {}
void OddSockets::eventThreadFunction() {}

void OddSockets::onWebSocketConnected() {
    // The raw WebSocket is up, but we are not a live Socket.IO client until the
    // Engine.IO OPEN -> CONNECT -> CONNECT-ack handshake completes.
    log(LogLevel::Debug, "WebSocket established, awaiting Socket.IO handshake");
}

void OddSockets::onWebSocketDisconnected() {
    setState(ConnectionState::Disconnected);
    // Do not reconnect when the socket was closed on purpose (disconnect()/
    // teardown), only on an unexpected drop.
    if (!intentionalClose_ && reconnectAttempts_ < config_.reconnectAttempts) {
        scheduleReconnect();
    }
}

void OddSockets::onWebSocketMessage(const std::string& message) {
    if (message.empty()) return;

    // The Engine.IO packet type is the leading digit of the frame.
    switch (message[0]) {
        case '0': // OPEN - reply with a Socket.IO CONNECT carrying auth
            sendSocketIoConnect();
            break;
        case '2': // PING - reply PONG
            if (websocket_) websocket_->send("3");
            break;
        case '3': // PONG
            break;
        case '4': // MESSAGE - a Socket.IO packet follows
            if (message.size() < 2) break;
            switch (message[1]) {
                case '0': // CONNECT ack - the handshake is complete
                    setState(ConnectionState::Connected);
                    reconnectAttempts_ = 0;
                    break;
                case '1': // DISCONNECT
                    setState(ConnectionState::Disconnected);
                    break;
                case '2': { // EVENT
                    std::string event, payload;
                    if (parseSocketIoEventFrame(message, event, payload)) {
                        dispatchSocketIoEvent(event, payload);
                    }
                    break;
                }
                case '4': // CONNECT_ERROR
                    handleError(ErrorCode::ConnectionFailed, "Socket.IO connect error");
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void OddSockets::onWebSocketError(const std::string& error) {
    handleError(ErrorCode::WebSocketError, error);
}

void OddSockets::sendSocketIoConnect() {
    if (!websocket_) return;
    std::string userId = config_.userId.empty() ? clientIdentifier_ : config_.userId;
    std::string frame = "40{\"apiKey\":\"" + config_.apiKey + "\",\"userId\":\"" + userId + "\"}";
    websocket_->send(frame);
}

void OddSockets::emit(const std::string& event, const std::string& payloadJson) {
    if (!websocket_) return;
    std::string frame;
    if (!payloadJson.empty()) {
        frame = "42[\"" + event + "\"," + payloadJson + "]";
    } else {
        frame = "42[\"" + event + "\"]";
    }
    websocket_->send(frame);
}

void OddSockets::on(const std::string& event, RawHandler handler) {
    std::lock_guard<std::mutex> lock(rawHandlersMutex_);
    rawHandlers_[event].push_back(std::move(handler));
}

EnhancedFeatures& OddSockets::enhanced() {
    return *enhanced_;
}

void OddSockets::dispatchSocketIoEvent(const std::string& event, const std::string& payload) {
    // Fan out to every raw listener registered for this event via on().
    {
        std::vector<RawHandler> handlers;
        {
            std::lock_guard<std::mutex> lock(rawHandlersMutex_);
            auto it = rawHandlers_.find(event);
            if (it != rawHandlers_.end()) handlers = it->second;
        }
        for (auto& h : handlers) {
            if (h) h(payload);
        }
    }

    if (payload.empty()) return;

    // Route core pub/sub events to the owning channel.
    std::string channelName;
    extractRawValue(payload, "channel", channelName);
    // extractRawValue leaves quotes on string values; strip them.
    if (channelName.size() >= 2 && channelName.front() == '"' && channelName.back() == '"') {
        channelName = channelName.substr(1, channelName.size() - 2);
    }

    if (event == "message") {
        std::string body;
        if (!extractRawValue(payload, "message", body)) body = payload;
        std::lock_guard<std::mutex> lock(channelsMutex_);
        auto it = channels_.find(channelName);
        if (it != channels_.end() && it->second->messageCallback_) {
            it->second->messageCallback_(body);
        }
    } else if (event == "subscribed") {
        std::lock_guard<std::mutex> lock(channelsMutex_);
        auto it = channels_.find(channelName);
        if (it != channels_.end()) { it->second->subscribed_ = true; it->second->subscribing_ = false; }
    } else if (event == "unsubscribed") {
        std::lock_guard<std::mutex> lock(channelsMutex_);
        auto it = channels_.find(channelName);
        if (it != channels_.end()) it->second->subscribed_ = false;
    } else if (event == "error") {
        std::string msg;
        extractRawValue(payload, "message", msg);
        handleError(ErrorCode::WebSocketError, msg.empty() ? "worker error" : msg);
    }
}

void OddSockets::handleChannelMessage(const json::Value&) {}
void OddSockets::handleSubscribedMessage(const json::Value&) {}
void OddSockets::handleUnsubscribedMessage(const json::Value&) {}
void OddSockets::handlePublishedMessage(const json::Value&) {}
void OddSockets::handlePresenceMessage(const json::Value&) {}
void OddSockets::handlePresenceChangeMessage(const json::Value&) {}
void OddSockets::handleHistoryMessage(const json::Value&) {}

} // namespace oddsockets
