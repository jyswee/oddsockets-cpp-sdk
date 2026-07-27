/**
 * OddSockets C++ SDK - Channel Class
 * 
 * Modern C++ SDK for the OddSockets real-time messaging platform,
 * optimized for embedded systems and IoT devices.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#pragma once

#include "Types.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <deque>

namespace oddsockets {

// Forward declaration
class OddSockets;

/**
 * Channel class for pub/sub messaging
 * 
 * Provides methods for subscribing, publishing, and managing presence
 * on a specific channel within the OddSockets platform.
 * 
 * This class follows the same patterns as the JavaScript SDK Channel class.
 */
class Channel {
public:
    /**
     * Create a Channel instance
     * Note: Channels should be created through OddSockets::channel() method
     * @param name Channel name
     * @param client Parent OddSockets client
     */
    Channel(const std::string& name, OddSockets* client);
    
    /**
     * Destructor - automatically unsubscribes if subscribed
     */
    ~Channel();
    
    // Non-copyable but movable
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) = default;
    Channel& operator=(Channel&&) = default;
    
    /**
     * Subscribe to the channel
     * @param callback Message callback function
     * @param options Subscription options
     * @return Future that resolves to true on successful subscription
     */
    std::future<bool> subscribe(MessageCallback callback, 
                               const SubscribeOptions& options = {});
    
    /**
     * Unsubscribe from the channel
     * @return Future that resolves to true on successful unsubscription
     */
    std::future<bool> unsubscribe();
    
    /**
     * Publish a message to the channel
     * @param message Message to publish (string, object, or array)
     * @param options Publishing options
     * @return Future that resolves to PublishResult
     */
    std::future<PublishResult> publish(const std::string& message,
                                      const PublishOptions& options = {});
    
    /**
     * Get message history for the channel
     * @param options History options
     * @return Future that resolves to vector of messages
     */
    std::future<std::vector<std::string>> getHistory(const HistoryOptions& options = {});
    
    /**
     * Get current presence information
     * @return Future that resolves to PresenceInfo
     */
    std::future<PresenceInfo> getPresence();
    
    /**
     * Update user state
     * @param state User state object (JSON string)
     * @return Future that resolves to true on success
     */
    std::future<bool> updateState(const std::string& state);
    
    /**
     * Get channel subscription status
     * @return true if subscribed, false otherwise
     */
    bool isSubscribed() const;
    
    /**
     * Get channel name
     * @return Channel name
     */
    std::string getName() const;
    
    /**
     * Get current presence map
     * @return Copy of current presence information
     */
    PresenceInfo getCurrentPresence() const;
    
    /**
     * Get cached message history
     * @return Copy of cached messages
     */
    std::vector<std::string> getCachedHistory() const;
    
    /**
     * Set presence callback for presence change events
     * @param callback Presence callback function
     */
    void setPresenceCallback(PresenceCallback callback);
    
    /**
     * Set history callback for history responses
     * @param callback History callback function
     */
    void setHistoryCallback(HistoryCallback callback);

