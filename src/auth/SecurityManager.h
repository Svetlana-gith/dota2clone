#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <mutex>

namespace auth {

using u64 = uint64_t;
using u32 = uint32_t;
using u8 = uint8_t;

/**
 * Rate limit types for different operations
 */
enum class RateLimitType {
    Login,
    Register,
    TokenValidation,
    PasswordReset
};

/**
 * Rate limit state for tracking attempts
 */
struct RateLimitState {
    int attemptCount = 0;
    u64 windowStart = 0;
    u64 lastAttempt = 0;
};

/**
 * SecurityManager handles all security-related operations:
 * - Password hashing (bcrypt)
 * - Token generation (cryptographically secure)
 * - Rate limiting
 * - IP blacklist management
 * - Suspicious activity detection
 */
class SecurityManager {
public:
    SecurityManager() = default;
    ~SecurityManager() = default;

    // Disable copy
    SecurityManager(const SecurityManager&) = delete;
    SecurityManager& operator=(const SecurityManager&) = delete;

    /**
     * Hash a password using bcrypt.
     * @param password Plain text password (or SHA256 hash from client)
     * @param cost Bcrypt cost factor (default: 12)
     * @return Bcrypt hash string
     */
    std::string HashPassword(const std::string& password, int cost = 12);

    /**
     * Verify a password against a bcrypt hash.
     * @param password Plain text password to verify
     * @param hash Bcrypt hash to verify against
     * @return true if password matches
     */
    bool VerifyPassword(const std::string& password, const std::string& hash);

    /**
     * Hash data using SHA256.
     * @param data Input data to hash
     * @return Hex-encoded SHA256 hash
     */
    std::string SHA256Hash(const std::string& data);

    /**
     * Generate a cryptographically secure random token.
     * @param length Token length in bytes (default: 32)
     * @return Hex-encoded random token
     */
    std::string GenerateSecureToken(size_t length = 32);

    /**
     * Generate a cryptographically secure random number.
     * @return Random 64-bit unsigned integer
     */
    u64 GenerateSecureRandom();

    /**
     * Check if an IP address has exceeded rate limits.
     * @param ipAddress IP address to check
     * @param type Type of rate limit
     * @return true if rate limited
     */
    bool CheckRateLimit(const std::string& ipAddress, RateLimitType type);

    /**
     * Record an attempt for rate limiting.
     * @param ipAddress IP address making the attempt
     * @param type Type of rate limit
     */
    void RecordAttempt(const std::string& ipAddress, RateLimitType type);

    /**
     * Reset rate limit for an IP address.
     * @param ipAddress IP address to reset
     * @param type Type of rate limit
     */
    void ResetRateLimit(const std::string& ipAddress, RateLimitType type);

    /**
     * Check if an IP address is blacklisted.
     * @param ipAddress IP address to check
     * @return true if blacklisted
     */
    bool IsBlacklisted(const std::string& ipAddress);

    /**
     * Add an IP address to the blacklist.
     * @param ipAddress IP address to blacklist
     * @param durationSeconds Duration in seconds (0 = permanent)
     */
    void AddToBlacklist(const std::string& ipAddress, u64 durationSeconds = 0);

    /**
     * Remove an IP address from the blacklist.
     * @param ipAddress IP address to remove
     */
    void RemoveFromBlacklist(const std::string& ipAddress);

    /**
     * Clean up expired blacklist entries.
     * @return Number of entries removed
     */
    int CleanupExpiredBlacklist();

    /**
     * Detect suspicious activity for an account.
     * @param accountId Account ID to check
     * @param ipAddress Current IP address
     * @return true if activity is suspicious
     */
    bool IsSuspiciousActivity(u64 accountId, const std::string& ipAddress);

    /**
     * Record a successful login for suspicious activity detection.
     * @param accountId Account ID
     * @param ipAddress IP address used
     */
    void RecordLogin(u64 accountId, const std::string& ipAddress);

    /**
     * Get rate limit configuration for a type.
     * @param type Rate limit type
     * @param outMaxAttempts Maximum attempts allowed
     * @param outWindowSeconds Time window in seconds
     */
    void GetRateLimitConfig(RateLimitType type, int& outMaxAttempts, u64& outWindowSeconds);

private:
    std::string MakeRateLimitKey(const std::string& ipAddress, RateLimitType type);
    
    // Rate limiting state (in-memory)
    std::unordered_map<std::string, RateLimitState> rateLimits_;
    std::mutex rateLimitMutex_;

    // IP blacklist with expiration times
    struct BlacklistEntry {
        u64 expiresAt = 0; // 0 = permanent
    };
    std::unordered_map<std::string, BlacklistEntry> ipBlacklist_;
    std::mutex blacklistMutex_;

    // Account IP history for suspicious activity detection
    std::unordered_map<u64, std::vector<std::string>> accountIpHistory_;
    std::mutex activityMutex_;

    // Rate limit configuration
    struct RateLimitConfig {
        int maxAttempts;
        u64 windowSeconds;
    };
    std::unordered_map<RateLimitType, RateLimitConfig> rateLimitConfigs_ = {
        {RateLimitType::Login, {5, 60}},           // 5 attempts per minute
        {RateLimitType::Register, {3, 300}},       // 3 attempts per 5 minutes
        {RateLimitType::TokenValidation, {100, 60}}, // 100 per minute
        {RateLimitType::PasswordReset, {3, 3600}}  // 3 per hour
    };
};

} // namespace auth
