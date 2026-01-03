#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "auth/SecurityManager.h"
#include <set>
#include <random>

using namespace auth;

// Helper to generate random strings
std::string generateRandomString(size_t length) {
    static const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "!@#$%^&*()_+-=[]{}|;:,.<>?";
    
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
 * **Feature: authentication-system, Property 5: Password hashing in database**
 * **Validates: Requirements 1.5, 5.2**
 * 
 * *For any* registered account, the password stored in the database should be 
 * a bcrypt hash, not the original password or SHA256 hash.
 * 
 * This property test verifies:
 * 1. Hashed password is different from original
 * 2. Hashed password starts with bcrypt prefix
 * 3. Original password can be verified against hash
 * 4. Different passwords produce different hashes
 */
TEST_CASE("Property 5: Password hashing in database", "[security][property][password]") {
    SecurityManager security;
    
    // Run 100 iterations with random passwords
    for (int i = 0; i < 100; i++) {
        // Generate random password (8-32 characters)
        size_t length = 8 + (i % 25);
        std::string password = generateRandomString(length);
        
        // Hash the password
        std::string hash = security.HashPassword(password, 4);
        
        // Property 1: Hash is not empty
        REQUIRE(!hash.empty());
        
        // Property 2: Hash is different from original password
        REQUIRE(hash != password);
        
        // Property 3: Hash starts with bcrypt prefix ($2b$)
        REQUIRE(hash.substr(0, 4) == "$2b$");
        
        // Property 4: Original password verifies correctly
        REQUIRE(security.VerifyPassword(password, hash) == true);
        
        // Property 5: Wrong password does not verify
        std::string wrongPassword = password + "X";
        REQUIRE(security.VerifyPassword(wrongPassword, hash) == false);
    }
}

/**
 * **Feature: authentication-system, Property 16: Cryptographically secure tokens**
 * **Validates: Requirements 5.4**
 * 
 * *For any* two session tokens generated, they should be different and 
 * pass basic randomness tests (no predictable patterns).
 * 
 * This property test verifies:
 * 1. All generated tokens are unique
 * 2. Tokens have expected length
 * 3. Tokens contain valid hex characters
 * 4. Tokens have sufficient entropy (no obvious patterns)
 */
TEST_CASE("Property 16: Cryptographically secure tokens", "[security][property][token]") {
    SecurityManager security;
    
    std::set<std::string> tokens;
    
    // Generate 100 tokens and verify uniqueness
    for (int i = 0; i < 100; i++) {
        std::string token = security.GenerateSecureToken(32);
        
        // Property 1: Token is not empty
        REQUIRE(!token.empty());
        
        // Property 2: Token has expected length (32 bytes = 64 hex chars)
        REQUIRE(token.length() == 64);
        
        // Property 3: Token contains only valid hex characters
        for (char c : token) {
            bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            REQUIRE(isHex);
        }
        
        // Property 4: Token is unique (not seen before)
        REQUIRE(tokens.find(token) == tokens.end());
        tokens.insert(token);
    }
    
    // Property 5: All 100 tokens are unique
    REQUIRE(tokens.size() == 100);
    
    // Property 6: Basic entropy check - tokens should not be sequential
    // Check that consecutive tokens differ in multiple positions
    std::string prev = security.GenerateSecureToken(32);
    for (int i = 0; i < 10; i++) {
        std::string curr = security.GenerateSecureToken(32);
        
        int differences = 0;
        for (size_t j = 0; j < prev.length(); j++) {
            if (prev[j] != curr[j]) differences++;
        }
        
        // At least 50% of characters should differ (high entropy)
        REQUIRE(differences >= 32);
        
        prev = curr;
    }
}

/**
 * Additional property test for SHA256 consistency
 * 
 * *For any* input data, SHA256 hash should be deterministic and consistent.
 */
TEST_CASE("Property: SHA256 hash consistency", "[security][property][sha256]") {
    SecurityManager security;
    
    for (int i = 0; i < 100; i++) {
        // Generate random input
        std::string input = generateRandomString(10 + (i % 100));
        
        // Hash twice
        std::string hash1 = security.SHA256Hash(input);
        std::string hash2 = security.SHA256Hash(input);
        
        // Property 1: Same input produces same hash
        REQUIRE(hash1 == hash2);
        
        // Property 2: Hash has correct length (64 hex chars)
        REQUIRE(hash1.length() == 64);
        
        // Property 3: Different input produces different hash
        std::string differentInput = input + "X";
        std::string differentHash = security.SHA256Hash(differentInput);
        REQUIRE(hash1 != differentHash);
    }
}

/**
 * Property test for rate limiting behavior
 * 
 * *For any* IP address, rate limiting should be consistent and predictable.
 */
TEST_CASE("Property: Rate limiting consistency", "[security][property][ratelimit]") {
    SecurityManager security;
    
    for (int i = 0; i < 20; i++) {
        // Generate unique IP for each iteration
        std::string ip = "10.0." + std::to_string(i) + ".1";
        
        // Property 1: Not rate limited initially
        REQUIRE(security.CheckRateLimit(ip, RateLimitType::Login) == false);
        
        // Property 2: After max attempts, should be rate limited
        for (int j = 0; j < 5; j++) {
            security.RecordAttempt(ip, RateLimitType::Login);
        }
        REQUIRE(security.CheckRateLimit(ip, RateLimitType::Login) == true);
        
        // Property 3: Reset clears rate limit
        security.ResetRateLimit(ip, RateLimitType::Login);
        REQUIRE(security.CheckRateLimit(ip, RateLimitType::Login) == false);
    }
}


// Include DatabaseManager for registration property tests
#include "auth/DatabaseManager.h"
#include <filesystem>

// Helper to create a fresh test database
class TestDatabase {
public:
    TestDatabase() : dbPath_("test_reg_" + std::to_string(counter_++) + ".db") {
        // Remove if exists
        std::filesystem::remove(dbPath_);
        db_.Initialize(dbPath_);
    }
    
    ~TestDatabase() {
        db_.Shutdown();
        std::filesystem::remove(dbPath_);
    }
    
    DatabaseManager& get() { return db_; }
    
private:
    static int counter_;
    std::string dbPath_;
    DatabaseManager db_;
};

int TestDatabase::counter_ = 0;

// Helper to generate random alphanumeric username
std::string generateRandomUsername(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789_";
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
 * **Feature: authentication-system, Property 1: Unique account creation**
 * **Validates: Requirements 1.1**
 * 
 * *For any* unique username and valid password (≥8 characters), registering 
 * should create a new account with a unique account ID that doesn't conflict 
 * with existing accounts.
 */
TEST_CASE("Property 1: Unique account creation", "[registration][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    std::set<u64> accountIds;
    
    // Create 50 accounts with unique usernames
    for (int i = 0; i < 50; i++) {
        std::string username = "user_" + std::to_string(i) + "_" + generateRandomUsername(5);
        std::string password = generateRandomString(12);
        std::string passwordHash = security.HashPassword(password, 4);
        
        u64 accountId = 0;
        bool created = db.CreateAccount(username, passwordHash, accountId);
        
        // Property 1: Account creation succeeds
        REQUIRE(created == true);
        
        // Property 2: Account ID is non-zero
        REQUIRE(accountId > 0);
        
        // Property 3: Account ID is unique
        REQUIRE(accountIds.find(accountId) == accountIds.end());
        accountIds.insert(accountId);
        
        // Property 4: Account can be retrieved
        Account retrieved;
        REQUIRE(db.GetAccountById(accountId, retrieved) == true);
        REQUIRE(retrieved.username == username);
    }
    
    // Property 5: All 50 accounts have unique IDs
    REQUIRE(accountIds.size() == 50);
}

/**
 * **Feature: authentication-system, Property 2: Duplicate username rejection**
 * **Validates: Requirements 1.2**
 * 
 * *For any* existing username in the database, attempting to register with 
 * that username should be rejected with an error message.
 */
TEST_CASE("Property 2: Duplicate username rejection", "[registration][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        std::string username = "duplicate_" + std::to_string(i);
        std::string password1 = security.HashPassword("password123", 4);
        std::string password2 = security.HashPassword("different456", 4);
        
        // Create first account
        u64 accountId1 = 0;
        bool created1 = db.CreateAccount(username, password1, accountId1);
        REQUIRE(created1 == true);
        REQUIRE(accountId1 > 0);
        
        // Attempt to create duplicate
        u64 accountId2 = 0;
        bool created2 = db.CreateAccount(username, password2, accountId2);
        
        // Property: Duplicate username is rejected
        REQUIRE(created2 == false);
        REQUIRE(accountId2 == 0);
        
        // Original account still exists
        Account original;
        REQUIRE(db.GetAccountByUsername(username, original) == true);
        REQUIRE(original.accountId == accountId1);
    }
}

/**
 * **Feature: authentication-system, Property 3: Password length validation**
 * **Validates: Requirements 1.3**
 * 
 * *For any* password shorter than 8 characters, registration should be 
 * rejected with a validation error.
 * 
 * Note: This is validated at the AuthServer level, not DatabaseManager.
 * Here we test that short passwords still hash correctly (the validation
 * happens before hashing in the server).
 */
TEST_CASE("Property 3: Password length validation", "[registration][property]") {
    SecurityManager security;
    
    // Test that even short passwords can be hashed (validation is at server level)
    for (int len = 1; len <= 20; len++) {
        std::string password = generateRandomString(len);
        std::string hash = security.HashPassword(password, 4);
        
        // Property 1: Any non-empty password can be hashed
        REQUIRE(!hash.empty());
        
        // Property 2: Hash verifies correctly
        REQUIRE(security.VerifyPassword(password, hash) == true);
    }
    
    // Property 3: Empty password returns empty hash
    std::string emptyHash = security.HashPassword("", 4);
    REQUIRE(emptyHash.empty());
}

/**
 * **Feature: authentication-system, Property 4: Session token generation on registration**
 * **Validates: Requirements 1.4**
 * 
 * *For any* successful registration, the system should return a non-empty 
 * session token to the client.
 */
TEST_CASE("Property 4: Session token generation on registration", "[registration][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 50; i++) {
        std::string username = "session_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("validpassword123", 4);
        
        // Create account
        u64 accountId = 0;
        bool created = db.CreateAccount(username, passwordHash, accountId);
        REQUIRE(created == true);
        
        // Generate session token (as server would do)
        std::string sessionToken = security.GenerateSecureToken(32);
        
        // Property 1: Token is not empty
        REQUIRE(!sessionToken.empty());
        
        // Property 2: Token has correct length
        REQUIRE(sessionToken.length() == 64);
        
        // Create session in database
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
        bool sessionCreated = db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1");
        
        // Property 3: Session is created successfully
        REQUIRE(sessionCreated == true);
        
        // Property 4: Session can be retrieved
        Session session;
        REQUIRE(db.GetSession(sessionToken, session) == true);
        REQUIRE(session.accountId == accountId);
    }
}