private:
    // Channel properties
    std::string name_;
    OddSockets* client_;
    
    // Subscription state
    std::atomic<bool> subscribed_;
    std::atomic<bool> subscribing_;
    MessageCallback messageCallback_;
    PresenceCallback presenceCallback_;
    HistoryCallback historyCallback_;
    SubscribeOptions subscribeOptions_;
    
    // Message history
    std::deque<std::string> messageHistory_;
    int maxHistorySize_;
    mutable std::mutex historyMutex_;
    
    // Presence information
    PresenceInfo presenceInfo_;
    mutable std::mutex presenceMutex_;

    // In-flight getHistory waiter, fulfilled by the worker's query:true
    // "history" response routed through handleHistory (BUG-2026-0727-0012).
    std::shared_ptr<std::promise<std::vector<std::string>>> pendingHistory_;
    mutable std::mutex pendingHistoryMutex_;
    
    // Thread safety
    mutable std::mutex callbackMutex_;
    mutable std::mutex stateMutex_;
    
    // Internal methods
    
    /**
     * Handle incoming message (called by OddSockets)
     * @param data Message data from WebSocket
     */
    void handleMessage(const json::Value& data);
    
    /**
     * Handle subscription confirmation (called by OddSockets)
     * @param data Subscription data from WebSocket
     */
    void handleSubscribed(const json::Value& data);
    
    /**
     * Handle unsubscription confirmation (called by OddSockets)
     * @param data Unsubscription data from WebSocket
     */
    void handleUnsubscribed(const json::Value& data);
    
    /**
     * Handle publish confirmation (called by OddSockets)
     * @param data Publish confirmation data from WebSocket
     */
    void handlePublished(const json::Value& data);
    
    /**
     * Handle presence information (called by OddSockets)
     * @param data Presence data from WebSocket
     */
    void handlePresence(const json::Value& data);
    
    /**
     * Handle presence changes (called by OddSockets)
     * @param data Presence change data from WebSocket
     */
    void handlePresenceChange(const json::Value& data);
    
    /**
     * Handle message history (called by OddSockets)
     * @param data History data from WebSocket
     */
    void handleHistory(const json::Value& data);
    
    /**
     * Add message to history if enabled
     * @param message Message to add
     */
    void addToHistory(const std::string& message);
    
    /**
     * Update presence information
     * @param occupants List of occupants
     * @param occupantStates Map of occupant states
     */
    void updatePresence(const std::vector<std::string>& occupants,
                       const std::map<std::string, std::string>& occupantStates);
    
    /**
     * Send WebSocket message through parent client
     * @param message JSON message to send
     * @return Future that resolves to true on success
     */
    std::future<bool> sendMessage(const json::Value& message);
    
    /**
     * Create subscribe message
     * @return JSON subscribe message
     */
    json::Value createSubscribeMessage() const;
    
    /**
     * Create unsubscribe message
     * @return JSON unsubscribe message
     */
    json::Value createUnsubscribeMessage() const;
    
    /**
     * Create publish message
     * @param message Message content
     * @param options Publish options
     * @return JSON publish message
     */
    json::Value createPublishMessage(const std::string& message,
                                    const PublishOptions& options) const;
    
    /**
     * Create history request message
     * @param options History options
     * @return JSON history request message
     */
    json::Value createHistoryMessage(const HistoryOptions& options) const;
    
    /**
     * Create presence request message
     * @return JSON presence request message
     */
    json::Value createPresenceMessage() const;
    
    /**
     * Create state update message
     * @param state User state
     * @return JSON state update message
     */
    json::Value createStateUpdateMessage(const std::string& state) const;
    
    // Friend classes
    friend class OddSockets;
    
    // Promise/Future management for async operations
    struct PendingOperation {
        enum Type { Subscribe, Unsubscribe, Publish, History, Presence, StateUpdate };
        Type type;
        std::promise<bool> boolPromise;
        std::promise<PublishResult> publishPromise;
        std::promise<std::vector<std::string>> historyPromise;
        std::promise<PresenceInfo> presencePromise;
        std::chrono::steady_clock::time_point timestamp;
        
        explicit PendingOperation(Type t) : type(t), timestamp(std::chrono::steady_clock::now()) {}
    };
    
    std::map<std::string, std::unique_ptr<PendingOperation>> pendingOperations_;
    mutable std::mutex pendingOperationsMutex_;
    
    /**
     * Generate unique operation ID
     * @return Unique operation ID string
     */
    std::string generateOperationId() const;
    
    /**
     * Add pending operation
     * @param operationId Operation ID
     * @param operation Pending operation
     */
    void addPendingOperation(const std::string& operationId, 
                           std::unique_ptr<PendingOperation> operation);
    
    /**
     * Complete pending operation
     * @param operationId Operation ID
     * @param success Success status
     * @param data Optional response data
     */
    void completePendingOperation(const std::string& operationId, 
                                bool success, 
                                const json::Value& data = json::Value());
    
    /**
     * Cleanup expired operations
     */
    void cleanupExpiredOperations();
};

// Inline implementations for performance-critical methods

inline bool Channel::isSubscribed() const {
    return subscribed_.load();
}

inline std::string Channel::getName() const {
    return name_;
}

} // namespace oddsockets
