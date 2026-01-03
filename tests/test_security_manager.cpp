#include <catch2/catch_test_macros.hpp>
#include "auth/SecurityManager.h"
#include <set>

using namespace auth;

// Test password hashing and verification
TEST_CASE("SecurityManager - Password hashing", "[security][password]") {
    SecurityManager security;
    
    SECTION("Hash password successfully") {
        std::string hash = security.HashPassword("testpassword123", 4);
        
        REQUIRE(!hash.empty());
        REQUIRE(hash.substr(0, 4) == "$2b$");
    }
    
    SECTION("Verify correct password") {
        std::string password = "mySecurePassword!";
        std::string hash = security.HashPassword(password, 4);
        
        bool result = security.VerifyPassword(password, hash);
        REQUIRE(result == true);
    }
    
    SECTION("Reject incorrect password") {
        std::string password = "correctPassword";
        std::string hash = security.HashPassword(password, 4);
        
        bool result = security.VerifyPassword("wrongPassword", hash);
        REQUIRE(result == false);
    }
    
    SECTION("Empty password returns empty hash") {
        std::string hash = security.HashPassword("", 4);
        REQUIRE(hash.empty());
    }
    
    SECTION("Different passwords produce different hashes") {
        std::string hash1 = security.HashPassword("password1", 4);
        std::string hash2 = security.HashPassword("password2", 4);
        
        REQUIRE(hash1 != hash2);
    }
    
    SECTION("Same password produces different hashes (salt)") {
        std::string hash1 = security.HashPassword("samePassword", 4);
        std::string hash2 = security.HashPassword("samePassword", 4);
        
        // Hashes should be different due to random salt
        REQUIRE(hash1 != hash2);
        
        // But both should verify correctly
        REQUIRE(security.VerifyPassword("samePassword", hash1) == true);
        REQUIRE(security.VerifyPassword("samePassword", hash2) == true);
    }
}