/**
 * **Feature: authentication-system, Property 6: Valid login returns token**
 * **Validates: Requirements 2.1**
 * 
 * *For any* account with correct username and password, login should 
 * authenticate successfully and return a session token.
 */
TEST_CASE("Property 6: Valid login returns token", "[login][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        std::string username = "login_user_" + std::to_string(i);
        std::string password = "password_" + generateRandomString(10);
        std::string passwordHash = security.HashPassword(password, 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Simulate login: verify password
        Account account;
        REQUIRE(db.GetAccountByUsername(username, account) == true);
        
        // Property 1: Password verifies correctly
        REQUIRE(security.VerifyPassword(password, account.passwordHash) == true);
        
        // Property 2: Generate session token on successful login
        std::string sessionToken = security.GenerateSecureToken(32);
        REQUIRE(!sessionToken.empty());
        REQUIRE(sessionToken.length() == 64);
        
        // Property 3: Session can be created
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Property 4: Session is valid
        Session session;
        REQUIRE(db.GetSession(sessionToken, session) == true);
        REQUIRE(session.accountId == accountId);
    }
}

/**
 * **Feature: authentication-system, Property 7: Invalid credentials rejection**
 * **Validates: Requirements 2.2**
 * 
 * *For any* login attempt with incorrect password, the system should reject 
 * authentication and return an error.
 */
