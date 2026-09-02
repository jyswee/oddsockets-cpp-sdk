/**
 * OddSockets C++ SDK - Enhanced (Slack-like) Events
 *
 * Thin, honest wrapper over the live Socket.IO connection. Each method emits a
 * real worker event through OddSockets::emit(); the worker broadcasts the
 * matching event (e.g. add_reaction -> reaction_added, start_typing ->
 * user_typing) to the scoped channel room, where subscribers pick it up on
 * their public OddSockets::on() surface.
 *
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#pragma once

#include <string>
#include <vector>

namespace oddsockets {

class OddSockets;

/**
 * Enhanced event surface bound to a single OddSockets client.
 * Obtain via OddSockets::enhanced().
 */
class EnhancedFeatures {
public:
    explicit EnhancedFeatures(OddSockets* client);
    ~EnhancedFeatures() = default;

    EnhancedFeatures(const EnhancedFeatures&) = delete;
    EnhancedFeatures& operator=(const EnhancedFeatures&) = delete;

    /**
     * Signal that a user has started typing in a channel.
     * Worker broadcasts "user_typing" to other subscribers (excludes sender).
     */
    void startTyping(const std::string& userId, const std::string& channel);

    /**
     * Signal that a user has stopped typing in a channel.
     * Worker broadcasts "user_stopped_typing" to other subscribers.
     */
    void stopTyping(const std::string& userId, const std::string& channel);

    /**
     * Add an emoji reaction to a message.
     * Worker broadcasts "reaction_added" to the channel room (includes sender).
     */
    void addReaction(const std::string& messageId,
                     const std::string& channel,
                     const std::string& emoji,
                     const std::string& userId,
                     const std::string& userName);

    /**
     * Remove an emoji reaction from a message.
     * Worker broadcasts "reaction_removed" to the channel room.
     */
    void removeReaction(const std::string& messageId,
                        const std::string& channel,
                        const std::string& emoji,
                        const std::string& userId);

    // -----------------------------------------------------------------
    // Challenge / Leaderboard / Achievement surface
    //
    // Each method emits a real worker request event via OddSockets::emit().
    // The worker replies with a success ack event (e.g. "challenge_create" ->
    // "challenge_create_success") on failure an error event carrying
    // {event, message}. Subscribe to those, and to the inbound broadcast
    // events (see inboundChallengeEvents()), via OddSockets::on(). This mirrors
    // the reaction/typing surface: request out over emit(), results in over on().
    // -----------------------------------------------------------------

    /**
     * Create a challenge / leaderboard.
     * Emits "challenge_create". Ack: "challenge_create_success" | err "challenge_create".
     * @param challengeId       Unique challenge identifier.
     * @param metric            Metric being scored (e.g. "kills", "time").
     * @param ranked            Whether standings are ranked (default true).
     * @param channel           Optional scoped channel (empty = default scope).
     * @param resultWebhookUrl  Optional webhook fired on completion.
     * @param standingsUrl      Optional URL for out-of-band standings snapshots.
     */
    void createChallenge(const std::string& challengeId,
                         const std::string& metric,
                         bool ranked = true,
                         const std::string& channel = "",
                         const std::string& resultWebhookUrl = "",
                         const std::string& standingsUrl = "");

    /**
     * Report incremental progress toward a challenge. Fire-and-forget.
     * Emits "challenge_progress" (no ack).
     * @param challengeId  Target challenge.
     * @param value        Progress value for the metric.
     * @param metric       Optional metric override.
     * @param eventId      Optional idempotency / dedupe id.
     * @param cohort       Optional cohort / bracket label.
     * @param platform     Optional platform label.
     * @param channel      Optional scoped channel.
     */
    void reportProgress(const std::string& challengeId,
                        double value,
                        const std::string& metric = "",
                        const std::string& eventId = "",
                        const std::string& cohort = "",
                        const std::string& platform = "",
                        const std::string& channel = "");

