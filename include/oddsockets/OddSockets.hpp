/**
 * OddSockets C++ SDK - Main Client Class
 * 
 * Modern C++ SDK for the OddSockets real-time messaging platform,
 * optimized for embedded systems and IoT devices.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#pragma once

#include "Types.hpp"
#include "Channel.hpp"
#include "ManagerDiscovery.hpp"
#include "MessageSizeValidator.hpp"

#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>

namespace oddsockets {

/**
 * OddSockets C++ SDK Client
 * 
 * Provides a simple interface to the OddSockets real-time messaging platform.
 * Automatically handles manager discovery and worker load balancing internally.
 * 
 * This class follows the same architectural patterns as the JavaScript SDK,
 * ensuring consistency across all OddSockets SDKs.
 */
class OddSockets {
public:
    /**
     * Create an OddSockets client
     * @param config Configuration options
     */
    explicit OddSockets(const Config& config);
    
    /**
     * Destructor - automatically disconnects and cleans up resources
     */
    ~OddSockets();
    
    // Non-copyable but movable
    OddSockets(const OddSockets&) = delete;
    OddSockets& operator=(const OddSockets&) = delete;
    OddSockets(OddSockets&&) = default;
    OddSockets& operator=(OddSockets&&) = default;
    
    /**
     * Connect to the OddSockets platform
     * Handles the Manager → Worker assignment internally
     * @return Future that resolves to true on successful connection
     */
    std::future<bool> connect();
    
    /**
     * Disconnect from the platform
     */
    void disconnect();
    
    /**
     * Get current connection state
     * @return Current connection state
     */
    ConnectionState getState() const;
    
    /**
     * Check if currently connected
     * @return true if connected, false otherwise
     */
    bool isConnected() const;
    
    /**
     * Get or create a channel
     * @param channelName Name of the channel
     * @return Shared pointer to Channel instance
     */
    std::shared_ptr<Channel> channel(const std::string& channelName);
    
    /**
     * Get assigned worker information
     * @return Worker info if assigned, empty optional otherwise
     */
    std::optional<WorkerInfo> getWorkerInfo() const;
    
    /**
     * Get session information
     * @return Session info if available, empty optional otherwise
     */
    std::optional<SessionInfo> getSessionInfo() const;
    
    /**
     * Get client identifier used for session stickiness
     * @return Client identifier string
     */
    std::string getClientIdentifier() const;
    
    /**
     * Process pending events (call regularly in main loop for single-threaded mode)
     * In multi-threaded mode, this is handled automatically
     */
    void processEvents();
    
    /**
     * Publish multiple messages at once
     * @param messages Array of message objects with {channel, message, options} structure
     * @return Future that resolves to array of publish results
     */
    std::future<std::vector<PublishResult>> publishBulk(const std::vector<BulkMessage>& messages);
    
    /**
     * Get SDK version string
     * @return Version string
     */
    static std::string getVersion();
    
    /**
     * Validate message size
     * @param message Message to validate
     * @return true if valid, false if too large
     */
    static bool validateMessageSize(const std::string& message);

private:
    // Configuration and state
    Config config_;
    std::atomic<ConnectionState> state_;
    
    // Connection management
    std::unique_ptr<WebSocketClient> websocket_;
    std::string workerUrl_;
    std::string workerId_;
    std::string sessionId_;
    std::string clientIdentifier_;
    
    // Reconnection state
    std::atomic<int> reconnectAttempts_;
    std::chrono::steady_clock::time_point lastReconnectTime_;
    
    // Channels
    std::unordered_map<std::string, std::shared_ptr<Channel>> channels_;
    mutable std::mutex channelsMutex_;
    
    // Threading
    mutable std::mutex stateMutex_;
    std::unique_ptr<std::thread> eventThread_;
    std::atomic<bool> eventThreadRunning_;
    
    // Manager discovery
    std::unique_ptr<ManagerDiscovery> managerDiscovery_;
    
    // Internal methods
    
    /**
     * Get worker assignment from manager
     * @return Future that resolves to true on success
     */
    std::future<bool> getWorkerAssignment();
    
    /**
     * Connect to assigned worker
     * @return Future that resolves to true on success
     */
    std::future<bool> connectToWorker();
    
    /**
     * Setup WebSocket event handlers
     */
    void setupWebSocketEventHandlers();
    
    /**
     * Schedule reconnection with exponential backoff
     */
    void scheduleReconnect();
    
    /**
     * Set connection state and notify callbacks
     * @param newState New connection state
     */
    void setState(ConnectionState newState);
    
    /**
     * Handle error and notify callbacks
     * @param error Error code
     * @param message Error message
     */
    void handleError(ErrorCode error, const std::string& message);
    
    /**
     * Log message if logging is enabled
     * @param level Log level
     * @param message Log message
     */
    void log(LogLevel level, const std::string& message);
    
    /**
     * Generate consistent client identifier for session stickiness
     */
    void generateClientIdentifier();
    
    /**
     * Event processing thread function
     */
    void eventThreadFunction();
    
    // WebSocket event handlers
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketMessage(const std::string& message);
    void onWebSocketError(const std::string& error);
    
    // Message handlers
    void handleChannelMessage(const json::Value& data);
    void handleSubscribedMessage(const json::Value& data);
    void handleUnsubscribedMessage(const json::Value& data);
    void handlePublishedMessage(const json::Value& data);
    void handlePresenceMessage(const json::Value& data);
    void handlePresenceChangeMessage(const json::Value& data);
    void handleHistoryMessage(const json::Value& data);
    
    // Friend classes
    friend class Channel;
};

// Inline implementations for performance-critical methods

inline ConnectionState OddSockets::getState() const {
    return state_.load();
}

inline bool OddSockets::isConnected() const {
    return state_.load() == ConnectionState::Connected;
}

inline std::string OddSockets::getClientIdentifier() const {
    return clientIdentifier_;
}

inline std::string OddSockets::getVersion() {
    return VERSION;
}

inline bool OddSockets::validateMessageSize(const std::string& message) {
    return oddsockets::validateMessageSize(message);
}

} // namespace oddsockets