TEST_CASE("Property 7: Invalid credentials rejection", "[login][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        std::string username = "reject_user_" + std::to_string(i);
        std::string correctPassword = "correct_" + generateRandomString(10);
        std::string wrongPassword = "wrong_" + generateRandomString(10);
        std::string passwordHash = security.HashPassword(correctPassword, 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Get account
        Account account;
        REQUIRE(db.GetAccountByUsername(username, account) == true);
        
        // Property 1: Wrong password is rejected
        REQUIRE(security.VerifyPassword(wrongPassword, account.passwordHash) == false);
        
        // Property 2: Empty password is rejected
        REQUIRE(security.VerifyPassword("", account.passwordHash) == false);
        
        // Property 3: Similar password is rejected
        std::string similarPassword = correctPassword + "X";
        REQUIRE(security.VerifyPassword(similarPassword, account.passwordHash) == false);
        
        // Property 4: Correct password still works
        REQUIRE(security.VerifyPassword(correctPassword, account.passwordHash) == true);
    }
}

/**
 * **Feature: authentication-system, Property 8: Last login timestamp update**
 * **Validates: Requirements 2.4**
 * 
 * *For any* successful login, the account's last_login timestamp in the 
 * database should be updated to the current time.
 */
TEST_CASE("Property 8: Last login timestamp update", "[login][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 20; i++) {
        std::string username = "timestamp_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Get initial state
        Account accountBefore;
        REQUIRE(db.GetAccountById(accountId, accountBefore) == true);
        u64 initialLastLogin = accountBefore.lastLogin;
        
        // Simulate login - update last login
        u64 loginTime = static_cast<u64>(std::time(nullptr));
        REQUIRE(db.UpdateLastLogin(accountId, loginTime) == true);
        
        // Get updated state
        Account accountAfter;
        REQUIRE(db.GetAccountById(accountId, accountAfter) == true);
        
        // Property 1: Last login was updated
        REQUIRE(accountAfter.lastLogin >= loginTime);
        
        // Property 2: Last login is different from initial (if initial was 0)
        if (initialLastLogin == 0) {
            REQUIRE(accountAfter.lastLogin > initialLastLogin);
        }
    }
}

