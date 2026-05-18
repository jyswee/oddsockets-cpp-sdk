/**
 * OddSockets C++ SDK - Main Header
 * 
 * Modern C++ SDK for the OddSockets real-time messaging platform,
 * optimized for embedded systems and IoT devices.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#pragma once

// Main SDK components
#include "Types.hpp"
#include "OddSockets.hpp"
#include "Channel.hpp"
#include "ManagerDiscovery.hpp"
#include "MessageSizeValidator.hpp"

/**
 * OddSockets C++ SDK
 * 
 * This is the main header file for the OddSockets C++ SDK.
 * Include this file to access all SDK functionality.
 * 
 * Features:
 * - JavaScript Pattern Compliance: Follows the same architectural patterns as the JavaScript SDK
 * - Embedded/IoT Optimized: Minimal memory footprint and resource usage
 * - Modern C++17: Clean, type-safe API with RAII principles
 * - Header-Only Option: Can be used as header-only library for easy integration
 * - Cross-Platform: Works on Linux, Windows, macOS, and embedded platforms
 * - Thread-Safe: Safe for use in multi-threaded applications
 * - WebSocket Support: Built-in WebSocket client with SSL/TLS support
 * - Session Stickiness: Automatic worker assignment and session management
 * - Message Size Validation: Industry-standard 32KB message size limits
 * - Reconnection Logic: Automatic reconnection with exponential backoff
 * - Memory Management: Custom allocators for embedded systems
 * 
 * Quick Start:
 * 
 * ```cpp
 * #include <oddsockets/OddSockets.h>
 * 
 * int main() {
 *     // Create configuration
 *     oddsockets::Config config;
 *     config.apiKey = "your-api-key";
 *     config.userId = "user123";
 *     
 *     // Create client
 *     auto client = std::make_unique<oddsockets::OddSockets>(config);
 *     
 *     // Connect
 *     client->connect().then([&](bool success) {
 *         if (success) {
 *             // Get a channel
 *             auto channel = client->channel("my-channel");
 *             
 *             // Subscribe to messages
 *             channel->subscribe([](const std::string& message) {
 *                 std::cout << "Received: " << message << std::endl;
 *             });
 *             
 *             // Publish a message
 *             channel->publish("Hello, World!");
 *         }
 *     });
 *     
 *     // Process events
 *     while (client->isConnected()) {
 *         client->processEvents();
 *         std::this_thread::sleep_for(std::chrono::milliseconds(10));
 *     }
 *     
 *     return 0;
 * }
 * ```
 * 
 * For embedded systems:
 * 
 * ```cpp
 * #include <oddsockets/OddSockets.h>
 * 
 * // Custom memory allocator for embedded systems
 * class EmbeddedAllocator {
 * public:
 *     static void* allocate(size_t size) {
 *         return malloc(size);
 *     }
 *     
 *     static void deallocate(void* ptr) {
 *         free(ptr);
 *     }
 * };
 * 
 * int main() {
 *     // Set custom allocator
 *     oddsockets::setCustomAllocator<EmbeddedAllocator>();
 *     
 *     // Minimal configuration for embedded systems
 *     oddsockets::Config config;
 *     config.apiKey = "your-api-key";
 *     config.maxChannels = 4;           // Limit channels
 *     config.maxMessageSize = 1024;     // Smaller messages
 *     config.enableLogging = false;     // Disable logging
 *     config.enableSSL = false;         // Disable SSL if not needed
 *     
 *     auto client = std::make_unique<oddsockets::OddSockets>(config);
 *     
 *     // Rest of your application...
 *     
 *     return 0;
 * }
 * ```
 */

namespace oddsockets {

// Version information
constexpr const char* SDK_VERSION = VERSION;
constexpr int SDK_VERSION_MAJOR = VERSION_MAJOR;
constexpr int SDK_VERSION_MINOR = VERSION_MINOR;
constexpr int SDK_VERSION_PATCH = VERSION_PATCH;

/**
 * Get SDK version string
 * @return Version string
 */
inline std::string getSDKVersion() {
    return VERSION;
}

/**
 * Get SDK version components
 * @param major Output for major version
 * @param minor Output for minor version
 * @param patch Output for patch version
 */
inline void getSDKVersionComponents(int& major, int& minor, int& patch) {
    major = VERSION_MAJOR;
    minor = VERSION_MINOR;
    patch = VERSION_PATCH;
}

} // namespace oddsockets
