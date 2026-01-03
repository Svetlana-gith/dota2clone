#include <catch2/catch_test_macros.hpp>
#include "auth/AuthProtocol.h"
#include <cstring>

using namespace auth;

// Test packet building
TEST_CASE("AuthProtocol - Packet building", "[protocol][build]") {
    
    SECTION("Build register request packet") {
        RegisterRequestPayload payload;
        CopyString(payload.username, sizeof(payload.username), "testuser");
        CopyString(payload.passwordHashSHA256, sizeof(payload.passwordHashSHA256), 
                   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        CopyString(payload.email, sizeof(payload.email), "test@example.com");
        
        std::vector<u8> packet;
        bool result = BuildPacket(packet, 
                                  AuthMessageType::RegisterRequest,
                                  0,  // No account ID yet
                                  1,  // Request ID
                                  &payload,
                                  sizeof(payload));
        
        REQUIRE(result == true);
        REQUIRE(packet.size() == sizeof(AuthHeader) + sizeof(RegisterRequestPayload));
    }
    
    SECTION("Build login request packet") {
        LoginRequestPayload payload;
        CopyString(payload.username, sizeof(payload.username), "myuser");
        CopyString(payload.passwordHashSHA256, sizeof(payload.passwordHashSHA256),
                   "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890");
        
        std::vector<u8> packet;
        bool result = BuildPacket(packet,
                                  AuthMessageType::LoginRequest,
                                  0,
                                  2,
                                  &payload,
                                  sizeof(payload));
        
        REQUIRE(result == true);
        REQUIRE(packet.size() == sizeof(AuthHeader) + sizeof(LoginRequestPayload));
    }
    
    SECTION("Build validate token request packet") {
        ValidateTokenRequestPayload payload;
        CopyString(payload.sessionToken, sizeof(payload.sessionToken),
                   "abc123def456abc123def456abc123def456abc123def456abc123def456abcd");
        CopyString(payload.ipAddress, sizeof(payload.ipAddress), "192.168.1.100");
        
        std::vector<u8> packet;
        bool result = BuildPacket(packet,
                                  AuthMessageType::ValidateTokenRequest,
                                  12345,  // Account ID
                                  3,
                                  &payload,
                                  sizeof(payload));
        
        REQUIRE(result == true);
    }
    
    SECTION("Build packet without payload") {
        std::vector<u8> packet;
        bool result = BuildPacket(packet,
                                  AuthMessageType::Error,
                                  0,
                                  0,
                                  nullptr,
                                  0);
        
        REQUIRE(result == true);
        REQUIRE(packet.size() == sizeof(AuthHeader));
    }
}

// Test packet parsing
TEST_CASE("AuthProtocol - Packet parsing", "[protocol][parse]") {
    
    SECTION("Parse register request packet") {
        // Build packet first
        RegisterRequestPayload sendPayload;
        CopyString(sendPayload.username, sizeof(sendPayload.username), "parsetest");
        CopyString(sendPayload.passwordHashSHA256, sizeof(sendPayload.passwordHashSHA256),
                   "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");
        
        std::vector<u8> packet;
        BuildPacket(packet, AuthMessageType::RegisterRequest, 0, 100, &sendPayload, sizeof(sendPayload));
        
        // Parse packet
        AuthHeader header;
        const void* payload = nullptr;
        u32 payloadSize = 0;
        
        bool result = ParsePacket(packet.data(), packet.size(), header, payload, payloadSize);
        
        REQUIRE(result == true);
        REQUIRE(header.magic == kAuthMagic);
        REQUIRE(header.version == kAuthVersion);
        REQUIRE(header.type == static_cast<u16>(AuthMessageType::RegisterRequest));
        REQUIRE(header.requestId == 100);
        REQUIRE(payloadSize == sizeof(RegisterRequestPayload));
        REQUIRE(payload != nullptr);
        
        // Verify payload content
        const RegisterRequestPayload* recvPayload = static_cast<const RegisterRequestPayload*>(payload);
        REQUIRE(std::string(recvPayload->username) == "parsetest");
    }
    
    SECTION("Parse login response packet") {
        LoginResponsePayload sendPayload;
        sendPayload.result = static_cast<u8>(AuthResult::Success);
        sendPayload.accountId = 999;
        CopyString(sendPayload.sessionToken, sizeof(sendPayload.sessionToken), "session_token_here");
        
        std::vector<u8> packet;
        BuildPacket(packet, AuthMessageType::LoginResponse, 999, 50, &sendPayload, sizeof(sendPayload));
        
        AuthHeader header;
        const void* payload = nullptr;
        u32 payloadSize = 0;
        
        bool result = ParsePacket(packet.data(), packet.size(), header, payload, payloadSize);
        
        REQUIRE(result == true);
        REQUIRE(header.accountId == 999);
        REQUIRE(header.type == static_cast<u16>(AuthMessageType::LoginResponse));
        
        const LoginResponsePayload* recvPayload = static_cast<const LoginResponsePayload*>(payload);
        REQUIRE(recvPayload->result == static_cast<u8>(AuthResult::Success));
        REQUIRE(recvPayload->accountId == 999);
    }
    
    SECTION("Parse packet with invalid magic") {
        std::vector<u8> packet(sizeof(AuthHeader));
        AuthHeader* header = reinterpret_cast<AuthHeader*>(packet.data());
        header->magic = 0xDEADBEEF;  // Wrong magic
        header->version = kAuthVersion;
        header->type = 1;
        header->payloadSize = 0;
        
        AuthHeader outHeader;
        const void* payload = nullptr;
        u32 payloadSize = 0;
        
        bool result = ParsePacket(packet.data(), packet.size(), outHeader, payload, payloadSize);
        
        REQUIRE(result == false);
    }
    
    SECTION("Parse packet with invalid version") {
        std::vector<u8> packet(sizeof(AuthHeader));
        AuthHeader* header = reinterpret_cast<AuthHeader*>(packet.data());
        header->magic = kAuthMagic;
        header->version = 99;  // Wrong version
        header->type = 1;
        header->payloadSize = 0;
        
        AuthHeader outHeader;
        const void* payload = nullptr;
        u32 payloadSize = 0;
        
        bool result = ParsePacket(packet.data(), packet.size(), outHeader, payload, payloadSize);
        
        REQUIRE(result == false);
    }
    
    SECTION("Parse packet too small") {
        std::vector<u8> packet(10);  // Too small for header
        
        AuthHeader outHeader;
        const void* payload = nullptr;
        u32 payloadSize = 0;
        
        bool result = ParsePacket(packet.data(), packet.size(), outHeader, payload, payloadSize);
        
        REQUIRE(result == false);
    }
    
    SECTION("Parse packet with truncated payload") {
        RegisterRequestPayload sendPayload;
        CopyString(sendPayload.username, sizeof(sendPayload.username), "truncated");
        
        std::vector<u8> packet;
        BuildPacket(packet, AuthMessageType::RegisterRequest, 0, 1, &sendPayload, sizeof(sendPayload));
        
        // Truncate the packet
        packet.resize(sizeof(AuthHeader) + 10);
        
        AuthHeader outHeader;
        const void* payload = nullptr;
        u32 payloadSize = 0;
        
        bool result = ParsePacket(packet.data(), packet.size(), outHeader, payload, payloadSize);
        
        REQUIRE(result == false);
    }
}

// Test magic number validation
TEST_CASE("AuthProtocol - Magic number validation", "[protocol][magic]") {
    
    SECTION("Valid magic number") {
        AuthHeader header;
        header.magic = kAuthMagic;
        header.version = kAuthVersion;
        header.type = 1;
        
        REQUIRE(ValidateHeader(header) == true);
    }
    
    SECTION("Invalid magic number") {
        AuthHeader header;
        header.magic = 0x12345678;
        header.version = kAuthVersion;
        header.type = 1;
        
        REQUIRE(ValidateHeader(header) == false);
    }
    
    SECTION("Magic number is AUTH") {
        // 'AUTH' = 0x41555448
        REQUIRE(kAuthMagic == 0x41555448);
    }
}

// Test string copy helper
TEST_CASE("AuthProtocol - String copy", "[protocol][string]") {
    
    SECTION("Copy normal string") {
        char dst[32] = {0};
        CopyString(dst, sizeof(dst), "hello");
        
        REQUIRE(std::string(dst) == "hello");
    }
    
    SECTION("Copy string that fits exactly") {
        char dst[6] = {0};
        CopyString(dst, sizeof(dst), "hello");
        
        REQUIRE(std::string(dst) == "hello");
    }
    
    SECTION("Truncate long string") {
        char dst[6] = {0};
        CopyString(dst, sizeof(dst), "hello world");
        
        REQUIRE(std::string(dst) == "hello");
        REQUIRE(dst[5] == '\0');
    }
    
    SECTION("Copy empty string") {
        char dst[32] = {'x', 'x', 'x', 0};
        CopyString(dst, sizeof(dst), "");
        
        REQUIRE(std::string(dst) == "");
    }
}

// Test message type names
TEST_CASE("AuthProtocol - Message type names", "[protocol][names]") {
    
    SECTION("Request type names") {
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::RegisterRequest)) == "RegisterRequest");
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::LoginRequest)) == "LoginRequest");
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::ValidateTokenRequest)) == "ValidateTokenRequest");
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::LogoutRequest)) == "LogoutRequest");
    }
    
    SECTION("Response type names") {
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::RegisterResponse)) == "RegisterResponse");
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::LoginResponse)) == "LoginResponse");
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::ValidateTokenResponse)) == "ValidateTokenResponse");
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::LogoutResponse)) == "LogoutResponse");
    }
    
    SECTION("Error type name") {
        REQUIRE(std::string(GetMessageTypeName(AuthMessageType::Error)) == "Error");
    }
}

