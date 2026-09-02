/**
 * OddSockets C++ SDK - Enhanced (Slack-like) Events Implementation
 *
 * Each method builds the worker's expected camelCase payload and emits the real
 * event over the live Socket.IO connection via OddSockets::emit().
 */

#include "../include/oddsockets/EnhancedFeatures.hpp"
#include "../include/oddsockets/OddSockets.hpp"

#include <sstream>

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

// Render a number without a trailing ".0" for whole values, matching the
// compact JSON the worker expects for metric/progress values.
static std::string jsonNumber(double v) {
    std::ostringstream ss;
    if (v == static_cast<long long>(v)) {
        ss << static_cast<long long>(v);
    } else {
        ss << v;
    }
    return ss.str();
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

// -----------------------------------------------------------------
// Challenge / Leaderboard / Achievement surface
//
// Same honest model as the reaction/typing methods above: build the worker's
// expected camelCase payload and emit the real request event. Success acks and
// error events ({event, message}) arrive back on the client's on() surface.
// -----------------------------------------------------------------

void EnhancedFeatures::createChallenge(const std::string& challengeId,
                                       const std::string& metric,
                                       bool ranked,
                                       const std::string& channel,
                                       const std::string& resultWebhookUrl,
                                       const std::string& standingsUrl) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"challengeId\":\"" + jsonEscape(challengeId) +
                          "\",\"metric\":\"" + jsonEscape(metric) +
                          "\",\"ranked\":" + (ranked ? "true" : "false");
    if (!channel.empty())
        payload += ",\"channel\":\"" + jsonEscape(channel) + "\"";
    if (!resultWebhookUrl.empty())
        payload += ",\"resultWebhookUrl\":\"" + jsonEscape(resultWebhookUrl) + "\"";
    if (!standingsUrl.empty())
        payload += ",\"standingsUrl\":\"" + jsonEscape(standingsUrl) + "\"";
    payload += "}";
    client_->emit("challenge_create", payload);
}

void EnhancedFeatures::reportProgress(const std::string& challengeId,
                                      double value,
                                      const std::string& metric,
                                      const std::string& eventId,
                                      const std::string& cohort,
                                      const std::string& platform,
                                      const std::string& channel) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"challengeId\":\"" + jsonEscape(challengeId) +
                          "\",\"value\":" + jsonNumber(value);
    if (!metric.empty())
        payload += ",\"metric\":\"" + jsonEscape(metric) + "\"";
    if (!eventId.empty())
        payload += ",\"eventId\":\"" + jsonEscape(eventId) + "\"";
    if (!cohort.empty())
        payload += ",\"cohort\":\"" + jsonEscape(cohort) + "\"";
    if (!platform.empty())
        payload += ",\"platform\":\"" + jsonEscape(platform) + "\"";
    if (!channel.empty())
        payload += ",\"channel\":\"" + jsonEscape(channel) + "\"";
    payload += "}";
    // Fire-and-forget: no ack expected.
    client_->emit("challenge_progress", payload);
}

void EnhancedFeatures::completeChallenge(const std::string& challengeId,
                                         const std::string& outcome,
                                         const std::string& eventId,
                                         const std::string& reward) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"challengeId\":\"" + jsonEscape(challengeId) +
                          "\",\"outcome\":\"" + jsonEscape(outcome) + "\"";
    if (!eventId.empty())
        payload += ",\"eventId\":\"" + jsonEscape(eventId) + "\"";
    if (!reward.empty())
        payload += ",\"reward\":\"" + jsonEscape(reward) + "\"";
    payload += "}";
    client_->emit("challenge_complete", payload);
}

