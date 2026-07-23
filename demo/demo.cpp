// OddSockets C++ SDK - two-client honest regression demo
//
// Everything here runs through the REAL OddSockets platform: Manager -> Worker
// discovery over HTTP, a genuine Socket.IO (Engine.IO v4) connection per client
// over a WebSocket, and live broadcast fan-out between two SEPARATE connections.
// No mocks, no local echo.
//
// Scenario 1 - core pub/sub:
//   alice subscribes, bob publishes a nonce-tagged message on a second
//   connection, alice receives it via her subscription handler.
//
// Scenario 2 - enhanced (Slack-like) events:
//   both clients subscribe to an enhanced channel. alice registers public
//   on("user_typing") + on("reaction_added") listeners. bob fires
//   enhanced().startTyping() + enhanced().addReaction(). alice receives both
//   broadcasts across the wire.
//
// Run:
//   export ODDSOCKETS_API_KEY="ak_..."   // get a free key: see README
//   ./oddsockets-demo
//
// Exit codes: 0 all green, 1 missing key / setup error, 2 a scenario timed out.

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>

#include "oddsockets/OddSockets.hpp"
#include "oddsockets/EnhancedFeatures.hpp"

using namespace oddsockets;
using namespace std::chrono_literals;

namespace {

std::atomic<bool> g_coreReceived{false};
std::atomic<bool> g_typingSeen{false};
std::atomic<bool> g_reactionSeen{false};

std::string randHex() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llx", static_cast<unsigned long long>(dist(gen)));
    return std::string(buf);
}

// Poll a predicate until true or the deadline passes.
bool waitFor(const std::atomic<bool>& flag, int seconds) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (flag.load()) return true;
        std::this_thread::sleep_for(100ms);
    }
    return flag.load();
}

std::unique_ptr<OddSockets> connectClient(const std::string& apiKey, const std::string& userId) {
    Config config;
    config.apiKey = apiKey;
    config.userId = userId;
    config.autoConnect = false;
    config.enableLogging = false;
    auto client = std::make_unique<OddSockets>(config);
    if (!client->connect().get()) {
        return nullptr;
    }
    return client;
}

// Scenario 1: bob publishes, alice (separate connection) receives.
bool scenarioCore(OddSockets& alice, OddSockets& bob, const std::string& nonce) {
    std::string channelName = "demo-core-" + nonce;
    // Marker lives only inside the message payload, never in the channel name,
    // so a match proves the real payload crossed the wire (not an echo of the
    // channel it arrived on).
    std::string marker = "marker=" + nonce;
    std::cout << "\n=== Scenario 1: core pub/sub on " << channelName << " ===\n";

    auto aliceCh = alice.channel(channelName);
    aliceCh->subscribe([marker](const std::string& message) {
        std::cout << "[alice recv] " << message << "\n";
        if (message.find(marker) != std::string::npos) {
            g_coreReceived = true;
        }
    }).get();
    std::cout << "[alice] subscribed\n";

    std::this_thread::sleep_for(500ms);

    std::string body = "{\"text\":\"hello from bob " + marker +
                       "\",\"username\":\"bob\",\"messageType\":\"demo\"}";
    auto bobCh = bob.channel(channelName);
    auto result = bobCh->publish(body).get();
    std::cout << "[bob] published (ok=" << (result.success ? "true" : "false") << ")\n";

    if (waitFor(g_coreReceived, 15)) {
        std::cout << "[PASS] alice received bob's message across separate connections\n";
        aliceCh->unsubscribe().get();
        return true;
    }
    std::cerr << "[FAIL] alice never received bob's message within 15s\n";
    return false;
}

// Scenario 2: bob fires enhanced typing + reaction, alice receives both
// broadcasts on her public on() surface.
bool scenarioEnhanced(OddSockets& alice, OddSockets& bob, const std::string& nonce) {
    std::string channelName = "demo-enh-" + nonce;
    std::cout << "\n=== Scenario 2: enhanced events on " << channelName << " ===\n";

    auto aliceCh = alice.channel(channelName);
    aliceCh->subscribe([](const std::string&) {}).get();
    auto bobCh = bob.channel(channelName);
    bobCh->subscribe([](const std::string&) {}).get();
    std::cout << "[alice/bob] subscribed to enhanced channel\n";

    alice.on("user_typing", [](const std::string& payload) {
        std::cout << "[alice on user_typing] " << payload << "\n";
        g_typingSeen = true;
    });
    alice.on("reaction_added", [](const std::string& payload) {
        std::cout << "[alice on reaction_added] " << payload << "\n";
        g_reactionSeen = true;
    });

    std::this_thread::sleep_for(500ms);

    std::string anchorBody = "{\"text\":\"enhanced anchor nonce=" + nonce +
                             "\",\"username\":\"bob\",\"messageType\":\"demo\"}";
    bobCh->publish(anchorBody).get();
    std::string messageId = "msg-" + nonce;
    std::cout << "[bob] published anchor\n";

    bob.enhanced().startTyping("bob", channelName);
    std::cout << "[bob] enhanced.startTyping fired\n";

    bob.enhanced().addReaction(messageId, channelName, ":thumbsup:", "bob", "Bob");
    std::cout << "[bob] enhanced.addReaction fired\n";

    bool typing = waitFor(g_typingSeen, 15);
    bool reaction = waitFor(g_reactionSeen, 15);
    aliceCh->unsubscribe().get();
    bobCh->unsubscribe().get();

    if (typing && reaction) {
        std::cout << "[PASS] alice received user_typing AND reaction_added from bob\n";
        return true;
    }
    std::cerr << "[FAIL] enhanced broadcasts missing (typing="
              << (typing ? "true" : "false") << ", reaction="
              << (reaction ? "true" : "false") << ")\n";
    return false;
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    const char* keyEnv = std::getenv("ODDSOCKETS_API_KEY");
    if (!keyEnv || !keyEnv[0]) {
        std::cerr << "Missing ODDSOCKETS_API_KEY. Get a free key (see README), then:\n"
                  << "  export ODDSOCKETS_API_KEY=\"ak_...\"\n";
        return 1;
    }
    std::string apiKey(keyEnv);
    std::string nonce = randHex();

    try {
        auto alice = connectClient(apiKey, "alice");
        auto bob = connectClient(apiKey, "bob");
        if (!alice || !bob) {
            std::cerr << "ERROR: could not connect both clients (worker assignment failed?)\n";
            return 1;
        }
        auto wa = alice->getWorkerInfo();
        auto wb = bob->getWorkerInfo();
        std::cout << "[connect] alice -> " << (wa ? wa->workerId : "?")
                  << ", bob -> " << (wb ? wb->workerId : "?") << "\n";

        if (!scenarioCore(*alice, *bob, nonce)) return 2;
        if (!scenarioEnhanced(*alice, *bob, nonce)) return 2;

        alice->disconnect();
        bob->disconnect();

        std::cout << "\nOK - all scenarios verified live through the OddSockets platform\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
