/**
 * OddSockets C++ SDK - Manager Discovery Service
 * 
 * Modern C++ SDK for the OddSockets real-time messaging platform,
 * optimized for embedded systems and IoT devices.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#pragma once

#include "Types.hpp"

#include <string>
#include <future>
#include <memory>

namespace oddsockets {

/**
 * Simple Manager Discovery Service
 * 
 * Always connects to the main manager endpoint which handles
 * all routing and load balancing transparently.
 * 
 * This follows the same pattern as the JavaScript SDK ManagerDiscovery.
 */
class ManagerDiscovery {
public:
    /**
     * Create a ManagerDiscovery instance
     */
    ManagerDiscovery();
    
    /**
     * Destructor
     */
    ~ManagerDiscovery() = default;
    
    // Non-copyable but movable
    ManagerDiscovery(const ManagerDiscovery&) = delete;
    ManagerDiscovery& operator=(const ManagerDiscovery&) = delete;
    ManagerDiscovery(ManagerDiscovery&&) = default;
    ManagerDiscovery& operator=(ManagerDiscovery&&) = default;
    
    /**
     * Get the manager URL (always returns the main endpoint)
     * @param apiKey The OddSockets API key (not used, kept for compatibility)
     * @return Future that resolves to the manager URL
     */
    std::future<std::string> discoverManagerUrl(const std::string& apiKey);
    
    /**
     * Clear cache (no-op, kept for compatibility)
     */
    void clearCache();
    
    /**
     * Get the default manager URL
     * @return Default manager URL
     */
    static std::string getDefaultManagerUrl();

private:
    std::string managerUrl_;
};

// Inline implementations

inline std::string ManagerDiscovery::getDefaultManagerUrl() {
    return DEFAULT_MANAGER_URL;
}

} // namespace oddsockets
