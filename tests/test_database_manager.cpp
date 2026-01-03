#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "auth/DatabaseManager.h"
#include <filesystem>
#include <ctime>

using namespace auth;

// Helper to create a temporary test database
class TestDatabase {
public:
    TestDatabase() : dbPath_("test_auth.db") {
        // Remove existing test database
        std::filesystem::remove(dbPath_);
        
        db_.Initialize(dbPath_);
    }
    
    ~TestDatabase() {
        db_.Shutdown();
        std::filesystem::remove(dbPath_);
    }
    
    DatabaseManager& GetDB() { return db_; }
    
private:
    std::string dbPath_;
    DatabaseManager db_;
};

// Test account CRUD operations
TEST_CASE("DatabaseManager - Account CRUD operations", "[database][account]") {
    TestDatabase testDb;
    auto& db = testDb.GetDB();
    
    SECTION("Create account successfully") {
        u64 accountId = 0;
        bool result = db.CreateAccount("testuser", "hashed_password_123", accountId);
        
        REQUIRE(result == true);
        REQUIRE(accountId > 0);
    }
    
    SECTION("Create account with duplicate username fails") {
        u64 accountId1 = 0;
        u64 accountId2 = 0;
        
        bool result1 = db.CreateAccount("duplicate_user", "hash1", accountId1);
        bool result2 = db.CreateAccount("duplicate_user", "hash2", accountId2);
        
        REQUIRE(result1 == true);
        REQUIRE(result2 == false);
    }
    
    SECTION("Get account by username") {
        u64 accountId = 0;
        db.CreateAccount("findme", "password_hash", accountId);
        
        Account account;
        bool result = db.GetAccountByUsername("findme", account);
        
        REQUIRE(result == true);
        REQUIRE(account.username == "findme");
        REQUIRE(account.passwordHash == "password_hash");
        REQUIRE(account.accountId == accountId);
    }
    
    SECTION("Get account by username - not found") {
        Account account;
        bool result = db.GetAccountByUsername("nonexistent", account);
        
        REQUIRE(result == false);
    }
    
    SECTION("Get account by ID") {
        u64 accountId = 0;
        db.CreateAccount("user_by_id", "hash", accountId);
        
        Account account;
        bool result = db.GetAccountById(accountId, account);
        
        REQUIRE(result == true);
        REQUIRE(account.accountId == accountId);
        REQUIRE(account.username == "user_by_id");
    }
    
    SECTION("Update last login timestamp") {
        u64 accountId = 0;
        db.CreateAccount("login_user", "hash", accountId);
        
        u64 loginTime = static_cast<u64>(std::time(nullptr));
        bool result = db.UpdateLastLogin(accountId, loginTime);
        
        REQUIRE(result == true);
        
        Account account;
        db.GetAccountById(accountId, account);
        REQUIRE(account.lastLogin == loginTime);
    }
}

// Test session management
TEST_CASE("DatabaseManager - Session management", "[database][session]") {
    TestDatabase testDb;
    auto& db = testDb.GetDB();
    
    // Create a test account first
    u64 accountId = 0;
    db.CreateAccount("session_user", "hash", accountId);
    
    SECTION("Create session successfully") {
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 3600;
        bool result = db.CreateSession(accountId, "token123", expiresAt, "192.168.1.1");
        
        REQUIRE(result == true);
    }
    
    SECTION("Get session by token") {
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 3600;
        db.CreateSession(accountId, "token456", expiresAt, "192.168.1.2");
        
        Session session;
        bool result = db.GetSession("token456", session);
        
        REQUIRE(result == true);
        REQUIRE(session.token == "token456");
        REQUIRE(session.accountId == accountId);
        REQUIRE(session.ipAddress == "192.168.1.2");
        REQUIRE(session.expiresAt == expiresAt);
    }
    
    SECTION("Get session - not found") {
        Session session;
        bool result = db.GetSession("nonexistent_token", session);
        
        REQUIRE(result == false);
    }
    
    SECTION("Update session expiration") {
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 3600;
        db.CreateSession(accountId, "token789", expiresAt, "192.168.1.3");
        
        u64 newExpiresAt = expiresAt + 7200;
        bool result = db.UpdateSessionExpiration("token789", newExpiresAt);
        
        REQUIRE(result == true);
        
        Session session;
        db.GetSession("token789", session);
        REQUIRE(session.expiresAt == newExpiresAt);
    }
    
    SECTION("Delete session") {
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 3600;
        db.CreateSession(accountId, "token_delete", expiresAt, "192.168.1.4");
        
        bool result = db.DeleteSession("token_delete");
        REQUIRE(result == true);
        
        Session session;
        bool found = db.GetSession("token_delete", session);
        REQUIRE(found == false);
    }
    
    SECTION("Delete all sessions for account") {
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 3600;
        db.CreateSession(accountId, "token_a", expiresAt, "192.168.1.5");
        db.CreateSession(accountId, "token_b", expiresAt, "192.168.1.6");
        db.CreateSession(accountId, "token_c", expiresAt, "192.168.1.7");
        
        bool result = db.DeleteAllSessionsForAccount(accountId);
        REQUIRE(result == true);
        
        Session session;
        REQUIRE(db.GetSession("token_a", session) == false);
        REQUIRE(db.GetSession("token_b", session) == false);
        REQUIRE(db.GetSession("token_c", session) == false);
    }
    
    SECTION("Delete all sessions except one") {
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 3600;
        db.CreateSession(accountId, "keep_token", expiresAt, "192.168.1.8");
        db.CreateSession(accountId, "delete_token1", expiresAt, "192.168.1.9");
        db.CreateSession(accountId, "delete_token2", expiresAt, "192.168.1.10");
        
        bool result = db.DeleteAllSessionsForAccount(accountId, "keep_token");
        REQUIRE(result == true);
        
        Session session;
        REQUIRE(db.GetSession("keep_token", session) == true);
        REQUIRE(db.GetSession("delete_token1", session) == false);
        REQUIRE(db.GetSession("delete_token2", session) == false);
    }
}