// Test SHA256 hashing
TEST_CASE("SecurityManager - SHA256 hashing", "[security][sha256]") {
    SecurityManager security;
    
    SECTION("Hash data successfully") {
        std::string hash = security.SHA256Hash("test data");
        
        REQUIRE(!hash.empty());
        REQUIRE(hash.length() == 64); // SHA256 produces 64 hex chars
    }
    
    SECTION("Same input produces same hash") {
        std::string hash1 = security.SHA256Hash("identical input");
        std::string hash2 = security.SHA256Hash("identical input");
        
        REQUIRE(hash1 == hash2);
    }
    
    SECTION("Different input produces different hash") {
        std::string hash1 = security.SHA256Hash("input1");
        std::string hash2 = security.SHA256Hash("input2");
        
        REQUIRE(hash1 != hash2);
    }
    
    SECTION("Empty input returns empty hash") {
        std::string hash = security.SHA256Hash("");
        REQUIRE(hash.empty());
    }
    
    SECTION("Known test vector") {
        // SHA256("hello") = 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
        std::string hash = security.SHA256Hash("hello");
        REQUIRE(hash == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    }
}

// Test secure token generation
TEST_CASE("SecurityManager - Token generation", "[security][token]") {
    SecurityManager security;
    
    SECTION("Generate token successfully") {
        std::string token = security.GenerateSecureToken(32);
        
        REQUIRE(!token.empty());
        REQUIRE(token.length() == 64); // 32 bytes = 64 hex chars
    }
    
    SECTION("Tokens are unique") {
        std::set<std::string> tokens;
        
        for (int i = 0; i < 100; i++) {
            std::string token = security.GenerateSecureToken(32);
            REQUIRE(tokens.find(token) == tokens.end());
            tokens.insert(token);
        }
        
        REQUIRE(tokens.size() == 100);
    }
    
    SECTION("Token length is configurable") {
        std::string token16 = security.GenerateSecureToken(16);
        std::string token64 = security.GenerateSecureToken(64);
        
        REQUIRE(token16.length() == 32);  // 16 bytes = 32 hex chars
        REQUIRE(token64.length() == 128); // 64 bytes = 128 hex chars
    }
}

// Test rate limiting
TEST_CASE("SecurityManager - Rate limiting", "[security][ratelimit]") {
    SecurityManager security;
    
    SECTION("Not rate limited initially") {
        bool limited = security.CheckRateLimit("192.168.1.1", RateLimitType::Login);
        REQUIRE(limited == false);
    }
    
    SECTION("Rate limited after max attempts") {
        std::string ip = "192.168.1.2";
        
        // Record 5 attempts (default limit for login)
        for (int i = 0; i < 5; i++) {
            security.RecordAttempt(ip, RateLimitType::Login);
        }
        
        bool limited = security.CheckRateLimit(ip, RateLimitType::Login);
        REQUIRE(limited == true);
    }
    
    SECTION("Different IPs have separate limits") {
        std::string ip1 = "192.168.1.3";
        std::string ip2 = "192.168.1.4";
        
        // Rate limit ip1
        for (int i = 0; i < 5; i++) {
            security.RecordAttempt(ip1, RateLimitType::Login);
        }
        
        REQUIRE(security.CheckRateLimit(ip1, RateLimitType::Login) == true);
        REQUIRE(security.CheckRateLimit(ip2, RateLimitType::Login) == false);
    }
    
    SECTION("Reset rate limit") {
        std::string ip = "192.168.1.5";
        
        // Rate limit
        for (int i = 0; i < 5; i++) {
            security.RecordAttempt(ip, RateLimitType::Login);
        }
        REQUIRE(security.CheckRateLimit(ip, RateLimitType::Login) == true);
        
        // Reset
        security.ResetRateLimit(ip, RateLimitType::Login);
        REQUIRE(security.CheckRateLimit(ip, RateLimitType::Login) == false);
    }
    
    SECTION("Different rate limit types are independent") {
        std::string ip = "192.168.1.6";
        
        // Rate limit login
        for (int i = 0; i < 5; i++) {
            security.RecordAttempt(ip, RateLimitType::Login);
        }
        
        REQUIRE(security.CheckRateLimit(ip, RateLimitType::Login) == true);
        REQUIRE(security.CheckRateLimit(ip, RateLimitType::Register) == false);
    }
}

// Test IP blacklist
TEST_CASE("SecurityManager - IP blacklist", "[security][blacklist]") {
    SecurityManager security;
    
    SECTION("IP not blacklisted initially") {
        bool blacklisted = security.IsBlacklisted("10.0.0.1");
        REQUIRE(blacklisted == false);
    }
    
    SECTION("Add IP to blacklist permanently") {
        std::string ip = "10.0.0.2";
        
        security.AddToBlacklist(ip, 0); // 0 = permanent
        
        REQUIRE(security.IsBlacklisted(ip) == true);
    }
    
    SECTION("Remove IP from blacklist") {
        std::string ip = "10.0.0.3";
        
        security.AddToBlacklist(ip, 0);
        REQUIRE(security.IsBlacklisted(ip) == true);
        
        security.RemoveFromBlacklist(ip);
        REQUIRE(security.IsBlacklisted(ip) == false);
    }
    
    SECTION("Temporary blacklist with duration") {
        std::string ip = "10.0.0.4";
        
        // Add with 1 hour duration
        security.AddToBlacklist(ip, 3600);
        
        REQUIRE(security.IsBlacklisted(ip) == true);
    }
}

// Test suspicious activity detection
TEST_CASE("SecurityManager - Suspicious activity", "[security][suspicious]") {
    SecurityManager security;
    
    SECTION("No suspicious activity initially") {
        bool suspicious = security.IsSuspiciousActivity(1, "192.168.1.1");
        REQUIRE(suspicious == false);
    }
    
    SECTION("Record login from single IP") {
        u64 accountId = 100;
        std::string ip = "192.168.1.100";
        
        security.RecordLogin(accountId, ip);
        
        // Same IP should not be suspicious
        bool suspicious = security.IsSuspiciousActivity(accountId, ip);
        REQUIRE(suspicious == false);
    }
    
    SECTION("Multiple IPs triggers suspicious activity") {
        u64 accountId = 200;
        
        // Record logins from 5 different IPs
        for (int i = 0; i < 5; i++) {
            std::string ip = "192.168.2." + std::to_string(i);
            security.RecordLogin(accountId, ip);
        }
        
        // New IP should be suspicious
        bool suspicious = security.IsSuspiciousActivity(accountId, "192.168.2.100");
        REQUIRE(suspicious == true);
    }
    
    SECTION("Known IP is not suspicious") {
        u64 accountId = 300;
        
        // Record logins from 5 different IPs
        for (int i = 0; i < 5; i++) {
            std::string ip = "192.168.3." + std::to_string(i);
            security.RecordLogin(accountId, ip);
        }
        
        // Known IP should not be suspicious
        bool suspicious = security.IsSuspiciousActivity(accountId, "192.168.3.0");
        REQUIRE(suspicious == false);
    }
}

// Test rate limit configuration
TEST_CASE("SecurityManager - Rate limit config", "[security][config]") {
    SecurityManager security;
    
    SECTION("Get login rate limit config") {
        int maxAttempts;
        u64 windowSeconds;
        
        security.GetRateLimitConfig(RateLimitType::Login, maxAttempts, windowSeconds);
        
        REQUIRE(maxAttempts == 5);
        REQUIRE(windowSeconds == 60);
    }
    
    SECTION("Get register rate limit config") {
        int maxAttempts;
        u64 windowSeconds;
        
        security.GetRateLimitConfig(RateLimitType::Register, maxAttempts, windowSeconds);
        
        REQUIRE(maxAttempts == 3);
        REQUIRE(windowSeconds == 300);
    }
}
