/**
 * OddSockets C++ SDK - Main Client Implementation
 */

#include "../include/oddsockets/OddSockets.hpp"
#include <sstream>

namespace oddsockets {

OddSockets::OddSockets(const Config& config)
    : config_(config)
    , state_(ConnectionState::Disconnected)
    , reconnectAttempts_(0)
    , eventThreadRunning_(false) {

    if (config_.apiKey.empty()) {
        throw Exception(ErrorCode::InvalidApiKey, "API key is required");
    }

    managerDiscovery_ = std::make_unique<ManagerDiscovery>();
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

        try {
            if (!getWorkerAssignment().get()) {
                setState(ConnectionState::Error);
                return false;
            }
            if (!connectToWorker().get()) {
                setState(ConnectionState::Error);
                return false;
            }
            setState(ConnectionState::Connected);
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
            std::string managerUrl = managerDiscovery_->discoverManagerUrl(config_.apiKey);
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

void OddSockets::onWebSocketConnected() { setState(ConnectionState::Connected); reconnectAttempts_ = 0; }
void OddSockets::onWebSocketDisconnected() {
    setState(ConnectionState::Disconnected);
    if (reconnectAttempts_ < config_.reconnectAttempts) scheduleReconnect();
}

void OddSockets::onWebSocketMessage(const std::string& message) {
    auto data = json::parse(message);
    handleChannelMessage(data);
}

void OddSockets::onWebSocketError(const std::string& error) {
    handleError(ErrorCode::WebSocketError, error);
}

void OddSockets::handleChannelMessage(const json::Value& data) {
    std::string ch = data.get("channel").asString();
    std::lock_guard<std::mutex> lock(channelsMutex_);
    auto it = channels_.find(ch);
    if (it != channels_.end() && it->second->messageCallback_) {
        it->second->messageCallback_(data.get("message").asString());
    }
}

void OddSockets::handleSubscribedMessage(const json::Value& data) {
    std::string ch = data.get("channel").asString();
    std::lock_guard<std::mutex> lock(channelsMutex_);
    auto it = channels_.find(ch);
    if (it != channels_.end()) { it->second->subscribed_ = true; it->second->subscribing_ = false; }
}

void OddSockets::handleUnsubscribedMessage(const json::Value& data) {
    std::string ch = data.get("channel").asString();
    std::lock_guard<std::mutex> lock(channelsMutex_);
    auto it = channels_.find(ch);
    if (it != channels_.end()) it->second->subscribed_ = false;
}

void OddSockets::handlePublishedMessage(const json::Value&) {}
void OddSockets::handlePresenceMessage(const json::Value&) {}
void OddSockets::handlePresenceChangeMessage(const json::Value&) {}
void OddSockets::handleHistoryMessage(const json::Value&) {}

} // namespace oddsockets
