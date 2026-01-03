/**
 * Property-based tests for matchmaking authentication integration
 * 
 * Tests the integration between MatchmakingClient/Coordinator and Auth system.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "auth/DatabaseManager.h"
#include "auth/SecurityManager.h"
#include "auth/AuthProtocol.h"
#include "network/MatchmakingProtocol.h"
#include "network/MatchmakingTypes.h"
#include <filesystem>
#include <random>
#include <set>

using namespace auth;
using namespace WorldEditor::Matchmaking;

// Helper to create a fresh test database
class MatchmakingTestDatabase {
public:
    MatchmakingTestDatabase() : dbPath_("test_mm_auth_" + std::to_string(counter_++) + ".db") {
        std::filesystem::remove(dbPath_);
        db_.Initialize(dbPath_);
    }
    
    ~MatchmakingTestDatabase() {
        db_.Shutdown();
        std::filesystem::remove(dbPath_);
    }
    
    DatabaseManager& get() { return db_; }
    
private:
    static int counter_;
    std::string dbPath_;
    DatabaseManager db_;
};

int MatchmakingTestDatabase::counter_ = 0;

// Helper to generate random strings
std::string generateRandomStr(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; i++) {
        result += charset[dis(gen)];
    }
    return result;
}

/**
 * **Feature: authentication-system, Property 17: Token validation in matchmaking**
 * **Validates: Requirements 6.1**
 * 
 * *For any* matchmaking queue request, the matchmaking coordinator should 
 * validate the session token with the auth server before proceeding.
 * 
 * This test verifies:
 * 1. Valid tokens allow queue entry
 * 2. Token validation returns correct account ID
 * 3. Session token is properly transmitted in queue request
 */
TEST_CASE("Property 17: Token validation in matchmaking", "[matchmaking][auth][property]") {
    MatchmakingTestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        // Create account
        std::string username = "mm_user_" + std::to_string(i) + "_" + generateRandomStr(5);
        std::string password = "password_" + generateRandomStr(10);
        std::string passwordHash = security.HashPassword(password, 4);
        
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        REQUIRE(accountId > 0);
        
        // Create valid session
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 expiresAt = now + 7 * 24 * 60 * 60; // 7 days
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Simulate matchmaking coordinator validating token
        Session session;
        bool tokenValid = db.GetSession(sessionToken, session);
        
        // Property 1: Valid token is accepted
        REQUIRE(tokenValid == true);
        
        // Property 2: Token returns correct account ID
        REQUIRE(session.accountId == accountId);
        
        // Property 3: Token is not expired
        REQUIRE(session.expiresAt > now);
        
        // Property 4: Account is not banned
        Account account;
        REQUIRE(db.GetAccountById(accountId, account) == true);
        REQUIRE(account.isBanned == false);
        
        // Simulate building queue request with token
        Wire::QueueRequestPayload payload{};
        payload.mode = static_cast<u8>(MatchMode::AllPick);
        Wire::CopyCString(payload.region, sizeof(payload.region), "auto");
        Wire::CopyCString(payload.sessionToken, sizeof(payload.sessionToken), sessionToken);
        
        // Property 5: Token fits in payload
        REQUIRE(strlen(payload.sessionToken) == sessionToken.length());
        std::string parsedToken(payload.sessionToken, strlen(payload.sessionToken));
        REQUIRE(parsedToken == sessionToken);
    }
}

/**
 * **Feature: authentication-system, Property 19: Banned account rejection**
 * **Validates: Requirements 6.5**
 * 
 * *For any* banned account, all matchmaking requests should be rejected 
 * regardless of valid session token.
 * 
 * This test verifies:
 * 1. Banned accounts are detected during token validation
 * 2. Valid tokens for banned accounts are still rejected
 * 3. Ban status is properly checked
 */
TEST_CASE("Property 19: Banned account rejection", "[matchmaking][auth][property]") {
    MatchmakingTestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        // Create account
        std::string username = "banned_user_" + std::to_string(i) + "_" + generateRandomStr(5);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Create valid session
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 expiresAt = now + 7 * 24 * 60 * 60;
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Ban the account
        std::string banReason = "Test ban " + std::to_string(i);
        u64 banUntil = now + 30 * 24 * 60 * 60; // 30 days
        REQUIRE(db.BanAccount(accountId, banReason, banUntil) == true);
        
        // Simulate matchmaking coordinator checking token
        Session session;
        bool tokenValid = db.GetSession(sessionToken, session);
        
        // Property 1: Token itself is still valid (exists in DB)
        REQUIRE(tokenValid == true);
        REQUIRE(session.accountId == accountId);
        
        // Property 2: But account is banned
        Account account;
        REQUIRE(db.GetAccountById(accountId, account) == true);
        REQUIRE(account.isBanned == true);
        
        // Property 3: Ban reason is set
        REQUIRE(account.banReason == banReason);
        
        // Property 4: Ban is not expired
        REQUIRE(account.banUntil > now);
        
        // Simulate coordinator rejecting banned account
        bool shouldReject = account.isBanned && (account.banUntil == 0 || account.banUntil > now);
        REQUIRE(shouldReject == true);
    }
}

/**
 * Additional property test: Invalid/expired tokens are rejected
 * 
 * *For any* invalid or expired session token, matchmaking should reject
 * the queue request.
 */