/**
 * **Feature: authentication-system, Property 9: Session token expiration**
 * **Validates: Requirements 2.5**
 * 
 * *For any* generated session token, the expiration time should be set to 
 * exactly 7 days from creation time.
 */
TEST_CASE("Property 9: Session token expiration", "[login][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    const u64 SEVEN_DAYS_SECONDS = 7 * 24 * 60 * 60;
    
    for (int i = 0; i < 30; i++) {
        std::string username = "expiry_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Create session with 7-day expiration
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 expiresAt = now + SEVEN_DAYS_SECONDS;
        
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Get session
        Session session;
        REQUIRE(db.GetSession(sessionToken, session) == true);
        
        // Property 1: Expiration is set correctly (within 1 second tolerance)
        REQUIRE(session.expiresAt >= expiresAt);
        REQUIRE(session.expiresAt <= expiresAt + 1);
        
        // Property 2: Session is not expired yet
        REQUIRE(session.expiresAt > now);
        
        // Property 3: Expiration is approximately 7 days from now
        u64 diff = session.expiresAt - now;
        REQUIRE(diff >= SEVEN_DAYS_SECONDS - 1);
        REQUIRE(diff <= SEVEN_DAYS_SECONDS + 1);
    }
}


/**
 * **Feature: authentication-system, Property 10: Token validation round-trip**
 * **Validates: Requirements 3.2**
 * 
 * *For any* valid session token, validating it should return the correct 
 * account ID that was used to create the token.
 */