// Test login history recording
TEST_CASE("DatabaseManager - Login history", "[database][login_history]") {
    TestDatabase testDb;
    auto& db = testDb.GetDB();
    
    // Create a test account
    u64 accountId = 0;
    db.CreateAccount("history_user", "hash", accountId);
    
    SECTION("Log successful login attempt") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        bool result = db.LogLoginAttempt(accountId, "192.168.1.100", true, timestamp);
        
        REQUIRE(result == true);
    }
    
    SECTION("Log failed login attempt") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        bool result = db.LogLoginAttempt(accountId, "192.168.1.101", false, timestamp);
        
        REQUIRE(result == true);
    }
    
    SECTION("Get login history") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        
        db.LogLoginAttempt(accountId, "192.168.1.102", true, timestamp);
        db.LogLoginAttempt(accountId, "192.168.1.103", false, timestamp + 1);
        db.LogLoginAttempt(accountId, "192.168.1.104", true, timestamp + 2);
        
        std::vector<LoginHistoryEntry> history;
        bool result = db.GetLoginHistory(accountId, history, 10);
        
        REQUIRE(result == true);
        REQUIRE(history.size() == 3);
        
        // Should be in reverse chronological order
        REQUIRE(history[0].ipAddress == "192.168.1.104");
        REQUIRE(history[0].success == true);
        REQUIRE(history[1].ipAddress == "192.168.1.103");
        REQUIRE(history[1].success == false);
        REQUIRE(history[2].ipAddress == "192.168.1.102");
        REQUIRE(history[2].success == true);
    }
    
    SECTION("Get login history with limit") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        
        for (int i = 0; i < 5; i++) {
            db.LogLoginAttempt(accountId, "192.168.1." + std::to_string(i), true, timestamp + i);
        }
        
        std::vector<LoginHistoryEntry> history;
        bool result = db.GetLoginHistory(accountId, history, 3);
        
        REQUIRE(result == true);
        REQUIRE(history.size() == 3);
    }
}

// Test SQL injection prevention
TEST_CASE("DatabaseManager - SQL injection prevention", "[database][security]") {
    TestDatabase testDb;
    auto& db = testDb.GetDB();
    
    SECTION("SQL injection in username - CREATE") {
        u64 accountId = 0;
        std::string maliciousUsername = "admin'; DROP TABLE accounts; --";
        
        // Should safely handle the malicious input
        bool result = db.CreateAccount(maliciousUsername, "hash", accountId);
        
        // The account should be created with the literal string
        REQUIRE(result == true);
        
        // Verify we can retrieve it
        Account account;
        bool found = db.GetAccountByUsername(maliciousUsername, account);
        REQUIRE(found == true);
        REQUIRE(account.username == maliciousUsername);
    }
    
    SECTION("SQL injection in username - GET") {
        u64 accountId = 0;
        db.CreateAccount("legitimate_user", "hash", accountId);
        
        // Try to inject SQL in the query
        Account account;
        std::string maliciousQuery = "' OR '1'='1";
        bool result = db.GetAccountByUsername(maliciousQuery, account);
        
        // Should not find anything (parameterized query prevents injection)
        REQUIRE(result == false);
    }
    
    SECTION("SQL injection in session token") {
        u64 accountId = 0;
        db.CreateAccount("token_user", "hash", accountId);
        
        std::string maliciousToken = "token'; DELETE FROM sessions; --";
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 3600;
        
        // Should safely store the malicious string
        bool created = db.CreateSession(accountId, maliciousToken, expiresAt, "192.168.1.1");
        REQUIRE(created == true);
        
        // Should be able to retrieve it
        Session session;
        bool found = db.GetSession(maliciousToken, session);
        REQUIRE(found == true);
        REQUIRE(session.token == maliciousToken);
    }
    
    SECTION("SQL injection in IP address") {
        u64 accountId = 0;
        db.CreateAccount("ip_user", "hash", accountId);
        
        std::string maliciousIP = "192.168.1.1'; DROP TABLE login_history; --";
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        
        // Should safely log the attempt
        bool result = db.LogLoginAttempt(accountId, maliciousIP, true, timestamp);
        REQUIRE(result == true);
        
        // Verify it was logged correctly
        std::vector<LoginHistoryEntry> history;
        db.GetLoginHistory(accountId, history, 10);
        REQUIRE(history.size() == 1);
        REQUIRE(history[0].ipAddress == maliciousIP);
    }
}

