/**
 * OddSockets C++ SDK - Basic Usage Example
 * 
 * This example demonstrates the basic usage of the OddSockets C++ SDK,
 * following the same patterns as the JavaScript SDK.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#include <oddsockets/OddSockets.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    std::cout << "OddSockets C++ SDK Basic Usage Example" << std::endl;
    std::cout << "SDK Version: " << oddsockets::getSDKVersion() << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Create configuration
        oddsockets::Config config;
        config.apiKey = "your-api-key-here";  // Replace with your actual API key
        config.userId = "cpp-example-user";
        config.autoConnect = true;
        
        // Optional: Configure for embedded systems
        // config.maxChannels = 4;
        // config.maxMessageSize = 1024;
        // config.enableLogging = false;
        // config.enableSSL = false;
        
        // Set up callbacks
        config.connectionCallback = [](oddsockets::ConnectionState state) {
            std::cout << "Connection state changed to: " 
                      << oddsockets::connectionStateToString(state) << std::endl;
        };
        
        config.errorCallback = [](oddsockets::ErrorCode error, const std::string& message) {
            std::cout << "Error: " << message << " (Code: " << static_cast<int>(error) << ")" << std::endl;
        };
        
        config.logCallback = [](oddsockets::LogLevel level, const std::string& message) {
            std::cout << "[" << oddsockets::logLevelToString(level) << "] " << message << std::endl;
        };
        
        // Create client
        std::cout << "Creating OddSockets client..." << std::endl;
        auto client = std::make_unique<oddsockets::OddSockets>(config);
        
        // Connect to the platform
        std::cout << "Connecting to OddSockets platform..." << std::endl;
        auto connectFuture = client->connect();
        
        // Wait for connection (with timeout)
        auto status = connectFuture.wait_for(std::chrono::seconds(10));
        if (status == std::future_status::timeout) {
            std::cout << "Connection timeout!" << std::endl;
            return 1;
        }
        
        bool connected = connectFuture.get();
        if (!connected) {
            std::cout << "Failed to connect!" << std::endl;
            return 1;
        }
        
        std::cout << "Connected successfully!" << std::endl;
        
        // Get worker information
        auto workerInfo = client->getWorkerInfo();
        if (workerInfo) {
            std::cout << "Assigned to worker: " << workerInfo->workerId 
                      << " (" << workerInfo->workerUrl << ")" << std::endl;
        }
        
        // Get session information
        auto sessionInfo = client->getSessionInfo();
        if (sessionInfo) {
            std::cout << "Session ID: " << sessionInfo->sessionId << std::endl;
            std::cout << "Client Identifier: " << sessionInfo->clientIdentifier << std::endl;
        }
        
        // Create a channel
        std::cout << "Creating channel 'example-channel'..." << std::endl;
        auto channel = client->channel("example-channel");
        
        // Subscribe to the channel
        std::cout << "Subscribing to channel..." << std::endl;
        auto subscribeFuture = channel->subscribe([](const std::string& message) {
            std::cout << "Received message: " << message << std::endl;
        });
        
        // Wait for subscription
        status = subscribeFuture.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::timeout) {
            std::cout << "Subscription timeout!" << std::endl;
            return 1;
        }
        
        bool subscribed = subscribeFuture.get();
        if (!subscribed) {
            std::cout << "Failed to subscribe!" << std::endl;
            return 1;
        }
        
        std::cout << "Subscribed successfully!" << std::endl;
        
        // Publish some messages
        std::cout << "Publishing messages..." << std::endl;
        
        for (int i = 1; i <= 5; ++i) {
            std::string message = "Hello from C++ SDK! Message #" + std::to_string(i);
            
            // Validate message size before publishing
            if (!oddsockets::MessageSizeValidator::validate(message)) {
                std::cout << "Message too large: " << message << std::endl;
                continue;
            }
            
            auto publishFuture = channel->publish(message);
            
            // Wait for publish result
            status = publishFuture.wait_for(std::chrono::seconds(5));
            if (status == std::future_status::timeout) {
                std::cout << "Publish timeout for message " << i << std::endl;
                continue;
            }
            
            auto result = publishFuture.get();
            if (result.success) {
                std::cout << "Published message " << i << " successfully (ID: " 
                          << result.messageId << ")" << std::endl;
            } else {
                std::cout << "Failed to publish message " << i << ": " 
                          << result.error << std::endl;
            }
            
            // Wait a bit between messages
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Demonstrate bulk publishing
        std::cout << "Demonstrating bulk publishing..." << std::endl;
        
        std::vector<oddsockets::BulkMessage> bulkMessages = {
            {"example-channel", "Bulk message 1", {}},
            {"example-channel", "Bulk message 2", {}},
            {"example-channel", "Bulk message 3", {}}
        };
        
        auto bulkFuture = client->publishBulk(bulkMessages);
        status = bulkFuture.wait_for(std::chrono::seconds(10));
        
        if (status != std::future_status::timeout) {
            auto results = bulkFuture.get();
            for (size_t i = 0; i < results.size(); ++i) {
                const auto& result = results[i];
                std::cout << "Bulk message " << (i + 1) << ": " 
                          << (result.success ? "Success" : "Failed") << std::endl;
            }
        }
        
        // Get message history
        std::cout << "Getting message history..." << std::endl;
        auto historyFuture = channel->getHistory();
        status = historyFuture.wait_for(std::chrono::seconds(5));
        
        if (status != std::future_status::timeout) {
            auto history = historyFuture.get();
            std::cout << "Retrieved " << history.size() << " messages from history:" << std::endl;
            for (const auto& msg : history) {
                std::cout << "  - " << msg << std::endl;
            }
        }
        
        // Get presence information
        std::cout << "Getting presence information..." << std::endl;
        auto presenceFuture = channel->getPresence();
        status = presenceFuture.wait_for(std::chrono::seconds(5));
        
        if (status != std::future_status::timeout) {
            auto presence = presenceFuture.get();
            std::cout << "Channel occupancy: " << presence.occupancy << std::endl;
            std::cout << "Occupants: ";
            for (const auto& occupant : presence.occupants) {
                std::cout << occupant << " ";
            }
            std::cout << std::endl;
        }
        
        // Keep the connection alive for a while to receive messages
        std::cout << "Keeping connection alive for 10 seconds..." << std::endl;
        std::cout << "You can send messages from other clients to see them here." << std::endl;
        
        auto startTime = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - startTime < std::chrono::seconds(10)) {
            // Process events (important for single-threaded mode)
            client->processEvents();
            
            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Unsubscribe from the channel
        std::cout << "Unsubscribing from channel..." << std::endl;
        auto unsubscribeFuture = channel->unsubscribe();
        status = unsubscribeFuture.wait_for(std::chrono::seconds(5));
        
        if (status != std::future_status::timeout) {
            bool unsubscribed = unsubscribeFuture.get();
            if (unsubscribed) {
                std::cout << "Unsubscribed successfully!" << std::endl;
            } else {
                std::cout << "Failed to unsubscribe!" << std::endl;
            }
        }
        
        // Disconnect
        std::cout << "Disconnecting..." << std::endl;
        client->disconnect();
        
        std::cout << "Example completed successfully!" << std::endl;
        
    } catch (const oddsockets::Exception& e) {
        std::cout << "OddSockets exception: " << e.what() 
                  << " (Code: " << static_cast<int>(e.getCode()) << ")" << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cout << "Standard exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unknown exception occurred!" << std::endl;
        return 1;
    }
    
    return 0;
}
