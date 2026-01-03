#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward declare sqlite3 to avoid including the header
struct sqlite3;
struct sqlite3_stmt;

namespace auth {

using u64 = uint64_t;
using u32 = uint32_t;
using u8 = uint8_t;

// Data structures
struct Account {
    u64 accountId = 0;
    std::string username;
    std::string passwordHash;
    std::string email;
    u64 createdAt = 0;
    u64 lastLogin = 0;
    bool isBanned = false;
    std::string banReason;
    u64 banUntil = 0;
    // Account locking fields
    int failedLoginAttempts = 0;
    u64 lockedUntil = 0;
    u64 lastFailedAttempt = 0;
};

struct Session {
    std::string token;
    u64 accountId = 0;
    u64 createdAt = 0;
    u64 expiresAt = 0;
    std::string ipAddress;
    u64 lastUsed = 0;
};

struct LoginHistoryEntry {
    u64 historyId = 0;
    u64 accountId = 0;
    std::string ipAddress;
    u64 timestamp = 0;
    bool success = false;
    std::string failureReason;
};

/**
 * DatabaseManager handles all database operations for the authentication system.
 * Uses SQLite for persistent storage with parameterized queries to prevent SQL injection.
 */
class DatabaseManager {
public:
    DatabaseManager() = default;
    ~DatabaseManager();

    // Disable copy
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    /**
     * Initialize database connection and create schema if needed.
     * @param dbPath Path to SQLite database file
     * @return true if successful
     */
    bool Initialize(const std::string& dbPath);

    /**
     * Close database connection and cleanup resources.
     */
    void Shutdown();

    /**
     * Cleanup expired sessions from database.
     * @return Number of sessions deleted
     */
    int CleanupExpiredSessions();

    /**
     * Cleanup old login history entries.
     * @param olderThanDays Delete entries older than this many days
     * @return Number of entries deleted
     */
    int CleanupOldLoginHistory(int olderThanDays = 90);

    /**
     * Cleanup expired rate limit entries.
     * @return Number of entries deleted
     */
    int CleanupExpiredRateLimits();

    // Account operations
    bool CreateAccount(const std::string& username, const std::string& passwordHash, u64& outAccountId);
    bool GetAccountByUsername(const std::string& username, Account& outAccount);
    bool GetAccountById(u64 accountId, Account& outAccount);
    bool UpdateLastLogin(u64 accountId, u64 timestamp);
    bool UpdatePassword(u64 accountId, const std::string& newPasswordHash);
    bool BanAccount(u64 accountId, const std::string& reason, u64 banUntil);
    bool UnbanAccount(u64 accountId);
    
    // Account locking (for failed login attempts)
    bool LockAccount(u64 accountId, u64 lockUntil);
    bool UnlockAccount(u64 accountId);
    bool IsAccountLocked(u64 accountId);
    bool IncrementFailedLoginAttempts(u64 accountId, int& outAttemptCount);
    bool ResetFailedLoginAttempts(u64 accountId);
    bool GetFailedLoginAttempts(u64 accountId, int& outAttemptCount, u64& outLastAttempt);

    // Session operations
    bool CreateSession(u64 accountId, const std::string& token, u64 expiresAt, const std::string& ipAddress);
    bool GetSession(const std::string& token, Session& outSession);
    bool UpdateSessionExpiration(const std::string& token, u64 newExpiresAt);
    bool DeleteSession(const std::string& token);
    u32 DeleteAllSessionsForAccount(u64 accountId, const std::string& exceptToken = "");

    // Login history
    bool LogLoginAttempt(u64 accountId, const std::string& ipAddress, bool success, u64 timestamp);
    bool GetLoginHistory(u64 accountId, std::vector<LoginHistoryEntry>& outHistory, int limit = 10);

    // Rate limiting
    bool IncrementRateLimit(const std::string& key, u64 timestamp, int& outCount);
    bool IsRateLimited(const std::string& key, u64 timestamp, int maxAttempts, u64 windowSeconds);

private:
    bool CreateSchema();
    bool ExecuteSQL(const char* sql);
    bool EnableWALMode();
    bool OptimizeDatabase();
    
    sqlite3* db_ = nullptr;
};

} // namespace auth
