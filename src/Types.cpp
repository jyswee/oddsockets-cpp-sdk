/**
 * OddSockets C++ SDK - Types & Utility Implementation
 * Implements json::Value, http::get/post, WebSocketClient, and utility functions.
 */

#include "../include/oddsockets/Types.hpp"
#include <curl/curl.h>
#include <libwebsockets.h>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>
#include <cstring>

namespace oddsockets {

// --- Exception ---

std::string Exception::errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::InvalidParameter: return "Invalid parameter";
        case ErrorCode::InvalidApiKey: return "Invalid API key";
        case ErrorCode::ConnectionFailed: return "Connection failed";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::MemoryAllocation: return "Memory allocation failed";
        case ErrorCode::MessageTooLarge: return "Message too large";
        case ErrorCode::ChannelNotFound: return "Channel not found";
        case ErrorCode::NotConnected: return "Not connected";
        case ErrorCode::AlreadyConnected: return "Already connected";
        case ErrorCode::AlreadySubscribed: return "Already subscribed";
        case ErrorCode::NotSubscribed: return "Not subscribed";
        case ErrorCode::WebSocketError: return "WebSocket error";
        case ErrorCode::HttpError: return "HTTP error";
        case ErrorCode::JsonParseError: return "JSON parse error";
        case ErrorCode::SslError: return "SSL error";
        case ErrorCode::ManagerOffline: return "Manager offline";
        case ErrorCode::WorkerAssignmentFailed: return "Worker assignment failed";
        default: return "Unknown error";
    }
}

// --- Utility Functions ---

std::string connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected: return "disconnected";
        case ConnectionState::Connecting: return "connecting";
        case ConnectionState::Connected: return "connected";
        case ConnectionState::Reconnecting: return "reconnecting";
        case ConnectionState::Error: return "error";
        default: return "unknown";
    }
}

std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::None: return "none";
        case LogLevel::Error: return "error";
        case LogLevel::Warn: return "warn";
        case LogLevel::Info: return "info";
        case LogLevel::Debug: return "debug";
        default: return "unknown";
    }
}

bool validateMessageSize(const std::string& message) {
    return message.size() <= MAX_MESSAGE_SIZE;
}

uint32_t hashString(const std::string& str) {
    uint32_t hash = 0;
    for (char c : str) {
        hash = ((hash << 5) - hash) + static_cast<uint32_t>(c);
    }
    return hash;
}

std::string generateClientIdentifier(const std::string& apiKey, const std::string& userId) {
    uint32_t hash = hashString(apiKey);
    std::ostringstream oss;
    oss << std::hex << hash << "_" << (userId.empty() ? "default" : userId);
    return oss.str();
}

// --- json::Value Implementation ---

namespace json {

static const Value NULL_VALUE;

bool Value::has(const std::string& key) const {
    return objectValue_.find(key) != objectValue_.end();
}

const Value& Value::get(const std::string& key) const {
    auto it = objectValue_.find(key);
    if (it != objectValue_.end()) return it->second;
    return NULL_VALUE;
}

void Value::set(const std::string& key, const Value& value) {
    type_ = Object;
    objectValue_[key] = value;
}

size_t Value::size() const {
    if (type_ == Array) return arrayValue_.size();
    if (type_ == Object) return objectValue_.size();
    return 0;
}

const Value& Value::at(size_t index) const {
    if (type_ == Array && index < arrayValue_.size()) return arrayValue_[index];
    return NULL_VALUE;
}

void Value::push(const Value& value) {
    type_ = Array;
    arrayValue_.push_back(value);
}

std::string Value::toString() const {
    switch (type_) {
        case Null: return "null";
        case Bool: return boolValue_ ? "true" : "false";
        case Number: {
            std::ostringstream oss;
            if (numberValue_ == std::floor(numberValue_))
                oss << static_cast<long long>(numberValue_);
            else
                oss << numberValue_;
            return oss.str();
        }
        case String: return "\"" + stringValue_ + "\"";
        case Array: {
            std::string r = "[";
            for (size_t i = 0; i < arrayValue_.size(); i++) {
                if (i > 0) r += ",";
                r += arrayValue_[i].toString();
            }
            return r + "]";
        }
        case Object: {
            std::string r = "{";
            bool first = true;
            for (const auto& kv : objectValue_) {
                if (!first) r += ",";
                r += "\"" + kv.first + "\":" + kv.second.toString();
                first = false;
            }
            return r + "}";
        }
    }
    return "null";
}

// Minimal JSON parser
static const char* skipWs(const char* p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static std::string parseString(const char*& p) {
    if (*p != '"') return "";
    p++;
    std::string result;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) { p++; }
        result += *p++;
    }
    if (*p == '"') p++;
    return result;
}

