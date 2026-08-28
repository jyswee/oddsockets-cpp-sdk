/**
 * OddSockets C++ SDK - Core Types and Definitions
 * 
 * Modern C++ SDK for the OddSockets real-time messaging platform,
 * optimized for embedded systems and IoT devices.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <chrono>
#include <future>
#include <optional>

namespace oddsockets {

// Version Information
constexpr const char* VERSION = "1.0.0";
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

// Message Size Limits (industry standard - matches PubNub and JavaScript SDK)
constexpr size_t MAX_MESSAGE_SIZE = 32768;  // 32KB in bytes
constexpr size_t MAX_MESSAGE_SIZE_KB = 32;

// Default Configuration Values
constexpr const char* DEFAULT_MANAGER_URL = "https://connect.oddsockets.tyga.network";

// Environment variable consulted when no manager URL has been configured.
constexpr const char* MANAGER_URL_ENV_VAR = "ODDSOCKETS_MANAGER_URL";

// Returns the manager URL used when the caller configures none: the
// ODDSOCKETS_MANAGER_URL environment value if set, otherwise
// DEFAULT_MANAGER_URL. Defined in ManagerDiscovery.cpp.
std::string defaultManagerUrl();

constexpr int DEFAULT_RECONNECT_ATTEMPTS = 5;
constexpr int DEFAULT_RECONNECT_DELAY_MS = 1000;
constexpr int DEFAULT_CONNECTION_TIMEOUT_MS = 10000;
constexpr int DEFAULT_MESSAGE_TIMEOUT_MS = 5000;
constexpr int DEFAULT_MAX_CHANNELS = 32;
constexpr int DEFAULT_MAX_HISTORY = 100;

// Forward Declarations
class OddSockets;
class Channel;

// Connection States
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

// Log Levels
enum class LogLevel {
    None = 0,
    Error = 1,
    Warn = 2,
    Info = 3,
    Debug = 4
};

// Error Codes
enum class ErrorCode {
    Success = 0,
    InvalidParameter = -1,
    InvalidApiKey = -2,
    ConnectionFailed = -3,
    Timeout = -4,
    MemoryAllocation = -5,
    MessageTooLarge = -6,
    ChannelNotFound = -7,
    NotConnected = -8,
    AlreadyConnected = -9,
    AlreadySubscribed = -10,
    NotSubscribed = -11,
    WebSocketError = -12,
    HttpError = -13,
    JsonParseError = -14,
    SslError = -15,
    ManagerOffline = -16,
    WorkerAssignmentFailed = -17,
    Unknown = -99
};

// Exception class for OddSockets errors
class Exception : public std::exception {
public:
    explicit Exception(ErrorCode code, const std::string& message = "")
        : code_(code), message_(message) {
        if (message_.empty()) {
            message_ = errorCodeToString(code_);
        }
    }
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
    
    ErrorCode getCode() const noexcept {
        return code_;
    }
    
    static std::string errorCodeToString(ErrorCode code);

private:
    ErrorCode code_;
    std::string message_;
};

// Callback Types
using MessageCallback = std::function<void(const std::string& message)>;
using ConnectionCallback = std::function<void(ConnectionState state)>;
using ErrorCallback = std::function<void(ErrorCode error, const std::string& message)>;
using HistoryCallback = std::function<void(const std::vector<std::string>& messages)>;
using PresenceCallback = std::function<void(const std::string& action, const std::string& userId)>;
using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

// Minted-token auth: the shape returned by a TokenProvider. Fill `token` with
// the JWT; expiry is resolved from `exp` (epoch seconds), then `expiresAt`
// (ISO 8601), then the JWT's own exp claim when both are left unset.
struct Token {
    std::string token;
    std::string expiresAt;   // ISO 8601, optional
    long long exp = 0;       // epoch seconds, optional
    std::string baseUrl;     // optional
    std::string identity;    // optional
};

// Callback the SDK invokes whenever it needs a fresh token (every connect and
// each pre-expiry refresh). Throw on failure.
using TokenProvider = std::function<Token()>;

// Configuration Structures
struct PublishOptions {
    int ttlSeconds = 0;
    std::string metadata;
    bool storeInHistory = true;
};

struct SubscribeOptions {
    int maxHistory = DEFAULT_MAX_HISTORY;
    bool retainHistory = true;
    bool enablePresence = false;
};

struct HistoryOptions {
    int count = 50;
    std::optional<std::string> startTime;  // ISO 8601 format
    std::optional<std::string> endTime;    // ISO 8601 format
};

struct Config {
    // Credentials: either an apiKey OR a tokenProvider is required.
    std::string apiKey;

    // Keyless auth: mint a short-lived token via your backend instead of
    // embedding an API key. Called for a fresh token before every (re)connect
    // and again tokenRefreshLeadMs before the current token expires.
    TokenProvider tokenProvider;
    int tokenRefreshLeadMs = 120000;

    // Optional
    std::string userId;

    // The manager the client will contact. Resolved from ODDSOCKETS_MANAGER_URL
    // and then the built-in default when left unset; used verbatim either way.
    std::string managerUrl = defaultManagerUrl();
    
    // Connection Options
    bool autoConnect = true;
    int reconnectAttempts = DEFAULT_RECONNECT_ATTEMPTS;
    int reconnectDelayMs = DEFAULT_RECONNECT_DELAY_MS;
    int connectionTimeoutMs = DEFAULT_CONNECTION_TIMEOUT_MS;
    int messageTimeoutMs = DEFAULT_MESSAGE_TIMEOUT_MS;
    
    // Resource Limits (for embedded systems)
    int maxChannels = DEFAULT_MAX_CHANNELS;
    size_t maxMessageSize = MAX_MESSAGE_SIZE;
    
    // SSL/TLS Options
    bool enableSsl = true;
    bool sslVerifyPeer = true;
    std::string caCertPath;
    
    // Logging
    LogLevel logLevel = LogLevel::Info;
    LogCallback logCallback;
    
    // Callbacks
    ConnectionCallback connectionCallback;
    ErrorCallback errorCallback;
    
    // Embedded System Options
    bool enableLogging = true;
    bool enableExceptions = true;  // Set to false for no-exception mode
    bool enableThreading = true;   // Set to false for single-threaded mode
};

// Worker Information
struct WorkerInfo {
    std::string workerId;
    std::string workerUrl;
    std::string sessionId;
};

// Session Information
struct SessionInfo {
    std::string sessionId;
    std::string clientIdentifier;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastActivity;
};

// Publish Result
struct PublishResult {
    bool success = false;
    std::string messageId;
    std::chrono::system_clock::time_point timestamp;
    std::string error;
};

// Bulk Message for bulk publishing
struct BulkMessage {
    std::string channel;
    std::string message;
    PublishOptions options;
};

// Presence Information
struct PresenceInfo {
    int occupancy = 0;
    std::vector<std::string> occupants;
    std::map<std::string, std::string> occupantStates;
};

// Memory Management (for embedded systems)
template<typename Allocator>
void setCustomAllocator();

// Utility Functions
std::string connectionStateToString(ConnectionState state);
std::string logLevelToString(LogLevel level);
bool validateMessageSize(const std::string& message);
std::string generateClientIdentifier(const std::string& apiKey, const std::string& userId);
uint32_t hashString(const std::string& str);

// JSON Utilities (lightweight for embedded systems)
namespace json {
    class Value;
    
    Value parse(const std::string& jsonString);
    std::string stringify(const Value& value);
    
    class Value {
    public:
        enum Type { Null, Bool, Number, String, Array, Object };
        
        Value() : type_(Null) {}
        explicit Value(bool b) : type_(Bool), boolValue_(b) {}
        explicit Value(double d) : type_(Number), numberValue_(d) {}
        explicit Value(const std::string& s) : type_(String), stringValue_(s) {}
        
        Type getType() const { return type_; }
        
        bool asBool() const { return boolValue_; }
        double asNumber() const { return numberValue_; }
        const std::string& asString() const { return stringValue_; }
        
        bool has(const std::string& key) const;
        const Value& get(const std::string& key) const;
        void set(const std::string& key, const Value& value);
        
        size_t size() const;
        const Value& at(size_t index) const;
        void push(const Value& value);
        
        std::string toString() const;
        
    private:
        Type type_;
        bool boolValue_ = false;
        double numberValue_ = 0.0;
        std::string stringValue_;
        std::map<std::string, Value> objectValue_;
        std::vector<Value> arrayValue_;
    };
}

// HTTP Client (lightweight for embedded systems)
namespace http {
    struct Response {
        int statusCode = 0;
        std::string body;
        std::map<std::string, std::string> headers;
        bool success = false;
        std::string error;
    };
    
    std::future<Response> get(const std::string& url, 
                             const std::map<std::string, std::string>& headers = {},
                             int timeoutMs = DEFAULT_CONNECTION_TIMEOUT_MS);
    
    std::future<Response> post(const std::string& url,
                              const std::string& body,
                              const std::map<std::string, std::string>& headers = {},
                              int timeoutMs = DEFAULT_CONNECTION_TIMEOUT_MS);
}

// WebSocket Client Interface
class WebSocketClient {
public:
    struct Config {
        std::string url;
        std::map<std::string, std::string> headers;
        bool enableSsl = true;
        bool sslVerifyPeer = true;
        std::string caCertPath;
        int connectionTimeoutMs = DEFAULT_CONNECTION_TIMEOUT_MS;
        
        std::function<void()> onConnected;
        std::function<void()> onDisconnected;
        std::function<void(const std::string&)> onMessage;
        std::function<void(const std::string&)> onError;
    };
    
    virtual ~WebSocketClient() = default;
    
    virtual std::future<bool> connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual std::future<bool> send(const std::string& message) = 0;
    virtual void processEvents() = 0;
    
    static std::unique_ptr<WebSocketClient> create(const Config& config);
};

} // namespace oddsockets
