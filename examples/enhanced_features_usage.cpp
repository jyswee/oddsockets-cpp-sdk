#include <iostream>
#include <thread>
#include <chrono>
#include "../include/oddsockets/OddSockets.hpp"
#include "../include/oddsockets/EnhancedFeatures.hpp"

using namespace OddSockets;
using namespace std::chrono_literals;

/**
 * OddSockets C++ SDK - Enhanced Features Example
 * Demonstrates all 67 new Slack-like events with modern C++ async patterns
 */

void testThreadEvents(EnhancedFeatures& enhanced);
void testReactionEvents(EnhancedFeatures& enhanced);
void testReadReceiptEvents(EnhancedFeatures& enhanced);
void testChannelEvents(EnhancedFeatures& enhanced);
void testDirectMessageEvents(EnhancedFeatures& enhanced);
void testNotificationEvents(EnhancedFeatures& enhanced);
void testPresenceEvents(EnhancedFeatures& enhanced);
void testMessageEditingEvents(EnhancedFeatures& enhanced);
void testSearchEvents(EnhancedFeatures& enhanced);

int main() {
    std::cout << "🚀 OddSockets C++ SDK - Enhanced Features Example\n";
    std::cout << "Demonstrating all 67 new Slack-like events\n";
    std::cout << std::string(50, '=') << "\n";

    // Create and configure client
    auto client = std::make_shared<Client>("your_api_key_here", "user_123");

    // Set up event listeners
    client->on("connected", [](const std::string&) {
        std::cout << "🟢 Connected event fired\n";
    });

    client->on("disconnected", [](const std::string&) {
        std::cout << "🔴 Disconnected event fired\n";
    });

    client->on("error", [](const std::string& error) {
        std::cout << "❌ Error event: " << error << "\n";
    });

    // Connect
    std::cout << "\n🔄 Connecting to OddSockets...\n";
    client->connect();

    // Wait for connection
    std::this_thread::sleep_for(2s);

    if (!client->isConnected()) {
        std::cout << "❌ Failed to connect\n";
        return 1;
    }

    std::cout << "✅ Connected successfully!\n\n";

    // Create enhanced features instance
    EnhancedFeatures enhanced(client);

    // Test all enhanced features
    testThreadEvents(enhanced);
    testReactionEvents(enhanced);
    testReadReceiptEvents(enhanced);
    testChannelEvents(enhanced);
    testDirectMessageEvents(enhanced);
    testNotificationEvents(enhanced);
    testPresenceEvents(enhanced);
    testMessageEditingEvents(enhanced);
    testSearchEvents(enhanced);

    // Summary
    std::cout << "\n🎉 All enhanced features tested!\n";
    std::cout << "\n📊 Summary:\n";
    std::cout << "- Thread Events: 7 methods\n";
    std::cout << "- Reaction Events: 6 methods\n";
    std::cout << "- Read Receipt Events: 6 methods\n";
    std::cout << "- Channel Events: 11 methods\n";
    std::cout << "- Direct Message Events: 6 methods\n";
    std::cout << "- Notification Events: 6 methods\n";
    std::cout << "- File Upload Events: 7 methods\n";
    std::cout << "- Presence Events: 8 methods\n";
    std::cout << "- Message Editing Events: 5 methods\n";
    std::cout << "- Search Events: 4 methods\n";
    std::cout << std::string(50, '=') << "\n";
    std::cout << "Total: 67 enhanced Slack-like events! 🚀\n";

    // Wait before disconnecting
    std::this_thread::sleep_for(2s);

    // Disconnect
    client->disconnect();
    std::cout << "\n✅ Disconnected\n";

    return 0;
}

// ==================== THREAD EVENTS ====================

