#include "auth/AuthProtocol.h"
#include <cstring>
#include <algorithm>

namespace auth {

bool BuildPacket(std::vector<u8>& out,
                 AuthMessageType type,
                 u64 accountId,
                 u32 requestId,
                 const void* payload,
                 u32 payloadSize) {
    // Calculate total size
    size_t totalSize = sizeof(AuthHeader) + payloadSize;
    out.resize(totalSize);

    // Build header
    AuthHeader header;
    header.magic = kAuthMagic;
    header.version = kAuthVersion;
    header.type = static_cast<u16>(type);
    header.payloadSize = payloadSize;
    header.accountId = accountId;
    header.requestId = requestId;

    // Copy header
    std::memcpy(out.data(), &header, sizeof(AuthHeader));

    // Copy payload if present
    if (payload && payloadSize > 0) {
        std::memcpy(out.data() + sizeof(AuthHeader), payload, payloadSize);
    }

    return true;
}

bool ParsePacket(const void* data,
                 size_t size,
                 AuthHeader& outHeader,
                 const void*& outPayload,
                 u32& outPayloadSize) {
    // Check minimum size
    if (!data || size < sizeof(AuthHeader)) {
        return false;
    }

    // Copy header
    std::memcpy(&outHeader, data, sizeof(AuthHeader));

    // Validate header
    if (!ValidateHeader(outHeader)) {
        return false;
    }

    // Check payload size
    if (size < sizeof(AuthHeader) + outHeader.payloadSize) {
        return false;
    }

    // Set payload pointer
    if (outHeader.payloadSize > 0) {
        outPayload = static_cast<const u8*>(data) + sizeof(AuthHeader);
    } else {
        outPayload = nullptr;
    }
    outPayloadSize = outHeader.payloadSize;

    return true;
}

bool ValidateHeader(const AuthHeader& header) {
    // Check magic number
    if (header.magic != kAuthMagic) {
        return false;
    }

    // Check version
    if (header.version != kAuthVersion) {
        return false;
    }

    // Check message type is valid
    u16 type = header.type;
    bool validType = (type >= 1 && type <= 6) ||   // Client requests
                     (type >= 10 && type <= 15) || // Server responses
                     (type == 255);                // Error
    
    if (!validType) {
        return false;
    }

    return true;
}

void CopyString(char* dst, size_t dstSize, const std::string& src) {
    if (!dst || dstSize == 0) {
        return;
    }

    size_t copyLen = std::min(src.size(), dstSize - 1);
    std::memcpy(dst, src.c_str(), copyLen);
    dst[copyLen] = '\0';
}

const char* GetMessageTypeName(AuthMessageType type) {
    switch (type) {
        case AuthMessageType::RegisterRequest:      return "RegisterRequest";
        case AuthMessageType::LoginRequest:         return "LoginRequest";
        case AuthMessageType::ValidateTokenRequest: return "ValidateTokenRequest";
        case AuthMessageType::LogoutRequest:        return "LogoutRequest";
        case AuthMessageType::Enable2FARequest:     return "Enable2FARequest";
        case AuthMessageType::ChangePasswordRequest: return "ChangePasswordRequest";
        case AuthMessageType::RegisterResponse:     return "RegisterResponse";
        case AuthMessageType::LoginResponse:        return "LoginResponse";
        case AuthMessageType::ValidateTokenResponse: return "ValidateTokenResponse";
        case AuthMessageType::LogoutResponse:       return "LogoutResponse";
        case AuthMessageType::Enable2FAResponse:    return "Enable2FAResponse";
        case AuthMessageType::ChangePasswordResponse: return "ChangePasswordResponse";
        case AuthMessageType::Error:                return "Error";
        default:                                    return "Unknown";
    }
}

const char* GetResultName(AuthResult result) {
    switch (result) {
        case AuthResult::Success:           return "Success";
        case AuthResult::InvalidCredentials: return "InvalidCredentials";
        case AuthResult::UsernameTaken:     return "UsernameTaken";
        case AuthResult::InvalidUsername:   return "InvalidUsername";
        case AuthResult::PasswordTooShort:  return "PasswordTooShort";
        case AuthResult::AccountLocked:     return "AccountLocked";
        case AuthResult::AccountBanned:     return "AccountBanned";
        case AuthResult::TokenExpired:      return "TokenExpired";
        case AuthResult::TokenInvalid:      return "TokenInvalid";
        case AuthResult::RateLimited:       return "RateLimited";
        case AuthResult::ServerError:       return "ServerError";
        case AuthResult::Requires2FA:       return "Requires2FA";
        case AuthResult::Invalid2FACode:    return "Invalid2FACode";
        default:                            return "Unknown";
    }
}

} // namespace auth