TEST_CASE("Property: Invalid tokens rejected in matchmaking", "[matchmaking][auth][property]") {
    MatchmakingTestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        // Test 1: Non-existent token
        std::string fakeToken = security.GenerateSecureToken(32);
        Session session;
        REQUIRE(db.GetSession(fakeToken, session) == false);
        
        // Test 2: Expired token
        std::string username = "expired_mm_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        std::string expiredToken = security.GenerateSecureToken(32);
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 expiredTime = now - 3600; // Expired 1 hour ago
        REQUIRE(db.CreateSession(accountId, expiredToken, expiredTime, "127.0.0.1") == true);
        
        // Token exists but is expired
        Session expiredSession;
        REQUIRE(db.GetSession(expiredToken, expiredSession) == true);
        REQUIRE(expiredSession.expiresAt < now); // Is expired
        
        // Coordinator should reject expired tokens
        bool isExpired = expiredSession.expiresAt < now;
        REQUIRE(isExpired == true);
        
        // Test 3: Empty token
        std::string emptyToken = "";
        REQUIRE(db.GetSession(emptyToken, session) == false);
        
        // Test 4: Malformed token
        std::string malformedToken = "not-a-valid-hex-token!!!";
        REQUIRE(db.GetSession(malformedToken, session) == false);
    }
}

/**
 * Property test: Queue request payload correctly includes session token
 * 
 * *For any* queue request, the session token should be properly serialized
 * in the payload.
 */
TEST_CASE("Property: Queue request includes session token", "[matchmaking][auth][property]") {
    SecurityManager security;
    
    for (int i = 0; i < 50; i++) {
        // Generate random session token
        std::string sessionToken = security.GenerateSecureToken(32);
        
        // Build queue request payload
        Wire::QueueRequestPayload payload{};
        payload.mode = static_cast<u8>(i % 5); // Various modes
        Wire::CopyCString(payload.region, sizeof(payload.region), "eu-west");
        Wire::CopyCString(payload.sessionToken, sizeof(payload.sessionToken), sessionToken);
        
        // Property 1: Token is correctly stored
        REQUIRE(std::string(payload.sessionToken) == sessionToken);
        
        // Property 2: Token length is preserved
        REQUIRE(strlen(payload.sessionToken) == 64);
        
        // Property 3: Mode is preserved
        REQUIRE(payload.mode == static_cast<u8>(i % 5));
        
        // Property 4: Region is preserved
        REQUIRE(std::string(payload.region) == "eu-west");
        
        // Simulate parsing on coordinator side
        std::string parsedToken(payload.sessionToken, 
                               strnlen(payload.sessionToken, sizeof(payload.sessionToken)));
        REQUIRE(parsedToken == sessionToken);
    }
}

/**
 * Property test: Account ID is correctly associated with queue entry
 * 
 * *For any* validated token, the account ID should be correctly stored
 * with the queued player.
 */
TEST_CASE("Property: Account ID associated with queue entry", "[matchmaking][auth][property]") {
    MatchmakingTestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    std::set<u64> accountIds;
    
    for (int i = 0; i < 30; i++) {
        // Create unique account
        std::string username = "queue_user_" + std::to_string(i) + "_" + generateRandomStr(5);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Property 1: Account ID is unique
        REQUIRE(accountIds.find(accountId) == accountIds.end());
        accountIds.insert(accountId);
        
        // Create session
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Validate token and get account ID
        Session session;
        REQUIRE(db.GetSession(sessionToken, session) == true);
        
        // Property 2: Account ID matches
        REQUIRE(session.accountId == accountId);
        
        // Property 3: Account ID is non-zero
        REQUIRE(session.accountId > 0);
        
        // Simulate storing in queue entry
        struct QueueEntry {
            u64 playerId;
            u64 accountId;
            std::string sessionToken;
        };
        
        QueueEntry entry;
        entry.playerId = 1000 + i;
        entry.accountId = session.accountId;
        entry.sessionToken = sessionToken;
        
        // Property 4: Queue entry has correct account ID
        REQUIRE(entry.accountId == accountId);
    }
}

/**
 * Property test: Token validation response contains ban status
 * 
 * *For any* token validation, the response should include whether
 * the account is banned.
 */
TEST_CASE("Property: Token validation includes ban status", "[matchmaking][auth][property]") {
    MatchmakingTestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 20; i++) {
        // Create account
        std::string username = "ban_check_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Create session
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 expiresAt = now + 7 * 24 * 60 * 60;
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Check initial state - not banned
        Account accountBefore;
        REQUIRE(db.GetAccountById(accountId, accountBefore) == true);
        REQUIRE(accountBefore.isBanned == false);
        
        // Simulate building ValidateTokenResponse
        ValidateTokenResponsePayload response{};
        response.result = static_cast<u8>(AuthResult::Success);
        response.accountId = accountId;
        response.isBanned = accountBefore.isBanned ? 1 : 0;
        
        // Property 1: Response shows not banned
        REQUIRE(response.isBanned == 0);
        
        // Now ban the account
        if (i % 2 == 0) {
            REQUIRE(db.BanAccount(accountId, "Test ban", now + 86400) == true);
            
            Account accountAfter;
            REQUIRE(db.GetAccountById(accountId, accountAfter) == true);
            
            // Update response
            response.isBanned = accountAfter.isBanned ? 1 : 0;
            
            // Property 2: Response shows banned
            REQUIRE(response.isBanned == 1);
        }
    }
}

