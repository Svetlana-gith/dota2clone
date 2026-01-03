#include "auth/AuthServer.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace auth {

// Session expiration: 7 days in seconds
static constexpr u64 kSessionExpirationSeconds = 7 * 24 * 60 * 60;

std::string AuthNetworkAddress::toString() const {
    char ipStr[INET_ADDRSTRLEN];
    in_addr addr;
    addr.s_addr = ip;
    inet_ntop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);
    return std::string(ipStr) + ":" + std::to_string(port);
}

AuthServer::AuthServer() = default;

AuthServer::~AuthServer() {
    Shutdown();
}

bool AuthServer::Initialize(u16 port, const std::string& dbPath) {
    if (initialized_.load()) {
        spdlog::warn("AuthServer already initialized");
        return false;
    }
    
    spdlog::info("Initializing Auth Server on port {} with database {}", port, dbPath);
    
    // Initialize Winsock
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        spdlog::error("WSAStartup failed: {}", result);
        return false;
    }
#endif
    
    // Initialize database
    if (!db_.Initialize(dbPath)) {
        spdlog::error("Failed to initialize database");
        return false;
    }
    
    // Create UDP socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        spdlog::error("Failed to create socket: {}", WSAGetLastError());
        return false;
    }
    
    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
    
    // Bind to port
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (::bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        spdlog::error("Failed to bind socket to port {}: {}", port, WSAGetLastError());
        closesocket(sock);
        return false;
    }
    
    socket_ = reinterpret_cast<void*>(sock);
    port_ = port;
    initialized_.store(true);
    
    spdlog::info("Auth Server initialized successfully on port {}", port);
    return true;
}