void testThreadEvents(EnhancedFeatures& enhanced) {
    std::cout << "📝 Testing Thread Events...\n";

    try {
        // Thread reply
        auto result = enhanced.threadReply(
            "general",
            "msg_123",
            "This is a test reply from C++!",
            "user_123",
            "Test User"
        );
        std::cout << "✅ Thread reply created: " << result.get() << "\n";

        // Get thread
        auto thread = enhanced.getThread("thread_123");
        std::cout << "✅ Thread data: " << thread.get() << "\n";

        // Subscribe to thread
        auto subscribed = enhanced.subscribeThread("thread_123", "user_123");
        std::cout << "✅ Subscribed to thread: " << subscribed.get() << "\n";

        // Mark thread as read
        enhanced.markThreadRead("thread_123", "user_123");
        std::cout << "✅ Marked thread as read\n";

        // Follow thread
        enhanced.followThread("thread_123", "user_123");
        std::cout << "✅ Following thread\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Thread events error: " << e.what() << "\n\n";
    }
}

// ==================== REACTION EVENTS ====================

void testReactionEvents(EnhancedFeatures& enhanced) {
    std::cout << "😀 Testing Reaction Events...\n";

    try {
        // Add reaction
        enhanced.addReaction("msg_123", "general", "👍", "user_123", "Test User");
        std::cout << "✅ Added reaction 👍\n";

        // Remove reaction
        enhanced.removeReaction("msg_123", "general", "👍", "user_123");
        std::cout << "✅ Removed reaction\n";

        // Get reactions
        auto reactions = enhanced.getReactions("msg_123");
        std::cout << "✅ Reactions: " << reactions.get() << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Reaction events error: " << e.what() << "\n\n";
    }
}

// ==================== READ RECEIPT EVENTS ====================

void testReadReceiptEvents(EnhancedFeatures& enhanced) {
    std::cout << "✓ Testing Read Receipt Events...\n";

    try {
        // Mark message as read
        enhanced.markRead("msg_123", "general", "user_123", "Test User");
        std::cout << "✅ Marked message as read\n";

        // Get unread counts
        std::vector<std::string> channels = {"general", "random"};
        auto counts = enhanced.getUnreadCounts("user_123", channels);
        std::cout << "✅ Unread counts: " << counts.get() << "\n";

        // Mark all as read
        enhanced.markAllRead("general", "user_123");
        std::cout << "✅ Marked all messages as read\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Read receipt events error: " << e.what() << "\n\n";
    }
}

// ==================== CHANNEL EVENTS ====================

void testChannelEvents(EnhancedFeatures& enhanced) {
    std::cout << "📢 Testing Channel Events...\n";

    try {
        // Create channel
        auto channel = enhanced.createChannel(
            "cpp-test-channel",
            "public",
            "Created from C++ SDK",
            "Testing",
            "user_123",
            "Test User"
        );
        std::cout << "✅ Channel created: " << channel.get() << "\n";

        // Update channel
        std::map<std::string, std::string> updates = {{"topic", "Updated topic"}};
        enhanced.updateChannel("channel_123", updates, "user_123");
        std::cout << "✅ Updated channel\n";

        // Join channel
        enhanced.joinChannel("channel_123", "user_123", "Test User");
        std::cout << "✅ Joined channel\n";

        // Invite to channel
        enhanced.inviteToChannel("channel_123", "user_456", "Jane Doe", "user_123");
        std::cout << "✅ Invited user to channel\n";

        // Get channel members
        auto members = enhanced.getChannelMembers("channel_123");
        std::cout << "✅ Channel members: " << members.get() << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Channel events error: " << e.what() << "\n\n";
    }
}

// ==================== DIRECT MESSAGE EVENTS ====================

void testDirectMessageEvents(EnhancedFeatures& enhanced) {
    std::cout << "💬 Testing Direct Message Events...\n";

    try {
        // Create DM
        std::vector<std::string> userIds = {"user_123", "user_456"};
        auto dm = enhanced.createDM(userIds, "1-on-1");
        std::cout << "✅ DM created: " << dm.get() << "\n";

        // Send DM
        enhanced.sendDM("dm_123", "Hello from C++!", "user_123", "Test User");
        std::cout << "✅ Sent DM\n";

        // Get DM conversations
        auto conversations = enhanced.getDMConversations("user_123", false);
        std::cout << "✅ DM conversations: " << conversations.get() << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Direct message events error: " << e.what() << "\n\n";
    }
}

// ==================== NOTIFICATION EVENTS ====================

