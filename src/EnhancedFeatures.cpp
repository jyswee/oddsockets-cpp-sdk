/**
 * OddSockets C++ SDK - Enhanced (Slack-like) Events Implementation
 *
 * Each method builds the worker's expected camelCase payload and emits the real
 * event over the live Socket.IO connection via OddSockets::emit().
 */

#include "../include/oddsockets/EnhancedFeatures.hpp"
#include "../include/oddsockets/OddSockets.hpp"

namespace oddsockets {

// Escape a value for embedding inside a JSON string literal.
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

EnhancedFeatures::EnhancedFeatures(OddSockets* client) : client_(client) {}

void EnhancedFeatures::startTyping(const std::string& userId, const std::string& channel) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"userId\":\"" + jsonEscape(userId) +
                          "\",\"channel\":\"" + jsonEscape(channel) + "\"}";
    client_->emit("start_typing", payload);
}

void EnhancedFeatures::stopTyping(const std::string& userId, const std::string& channel) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"userId\":\"" + jsonEscape(userId) +
                          "\",\"channel\":\"" + jsonEscape(channel) + "\"}";
    client_->emit("stop_typing", payload);
}

void EnhancedFeatures::addReaction(const std::string& messageId,
                                   const std::string& channel,
                                   const std::string& emoji,
                                   const std::string& userId,
                                   const std::string& userName) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"messageId\":\"" + jsonEscape(messageId) +
                          "\",\"channel\":\"" + jsonEscape(channel) +
                          "\",\"emoji\":\"" + jsonEscape(emoji) +
                          "\",\"userId\":\"" + jsonEscape(userId) +
                          "\",\"userName\":\"" + jsonEscape(userName) + "\"}";
    client_->emit("add_reaction", payload);
}

void EnhancedFeatures::removeReaction(const std::string& messageId,
                                      const std::string& channel,
                                      const std::string& emoji,
                                      const std::string& userId) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"messageId\":\"" + jsonEscape(messageId) +
                          "\",\"channel\":\"" + jsonEscape(channel) +
                          "\",\"emoji\":\"" + jsonEscape(emoji) +
                          "\",\"userId\":\"" + jsonEscape(userId) + "\"}";
    client_->emit("remove_reaction", payload);
}

} // namespace oddsockets