// Test result names
TEST_CASE("AuthProtocol - Result names", "[protocol][results]") {
    
    SECTION("Success result") {
        REQUIRE(std::string(GetResultName(AuthResult::Success)) == "Success");
    }
    
    SECTION("Error results") {
        REQUIRE(std::string(GetResultName(AuthResult::InvalidCredentials)) == "InvalidCredentials");
        REQUIRE(std::string(GetResultName(AuthResult::UsernameTaken)) == "UsernameTaken");
        REQUIRE(std::string(GetResultName(AuthResult::AccountLocked)) == "AccountLocked");
        REQUIRE(std::string(GetResultName(AuthResult::TokenExpired)) == "TokenExpired");
        REQUIRE(std::string(GetResultName(AuthResult::RateLimited)) == "RateLimited");
    }
}

// Test round-trip serialization
TEST_CASE("AuthProtocol - Round-trip serialization", "[protocol][roundtrip]") {
    
    SECTION("Register request round-trip") {
        RegisterRequestPayload original;
        CopyString(original.username, sizeof(original.username), "roundtrip_user");
        CopyString(original.passwordHashSHA256, sizeof(original.passwordHashSHA256),
                   "fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321");
        CopyString(original.email, sizeof(original.email), "roundtrip@test.com");
        
        // Serialize
        std::vector<u8> packet;
        BuildPacket(packet, AuthMessageType::RegisterRequest, 0, 42, &original, sizeof(original));
        
        // Deserialize
        AuthHeader header;
        const void* payload = nullptr;
        u32 payloadSize = 0;
        
        bool result = ParsePacket(packet.data(), packet.size(), header, payload, payloadSize);
        REQUIRE(result == true);
        
        const RegisterRequestPayload* parsed = static_cast<const RegisterRequestPayload*>(payload);
        
        // Verify all fields
        REQUIRE(std::string(parsed->username) == "roundtrip_user");
        REQUIRE(std::string(parsed->passwordHashSHA256) == 
                "fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321");
        REQUIRE(std::string(parsed->email) == "roundtrip@test.com");
    }
    
    SECTION("Login response round-trip") {
        LoginResponsePayload original;
        original.result = static_cast<u8>(AuthResult::Success);
        original.requires2FA = 0;
        original.accountId = 123456789;
        CopyString(original.sessionToken, sizeof(original.sessionToken), "session123");
        CopyString(original.errorMessage, sizeof(original.errorMessage), "");
        
        // Serialize
        std::vector<u8> packet;
        BuildPacket(packet, AuthMessageType::LoginResponse, 123456789, 99, &original, sizeof(original));
        
        // Deserialize
        AuthHeader header;
        const void* payload = nullptr;
        u32 payloadSize = 0;
        
        bool result = ParsePacket(packet.data(), packet.size(), header, payload, payloadSize);
        REQUIRE(result == true);
        
        const LoginResponsePayload* parsed = static_cast<const LoginResponsePayload*>(payload);
        
        // Verify all fields
        REQUIRE(parsed->result == static_cast<u8>(AuthResult::Success));
        REQUIRE(parsed->accountId == 123456789);
        REQUIRE(std::string(parsed->sessionToken) == "session123");
    }
}