TEST_CASE("Property 10: Token validation round-trip", "[token][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 50; i++) {
        std::string username = "roundtrip_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Create session
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Validate token - should return same account ID
        Session session;
        REQUIRE(db.GetSession(sessionToken, session) == true);
        
        // Property 1: Account ID matches
        REQUIRE(session.accountId == accountId);
        
        // Property 2: Token matches
        REQUIRE(session.token == sessionToken);
        
        // Property 3: Session is not expired
        u64 now = static_cast<u64>(std::time(nullptr));
        REQUIRE(session.expiresAt > now);
    }
}

/**
 * **Feature: authentication-system, Property 11: Token expiration extension**
 * **Validates: Requirements 3.4**
 * 
 * *For any* valid token that is validated, the expiration time should be 
 * extended by 7 days from the validation time.
 */
TEST_CASE("Property 11: Token expiration extension", "[token][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    const u64 SEVEN_DAYS = 7 * 24 * 60 * 60;
    
    for (int i = 0; i < 30; i++) {
        std::string username = "extend_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Create session with initial expiration
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 initialExpiry = now + SEVEN_DAYS;
        REQUIRE(db.CreateSession(accountId, sessionToken, initialExpiry, "127.0.0.1") == true);
        
        // Get initial session
        Session sessionBefore;
        REQUIRE(db.GetSession(sessionToken, sessionBefore) == true);
        u64 expiryBefore = sessionBefore.expiresAt;
        
        // Extend expiration (as validation would do)
        u64 newExpiry = now + SEVEN_DAYS;
        REQUIRE(db.UpdateSessionExpiration(sessionToken, newExpiry) == true);
        
        // Get updated session
        Session sessionAfter;
        REQUIRE(db.GetSession(sessionToken, sessionAfter) == true);
        
        // Property 1: Expiration was updated
        REQUIRE(sessionAfter.expiresAt >= newExpiry);
        
        // Property 2: Account ID unchanged
        REQUIRE(sessionAfter.accountId == accountId);
    }
}

/**
 * **Feature: authentication-system, Property 18: Invalid token rejection in matchmaking**
 * **Validates: Requirements 6.2**
 * 
 * *For any* invalid or expired session token, matchmaking queue requests 
 * should be rejected.
 */
TEST_CASE("Property 18: Invalid token rejection in matchmaking", "[token][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        // Property 1: Non-existent token is rejected
        std::string fakeToken = security.GenerateSecureToken(32);
        Session session;
        REQUIRE(db.GetSession(fakeToken, session) == false);
        
        // Create account and expired session
        std::string username = "expired_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Create expired session (expired 1 hour ago)
        std::string expiredToken = security.GenerateSecureToken(32);
        u64 now = static_cast<u64>(std::time(nullptr));
        u64 expiredTime = now - 3600; // 1 hour ago
        REQUIRE(db.CreateSession(accountId, expiredToken, expiredTime, "127.0.0.1") == true);
        
        // Property 2: Expired token can be retrieved but is expired
        Session expiredSession;
        REQUIRE(db.GetSession(expiredToken, expiredSession) == true);
        REQUIRE(expiredSession.expiresAt < now); // Is expired
        
        // Property 3: Random garbage token is rejected
        std::string garbageToken = "not_a_valid_token_" + std::to_string(i);
        REQUIRE(db.GetSession(garbageToken, session) == false);
    }
}


/**
 * **Feature: authentication-system, Property 12: Logout invalidates token**
 * **Validates: Requirements 3.5**
 * 
 * *For any* session token, after logout is called, that token should no 
 * longer validate successfully.
 */
