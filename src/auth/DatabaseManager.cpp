#include "auth/DatabaseManager.h"
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <ctime>

namespace auth {

DatabaseManager::~DatabaseManager() {
    Shutdown();
}

bool DatabaseManager::Initialize(const std::string& dbPath) {
    if (db_) {
        spdlog::warn("DatabaseManager already initialized");
        return true;
    }

    int rc = sqlite3_open(dbPath.c_str(), &db_);
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to open database: {}", sqlite3_errmsg(db_));
        db_ = nullptr;
        return false;
    }

    spdlog::info("Database opened: {}", dbPath);

    // Enable WAL mode for better concurrency
    if (!EnableWALMode()) {
        spdlog::warn("Failed to enable WAL mode, continuing with default journal mode");
    }

    // Optimize database settings
    if (!OptimizeDatabase()) {
        spdlog::warn("Failed to optimize database settings");
    }

    if (!CreateSchema()) {
        spdlog::error("Failed to create database schema");
        Shutdown();
        return false;
    }

    return true;
}

void DatabaseManager::Shutdown() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
        spdlog::info("Database closed");
    }
}

bool DatabaseManager::CreateSchema() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS accounts (
            account_id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            email TEXT UNIQUE,
            created_at INTEGER NOT NULL,
            last_login INTEGER,
            is_banned INTEGER DEFAULT 0,
            ban_reason TEXT,
            ban_until INTEGER,
            failed_login_attempts INTEGER DEFAULT 0,
            locked_until INTEGER DEFAULT 0,
            last_failed_attempt INTEGER DEFAULT 0
        );

        CREATE INDEX IF NOT EXISTS idx_accounts_username ON accounts(username);
        CREATE INDEX IF NOT EXISTS idx_accounts_email ON accounts(email);

        CREATE TABLE IF NOT EXISTS sessions (
            session_token TEXT PRIMARY KEY,
            account_id INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            ip_address TEXT,
            last_used INTEGER,
            FOREIGN KEY(account_id) REFERENCES accounts(account_id)
        );

        CREATE INDEX IF NOT EXISTS idx_sessions_account ON sessions(account_id);
        CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(expires_at);

        CREATE TABLE IF NOT EXISTS login_history (
            history_id INTEGER PRIMARY KEY AUTOINCREMENT,
            account_id INTEGER NOT NULL,
            ip_address TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            success INTEGER NOT NULL,
            failure_reason TEXT,
            FOREIGN KEY(account_id) REFERENCES accounts(account_id)
        );

        CREATE INDEX IF NOT EXISTS idx_login_history_account ON login_history(account_id, timestamp DESC);
        CREATE INDEX IF NOT EXISTS idx_login_history_ip ON login_history(ip_address, timestamp DESC);

        CREATE TABLE IF NOT EXISTS rate_limits (
            limit_key TEXT PRIMARY KEY,
            attempt_count INTEGER NOT NULL,
            window_start INTEGER NOT NULL,
            last_attempt INTEGER NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_rate_limits_window ON rate_limits(window_start);
    )";

    // First create the schema
    if (!ExecuteSQL(schema)) {
        return false;
    }
    
    // Add new columns if they don't exist (for migration from older schema)
    // SQLite doesn't support IF NOT EXISTS for ALTER TABLE, so we check first
    const char* checkColumn = "SELECT failed_login_attempts FROM accounts LIMIT 1";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, checkColumn, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        // Column doesn't exist, add it
        ExecuteSQL("ALTER TABLE accounts ADD COLUMN failed_login_attempts INTEGER DEFAULT 0");
        ExecuteSQL("ALTER TABLE accounts ADD COLUMN locked_until INTEGER DEFAULT 0");
        ExecuteSQL("ALTER TABLE accounts ADD COLUMN last_failed_attempt INTEGER DEFAULT 0");
    }
    if (stmt) {
        sqlite3_finalize(stmt);
    }
    
    return true;
}

