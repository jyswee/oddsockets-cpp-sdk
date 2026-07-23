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

private:
    OddSockets* client_;
};

} // namespace oddsockets