void EnhancedFeatures::unlockAchievement(const std::string& achievementId,
                                         const std::string& name,
                                         const std::string& tier,
                                         double percentComplete,
                                         const std::string& challengeId,
                                         const std::string& channel) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"achievementId\":\"" + jsonEscape(achievementId) + "\"";
    if (!name.empty())
        payload += ",\"name\":\"" + jsonEscape(name) + "\"";
    if (!tier.empty())
        payload += ",\"tier\":\"" + jsonEscape(tier) + "\"";
    // percentComplete < 0 means "omitted" -> treated as a full unlock.
    bool hasPercent = percentComplete >= 0.0;
    if (hasPercent)
        payload += ",\"percentComplete\":" + jsonNumber(percentComplete);
    if (!challengeId.empty())
        payload += ",\"challengeId\":\"" + jsonEscape(challengeId) + "\"";
    if (!channel.empty())
        payload += ",\"channel\":\"" + jsonEscape(channel) + "\"";
    payload += "}";
    // Wire contract (worker challengeEvents.js): the client ALWAYS emits
    // "achievement_unlock" carrying percentComplete; the worker derives the
    // OUTBOUND broadcast from it — <100 => achievement_progress (in_progress),
    // >=100 or omitted => achievement_unlock (unlocked). There is no inbound
    // "achievement_progress" listener, so emitting that name drops the progress
    // update silently. (void) the local branch flag; kept for doc clarity.
    (void)hasPercent;
    // Fire-and-forget: no ack expected.
    client_->emit("achievement_unlock", payload);
}

void EnhancedFeatures::getStandings(const std::string& challengeId,
                                    int limit,
                                    int offset) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"challengeId\":\"" + jsonEscape(challengeId) +
                          "\",\"limit\":" + std::to_string(limit) +
                          ",\"offset\":" + std::to_string(offset) + "}";
    client_->emit("challenge_standings", payload);
}

void EnhancedFeatures::getAchievements(const std::string& achievementId) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload;
    if (!achievementId.empty()) {
        payload = "{\"achievementId\":\"" + jsonEscape(achievementId) + "\"}";
    } else {
        payload = "{}";
    }
    client_->emit("achievement_query", payload);
}

void EnhancedFeatures::sendChallengeInvite(const std::string& toUserId,
                                           const std::string& type,
                                           const std::string& payloadJson,
                                           int ttl,
                                           const std::string& channel,
                                           const std::string& inviteId) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"toUserId\":\"" + jsonEscape(toUserId) +
                          "\",\"type\":\"" + jsonEscape(type) + "\"";
    // payload is a caller-supplied JSON value; embed it verbatim when present.
    if (!payloadJson.empty())
        payload += ",\"payload\":" + payloadJson;
    payload += ",\"ttl\":" + std::to_string(ttl);
    if (!channel.empty())
        payload += ",\"channel\":\"" + jsonEscape(channel) + "\"";
    if (!inviteId.empty())
        payload += ",\"inviteId\":\"" + jsonEscape(inviteId) + "\"";
    payload += "}";
    client_->emit("challenge_invite", payload);
}

void EnhancedFeatures::replyChallengeInvite(const std::string& inviteId,
                                            bool accept,
                                            const std::string& reason) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"inviteId\":\"" + jsonEscape(inviteId) +
                          "\",\"accept\":" + (accept ? "true" : "false");
    if (!reason.empty())
        payload += ",\"reason\":\"" + jsonEscape(reason) + "\"";
    payload += "}";
    client_->emit("challenge_reply", payload);
}

void EnhancedFeatures::cancelChallengeInvite(const std::string& inviteId) {
    if (!client_ || !client_->isConnected()) return;
    std::string payload = "{\"inviteId\":\"" + jsonEscape(inviteId) + "\"}";
    client_->emit("challenge_invite_cancel", payload);
}

void EnhancedFeatures::getChallengeInvites() {
    if (!client_ || !client_->isConnected()) return;
    // Empty request payload.
    client_->emit("challenge_invites_query", "{}");
}

const std::vector<std::string>& EnhancedFeatures::inboundChallengeEvents() {
    static const std::vector<std::string> kEvents = {
        "challenge_progress",
        "leaderboard_rank_change",
        "challenge_complete",
        "achievement_unlock",
        "achievement_progress",
        "challenge_invited",
        "challenge_reply_received",
        "challenge_invite_cancelled"
    };
    return kEvents;
}

} // namespace oddsockets