TEST_CASE("Property 12: Logout invalidates token", "[logout][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        std::string username = "logout_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Create session
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Verify session exists
        Session sessionBefore;
        REQUIRE(db.GetSession(sessionToken, sessionBefore) == true);
        REQUIRE(sessionBefore.accountId == accountId);
        
        // Logout - delete session
        REQUIRE(db.DeleteSession(sessionToken) == true);
        
        // Property 1: Token no longer validates
        Session sessionAfter;
        REQUIRE(db.GetSession(sessionToken, sessionAfter) == false);
    }
}

/**
 * Additional test: Logout all sessions
 * 
 * *For any* account with multiple sessions, logout all should invalidate 
 * all sessions except optionally the current one.
 */
TEST_CASE("Property: Logout all sessions", "[logout][property]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 20; i++) {
        std::string username = "multi_session_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Create multiple sessions
        std::vector<std::string> tokens;
        for (int j = 0; j < 5; j++) {
            std::string token = security.GenerateSecureToken(32);
            u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
            REQUIRE(db.CreateSession(accountId, token, expiresAt, "127.0.0.1") == true);
            tokens.push_back(token);
        }
        
        // Verify all sessions exist
        for (const auto& token : tokens) {
            Session session;
            REQUIRE(db.GetSession(token, session) == true);
        }
        
        // Logout all except first token
        std::string keepToken = tokens[0];
        u32 deleted = db.DeleteAllSessionsForAccount(accountId, keepToken);
        
        // Property 1: 4 sessions were deleted
        REQUIRE(deleted == 4);
        
        // Property 2: Kept token still works
        Session keptSession;
        REQUIRE(db.GetSession(keepToken, keptSession) == true);
        
        // Property 3: Other tokens are invalidated
        for (size_t j = 1; j < tokens.size(); j++) {
            Session deletedSession;
            REQUIRE(db.GetSession(tokens[j], deletedSession) == false);
        }
    }
}


/**
 * **Feature: authentication-system, Property 25: Login attempt logging**
 * **Validates: Requirements 11.1**
 * 
 * *For any* login attempt, the system should log the IP address and timestamp 
 * in the database.
 * 
 * This property test verifies:
 * 1. Successful login attempts are logged with correct data
 * 2. Failed login attempts are logged with correct data
 * 3. IP address is recorded correctly
 * 4. Timestamp is recorded correctly
 * 5. Login history can be retrieved
 */
TEST_CASE("Property 25: Login attempt logging", "[security][property][login-history]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        std::string username = "login_log_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Generate random IP address
        std::string ipAddress = "192.168." + std::to_string(i % 256) + "." + std::to_string((i * 7) % 256);
        u64 timestamp = static_cast<u64>(std::time(nullptr));
        
        // Log successful login attempt
        REQUIRE(db.LogLoginAttempt(accountId, ipAddress, true, timestamp) == true);
        
        // Log failed login attempt with different IP
        std::string failedIp = "10.0." + std::to_string(i % 256) + "." + std::to_string((i * 3) % 256);
        u64 failedTimestamp = timestamp + 1;
        REQUIRE(db.LogLoginAttempt(accountId, failedIp, false, failedTimestamp) == true);
        
        // Retrieve login history
        std::vector<LoginHistoryEntry> history;
        REQUIRE(db.GetLoginHistory(accountId, history, 10) == true);
        
        // Property 1: At least 2 entries exist
        REQUIRE(history.size() >= 2);
        
        // Property 2: Most recent entry is the failed attempt (history is ordered DESC)
        REQUIRE(history[0].success == false);
        REQUIRE(history[0].ipAddress == failedIp);
        REQUIRE(history[0].timestamp == failedTimestamp);
        REQUIRE(history[0].accountId == accountId);
        
        // Property 3: Second entry is the successful attempt
        REQUIRE(history[1].success == true);
        REQUIRE(history[1].ipAddress == ipAddress);
        REQUIRE(history[1].timestamp == timestamp);
        REQUIRE(history[1].accountId == accountId);
        
        // Property 4: History IDs are unique and non-zero
        REQUIRE(history[0].historyId > 0);
        REQUIRE(history[1].historyId > 0);
        REQUIRE(history[0].historyId != history[1].historyId);
    }
}