bool DatabaseManager::ExecuteSQL(const char* sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        spdlog::error("SQL error: {}", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::CreateAccount(const std::string& username, const std::string& passwordHash, u64& outAccountId) {
    const char* sql = "INSERT INTO accounts (username, password_hash, created_at) VALUES (?, ?, ?)";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    u64 now = static_cast<u64>(std::time(nullptr));
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to create account: {}", sqlite3_errmsg(db_));
        return false;
    }

    outAccountId = sqlite3_last_insert_rowid(db_);
    spdlog::info("Account created: {} (ID: {})", username, outAccountId);
    return true;
}

bool DatabaseManager::GetAccountByUsername(const std::string& username, Account& outAccount) {
    const char* sql = "SELECT account_id, username, password_hash, email, created_at, last_login, "
                      "is_banned, ban_reason, ban_until, failed_login_attempts, locked_until, last_failed_attempt "
                      "FROM accounts WHERE username = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        outAccount.accountId = sqlite3_column_int64(stmt, 0);
        outAccount.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        outAccount.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        
        const unsigned char* email = sqlite3_column_text(stmt, 3);
        outAccount.email = email ? reinterpret_cast<const char*>(email) : "";
        
        outAccount.createdAt = sqlite3_column_int64(stmt, 4);
        outAccount.lastLogin = sqlite3_column_int64(stmt, 5);
        outAccount.isBanned = sqlite3_column_int(stmt, 6) != 0;
        
        const unsigned char* banReason = sqlite3_column_text(stmt, 7);
        outAccount.banReason = banReason ? reinterpret_cast<const char*>(banReason) : "";
        
        outAccount.banUntil = sqlite3_column_int64(stmt, 8);
        outAccount.failedLoginAttempts = sqlite3_column_int(stmt, 9);
        outAccount.lockedUntil = sqlite3_column_int64(stmt, 10);
        outAccount.lastFailedAttempt = sqlite3_column_int64(stmt, 11);
        
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
}

bool DatabaseManager::GetAccountById(u64 accountId, Account& outAccount) {
    const char* sql = "SELECT account_id, username, password_hash, email, created_at, last_login, "
                      "is_banned, ban_reason, ban_until, failed_login_attempts, locked_until, last_failed_attempt "
                      "FROM accounts WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);

    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        outAccount.accountId = sqlite3_column_int64(stmt, 0);
        outAccount.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        outAccount.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        
        const unsigned char* email = sqlite3_column_text(stmt, 3);
        outAccount.email = email ? reinterpret_cast<const char*>(email) : "";
        
        outAccount.createdAt = sqlite3_column_int64(stmt, 4);
        outAccount.lastLogin = sqlite3_column_int64(stmt, 5);
        outAccount.isBanned = sqlite3_column_int(stmt, 6) != 0;
        
        const unsigned char* banReason = sqlite3_column_text(stmt, 7);
        outAccount.banReason = banReason ? reinterpret_cast<const char*>(banReason) : "";
        
        outAccount.banUntil = sqlite3_column_int64(stmt, 8);
        outAccount.failedLoginAttempts = sqlite3_column_int(stmt, 9);
        outAccount.lockedUntil = sqlite3_column_int64(stmt, 10);
        outAccount.lastFailedAttempt = sqlite3_column_int64(stmt, 11);
        
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
}

bool DatabaseManager::UpdateLastLogin(u64 accountId, u64 timestamp) {
    const char* sql = "UPDATE accounts SET last_login = ? WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, timestamp);
    sqlite3_bind_int64(stmt, 2, accountId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::CreateSession(u64 accountId, const std::string& token, u64 expiresAt, const std::string& ipAddress) {
    const char* sql = "INSERT INTO sessions (session_token, account_id, created_at, expires_at, ip_address, last_used) "
                      "VALUES (?, ?, ?, ?, ?, ?)";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    u64 now = static_cast<u64>(std::time(nullptr));
    
    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, accountId);
    sqlite3_bind_int64(stmt, 3, now);
    sqlite3_bind_int64(stmt, 4, expiresAt);
    sqlite3_bind_text(stmt, 5, ipAddress.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::GetSession(const std::string& token, Session& outSession) {
    const char* sql = "SELECT session_token, account_id, created_at, expires_at, ip_address, last_used "
                      "FROM sessions WHERE session_token = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        outSession.token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        outSession.accountId = sqlite3_column_int64(stmt, 1);
        outSession.createdAt = sqlite3_column_int64(stmt, 2);
        outSession.expiresAt = sqlite3_column_int64(stmt, 3);
        
        const unsigned char* ip = sqlite3_column_text(stmt, 4);
        outSession.ipAddress = ip ? reinterpret_cast<const char*>(ip) : "";
        
        outSession.lastUsed = sqlite3_column_int64(stmt, 5);
        
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
}

bool DatabaseManager::UpdateSessionExpiration(const std::string& token, u64 newExpiresAt) {
    const char* sql = "UPDATE sessions SET expires_at = ?, last_used = ? WHERE session_token = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    u64 now = static_cast<u64>(std::time(nullptr));
    
    sqlite3_bind_int64(stmt, 1, newExpiresAt);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, token.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::DeleteSession(const std::string& token) {
    const char* sql = "DELETE FROM sessions WHERE session_token = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

u32 DatabaseManager::DeleteAllSessionsForAccount(u64 accountId, const std::string& exceptToken) {
    const char* sql = exceptToken.empty() 
        ? "DELETE FROM sessions WHERE account_id = ?"
        : "DELETE FROM sessions WHERE account_id = ? AND session_token != ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, accountId);
    
    if (!exceptToken.empty()) {
        sqlite3_bind_text(stmt, 2, exceptToken.c_str(), -1, SQLITE_TRANSIENT);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return 0;
    }
    return static_cast<u32>(sqlite3_changes(db_));
}

bool DatabaseManager::UpdatePassword(u64 accountId, const std::string& newPasswordHash) {
    const char* sql = "UPDATE accounts SET password_hash = ? WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, newPasswordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, accountId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::BanAccount(u64 accountId, const std::string& reason, u64 banUntil) {
    const char* sql = "UPDATE accounts SET is_banned = 1, ban_reason = ?, ban_until = ? WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, banUntil);
    sqlite3_bind_int64(stmt, 3, accountId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        spdlog::info("Account {} banned: {} (until {})", accountId, reason, banUntil);
        return true;
    }
    return false;
}

bool DatabaseManager::UnbanAccount(u64 accountId) {
    const char* sql = "UPDATE accounts SET is_banned = 0, ban_reason = NULL, ban_until = 0 WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        spdlog::info("Account {} unbanned", accountId);
        return true;
    }
    return false;
}

bool DatabaseManager::LockAccount(u64 accountId, u64 lockUntil) {
    const char* sql = "UPDATE accounts SET locked_until = ? WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, lockUntil);
    sqlite3_bind_int64(stmt, 2, accountId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        spdlog::info("Account {} locked until {}", accountId, lockUntil);
        return true;
    }
    return false;
}

bool DatabaseManager::UnlockAccount(u64 accountId) {
    const char* sql = "UPDATE accounts SET locked_until = 0, failed_login_attempts = 0 WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        spdlog::info("Account {} unlocked", accountId);
        return true;
    }
    return false;
}

bool DatabaseManager::IsAccountLocked(u64 accountId) {
    const char* sql = "SELECT locked_until FROM accounts WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);

    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        u64 lockedUntil = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
        
        if (lockedUntil == 0) {
            return false; // Not locked
        }
        
        u64 now = static_cast<u64>(std::time(nullptr));
        if (now >= lockedUntil) {
            // Lock has expired, unlock the account
            UnlockAccount(accountId);
            return false;
        }
        
        return true; // Still locked
    }
    
    sqlite3_finalize(stmt);
    return false;
}

bool DatabaseManager::IncrementFailedLoginAttempts(u64 accountId, int& outAttemptCount) {
    u64 now = static_cast<u64>(std::time(nullptr));
    
    // First get current attempt count and check if we should reset (5 minute window)
    const char* selectSql = "SELECT failed_login_attempts, last_failed_attempt FROM accounts WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);
    rc = sqlite3_step(stmt);
    
    int currentAttempts = 0;
    u64 lastAttempt = 0;
    
    if (rc == SQLITE_ROW) {
        currentAttempts = sqlite3_column_int(stmt, 0);
        lastAttempt = sqlite3_column_int64(stmt, 1);
    }
    sqlite3_finalize(stmt);
    
    // Reset counter if last attempt was more than 5 minutes ago
    const u64 WINDOW_SECONDS = 5 * 60; // 5 minutes
    if (lastAttempt > 0 && (now - lastAttempt) > WINDOW_SECONDS) {
        currentAttempts = 0;
    }
    
    // Increment and update
    outAttemptCount = currentAttempts + 1;
    
    const char* updateSql = "UPDATE accounts SET failed_login_attempts = ?, last_failed_attempt = ? WHERE account_id = ?";
    rc = sqlite3_prepare_v2(db_, updateSql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int(stmt, 1, outAttemptCount);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_int64(stmt, 3, accountId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::ResetFailedLoginAttempts(u64 accountId) {
    const char* sql = "UPDATE accounts SET failed_login_attempts = 0, last_failed_attempt = 0 WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::GetFailedLoginAttempts(u64 accountId, int& outAttemptCount, u64& outLastAttempt) {
    const char* sql = "SELECT failed_login_attempts, last_failed_attempt FROM accounts WHERE account_id = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);

    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        outAttemptCount = sqlite3_column_int(stmt, 0);
        outLastAttempt = sqlite3_column_int64(stmt, 1);
        sqlite3_finalize(stmt);
        return true;
    }
    
    sqlite3_finalize(stmt);
    return false;
}

bool DatabaseManager::LogLoginAttempt(u64 accountId, const std::string& ipAddress, bool success, u64 timestamp) {
    const char* sql = "INSERT INTO login_history (account_id, ip_address, timestamp, success) VALUES (?, ?, ?, ?)";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);
    sqlite3_bind_text(stmt, 2, ipAddress.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, timestamp);
    sqlite3_bind_int(stmt, 4, success ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool DatabaseManager::GetLoginHistory(u64 accountId, std::vector<LoginHistoryEntry>& outHistory, int limit) {
    const char* sql = "SELECT history_id, account_id, ip_address, timestamp, success, failure_reason "
                      "FROM login_history WHERE account_id = ? ORDER BY timestamp DESC LIMIT ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, accountId);
    sqlite3_bind_int(stmt, 2, limit);

    outHistory.clear();
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        LoginHistoryEntry entry;
        entry.historyId = sqlite3_column_int64(stmt, 0);
        entry.accountId = sqlite3_column_int64(stmt, 1);
        entry.ipAddress = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        entry.timestamp = sqlite3_column_int64(stmt, 3);
        entry.success = sqlite3_column_int(stmt, 4) != 0;
        
        const unsigned char* reason = sqlite3_column_text(stmt, 5);
        entry.failureReason = reason ? reinterpret_cast<const char*>(reason) : "";
        
        outHistory.push_back(entry);
    }
    
    sqlite3_finalize(stmt);
    return true;
}

bool DatabaseManager::IncrementRateLimit(const std::string& key, u64 timestamp, int& outCount) {
    // First try to get existing rate limit
    const char* selectSql = "SELECT attempt_count, window_start FROM rate_limits WHERE limit_key = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, selectSql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        outCount = sqlite3_column_int(stmt, 0) + 1;
        u64 windowStart = sqlite3_column_int64(stmt, 1);
        sqlite3_finalize(stmt);
        
        // Update existing
        const char* updateSql = "UPDATE rate_limits SET attempt_count = ?, last_attempt = ? WHERE limit_key = ?";
        rc = sqlite3_prepare_v2(db_, updateSql, -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
            return false;
        }
        
        sqlite3_bind_int(stmt, 1, outCount);
        sqlite3_bind_int64(stmt, 2, timestamp);
        sqlite3_bind_text(stmt, 3, key.c_str(), -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE;
    } else {
        sqlite3_finalize(stmt);
        
        // Insert new
        outCount = 1;
        const char* insertSql = "INSERT INTO rate_limits (limit_key, attempt_count, window_start, last_attempt) VALUES (?, ?, ?, ?)";
        rc = sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr);
        
        if (rc != SQLITE_OK) {
            spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, outCount);
        sqlite3_bind_int64(stmt, 3, timestamp);
        sqlite3_bind_int64(stmt, 4, timestamp);
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE;
    }
}

bool DatabaseManager::IsRateLimited(const std::string& key, u64 timestamp, int maxAttempts, u64 windowSeconds) {
    const char* sql = "SELECT attempt_count, window_start FROM rate_limits WHERE limit_key = ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    
    if (rc == SQLITE_ROW) {
        int attemptCount = sqlite3_column_int(stmt, 0);
        u64 windowStart = sqlite3_column_int64(stmt, 1);
        sqlite3_finalize(stmt);
        
        // Check if window has expired
        if (timestamp - windowStart > windowSeconds) {
            // Reset the window
            const char* resetSql = "UPDATE rate_limits SET attempt_count = 0, window_start = ? WHERE limit_key = ?";
            rc = sqlite3_prepare_v2(db_, resetSql, -1, &stmt, nullptr);
            
            if (rc == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, timestamp);
                sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
            
            return false;
        }
        
        return attemptCount >= maxAttempts;
    }
    
    sqlite3_finalize(stmt);
    return false;
}

bool DatabaseManager::EnableWALMode() {
    // WAL (Write-Ahead Logging) mode allows concurrent readers and writers
    const char* sql = "PRAGMA journal_mode=WAL;";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to enable WAL mode: {}", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    
    spdlog::info("WAL mode enabled for better concurrency");
    return true;
}

bool DatabaseManager::OptimizeDatabase() {
    // Set pragmas for better performance
    const char* pragmas[] = {
        "PRAGMA synchronous = NORMAL;",      // Faster than FULL, still safe with WAL
        "PRAGMA cache_size = -64000;",       // 64MB cache
        "PRAGMA temp_store = MEMORY;",       // Store temp tables in memory
        "PRAGMA mmap_size = 268435456;",     // 256MB memory-mapped I/O
        "PRAGMA page_size = 4096;",          // Optimal page size
        "PRAGMA foreign_keys = ON;"          // Enable foreign key constraints
    };
    
    for (const char* pragma : pragmas) {
        if (!ExecuteSQL(pragma)) {
            spdlog::warn("Failed to execute pragma: {}", pragma);
        }
    }
    
    spdlog::info("Database optimizations applied");
    return true;
}

int DatabaseManager::CleanupExpiredSessions() {
    u64 now = static_cast<u64>(std::time(nullptr));
    
    const char* sql = "DELETE FROM sessions WHERE expires_at < ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, now);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to cleanup expired sessions: {}", sqlite3_errmsg(db_));
        return 0;
    }

    int deletedCount = sqlite3_changes(db_);
    if (deletedCount > 0) {
        spdlog::info("Cleaned up {} expired sessions", deletedCount);
    }
    
    return deletedCount;
}

int DatabaseManager::CleanupOldLoginHistory(int olderThanDays) {
    u64 now = static_cast<u64>(std::time(nullptr));
    u64 cutoffTime = now - (olderThanDays * 24 * 60 * 60);
    
    const char* sql = "DELETE FROM login_history WHERE timestamp < ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, cutoffTime);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to cleanup old login history: {}", sqlite3_errmsg(db_));
        return 0;
    }

    int deletedCount = sqlite3_changes(db_);
    if (deletedCount > 0) {
        spdlog::info("Cleaned up {} old login history entries", deletedCount);
    }
    
    return deletedCount;
}

int DatabaseManager::CleanupExpiredRateLimits() {
    u64 now = static_cast<u64>(std::time(nullptr));
    // Clean up rate limits older than 1 hour
    u64 cutoffTime = now - 3600;
    
    const char* sql = "DELETE FROM rate_limits WHERE last_attempt < ?";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, cutoffTime);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        spdlog::error("Failed to cleanup expired rate limits: {}", sqlite3_errmsg(db_));
        return 0;
    }

    int deletedCount = sqlite3_changes(db_);
    if (deletedCount > 0) {
        spdlog::info("Cleaned up {} expired rate limit entries", deletedCount);
    }
    
    return deletedCount;
}

} // namespace auth