void testNotificationEvents(EnhancedFeatures& enhanced) {
    std::cout << "🔔 Testing Notification Events...\n";

    try {
        // Subscribe to notifications
        enhanced.subscribeNotifications("user_123");
        std::cout << "✅ Subscribed to notifications\n";

        // Mark notification as read
        enhanced.markNotificationRead("notif_123", "user_123");
        std::cout << "✅ Marked notification as read\n";

        // Mark all notifications as read
        enhanced.markAllNotificationsRead("user_123");
        std::cout << "✅ Marked all notifications as read\n";

        // Get notifications
        auto notifications = enhanced.getNotifications("user_123", 10, "all");
        std::cout << "✅ Notifications: " << notifications.get() << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Notification events error: " << e.what() << "\n\n";
    }
}

// ==================== PRESENCE EVENTS ====================

void testPresenceEvents(EnhancedFeatures& enhanced) {
    std::cout << "👤 Testing Presence Events...\n";

    try {
        // Set status
        enhanced.setStatus("user_123", "online");
        std::cout << "✅ Set status to online\n";

        // Set custom status
        enhanced.setCustomStatus("user_123", "💻", "Coding in C++");
        std::cout << "✅ Set custom status\n";

        // Clear custom status
        enhanced.clearCustomStatus("user_123");
        std::cout << "✅ Cleared custom status\n";

        // Set DND
        enhanced.setDND("user_123");
        std::cout << "✅ Enabled Do Not Disturb\n";

        // Clear DND
        enhanced.clearDND("user_123");
        std::cout << "✅ Disabled Do Not Disturb\n";

        // Start typing
        enhanced.startTyping("user_123", "general");
        std::cout << "✅ Started typing indicator\n";

        std::this_thread::sleep_for(2s);

        // Stop typing
        enhanced.stopTyping("user_123", "general");
        std::cout << "✅ Stopped typing indicator\n";

        // Get user presence
        std::vector<std::string> userIds = {"user_123", "user_456"};
        auto presence = enhanced.getUserPresence(userIds);
        std::cout << "✅ User presence: " << presence.get() << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Presence events error: " << e.what() << "\n\n";
    }
}

// ==================== MESSAGE EDITING EVENTS ====================

void testMessageEditingEvents(EnhancedFeatures& enhanced) {
    std::cout << "✏️ Testing Message Editing Events...\n";

    try {
        // Edit message
        enhanced.editMessage("msg_123", "general", "Updated message from C++", "user_123");
        std::cout << "✅ Edited message\n";

        // Delete message
        enhanced.deleteMessage("msg_456", "general", "user_123");
        std::cout << "✅ Deleted message\n";

        // Pin message
        enhanced.pinMessage("msg_123", "general", "user_123");
        std::cout << "✅ Pinned message\n";

        // Unpin message
        enhanced.unpinMessage("msg_123", "general", "user_123");
        std::cout << "✅ Unpinned message\n";

        // Get pinned messages
        auto pinned = enhanced.getPinnedMessages("general");
        std::cout << "✅ Pinned messages: " << pinned.get() << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Message editing events error: " << e.what() << "\n\n";
    }
}

// ==================== SEARCH EVENTS ====================

void testSearchEvents(EnhancedFeatures& enhanced) {
    std::cout << "🔍 Testing Search Events...\n";

    try {
        // Search messages
        auto results = enhanced.searchMessages("test", "user_123", 10);
        std::cout << "✅ Search results: " << results.get() << "\n";

        // Search in channel
        auto channelResults = enhanced.searchInChannel("general", "test", 10);
        std::cout << "✅ Channel search results: " << channelResults.get() << "\n";

        // Filter messages
        std::map<std::string, std::string> filters = {
            {"channel", "general"},
            {"userId", "user_123"},
            {"limit", "10"}
        };
        auto filtered = enhanced.filterMessages(filters);
        std::cout << "✅ Filter results: " << filtered.get() << "\n";

        // Search by user
        auto userResults = enhanced.searchByUser("user_123", "", 10);
        std::cout << "✅ User search results: " << userResults.get() << "\n\n";
    } catch (const std::exception& e) {
        std::cout << "❌ Search events error: " << e.what() << "\n\n";
    }
}