    /**
     * Complete a challenge with a final outcome.
     * Emits "challenge_complete". Ack: "challenge_complete_success" | err "challenge_complete".
     * @param challengeId  Target challenge.
     * @param outcome      One of: completed, failed, expired, conceded, tied.
     * @param eventId      Optional idempotency id.
     * @param reward       Optional reward descriptor.
     */
    void completeChallenge(const std::string& challengeId,
                           const std::string& outcome,
                           const std::string& eventId = "",
                           const std::string& reward = "");

    /**
     * Unlock (or advance) an achievement. Fire-and-forget.
     * Emits "achievement_unlock" when percentComplete >= 100 or omitted,
     * otherwise "achievement_progress".
     * @param achievementId    Achievement identifier.
     * @param name             Optional display name.
     * @param tier             Optional tier label.
     * @param percentComplete  Progress percent; < 0 means "omitted" (full unlock).
     * @param challengeId      Optional owning challenge.
     * @param channel          Optional scoped channel.
     */
    void unlockAchievement(const std::string& achievementId,
                           const std::string& name = "",
                           const std::string& tier = "",
                           double percentComplete = -1.0,
                           const std::string& challengeId = "",
                           const std::string& channel = "");

    /**
     * Request leaderboard standings.
     * Emits "challenge_standings". Ack: "challenge_standings_success" | err "challenge_standings".
     * @param challengeId  Target challenge.
     * @param limit        Max rows (default 20).
     * @param offset       Row offset (default 0).
     */
    void getStandings(const std::string& challengeId,
                      int limit = 20,
                      int offset = 0);

    /**
     * Query achievement state.
     * Emits "achievement_query". Ack: "achievement_state" | err "achievement_query".
     * @param achievementId  Optional single achievement filter (empty = all).
     */
    void getAchievements(const std::string& achievementId = "");

    /**
     * Send a directed challenge invite to another user.
     * Emits "challenge_invite". Ack: "challenge_invite_success" | err "challenge_invite".
     * @param toUserId  Recipient user id.
     * @param type      Invite type (default "match").
     * @param payload   Optional JSON payload string (<= 8KB).
     * @param ttl       Time-to-live seconds (default 300).
     * @param channel   Optional scoped channel.
     * @param inviteId  Optional caller-supplied invite id.
     */
    void sendChallengeInvite(const std::string& toUserId,
                             const std::string& type = "match",
                             const std::string& payload = "",
                             int ttl = 300,
                             const std::string& channel = "",
                             const std::string& inviteId = "");

    /**
     * Reply to a received challenge invite.
     * Emits "challenge_reply". Ack: "challenge_reply_success" | err "challenge_reply".
     * @param inviteId  Invite being answered.
     * @param accept    Whether the invite is accepted.
     * @param reason    Optional reason (e.g. decline reason).
     */
    void replyChallengeInvite(const std::string& inviteId,
                              bool accept,
                              const std::string& reason = "");

    /**
     * Cancel a previously sent challenge invite.
     * Emits "challenge_invite_cancel". Ack: "challenge_invite_cancel_success" | err "challenge_invite_cancel".
     * @param inviteId  Invite to cancel.
     */
    void cancelChallengeInvite(const std::string& inviteId);

    /**
     * Query outstanding challenge invites (empty request payload).
     * Emits "challenge_invites_query". Ack: "challenge_invites" | err "challenge_invites_query".
     */
    void getChallengeInvites();

    /**
     * Names of the inbound challenge broadcast events pushed by the worker.
     * Subscribe to any of these on the public OddSockets::on() surface to
     * receive live updates. Delivery is generic: dispatchSocketIoEvent() fans
     * every decoded event to its registered on() handlers, exactly as it does
     * for "reaction_added".
     */
    static const std::vector<std::string>& inboundChallengeEvents();

private:
    OddSockets* client_;
};

} // namespace oddsockets
