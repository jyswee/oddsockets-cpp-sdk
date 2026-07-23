/**
 * OddSockets C++ SDK - Manager Discovery Implementation
 *
 * Mirrors the JavaScript SDK: the SDK always talks to the single main manager
 * endpoint, which handles all routing and worker load balancing transparently.
 * There is no client-side multi-region probing.
 */

#include "../include/oddsockets/ManagerDiscovery.hpp"

namespace oddsockets {

ManagerDiscovery::ManagerDiscovery()
    : managerUrl_(DEFAULT_MANAGER_URL) {}

std::future<std::string> ManagerDiscovery::discoverManagerUrl(const std::string& /*apiKey*/) {
    // The main endpoint fronts all managers; return it without probing.
    std::string url = managerUrl_;
    std::promise<std::string> p;
    p.set_value(url);
    return p.get_future();
}

void ManagerDiscovery::clearCache() {
    // No cache to clear - kept for API compatibility with other SDKs.
}

} // namespace oddsockets