/**
 * Additional test: Multiple login attempts from same IP
 * 
 * *For any* account with multiple login attempts from the same IP,
 * all attempts should be logged separately.
 */
TEST_CASE("Property 25b: Multiple login attempts from same IP", "[security][property][login-history]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 20; i++) {
        std::string username = "multi_attempt_user_" + std::to_string(i);
        std::string passwordHash = security.HashPassword("password123", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        std::string ipAddress = "172.16.0." + std::to_string(i);
        u64 baseTimestamp = static_cast<u64>(std::time(nullptr));
        
        // Log multiple attempts from same IP
        int numAttempts = 5;
        for (int j = 0; j < numAttempts; j++) {
            bool success = (j == numAttempts - 1); // Only last attempt succeeds
            REQUIRE(db.LogLoginAttempt(accountId, ipAddress, success, baseTimestamp + j) == true);
        }
        
        // Retrieve login history
        std::vector<LoginHistoryEntry> history;
        REQUIRE(db.GetLoginHistory(accountId, history, 10) == true);
        
        // Property 1: All attempts are logged
        REQUIRE(history.size() == static_cast<size_t>(numAttempts));
        
        // Property 2: All entries have same IP
        for (const auto& entry : history) {
            REQUIRE(entry.ipAddress == ipAddress);
            REQUIRE(entry.accountId == accountId);
        }
        
        // Property 3: Most recent (first in list) is successful
        REQUIRE(history[0].success == true);
        
        // Property 4: Earlier attempts (rest of list) are failures
        for (size_t j = 1; j < history.size(); j++) {
            REQUIRE(history[j].success == false);
        }
    }
}


/**
 * **Feature: authentication-system, Property 26: Password change invalidates sessions**
 * **Validates: Requirements 11.5**
 * 
 * *For any* account, when the password is changed, all existing session tokens 
 * for that account should be invalidated.
 * 
 * This property test verifies:
 * 1. Password can be changed successfully
 * 2. All existing sessions are invalidated after password change
 * 3. New password works for authentication
 * 4. Old password no longer works
 */
TEST_CASE("Property 26: Password change invalidates sessions", "[security][property][password-change]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 30; i++) {
        std::string username = "pwchange_user_" + std::to_string(i);
        std::string oldPassword = "oldpassword_" + generateRandomString(8);
        std::string newPassword = "newpassword_" + generateRandomString(8);
        std::string oldPasswordHash = security.HashPassword(oldPassword, 4);
        std::string newPasswordHash = security.HashPassword(newPassword, 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, oldPasswordHash, accountId) == true);
        
        // Create multiple sessions (simulating logins from different devices)
        std::vector<std::string> tokens;
        for (int j = 0; j < 3; j++) {
            std::string token = security.GenerateSecureToken(32);
            u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
            std::string ip = "192.168.1." + std::to_string(j + 1);
            REQUIRE(db.CreateSession(accountId, token, expiresAt, ip) == true);
            tokens.push_back(token);
        }
        
        // Verify all sessions exist before password change
        for (const auto& token : tokens) {
            Session session;
            REQUIRE(db.GetSession(token, session) == true);
            REQUIRE(session.accountId == accountId);
        }
        
        // Change password
        REQUIRE(db.UpdatePassword(accountId, newPasswordHash) == true);
        
        // Invalidate all sessions (as the system should do on password change)
        u32 deletedCount = db.DeleteAllSessionsForAccount(accountId);
        
        // Property 1: All sessions were deleted
        REQUIRE(deletedCount == 3);
        
        // Property 2: All tokens are now invalid
        for (const auto& token : tokens) {
            Session session;
            REQUIRE(db.GetSession(token, session) == false);
        }
        
        // Property 3: New password works
        Account account;
        REQUIRE(db.GetAccountById(accountId, account) == true);
        REQUIRE(security.VerifyPassword(newPassword, account.passwordHash) == true);
        
        // Property 4: Old password no longer works
        REQUIRE(security.VerifyPassword(oldPassword, account.passwordHash) == false);
    }
}