void AuthServer::Run(bool blocking) {
    if (!initialized_.load()) {
        spdlog::error("AuthServer not initialized");
        return;
    }
    
    running_.store(true);
    spdlog::info("Auth Server started");
    
    if (blocking) {
        while (running_.load()) {
            Update(100);
            
            // Small sleep to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void AuthServer::Update(int maxPackets) {
    if (!running_.load()) return;
    
    ReceivePackets(maxPackets);
}

void AuthServer::Shutdown() {
    if (!initialized_.load()) return;
    
    running_.store(false);
    
    // Close socket
    if (socket_) {
        SOCKET sock = reinterpret_cast<SOCKET>(socket_);
        closesocket(sock);
        socket_ = nullptr;
    }
    
    // Shutdown database
    db_.Shutdown();
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    initialized_.store(false);
    spdlog::info("Auth Server shutdown complete");
}

AuthServer::Stats AuthServer::GetStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void AuthServer::ReceivePackets(int maxPackets) {
    SOCKET sock = reinterpret_cast<SOCKET>(socket_);
    u8 buffer[1400];  // Max UDP packet size
    
    for (int i = 0; i < maxPackets; i++) {
        sockaddr_in senderAddr;
        int senderSize = sizeof(senderAddr);
        
        int bytesReceived = recvfrom(sock, (char*)buffer, sizeof(buffer), 0,
                                     (sockaddr*)&senderAddr, &senderSize);
        
        if (bytesReceived <= 0) {
            break;  // No more packets
        }
        
        AuthNetworkAddress sender;
        sender.ip = senderAddr.sin_addr.s_addr;
        sender.port = ntohs(senderAddr.sin_port);
        
        HandlePacket(sender, buffer, bytesReceived);
    }
}

void AuthServer::HandlePacket(const AuthNetworkAddress& sender, const u8* data, size_t size) {
    // Parse packet
    AuthHeader header;
    const void* payload = nullptr;
    u32 payloadSize = 0;
    
    if (!ParsePacket(data, size, header, payload, payloadSize)) {
        spdlog::warn("Invalid packet from {}", sender.toString());
        return;
    }
    
    // Check rate limiting
    std::string clientIP = GetClientIP(sender);
    if (security_.IsBlacklisted(clientIP)) {
        spdlog::debug("Blocked request from blacklisted IP: {}", clientIP);
        return;
    }
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.totalRequests++;
    }
    
    // Route to handler
    AuthMessageType type = static_cast<AuthMessageType>(header.type);
    
    spdlog::debug("Received {} from {} (requestId: {})", 
                  GetMessageTypeName(type), sender.toString(), header.requestId);
    
    switch (type) {
        case AuthMessageType::RegisterRequest:
            if (payloadSize >= sizeof(RegisterRequestPayload)) {
                HandleRegisterRequest(sender, header, 
                    *static_cast<const RegisterRequestPayload*>(payload));
            }
            break;
            
        case AuthMessageType::LoginRequest:
            if (payloadSize >= sizeof(LoginRequestPayload)) {
                HandleLoginRequest(sender, header,
                    *static_cast<const LoginRequestPayload*>(payload));
            }
            break;
            
        case AuthMessageType::ValidateTokenRequest:
            if (payloadSize >= sizeof(ValidateTokenRequestPayload)) {
                HandleValidateTokenRequest(sender, header,
                    *static_cast<const ValidateTokenRequestPayload*>(payload));
            }
            break;
            
        case AuthMessageType::LogoutRequest:
            if (payloadSize >= sizeof(LogoutRequestPayload)) {
                HandleLogoutRequest(sender, header,
                    *static_cast<const LogoutRequestPayload*>(payload));
            }
            break;
            
        case AuthMessageType::ChangePasswordRequest:
            if (payloadSize >= sizeof(ChangePasswordRequestPayload)) {
                HandleChangePasswordRequest(sender, header,
                    *static_cast<const ChangePasswordRequestPayload*>(payload));
            }
            break;
            
        default:
            spdlog::warn("Unknown message type {} from {}", header.type, sender.toString());
            SendError(sender, header.requestId, AuthResult::ServerError, "Unknown message type");
            break;
    }
}


void AuthServer::HandleRegisterRequest(const AuthNetworkAddress& sender,
                                       const AuthHeader& header,
                                       const RegisterRequestPayload& payload) {
    std::string clientIP = GetClientIP(sender);
    
    // Check rate limit for registration
    if (security_.CheckRateLimit(clientIP, RateLimitType::Register)) {
        spdlog::warn("Registration rate limited for IP: {}", clientIP);
        SendError(sender, header.requestId, AuthResult::RateLimited, 
                  "Too many registration attempts. Please try again later.");
        return;
    }
    security_.RecordAttempt(clientIP, RateLimitType::Register);
    
    // Extract and validate username
    std::string username(payload.username, strnlen(payload.username, kUsernameMax));
    std::string passwordHash(payload.passwordHashSHA256, 
                             strnlen(payload.passwordHashSHA256, kPasswordHashMax));
    std::string email(payload.email, strnlen(payload.email, kEmailMax));
    
    spdlog::info("Registration request for username: {}", username);
    
    // Validate username format (alphanumeric, 3-20 chars)
    if (username.length() < 3 || username.length() > 20) {
        SendError(sender, header.requestId, AuthResult::InvalidUsername,
                  "Username must be 3-20 characters");
        return;
    }
    
    for (char c : username) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            SendError(sender, header.requestId, AuthResult::InvalidUsername,
                      "Username must be alphanumeric");
            return;
        }
    }
    
    // Validate password hash (should be SHA256 = 64 hex chars)
    if (passwordHash.length() < 8) {
        SendError(sender, header.requestId, AuthResult::PasswordTooShort,
                  "Password must be at least 8 characters");
        return;
    }
    
    // Check if username already exists
    Account existingAccount;
    if (db_.GetAccountByUsername(username, existingAccount)) {
        SendError(sender, header.requestId, AuthResult::UsernameTaken,
                  "Username already exists");
        return;
    }
    
    // Hash password with bcrypt (password is already SHA256 from client)
    std::string bcryptHash = security_.HashPassword(passwordHash, 12);
    if (bcryptHash.empty()) {
        SendError(sender, header.requestId, AuthResult::ServerError,
                  "Failed to hash password");
        return;
    }
    
    // Create account
    u64 accountId = 0;
    if (!db_.CreateAccount(username, bcryptHash, accountId)) {
        SendError(sender, header.requestId, AuthResult::ServerError,
                  "Failed to create account");
        return;
    }
    
    // Generate session token
    std::string sessionToken = security_.GenerateSecureToken(32);
    u64 now = static_cast<u64>(std::time(nullptr));
    u64 expiresAt = now + kSessionExpirationSeconds;
    
    // Create session
    if (!db_.CreateSession(accountId, sessionToken, expiresAt, clientIP)) {
        spdlog::error("Failed to create session for account {}", accountId);
        // Account created but session failed - still return success
    }
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.registrations++;
    }
    
    // Send response
    RegisterResponsePayload response;
    response.result = static_cast<u8>(AuthResult::Success);
    response.accountId = accountId;
    CopyString(response.sessionToken, sizeof(response.sessionToken), sessionToken);
    
    SendResponse(sender, AuthMessageType::RegisterResponse, accountId, 
                 header.requestId, &response, sizeof(response));
    
    spdlog::info("Account created: {} (ID: {})", username, accountId);
}