// Test rate limiting
TEST_CASE("DatabaseManager - Rate limiting", "[database][rate_limit]") {
    TestDatabase testDb;
    auto& db = testDb.GetDB();
    
    SECTION("Increment rate limit - first attempt") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        int count = 0;
        
        bool result = db.IncrementRateLimit("test_key", timestamp, count);
        
        REQUIRE(result == true);
        REQUIRE(count == 1);
    }
    
    SECTION("Increment rate limit - multiple attempts") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        int count = 0;
        
        db.IncrementRateLimit("multi_key", timestamp, count);
        REQUIRE(count == 1);
        
        db.IncrementRateLimit("multi_key", timestamp + 1, count);
        REQUIRE(count == 2);
        
        db.IncrementRateLimit("multi_key", timestamp + 2, count);
        REQUIRE(count == 3);
    }
    
    SECTION("Check rate limit - not limited") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        int count = 0;
        
        db.IncrementRateLimit("check_key", timestamp, count);
        
        bool limited = db.IsRateLimited("check_key", timestamp, 5, 60);
        REQUIRE(limited == false);
    }
    
    SECTION("Check rate limit - is limited") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        int count = 0;
        
        // Make 5 attempts
        for (int i = 0; i < 5; i++) {
            db.IncrementRateLimit("limit_key", timestamp + i, count);
        }
        
        bool limited = db.IsRateLimited("limit_key", timestamp + 5, 5, 60);
        REQUIRE(limited == true);
    }
    
    SECTION("Rate limit window expiration") {
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        int count = 0;
        
        // Make 5 attempts
        for (int i = 0; i < 5; i++) {
            db.IncrementRateLimit("expire_key", timestamp + i, count);
        }
        
        // Should be limited within window
        bool limited1 = db.IsRateLimited("expire_key", timestamp + 30, 5, 60);
        REQUIRE(limited1 == true);
        
        // Should not be limited after window expires
        bool limited2 = db.IsRateLimited("expire_key", timestamp + 61, 5, 60);
        REQUIRE(limited2 == false);
    }
}

// Test cleanup operations
TEST_CASE("DatabaseManager - Cleanup operations", "[database][cleanup]") {
    TestDatabase testDb;
    auto& db = testDb.GetDB();
    
    SECTION("Cleanup expired sessions") {
        u64 accountId = 0;
        db.CreateAccount("cleanup_user", "hash", accountId);
        
        u64 now = static_cast<u64>(std::time(nullptr));
        
        // Create expired session
        db.CreateSession(accountId, "expired_token", now - 3600, "192.168.1.1");
        
        // Create valid session
        db.CreateSession(accountId, "valid_token", now + 3600, "192.168.1.2");
        
        int cleaned = db.CleanupExpiredSessions();
        
        REQUIRE(cleaned == 1);
        
        // Verify expired session is gone
        Session session;
        REQUIRE(db.GetSession("expired_token", session) == false);
        REQUIRE(db.GetSession("valid_token", session) == true);
    }
    
    SECTION("Cleanup old login history") {
        u64 accountId = 0;
        db.CreateAccount("history_cleanup_user", "hash", accountId);
        
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 oldTime = now - (100 * 24 * 60 * 60); // 100 days ago
        
        // Create old entry
        db.LogLoginAttempt(accountId, "192.168.1.1", true, oldTime);
        
        // Create recent entry
        db.LogLoginAttempt(accountId, "192.168.1.2", true, now);
        
        int cleaned = db.CleanupOldLoginHistory(90);
        
        REQUIRE(cleaned == 1);
        
        // Verify only recent entry remains
        std::vector<LoginHistoryEntry> history;
        db.GetLoginHistory(accountId, history, 10);
        REQUIRE(history.size() == 1);
        REQUIRE(history[0].ipAddress == "192.168.1.2");
    }
    
    SECTION("Cleanup expired rate limits") {
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 oldTime = now - 7200; // 2 hours ago
        
        int count = 0;
        
        // Create old rate limit entry
        db.IncrementRateLimit("old_key", oldTime, count);
        
        // Create recent rate limit entry
        db.IncrementRateLimit("recent_key", now, count);
        
        int cleaned = db.CleanupExpiredRateLimits();
        
        REQUIRE(cleaned == 1);
        
        // Verify old entry is gone, recent remains
        bool oldLimited = db.IsRateLimited("old_key", now, 5, 60);
        bool recentLimited = db.IsRateLimited("recent_key", now, 5, 60);
        
        REQUIRE(oldLimited == false);
        REQUIRE(recentLimited == false); // Not limited yet, just exists
    }
}
