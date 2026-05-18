#ifndef ODDSOCKETS_ENHANCED_FEATURES_HPP
#define ODDSOCKETS_ENHANCED_FEATURES_HPP

#include "OddSockets.hpp"
#include <future>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace OddSockets {

/**
 * Enhanced Features for OddSockets C++ SDK
 * Provides 67 new Slack-like events with modern C++ async patterns
 */
class EnhancedFeatures {
public:
    explicit EnhancedFeatures(std::shared_ptr<Client> client);
    ~EnhancedFeatures() = default;

    // Thread Events (7 methods)
    std::future<std::string> threadReply(
        const std::string& channel,
        const std::string& parentMessageId,
        const std::string& message,
        const std::string& userId,
        const std::string& userName);

    std::future<std::string> getThread(const std::string& threadId);

    std::future<std::string> subscribeThread(
        const std::string& threadId,
        const std::string& userId);

    void markThreadRead(const std::string& threadId, const std::string& userId);
    void followThread(const std::string& threadId, const std::string& userId);
    void unfollowThread(const std::string& threadId, const std::string& userId);

    // Reaction Events (6 methods)
    void addReaction(
        const std::string& messageId,
        const std::string& channel,
        const std::string& emoji,
        const std::string& userId,
        const std::string& userName);

    void removeReaction(
        const std::string& messageId,
        const std::string& channel,
        const std::string& emoji,
        const std::string& userId);

    std::future<std::string> getReactions(const std::string& messageId);

    // Read Receipt Events (6 methods)
    void markRead(
        const std::string& messageId,
        const std::string& channel,
        const std::string& userId,
        const std::string& userName);

    std::future<std::string> getUnreadCounts(
        const std::string& userId,
        const std::vector<std::string>& channels);

    void markAllRead(const std::string& channel, const std::string& userId);

    // Channel Events (11 methods)
    std::future<std::string> createChannel(
        const std::string& name,
        const std::string& type,
        const std::string& description,
        const std::string& topic,
        const std::string& createdBy,
        const std::string& createdByName);

    void updateChannel(
        const std::string& channelId,
        const std::map<std::string, std::string>& updates,
        const std::string& userId);

    void archiveChannel(const std::string& channelId, const std::string& userId);

    void inviteToChannel(
        const std::string& channelId,
        const std::string& invitedUserId,
        const std::string& invitedUserName,
        const std::string& invitedBy);

    void removeFromChannel(
        const std::string& channelId,
        const std::string& removedUserId,
        const std::string& removedBy);

    void joinChannel(
        const std::string& channelId,
        const std::string& userId,
        const std::string& userName);

    void leaveChannel(const std::string& channelId, const std::string& userId);

    std::future<std::string> getChannelMembers(const std::string& channelId);

    // Direct Message Events (6 methods)
    std::future<std::string> createDM(
        const std::vector<std::string>& userIds,
        const std::string& type);

    void sendDM(
        const std::string& conversationId,
        const std::string& message,
        const std::string& userId,
        const std::string& userName);

    std::future<std::string> getDMConversations(
        const std::string& userId,
        bool includeArchived);

    // Notification Events (6 methods)
    void subscribeNotifications(const std::string& userId);
    void markNotificationRead(const std::string& notificationId, const std::string& userId);
    void markAllNotificationsRead(const std::string& userId);
    void clearNotifications(const std::string& userId);

    std::future<std::string> getNotifications(
        const std::string& userId,
        int limit,
        const std::string& status = "all");

    // Presence Events (8 methods)
    void setStatus(const std::string& userId, const std::string& status);

    void setCustomStatus(
        const std::string& userId,
        const std::string& emoji,
        const std::string& text,
        const std::string& expiresAt = "");

    void clearCustomStatus(const std::string& userId);
    void setDND(const std::string& userId, const std::string& until = "");
    void clearDND(const std::string& userId);
    void startTyping(const std::string& userId, const std::string& channel);
    void stopTyping(const std::string& userId, const std::string& channel);

    std::future<std::string> getUserPresence(const std::vector<std::string>& userIds);

    // Message Editing Events (5 methods)
    void editMessage(
        const std::string& messageId,
        const std::string& channel,
        const std::string& newContent,
        const std::string& userId);

    void deleteMessage(
        const std::string& messageId,
        const std::string& channel,
        const std::string& userId);

    void pinMessage(
        const std::string& messageId,
        const std::string& channel,
        const std::string& userId);

    void unpinMessage(
        const std::string& messageId,
        const std::string& channel,
        const std::string& userId);

    std::future<std::string> getPinnedMessages(const std::string& channel);

    // Search Events (4 methods)
    std::future<std::string> searchMessages(
        const std::string& query,
        const std::string& userId,
        int limit);

    std::future<std::string> filterMessages(
        const std::map<std::string, std::string>& filters);

    std::future<std::string> searchInChannel(
        const std::string& channel,
        const std::string& query,
        int limit);

    std::future<std::string> searchByUser(
        const std::string& userId,
        const std::string& query,
        int limit);

private:
    std::shared_ptr<Client> client_;
    
    template<typename Func>
    std::future<std::string> asyncEmit(const std::string& event, Func&& buildParams);
};

} // namespace OddSockets

#endif // ODDSOCKETS_ENHANCED_FEATURES_HPP
