#pragma once
/**
 * AuthProtocol - binary wire protocol for Authentication Server
 *
 * Goal: deterministic, small UDP messages for authentication operations:
 * - Registration
 * - Login
 * - Token validation
 * - Logout
 *
 * Compatible with existing matchmaking protocol style.
 */

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace auth {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

// Protocol constants
static constexpr u32 kAuthMagic = 0x41555448;  // 'AUTH'
static constexpr u16 kAuthVersion = 1;
static constexpr u16 kAuthServerPort = 27015;

// String size limits
static constexpr size_t kUsernameMax = 32;
static constexpr size_t kPasswordHashMax = 65;  // SHA256 hex + null
static constexpr size_t kSessionTokenMax = 65;  // 32 bytes hex + null
static constexpr size_t kEmailMax = 64;
static constexpr size_t kIpAddressMax = 46;     // IPv6 max
static constexpr size_t kErrorMessageMax = 128;
static constexpr size_t k2FACodeMax = 8;        // 6 digits + null + padding

/**
 * Authentication message types
 */
enum class AuthMessageType : u16 {
    // Client -> Auth Server
    RegisterRequest = 1,
    LoginRequest = 2,
    ValidateTokenRequest = 3,
    LogoutRequest = 4,
    Enable2FARequest = 5,
    ChangePasswordRequest = 6,
    
    // Auth Server -> Client
    RegisterResponse = 10,
    LoginResponse = 11,
    ValidateTokenResponse = 12,
    LogoutResponse = 13,
    Enable2FAResponse = 14,
    ChangePasswordResponse = 15,
    
    // Error
    Error = 255
};

/**
 * Authentication result codes
 */
enum class AuthResult : u8 {
    Success = 0,
    InvalidCredentials = 1,
    UsernameTaken = 2,
    InvalidUsername = 3,
    PasswordTooShort = 4,
    AccountLocked = 5,
    AccountBanned = 6,
    TokenExpired = 7,
    TokenInvalid = 8,
    RateLimited = 9,
    ServerError = 10,
    Requires2FA = 11,
    Invalid2FACode = 12
};

#pragma pack(push, 1)

/**
 * Authentication protocol header
 * Similar to MMHeader for consistency
 */
struct AuthHeader {
    u32 magic = kAuthMagic;
    u16 version = kAuthVersion;
    u16 type = 0;           // AuthMessageType as u16
    u32 payloadSize = 0;
    u64 accountId = 0;      // 0 for requests before authentication
    u32 requestId = 0;      // For request/response correlation
};

static_assert(sizeof(AuthHeader) == 24, "AuthHeader size must be stable");

// ---- Request Payloads ----

/**
 * Registration request payload
 */
struct RegisterRequestPayload {
    char username[kUsernameMax]{};
    char passwordHashSHA256[kPasswordHashMax]{};  // Client-side SHA256 hash
    char email[kEmailMax]{};
};

/**
 * Login request payload
 */
struct LoginRequestPayload {
    char username[kUsernameMax]{};
    char passwordHashSHA256[kPasswordHashMax]{};
    char twoFactorCode[k2FACodeMax]{};  // Optional 6-digit code
};

/**
 * Token validation request payload
 */
struct ValidateTokenRequestPayload {
    char sessionToken[kSessionTokenMax]{};
    char ipAddress[kIpAddressMax]{};
};

/**
 * Logout request payload
 */
struct LogoutRequestPayload {
    char sessionToken[kSessionTokenMax]{};
    u8 logoutAllSessions = 0;  // 1 = logout all sessions for this account
    u8 _reserved[7]{};
};

/**
 * Change password request payload
 */
struct ChangePasswordRequestPayload {
    char sessionToken[kSessionTokenMax]{};
    char oldPasswordHashSHA256[kPasswordHashMax]{};
    char newPasswordHashSHA256[kPasswordHashMax]{};
};

// ---- Response Payloads ----

/**
 * Registration response payload
 */
struct RegisterResponsePayload {
    u8 result = 0;          // AuthResult
    u8 _reserved[7]{};
    u64 accountId = 0;
    char sessionToken[kSessionTokenMax]{};
    char errorMessage[kErrorMessageMax]{};
};

/**
 * Login response payload
 */
struct LoginResponsePayload {
    u8 result = 0;          // AuthResult
    u8 requires2FA = 0;     // 1 if 2FA is required
    u8 _reserved[6]{};
    u64 accountId = 0;
    char sessionToken[kSessionTokenMax]{};
    char errorMessage[kErrorMessageMax]{};
};

/**
 * Token validation response payload
 */
struct ValidateTokenResponsePayload {
    u8 result = 0;          // AuthResult
    u8 isBanned = 0;
    u8 _reserved[6]{};
    u64 accountId = 0;
    u64 expiresAt = 0;      // Unix timestamp
    char errorMessage[kErrorMessageMax]{};
};

/**
 * Logout response payload
 */
struct LogoutResponsePayload {
    u8 result = 0;          // AuthResult
    u8 _reserved[7]{};
    u32 sessionsInvalidated = 0;
    u32 _reserved2 = 0;
    char errorMessage[kErrorMessageMax]{};
};

/**
 * Change password response payload
 */
struct ChangePasswordResponsePayload {
    u8 result = 0;          // AuthResult
    u8 _reserved[7]{};
    u32 sessionsInvalidated = 0;
    u32 _reserved2 = 0;
    char errorMessage[kErrorMessageMax]{};
};

/**
 * Generic error payload
 */
struct ErrorPayload {
    u8 errorCode = 0;       // AuthResult
    u8 _reserved[7]{};
    char message[kErrorMessageMax]{};
};

#pragma pack(pop)

// ---- Helper Functions ----

/**
 * Build an authentication packet
 * @param out Output buffer
 * @param type Message type
 * @param accountId Account ID (0 for unauthenticated requests)
 * @param requestId Request ID for correlation
 * @param payload Payload data
 * @param payloadSize Payload size in bytes
 * @return true if successful
 */
bool BuildPacket(std::vector<u8>& out,
                 AuthMessageType type,
                 u64 accountId,
                 u32 requestId,
                 const void* payload,
                 u32 payloadSize);

/**
 * Parse an authentication packet
 * @param data Input data
 * @param size Input data size
 * @param outHeader Parsed header
 * @param outPayload Pointer to payload in original buffer
 * @param outPayloadSize Payload size
 * @return true if valid packet
 */
bool ParsePacket(const void* data,
                 size_t size,
                 AuthHeader& outHeader,
                 const void*& outPayload,
                 u32& outPayloadSize);

/**
 * Validate packet magic number and version
 * @param header Header to validate
 * @return true if valid
 */
bool ValidateHeader(const AuthHeader& header);

/**
 * Safe string copy into fixed char array (always null-terminated)
 * @param dst Destination buffer
 * @param dstSize Destination buffer size
 * @param src Source string
 */
void CopyString(char* dst, size_t dstSize, const std::string& src);

/**
 * Get message type name for logging
 * @param type Message type
 * @return String name
 */
const char* GetMessageTypeName(AuthMessageType type);

/**
 * Get result name for logging
 * @param result Auth result
 * @return String name
 */
const char* GetResultName(AuthResult result);

} // namespace auth
