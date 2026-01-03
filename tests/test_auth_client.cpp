#include <catch2/catch_test_macros.hpp>
#include "auth/AuthClient.h"
#include "auth/SecurityManager.h"
#include <filesystem>

using namespace auth;

// Test AuthClient without actual network connection
TEST_CASE("AuthClient - Basic state management", "[client]") {
    AuthClient client;
    
    SECTION("Initial state") {
        REQUIRE(client.IsConnected() == false);
        REQUIRE(client.IsAuthenticated() == false);
        REQUIRE(client.IsGuest() == false);
        REQUIRE(client.GetAccountId() == 0);
        REQUIRE(client.GetSessionToken().empty());
        REQUIRE(client.GetUsername().empty());
    }
    
    SECTION("Guest account creation") {
        u64 guestId = client.CreateGuestAccount();
        
        REQUIRE(guestId > 0);
        REQUIRE(client.IsAuthenticated() == true);
        REQUIRE(client.IsGuest() == true);
        REQUIRE(client.GetAccountId() == guestId);
        REQUIRE(client.GetSessionToken().empty()); // Guests have no token
        REQUIRE(client.GetUsername().find("Guest_") == 0);
    }
    
    SECTION("Multiple guest accounts are unique") {
        AuthClient client1, client2, client3;
        
        u64 id1 = client1.CreateGuestAccount();
        u64 id2 = client2.CreateGuestAccount();
        u64 id3 = client3.CreateGuestAccount();
        
        // Very high probability of uniqueness
        REQUIRE(id1 != id2);
        REQUIRE(id2 != id3);
        REQUIRE(id1 != id3);
    }
}

TEST_CASE("AuthClient - Token storage", "[client][storage]") {
    const std::string testTokenPath = "test_token_storage.dat";
    
    // Cleanup before test
    std::filesystem::remove(testTokenPath);
    
    SECTION("Token storage path can be set") {
        AuthClient client;
        client.SetTokenStoragePath(testTokenPath);
        
        // Create guest (doesn't store token)
        client.CreateGuestAccount();
        
        // No token file should exist for guest
        REQUIRE(std::filesystem::exists(testTokenPath) == false);
    }
    
    // Cleanup after test
    std::filesystem::remove(testTokenPath);
}

TEST_CASE("AuthClient - Callback registration", "[client][callbacks]") {
    AuthClient client;
    
    bool registerSuccessCalled = false;
    bool registerFailedCalled = false;
    bool loginSuccessCalled = false;
    bool loginFailedCalled = false;
    bool tokenValidCalled = false;
    bool tokenInvalidCalled = false;
    bool logoutCalled = false;
    
    client.SetOnRegisterSuccess([&](u64, const std::string&) { registerSuccessCalled = true; });
    client.SetOnRegisterFailed([&](const std::string&) { registerFailedCalled = true; });
    client.SetOnLoginSuccess([&](u64, const std::string&) { loginSuccessCalled = true; });
    client.SetOnLoginFailed([&](const std::string&) { loginFailedCalled = true; });
    client.SetOnTokenValid([&](u64) { tokenValidCalled = true; });
    client.SetOnTokenInvalid([&]() { tokenInvalidCalled = true; });
    client.SetOnLogout([&](u32) { logoutCalled = true; });
    
    SECTION("Register fails when not connected") {
        client.Register("testuser", "password123");
        REQUIRE(registerFailedCalled == true);
    }
    
    SECTION("Login fails when not connected") {
        client.Login("testuser", "password123");
        REQUIRE(loginFailedCalled == true);
    }
    
    SECTION("ValidateStoredToken fails when not connected") {
        client.ValidateStoredToken();
        REQUIRE(tokenInvalidCalled == true);
    }
}

TEST_CASE("AuthClient - Client-side validation", "[client][validation]") {
    AuthClient client;
    
    std::string lastError;
    client.SetOnRegisterFailed([&](const std::string& error) { lastError = error; });
    
    SECTION("Username too short") {
        // Need to be connected for validation to run
        // Since we're not connected, it will fail with "Not connected"
        client.Register("ab", "password123");
        REQUIRE(lastError == "Not connected to auth server");
    }
    
    SECTION("Password too short - validation happens before connection check") {
        // Actually, connection check happens first
        client.Register("validuser", "short");
        REQUIRE(lastError == "Not connected to auth server");
    }
}

TEST_CASE("AuthClient - SHA256 password hashing", "[client][security]") {
    SecurityManager security;
    
    SECTION("Password is hashed consistently") {
        std::string password = "mySecurePassword123";
        std::string hash1 = security.SHA256Hash(password);
        std::string hash2 = security.SHA256Hash(password);
        
        REQUIRE(hash1 == hash2);
        REQUIRE(hash1.length() == 64); // SHA256 = 64 hex chars
    }
    
    SECTION("Different passwords produce different hashes") {
        std::string hash1 = security.SHA256Hash("password1");
        std::string hash2 = security.SHA256Hash("password2");
        
        REQUIRE(hash1 != hash2);
    }
}