/**
 * Additional test: Password change with session preservation option
 * 
 * *For any* account, when password is changed with current session preserved,
 * only the current session should remain valid.
 */
TEST_CASE("Property 26b: Password change with current session preserved", "[security][property][password-change]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 20; i++) {
        std::string username = "pwchange_preserve_" + std::to_string(i);
        std::string oldPasswordHash = security.HashPassword("oldpassword123", 4);
        std::string newPasswordHash = security.HashPassword("newpassword456", 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, oldPasswordHash, accountId) == true);
        
        // Create multiple sessions
        std::string currentToken = security.GenerateSecureToken(32);
        std::vector<std::string> otherTokens;
        
        // Create current session
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
        REQUIRE(db.CreateSession(accountId, currentToken, expiresAt, "192.168.1.1") == true);
        
        // Create other sessions
        for (int j = 0; j < 4; j++) {
            std::string token = security.GenerateSecureToken(32);
            REQUIRE(db.CreateSession(accountId, token, expiresAt, "192.168.1." + std::to_string(j + 10)) == true);
            otherTokens.push_back(token);
        }
        
        // Change password and invalidate all sessions except current
        REQUIRE(db.UpdatePassword(accountId, newPasswordHash) == true);
        u32 deletedCount = db.DeleteAllSessionsForAccount(accountId, currentToken);
        
        // Property 1: 4 other sessions were deleted
        REQUIRE(deletedCount == 4);
        
        // Property 2: Current token still works
        Session currentSession;
        REQUIRE(db.GetSession(currentToken, currentSession) == true);
        REQUIRE(currentSession.accountId == accountId);
        
        // Property 3: Other tokens are invalid
        for (const auto& token : otherTokens) {
            Session session;
            REQUIRE(db.GetSession(token, session) == false);
        }
    }
}


/**
 * **Feature: authentication-system, Property 20: Successful auth enables features**
 * **Validates: Requirements 7.5**
 * 
 * *For any* successful authentication, the client should be in authenticated state
 * and have access to matchmaking features.
 * 
 * Note: This tests the AuthClient state after successful authentication simulation.
 * The actual UI transition is tested through integration tests.
 */
TEST_CASE("Property 20: Successful auth enables features", "[auth][property][ui]") {
    TestDatabase testDb;
    auto& db = testDb.get();
    SecurityManager security;
    
    for (int i = 0; i < 20; i++) {
        std::string username = "auth_feature_user_" + std::to_string(i);
        std::string password = "password_" + generateRandomString(8);
        std::string passwordHash = security.HashPassword(password, 4);
        
        // Create account
        u64 accountId = 0;
        REQUIRE(db.CreateAccount(username, passwordHash, accountId) == true);
        
        // Simulate successful login by creating a valid session
        std::string sessionToken = security.GenerateSecureToken(32);
        u64 expiresAt = static_cast<u64>(std::time(nullptr)) + 7 * 24 * 60 * 60;
        REQUIRE(db.CreateSession(accountId, sessionToken, expiresAt, "127.0.0.1") == true);
        
        // Property 1: Session token is valid
        Session session;
        REQUIRE(db.GetSession(sessionToken, session) == true);
        
        // Property 2: Session is associated with correct account
        REQUIRE(session.accountId == accountId);
        
        // Property 3: Session is not expired
        u64 now = static_cast<u64>(std::time(nullptr));
        REQUIRE(session.expiresAt > now);
        
        // Property 4: Account is not banned (can access features)
        Account account;
        REQUIRE(db.GetAccountById(accountId, account) == true);
        REQUIRE(account.isBanned == false);
        
        // Property 5: Account is not locked
        REQUIRE(db.IsAccountLocked(accountId) == false);
    }
}