static Value parseValue(const char*& p) {
    p = skipWs(p);
    if (*p == '"') {
        return Value(parseString(p));
    } else if (*p == '{') {
        p++;
        Value obj;
        while (*p && *p != '}') {
            p = skipWs(p);
            if (*p == '}') break;
            if (*p == ',') { p++; continue; }
            std::string key = parseString(p);
            p = skipWs(p);
            if (*p == ':') p++;
            Value val = parseValue(p);
            obj.set(key, val);
            p = skipWs(p);
        }
        if (*p == '}') p++;
        return obj;
    } else if (*p == '[') {
        p++;
        Value arr;
        while (*p && *p != ']') {
            p = skipWs(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            arr.push(parseValue(p));
            p = skipWs(p);
        }
        if (*p == ']') p++;
        return arr;
    } else if (*p == 't') {
        p += 4; return Value(true);
    } else if (*p == 'f') {
        p += 5; return Value(false);
    } else if (*p == 'n') {
        p += 4; return Value();
    } else {
        char* end;
        double d = strtod(p, &end);
        p = const_cast<const char*>(end);
        return Value(d);
    }
}

Value parse(const std::string& jsonString) {
    const char* p = jsonString.c_str();
    return parseValue(p);
}

std::string stringify(const Value& value) {
    return value.toString();
}

} // namespace json

// --- HTTP Client (libcurl) ---

namespace http {

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total = size * nmemb;
    response->append(static_cast<char*>(contents), total);
    return total;
}

static Response doRequest(const std::string& url, const std::string& method,
                           const std::string& body,
                           const std::map<std::string, std::string>& headers,
                           int timeoutMs) {
    Response resp;
    CURL* curl = curl_easy_init();
    if (!curl) { resp.error = "Failed to init curl"; return resp; }

    std::string responseBody;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeoutMs));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OddSockets-CPP-SDK/1.0.0");

    struct curl_slist* headerList = nullptr;
    for (const auto& h : headers) {
        std::string hdr = h.first + ": " + h.second;
        headerList = curl_slist_append(headerList, hdr.c_str());
    }

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        headerList = curl_slist_append(headerList, "Content-Type: application/json");
    }

    if (headerList) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long httpCode;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        resp.statusCode = static_cast<int>(httpCode);
        resp.body = responseBody;
        resp.success = (httpCode >= 200 && httpCode < 300);
    } else {
        resp.error = curl_easy_strerror(res);
    }

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return resp;
}

std::future<Response> get(const std::string& url,
                          const std::map<std::string, std::string>& headers,
                          int timeoutMs) {
    return std::async(std::launch::async, [=]() {
        return doRequest(url, "GET", "", headers, timeoutMs);
    });
}

std::future<Response> post(const std::string& url,
                           const std::string& body,
                           const std::map<std::string, std::string>& headers,
                           int timeoutMs) {
    return std::async(std::launch::async, [=]() {
        return doRequest(url, "POST", body, headers, timeoutMs);
    });
}

} // namespace http

// --- WebSocket Client (libwebsockets) ---
//
// A genuine WebSocket transport. libwebsockets carries only the raw text
// frames; the Engine.IO v4 / Socket.IO packet framing on top of it lives in
// OddSockets.cpp. Each client owns its own lws_context and a dedicated service
// thread, so two clients (e.g. alice + bob in the demo) run independently.
//
// Thread-safety: sends are queued under a mutex from arbitrary caller threads
// and flushed on the service thread. lws_cancel_service() - the one lws call
// documented safe to invoke from another thread - wakes the service loop, which
// then requests a writable callback to drain the queue.

class LibWebSocketClient : public WebSocketClient {
public:
    explicit LibWebSocketClient(const WebSocketClient::Config& config)
        : config_(config) {}

    ~LibWebSocketClient() override { disconnect(); }

    std::future<bool> connect() override {
        return std::async(std::launch::async, [this]() -> bool {
            lws_set_log_level(LLL_ERR, nullptr);

            struct lws_context_creation_info info;
            memset(&info, 0, sizeof(info));
            info.port = CONTEXT_PORT_NO_LISTEN;
            info.protocols = protocols_;
            info.user = this;
            info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

            context_ = lws_create_context(&info);
            if (!context_) return false;

            // Parse worker URL into host/port/ssl. The Socket.IO path is fixed.
            const std::string& url = config_.url;
            bool useSsl = (url.rfind("https://", 0) == 0 || url.rfind("wss://", 0) == 0);
            auto schemePos = url.find("://");
            if (schemePos == std::string::npos) return false;
            std::string rest = url.substr(schemePos + 3);

            std::string host = rest;
            int port = useSsl ? 443 : 80;
            auto slash = rest.find('/');
            if (slash != std::string::npos) host = rest.substr(0, slash);
            auto colon = host.find(':');
            if (colon != std::string::npos) {
                port = std::atoi(host.substr(colon + 1).c_str());
                host = host.substr(0, colon);
            }
            host_ = host;

            static const char* kPath = "/socket.io/?EIO=4&transport=websocket";

            struct lws_client_connect_info ci;
            memset(&ci, 0, sizeof(ci));
            ci.context = context_;
            ci.address = host_.c_str();
            ci.port = port;
            ci.path = kPath;
            ci.host = host_.c_str();
            ci.origin = host_.c_str();
            ci.protocol = "oddsockets";
            ci.ssl_connection = useSsl ? LCCSCF_USE_SSL : 0;

            wsi_ = lws_client_connect_via_info(&ci);
            if (!wsi_) {
                lws_context_destroy(context_);
                context_ = nullptr;
                return false;
            }

            shouldClose_ = false;
            serviceThread_ = std::thread([this]() {
                while (!shouldClose_) {
                    lws_service(context_, 50);
                }
            });

            return true;
        });
    }

