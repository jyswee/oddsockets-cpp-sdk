/**
 * Manager Discovery Implementation for OddSockets C++ SDK
 * 
 * Handles automatic discovery of the optimal manager URL for load balancing
 * and high availability. Follows the JavaScript SDK pattern for consistency.
 */

#include "../include/oddsockets/ManagerDiscovery.hpp"
#include "../include/oddsockets/Types.hpp"
#include <curl/curl.h>
#include <json/json.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>

namespace OddSockets {

// Static member definitions
std::vector<std::string> ManagerDiscovery::customManagerUrls_;
std::mutex ManagerDiscovery::mutex_;

// Default manager URLs for fallback
const std::vector<std::string> ManagerDiscovery::DEFAULT_MANAGER_URLS = {
    "https://manager1.oddsockets.tyga.network",
    "https://manager-us-east.oddsockets.com",
    "https://manager-us-west.oddsockets.com",
    "https://manager-eu.oddsockets.com",
    "https://manager-asia.oddsockets.com"
};

// Callback function for libcurl to write response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string ManagerDiscovery::discoverManagerUrl(const std::string& apiKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (apiKey.empty()) {
        throw OddSocketsException("API key cannot be empty", "INVALID_API_KEY");
    }
    
    std::vector<ManagerInfo> managers;
    discoverAllManagers(apiKey, managers);
    
    if (managers.empty()) {
        throw OddSocketsException("No available managers found", "NO_MANAGERS_AVAILABLE");
    }
    
    // Sort by response time (ascending) and availability
    std::sort(managers.begin(), managers.end(), [](const ManagerInfo& a, const ManagerInfo& b) {
        if (a.isAvailable != b.isAvailable) {
            return a.isAvailable > b.isAvailable; // Available managers first
        }
        return a.responseTimeMs < b.responseTimeMs; // Faster response time first
    });
    
    return managers[0].url;
}

void ManagerDiscovery::discoverAllManagers(const std::string& apiKey, std::vector<ManagerInfo>& managers) {
    managers.clear();
    
    std::vector<std::string> urlsToTest;
    
    // Use custom URLs if set, otherwise use defaults
    if (!customManagerUrls_.empty()) {
        urlsToTest = customManagerUrls_;
    } else {
        urlsToTest = DEFAULT_MANAGER_URLS;
    }
    
    // Test each manager URL
    for (const auto& url : urlsToTest) {
        ManagerInfo info;
        info.url = url;
        info.region = extractRegionFromUrl(url);
        info.loadScore = 0; // Will be populated by actual load balancing logic
        
        try {
            info.isAvailable = testManagerConnectivity(url, apiKey, info.responseTimeMs);
        } catch (const std::exception&) {
            info.isAvailable = false;
            info.responseTimeMs = INT_MAX;
        }
        
        managers.push_back(info);
    }
}

bool ManagerDiscovery::testManagerConnectivity(const std::string& managerUrl, 
                                              const std::string& apiKey, 
                                              int& responseTimeMs) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw OddSocketsException("Failed to initialize CURL", "NETWORK_ERROR");
    }
    
    std::string response;
    std::string url = managerUrl + "/api/health?apiKey=" + apiKey;
    
    // Record start time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Configure CURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 5 second timeout
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Set User-Agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OddSockets-CPP-SDK/1.0.0");
    
    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    
    // Calculate response time
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    responseTimeMs = static_cast<int>(duration.count());
    
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return false;
    }
    
    if (httpCode != 200) {
        return false;
    }
    
    // Parse JSON response to verify it's a valid manager
    try {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(response, root)) {
            return false;
        }
        
        // Check if response indicates healthy manager
        if (root.isMember("status") && root["status"].asString() == "healthy") {
            return true;
        }
    } catch (const std::exception&) {
        return false;
    }
    
    return false;
}

std::vector<std::string> ManagerDiscovery::getDefaultManagerUrls() {
    return DEFAULT_MANAGER_URLS;
}

void ManagerDiscovery::setCustomManagerUrls(const std::vector<std::string>& urls) {
    std::lock_guard<std::mutex> lock(mutex_);
    customManagerUrls_ = urls;
}

void ManagerDiscovery::clearCustomManagerUrls() {
    std::lock_guard<std::mutex> lock(mutex_);
    customManagerUrls_.clear();
}

std::string ManagerDiscovery::extractRegionFromUrl(const std::string& url) {
    // Simple region extraction from URL patterns
    if (url.find("us-east") != std::string::npos) {
        return "us-east";
    } else if (url.find("us-west") != std::string::npos) {
        return "us-west";
    } else if (url.find("eu") != std::string::npos) {
        return "europe";
    } else if (url.find("asia") != std::string::npos) {
        return "asia";
    } else {
        return "global";
    }
}

bool ManagerDiscovery::isValidUrl(const std::string& url) {
    // Basic URL validation
    if (url.empty()) {
        return false;
    }
    
    // Must start with http:// or https://
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        return false;
    }
    
    // Must have a domain
    size_t domainStart = url.find("://") + 3;
    if (domainStart >= url.length()) {
        return false;
    }
    
    return true;
}

} // namespace OddSockets