void AuthServer::HandleLoginRequest(const AuthNetworkAddress& sender,
                                    const AuthHeader& header,
                                    const LoginRequestPayload& payload) {
    std::string clientIP = GetClientIP(sender);
    
    // Check rate limit for login
    if (security_.CheckRateLimit(clientIP, RateLimitType::Login)) {
        spdlog::warn("Login rate limited for IP: {}", clientIP);
        SendError(sender, header.requestId, AuthResult::RateLimited,
                  "Too many login attempts. Please try again later.");
        return;
    }
    
    // Extract credentials
    std::string username(payload.username, strnlen(payload.username, kUsernameMax));
    std::string passwordHash(payload.passwordHashSHA256,
                             strnlen(payload.passwordHashSHA256, kPasswordHashMax));
    
    spdlog::info("Login request for username: {}", username);
    
    // Get account
    Account account;
    if (!db_.GetAccountByUsername(username, account)) {
        security_.RecordAttempt(clientIP, RateLimitType::Login);
        SendError(sender, header.requestId, AuthResult::InvalidCredentials,
                  "Invalid username or password");
        
        // Log failed attempt
        db_.LogLoginAttempt(0, clientIP, false, static_cast<u64>(std::time(nullptr)));
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.failedLogins++;
        return;
    }
    
    // Check if account is banned
    if (account.isBanned) {
        u64 now = static_cast<u64>(std::time(nullptr));
        if (account.banUntil == 0 || now < account.banUntil) {
            SendError(sender, header.requestId, AuthResult::AccountBanned,
                      account.banReason.empty() ? "Account is banned" : account.banReason);
            return;
        }
    }
    
    // Verify password
    if (!security_.VerifyPassword(passwordHash, account.passwordHash)) {
        security_.RecordAttempt(clientIP, RateLimitType::Login);
        SendError(sender, header.requestId, AuthResult::InvalidCredentials,
                  "Invalid username or password");
        
        // Log failed attempt
        db_.LogLoginAttempt(account.accountId, clientIP, false, 
                           static_cast<u64>(std::time(nullptr)));
        
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.failedLogins++;
        return;
    }
    
    // Check for suspicious activity
    if (security_.IsSuspiciousActivity(account.accountId, clientIP)) {
        spdlog::warn("Suspicious login activity for account {} from {}", 
                     account.accountId, clientIP);
        // For now, just log it - could require 2FA in the future
    }
    
    // Generate new session token
    std::string sessionToken = security_.GenerateSecureToken(32);
    u64 now = static_cast<u64>(std::time(nullptr));
    u64 expiresAt = now + kSessionExpirationSeconds;
    
    // Create session
    if (!db_.CreateSession(account.accountId, sessionToken, expiresAt, clientIP)) {
        SendError(sender, header.requestId, AuthResult::ServerError,
                  "Failed to create session");
        return;
    }
    
    // Update last login
    db_.UpdateLastLogin(account.accountId, now);
    
    // Log successful login
    db_.LogLoginAttempt(account.accountId, clientIP, true, now);
    
    // Record login for suspicious activity detection
    security_.RecordLogin(account.accountId, clientIP);
    
    // Reset rate limit on successful login
    security_.ResetRateLimit(clientIP, RateLimitType::Login);
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.successfulLogins++;
    }
    
    // Send response
    LoginResponsePayload response;
    response.result = static_cast<u8>(AuthResult::Success);
    response.requires2FA = 0;  // TODO: Check 2FA status
    response.accountId = account.accountId;
    CopyString(response.sessionToken, sizeof(response.sessionToken), sessionToken);
    
    SendResponse(sender, AuthMessageType::LoginResponse, account.accountId,
                 header.requestId, &response, sizeof(response));
    
    spdlog::info("Login successful: {} (ID: {})", username, account.accountId);
}

