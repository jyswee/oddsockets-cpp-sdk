/**
 * OddSockets C++ SDK - Message Size Validator
 * 
 * Modern C++ SDK for the OddSockets real-time messaging platform,
 * optimized for embedded systems and IoT devices.
 * 
 * Copyright (c) 2024 OddSockets
 * Licensed under the MIT License
 */

#pragma once

#include "Types.hpp"

#include <string>

namespace oddsockets {

/**
 * Message Size Validator
 * 
 * Validates message sizes against industry standard limits (32KB).
 * This matches the limits used by PubNub, Socket.IO, and other real-time messaging platforms.
 * 
 * This follows the same validation logic as the JavaScript SDK.
 */
class MessageSizeValidator {
public:
    /**
     * Validate message size
     * @param message Message to validate
     * @return true if message is within size limits, false otherwise
     */
    static bool validate(const std::string& message);
    
    /**
     * Validate message size and throw exception if invalid
     * @param message Message to validate
     * @throws Exception if message exceeds size limit
     */
    static void validateOrThrow(const std::string& message);
    
    /**
     * Get the size of a message in bytes
     * @param message Message to measure
     * @return Size in bytes
     */
    static size_t getMessageSize(const std::string& message);
    
    /**
     * Get the maximum allowed message size
     * @return Maximum message size in bytes
     */
    static constexpr size_t getMaxMessageSize() {
        return MAX_MESSAGE_SIZE;
    }
    
    /**
     * Get the maximum allowed message size in KB
     * @return Maximum message size in KB
     */
    static constexpr size_t getMaxMessageSizeKB() {
        return MAX_MESSAGE_SIZE_KB;
    }
    
    /**
     * Create a size validation error message
     * @param actualSize Actual message size in bytes
     * @return Formatted error message
     */
    static std::string createSizeErrorMessage(size_t actualSize);

private:
    // Static class - no instances allowed
    MessageSizeValidator() = delete;
    ~MessageSizeValidator() = delete;
    MessageSizeValidator(const MessageSizeValidator&) = delete;
    MessageSizeValidator& operator=(const MessageSizeValidator&) = delete;
};

// Inline implementations for performance

inline bool MessageSizeValidator::validate(const std::string& message) {
    return getMessageSize(message) <= MAX_MESSAGE_SIZE;
}

inline size_t MessageSizeValidator::getMessageSize(const std::string& message) {
    // For UTF-8 strings, byte size equals string length for ASCII characters
    // For proper UTF-8 support, we would need to count actual bytes
    // For embedded systems, this simple approach is often sufficient
    return message.size();
}

inline void MessageSizeValidator::validateOrThrow(const std::string& message) {
    size_t messageSize = getMessageSize(message);
    if (messageSize > MAX_MESSAGE_SIZE) {
        throw Exception(ErrorCode::MessageTooLarge, createSizeErrorMessage(messageSize));
    }
}

inline std::string MessageSizeValidator::createSizeErrorMessage(size_t actualSize) {
    size_t actualSizeKB = (actualSize + 1023) / 1024; // Round up to KB
    return "Message size (" + std::to_string(actualSizeKB) + "KB) exceeds maximum allowed size of " +
           std::to_string(MAX_MESSAGE_SIZE_KB) + "KB. " +
           "This limit matches industry standards (PubNub, Socket.IO) for reliable real-time messaging.";
}

// Utility function for backward compatibility with Types.hpp
inline bool validateMessageSize(const std::string& message) {
    return MessageSizeValidator::validate(message);
}

} // namespace oddsockets
