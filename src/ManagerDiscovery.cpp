/**
 * OddSockets C++ SDK - Manager Discovery Implementation
 *
 * Resolves the configured manager endpoint and hands it back verbatim. The
 * built-in default is a starting point for an unconfigured client, never a
 * recovery path for a configured manager that is unreachable.
 */

#include "../include/oddsockets/ManagerDiscovery.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace oddsockets {

namespace {

std::string trim(const std::string& value) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

    auto begin = std::find_if_not(value.begin(), value.end(), isSpace);
    auto end = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();

    return (begin < end) ? std::string(begin, end) : std::string();
}

bool hasSchemeCaseInsensitive(const std::string& value, const std::string& lowercaseScheme) {
    if (value.size() < lowercaseScheme.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lowercaseScheme.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) != lowercaseScheme[i]) {
            return false;
        }
    }
    return true;
}

// An acceptable manager URL is absolute, http(s), and names a host.
bool isAbsoluteHttpUrl(const std::string& value) {
    std::size_t schemeLength = 0;
    if (hasSchemeCaseInsensitive(value, "https://")) {
        schemeLength = 8;
    } else if (hasSchemeCaseInsensitive(value, "http://")) {
        schemeLength = 7;
    } else {
        return false;
    }

    if (value.size() <= schemeLength) {
        return false;
    }

    const char first = value[schemeLength];
    return first != '/' && first != '?' && first != '#';
}

} // namespace

ManagerDiscovery::ManagerDiscovery(const std::string& configuredUrl)
    : managerUrl_(resolveManagerUrl(configuredUrl)) {}

std::future<std::string> ManagerDiscovery::discoverManagerUrl() {
    std::promise<std::string> p;
    p.set_value(managerUrl_);
    return p.get_future();
}

std::string ManagerDiscovery::resolveManagerUrl(const std::string& configuredUrl) {
    const std::string configured = trim(configuredUrl);
    if (!configured.empty()) {
        return validateManagerUrl(configured);
    }
    return validateManagerUrl(getDefaultManagerUrl());
}

std::string ManagerDiscovery::validateManagerUrl(const std::string& value) {
    std::string normalized = trim(value);
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }

    if (!isAbsoluteHttpUrl(normalized)) {
        throw Exception(ErrorCode::InvalidParameter, "Invalid managerUrl: " + value);
    }

    return normalized;
}

std::string ManagerDiscovery::getDefaultManagerUrl() {
    const char* fromEnvironment = std::getenv("ODDSOCKETS_MANAGER_URL");
    if (fromEnvironment != nullptr) {
        const std::string trimmed = trim(fromEnvironment);
        if (!trimmed.empty()) {
            return trimmed;
        }
    }
    return DEFAULT_MANAGER_URL;
}

std::string defaultManagerUrl() {
    return ManagerDiscovery::getDefaultManagerUrl();
}

} // namespace oddsockets
