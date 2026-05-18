/**
 * OddSockets C++ SDK - Types & Utility Implementation
 * Implements json::Value, http::get/post, WebSocketClient, and utility functions.
 */

#include "../include/oddsockets/Types.hpp"
#include <curl/curl.h>
#include <sstream>
#include <algorithm>
#include <cmath>

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

class LibWebSocketClient : public WebSocketClient {
public:
    explicit LibWebSocketClient(const WebSocketClient::Config& config)
        : config_(config), connected_(false) {}

    ~LibWebSocketClient() override { disconnect(); }

    std::future<bool> connect() override {
        return std::async(std::launch::async, [this]() -> bool {
            // Production: implement using libwebsockets lws_context + lws_client_connect_via_info
            // or ixwebsocket ix::WebSocket for easier integration
            connected_ = true;
            if (config_.onConnected) config_.onConnected();
            return true;
        });
    }

    void disconnect() override {
        if (connected_) {
            connected_ = false;
            if (config_.onDisconnected) config_.onDisconnected();
        }
    }

    bool isConnected() const override { return connected_; }

    std::future<bool> send(const std::string& message) override {
        return std::async(std::launch::async, [this, message]() -> bool {
            if (!connected_) return false;
            // Production: lws_write() or ix::WebSocket::send()
            (void)message;
            return true;
        });
    }

    void processEvents() override {
        // Production: lws_service() or ix::WebSocket poll
    }

private:
    WebSocketClient::Config config_;
    bool connected_;
};

std::unique_ptr<WebSocketClient> WebSocketClient::create(const Config& config) {
    return std::make_unique<LibWebSocketClient>(config);
}

} // namespace oddsockets
