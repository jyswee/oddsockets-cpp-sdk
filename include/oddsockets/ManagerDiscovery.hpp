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
 * Manager Discovery Service
 *
 * Resolves the manager endpoint the client asks for a worker assignment.
 *
 * The resolved URL is used verbatim. There is deliberately no fallback to the
 * public endpoint when a configured manager is unreachable: silently
 * redirecting a self-hosted or staging deployment at production would make a
 * misconfigured client look healthy, and would invalidate any test aimed at a
 * non-production manager.
 */
class ManagerDiscovery {
public:
    /**
     * Create a ManagerDiscovery instance for a configured manager URL
     * @param configuredUrl The manager URL from the client configuration. An
     *        empty or whitespace-only value means "not configured"
     * @throws Exception with ErrorCode::InvalidParameter if the resolved URL is
     *         not an absolute http:// or https:// URL
     */
    explicit ManagerDiscovery(const std::string& configuredUrl);

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
     * Get the manager URL this instance was built with
     * @return Future that resolves to the manager URL, without trailing slashes
     */
    std::future<std::string> discoverManagerUrl();

    /**
     * Get the manager URL this instance was built with
     * @return The manager URL, without trailing slashes
     */
    const std::string& managerUrl() const { return managerUrl_; }

    /**
     * Resolve the manager URL to use
     *
     * Precedence is configuredUrl, then the ODDSOCKETS_MANAGER_URL environment
     * variable, then DEFAULT_MANAGER_URL. The default applies only when nothing
     * at all was configured.
     *
     * @param configuredUrl The manager URL from the client configuration
     * @return The validated manager URL, without trailing slashes
     * @throws Exception with ErrorCode::InvalidParameter if the resolved URL is
     *         not an absolute http:// or https:// URL
     */
    static std::string resolveManagerUrl(const std::string& configuredUrl);

    /**
     * Validate a manager URL and strip any trailing slashes
     * @param value The manager URL to validate
     * @return The manager URL, without trailing slashes
     * @throws Exception with ErrorCode::InvalidParameter if value is not an
     *         absolute http:// or https:// URL
     */
    static std::string validateManagerUrl(const std::string& value);

    /**
     * Get the manager URL used when the caller configures none
     * @return The ODDSOCKETS_MANAGER_URL value if set, otherwise DEFAULT_MANAGER_URL
     */
    static std::string getDefaultManagerUrl();

private:
    std::string managerUrl_;
};

} // namespace oddsockets