void AuthServer::HandleValidateTokenRequest(const AuthNetworkAddress& sender,
                                            const AuthHeader& header,
                                            const ValidateTokenRequestPayload& payload) {
    std::string clientIP = GetClientIP(sender);
    
    // Check rate limit
    if (security_.CheckRateLimit(clientIP, RateLimitType::TokenValidation)) {
        spdlog::warn("Token validation rate limited for IP: {}", clientIP);
        SendError(sender, header.requestId, AuthResult::RateLimited,
                  "Too many validation requests");
        return;
    }
    security_.RecordAttempt(clientIP, RateLimitType::TokenValidation);
    
    // Extract token
    std::string token(payload.sessionToken, strnlen(payload.sessionToken, kSessionTokenMax));
    std::string requestIP(payload.ipAddress, strnlen(payload.ipAddress, kIpAddressMax));
    
    // Get session
    Session session;
    if (!db_.GetSession(token, session)) {
        ValidateTokenResponsePayload response;
        response.result = static_cast<u8>(AuthResult::TokenInvalid);
        response.isBanned = 0;
        response.accountId = 0;
        response.expiresAt = 0;
        CopyString(response.errorMessage, sizeof(response.errorMessage), "Invalid token");
        
        SendResponse(sender, AuthMessageType::ValidateTokenResponse, 0,
                     header.requestId, &response, sizeof(response));
        return;
    }
    
    // Check expiration
    u64 now = static_cast<u64>(std::time(nullptr));
    if (now >= session.expiresAt) {
        // Delete expired session
        db_.DeleteSession(token);
        
        ValidateTokenResponsePayload response;
        response.result = static_cast<u8>(AuthResult::TokenExpired);
        response.isBanned = 0;
        response.accountId = session.accountId;
        response.expiresAt = 0;
        CopyString(response.errorMessage, sizeof(response.errorMessage), "Token expired");
        
        SendResponse(sender, AuthMessageType::ValidateTokenResponse, session.accountId,
                     header.requestId, &response, sizeof(response));
        return;
    }
    
    // Check if account is banned
    Account account;
    bool isBanned = false;
    if (db_.GetAccountById(session.accountId, account)) {
        if (account.isBanned && (account.banUntil == 0 || now < account.banUntil)) {
            isBanned = true;
        }
    }
    
    // Extend session expiration
    u64 newExpiresAt = now + kSessionExpirationSeconds;
    db_.UpdateSessionExpiration(token, newExpiresAt);
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.tokenValidations++;
    }
    
    // Send response
    ValidateTokenResponsePayload response;
    response.result = static_cast<u8>(AuthResult::Success);
    response.isBanned = isBanned ? 1 : 0;
    response.accountId = session.accountId;
    response.expiresAt = newExpiresAt;
    
    SendResponse(sender, AuthMessageType::ValidateTokenResponse, session.accountId,
                 header.requestId, &response, sizeof(response));
    
    spdlog::debug("Token validated for account {}", session.accountId);
}


void AuthServer::HandleLogoutRequest(const AuthNetworkAddress& sender,
                                     const AuthHeader& header,
                                     const LogoutRequestPayload& payload) {
    // Extract token
    std::string token(payload.sessionToken, strnlen(payload.sessionToken, kSessionTokenMax));
    bool logoutAll = payload.logoutAllSessions != 0;
    
    // Get session to find account ID
    Session session;
    if (!db_.GetSession(token, session)) {
        // Token doesn't exist - still return success (idempotent)
        LogoutResponsePayload response;
        response.result = static_cast<u8>(AuthResult::Success);
        response.sessionsInvalidated = 0;
        
        SendResponse(sender, AuthMessageType::LogoutResponse, 0,
                     header.requestId, &response, sizeof(response));
        return;
    }
    
    u32 sessionsInvalidated = 0;
    
    if (logoutAll) {
        // Delete all sessions for this account except current
        sessionsInvalidated = db_.DeleteAllSessionsForAccount(session.accountId, token);
        sessionsInvalidated++; // Include current session
    }
    
    // Delete the current session
    db_.DeleteSession(token);
    if (!logoutAll) {
        sessionsInvalidated = 1;
    }
    
    // Send response
    LogoutResponsePayload response;
    response.result = static_cast<u8>(AuthResult::Success);
    response.sessionsInvalidated = sessionsInvalidated;
    
    SendResponse(sender, AuthMessageType::LogoutResponse, session.accountId,
                 header.requestId, &response, sizeof(response));
    
    spdlog::info("Logout: account {} (sessions invalidated: {})", 
                 session.accountId, sessionsInvalidated);
}