    void disconnect() override {
        shouldClose_ = true;
        if (context_) lws_cancel_service(context_);
        if (serviceThread_.joinable()) serviceThread_.join();
        if (context_) {
            lws_context_destroy(context_);
            context_ = nullptr;
        }
        if (connected_.exchange(false)) {
            if (config_.onDisconnected) config_.onDisconnected();
        }
    }

    bool isConnected() const override { return connected_.load(); }

    std::future<bool> send(const std::string& message) override {
        {
            std::lock_guard<std::mutex> lock(sendMutex_);
            sendQueue_.push_back(message);
        }
        if (context_) lws_cancel_service(context_);
        std::promise<bool> p;
        p.set_value(true);
        return p.get_future();
    }

    // The service thread pumps events itself; nothing to do here. Kept so
    // single-threaded callers can still poke the loop harmlessly.
    void processEvents() override {}

private:
    static int lwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                           void* /*user*/, void* in, size_t len) {
        auto* self = static_cast<LibWebSocketClient*>(lws_context_user(lws_get_context(wsi)));
        if (!self) return 0;

        switch (reason) {
            case LWS_CALLBACK_CLIENT_ESTABLISHED:
                self->connected_ = true;
                if (self->config_.onConnected) self->config_.onConnected();
                break;

            case LWS_CALLBACK_CLIENT_RECEIVE:
                if (in && len > 0) {
                    self->rxBuffer_.append(static_cast<char*>(in), len);
                    // Only dispatch once the whole WebSocket message has arrived.
                    if (lws_is_final_fragment(wsi) && lws_remaining_packet_payload(wsi) == 0) {
                        if (self->config_.onMessage) self->config_.onMessage(self->rxBuffer_);
                        self->rxBuffer_.clear();
                    }
                }
                break;

            case LWS_CALLBACK_CLIENT_WRITEABLE: {
                std::string msg;
                bool more = false;
                {
                    std::lock_guard<std::mutex> lock(self->sendMutex_);
                    if (!self->sendQueue_.empty()) {
                        msg = std::move(self->sendQueue_.front());
                        self->sendQueue_.pop_front();
                    }
                    more = !self->sendQueue_.empty();
                }
                if (!msg.empty()) {
                    std::vector<unsigned char> buf(LWS_PRE + msg.size());
                    memcpy(buf.data() + LWS_PRE, msg.data(), msg.size());
                    lws_write(wsi, buf.data() + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
                }
                if (more) lws_callback_on_writable(wsi);
                break;
            }

            case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
                // Woken by lws_cancel_service() after send() queued a frame.
                if (self->wsi_) lws_callback_on_writable(self->wsi_);
                break;

            case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                self->connected_ = false;
                if (self->config_.onError) {
                    self->config_.onError(in ? static_cast<const char*>(in) : "Connection error");
                }
                break;

            case LWS_CALLBACK_CLOSED:
            case LWS_CALLBACK_CLIENT_CLOSED:
                self->connected_ = false;
                self->wsi_ = nullptr;
                if (self->config_.onDisconnected) self->config_.onDisconnected();
                break;

            default:
                break;
        }
        return 0;
    }

    static const struct lws_protocols protocols_[];

    WebSocketClient::Config config_;
    struct lws_context* context_ = nullptr;
    struct lws* wsi_ = nullptr;
    std::atomic<bool> connected_{false};
    std::atomic<bool> shouldClose_{false};
    std::thread serviceThread_;
    std::string host_;
    std::string rxBuffer_;
    std::deque<std::string> sendQueue_;
    std::mutex sendMutex_;
};

const struct lws_protocols LibWebSocketClient::protocols_[] = {
    { "oddsockets", LibWebSocketClient::lwsCallback, 0, 65536 },
    { nullptr, nullptr, 0, 0 }
};

std::unique_ptr<WebSocketClient> WebSocketClient::create(const Config& config) {
    return std::make_unique<LibWebSocketClient>(config);
}

} // namespace oddsockets