void AuthServer::HandleChangePasswordRequest(const AuthNetworkAddress& sender,
                                             const AuthHeader& header,
                                             const ChangePasswordRequestPayload& payload) {
    // Extract data
    std::string token(payload.sessionToken, strnlen(payload.sessionToken, kSessionTokenMax));
    std::string oldPasswordHash(payload.oldPasswordHashSHA256,
                                strnlen(payload.oldPasswordHashSHA256, kPasswordHashMax));
    std::string newPasswordHash(payload.newPasswordHashSHA256,
                                strnlen(payload.newPasswordHashSHA256, kPasswordHashMax));
    
    // Validate session
    Session session;
    if (!db_.GetSession(token, session)) {
        SendError(sender, header.requestId, AuthResult::TokenInvalid, "Invalid session");
        return;
    }
    
    // Check token expiration
    u64 now = static_cast<u64>(std::time(nullptr));
    if (now >= session.expiresAt) {
        SendError(sender, header.requestId, AuthResult::TokenExpired, "Session expired");
        return;
    }
    
    // Get account
    Account account;
    if (!db_.GetAccountById(session.accountId, account)) {
        SendError(sender, header.requestId, AuthResult::ServerError, "Account not found");
        return;
    }
    
    // Verify old password
    if (!security_.VerifyPassword(oldPasswordHash, account.passwordHash)) {
        SendError(sender, header.requestId, AuthResult::InvalidCredentials, 
                  "Current password is incorrect");
        return;
    }
    
    // Validate new password
    if (newPasswordHash.length() < 8) {
        SendError(sender, header.requestId, AuthResult::PasswordTooShort,
                  "New password must be at least 8 characters");
        return;
    }
    
    // Hash new password with bcrypt
    std::string newBcryptHash = security_.HashPassword(newPasswordHash, 12);
    if (newBcryptHash.empty()) {
        SendError(sender, header.requestId, AuthResult::ServerError,
                  "Failed to hash password");
        return;
    }
    
    // Update password in database
    if (!db_.UpdatePassword(session.accountId, newBcryptHash)) {
        SendError(sender, header.requestId, AuthResult::ServerError,
                  "Failed to update password");
        return;
    }
    
    // Invalidate all other sessions (security requirement)
    u32 sessionsInvalidated = db_.DeleteAllSessionsForAccount(session.accountId, token);
    
    // Send response
    ChangePasswordResponsePayload response;
    response.result = static_cast<u8>(AuthResult::Success);
    response.sessionsInvalidated = sessionsInvalidated;
    
    SendResponse(sender, AuthMessageType::ChangePasswordResponse, session.accountId,
                 header.requestId, &response, sizeof(response));
    
    spdlog::info("Password changed for account {} (sessions invalidated: {})",
                 session.accountId, sessionsInvalidated);
}

void AuthServer::SendResponse(const AuthNetworkAddress& dest,
                              AuthMessageType type,
                              u64 accountId,
                              u32 requestId,
                              const void* payload,
                              u32 payloadSize) {
    std::vector<u8> packet;
    BuildPacket(packet, type, accountId, requestId, payload, payloadSize);
    
    SOCKET sock = reinterpret_cast<SOCKET>(socket_);
    sockaddr_in destAddr;
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = dest.ip;
    destAddr.sin_port = htons(dest.port);
    
    sendto(sock, (const char*)packet.data(), (int)packet.size(), 0,
           (sockaddr*)&destAddr, sizeof(destAddr));
    
    spdlog::debug("Sent {} to {} (requestId: {})",
                  GetMessageTypeName(type), dest.toString(), requestId);
}

void AuthServer::SendError(const AuthNetworkAddress& dest,
                           u32 requestId,
                           AuthResult errorCode,
                           const std::string& message) {
    ErrorPayload payload;
    payload.errorCode = static_cast<u8>(errorCode);
    CopyString(payload.message, sizeof(payload.message), message);
    
    SendResponse(dest, AuthMessageType::Error, 0, requestId, &payload, sizeof(payload));
    
    spdlog::debug("Sent error to {}: {} - {}", 
                  dest.toString(), GetResultName(errorCode), message);
}

std::string AuthServer::GetClientIP(const AuthNetworkAddress& addr) const {
    char ipStr[INET_ADDRSTRLEN];
    in_addr inAddr;
    inAddr.s_addr = addr.ip;
    inet_ntop(AF_INET, &inAddr, ipStr, INET_ADDRSTRLEN);
    return std::string(ipStr);
}

} // namespace auth
