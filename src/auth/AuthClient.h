#pragma once
/**
 * AuthClient - Client-side authentication module
 * 
 * Provides API for game client to:
 * - Register new accounts
 * - Login with credentials
 * - Validate stored session tokens
 * - Logout
 * - Guest mode
 */

#include "auth/AuthProtocol.h"
#include <string>
#include <functional>
#include <atomic>
#include <mutex>

namespace auth {

/**
 * Authentication Client
 * 
 * Handles communication with Auth Server and local token storage.
 */
class AuthClient {
public:
    AuthClient();
    ~AuthClient();
    
    // Disable copy
    AuthClient(const AuthClient&) = delete;
    AuthClient& operator=(const AuthClient&) = delete;
    
    /**
     * Connect to Auth Server.
     * @param serverIP Auth server IP address
     * @param port Auth server port
     * @return true if connection established
     */
    bool Connect(const std::string& serverIP, u16 port = kAuthServerPort);
    
    /**
     * Disconnect from Auth Server.
     */
    void Disconnect();
    
    /**
     * Check if connected to Auth Server.
     */
    bool IsConnected() const { return connected_.load(); }
    
    /**
     * Process incoming packets (call in game loop).
     */
    void Update();
    
    // ---- Authentication Operations ----
    
    /**
     * Register a new account.
     * @param username Username (3-20 alphanumeric chars)
     * @param password Password (min 8 chars)
     */
    void Register(const std::string& username, const std::string& password);
    
    /**
     * Login with credentials.
     * @param username Username
     * @param password Password
     */
    void Login(const std::string& username, const std::string& password);
    
    /**
     * Validate stored session token.
     * Call on game startup to check if previous session is still valid.
     */
    void ValidateStoredToken();
    
    /**
     * Logout current session.
     * @param logoutAll If true, invalidate all sessions for this account
     */
    void Logout(bool logoutAll = false);
    
    /**
     * Create a guest account (no server communication).
     * @return Temporary guest account ID
     */
    u64 CreateGuestAccount();
    
    // ---- Callbacks ----
    
    using RegisterSuccessCallback = std::function<void(u64 accountId, const std::string& token)>;
    using RegisterFailedCallback = std::function<void(const std::string& error)>;
    using LoginSuccessCallback = std::function<void(u64 accountId, const std::string& token)>;
    using LoginFailedCallback = std::function<void(const std::string& error)>;
    using TokenValidCallback = std::function<void(u64 accountId)>;
    using TokenInvalidCallback = std::function<void()>;
    using LogoutCallback = std::function<void(u32 sessionsInvalidated)>;
    
    void SetOnRegisterSuccess(RegisterSuccessCallback cb) { onRegisterSuccess_ = cb; }
    void SetOnRegisterFailed(RegisterFailedCallback cb) { onRegisterFailed_ = cb; }
    void SetOnLoginSuccess(LoginSuccessCallback cb) { onLoginSuccess_ = cb; }
    void SetOnLoginFailed(LoginFailedCallback cb) { onLoginFailed_ = cb; }
    void SetOnTokenValid(TokenValidCallback cb) { onTokenValid_ = cb; }
    void SetOnTokenInvalid(TokenInvalidCallback cb) { onTokenInvalid_ = cb; }
    void SetOnLogout(LogoutCallback cb) { onLogout_ = cb; }
    
    // ---- State ----
    
    bool IsAuthenticated() const { return authenticated_.load(); }
    bool IsGuest() const { return isGuest_.load(); }
    u64 GetAccountId() const { return accountId_.load(); }
    std::string GetSessionToken() const;
    std::string GetUsername() const;
    
    // ---- Token Storage ----
    
    /**
     * Set path for token storage file.
     * @param path File path (default: "auth_token.dat")
     */
    void SetTokenStoragePath(const std::string& path) { tokenStoragePath_ = path; }
    
private:
    // Network
    void SendPacket(AuthMessageType type, const void* payload, u32 payloadSize);
    void ReceivePackets();
    void HandlePacket(const u8* data, size_t size);
    
    // Response handlers
    void HandleRegisterResponse(const RegisterResponsePayload& payload);
    void HandleLoginResponse(const LoginResponsePayload& payload);
    void HandleValidateTokenResponse(const ValidateTokenResponsePayload& payload);
    void HandleLogoutResponse(const LogoutResponsePayload& payload);
    void HandleError(const ErrorPayload& payload);
    
    // Token storage
    void StoreToken(const std::string& token, const std::string& username);
    bool LoadToken(std::string& outToken, std::string& outUsername);
    void ClearStoredToken();
    
    // Helpers
    std::string HashPasswordSHA256(const std::string& password);
    u32 NextRequestId() { return ++nextRequestId_; }
    
    // Network state
    void* socket_ = nullptr;  // SOCKET handle
    std::string serverIP_;
    u16 serverPort_ = 0;
    std::atomic<bool> connected_{false};
    
    // Auth state
    std::atomic<bool> authenticated_{false};
    std::atomic<bool> isGuest_{false};
    std::atomic<u64> accountId_{0};
    std::string sessionToken_;
    std::string username_;
    mutable std::mutex stateMutex_;
    
    // Request tracking
    std::atomic<u32> nextRequestId_{0};
    
    // Token storage
    std::string tokenStoragePath_ = "auth_token.dat";
    
    // Callbacks
    RegisterSuccessCallback onRegisterSuccess_;
    RegisterFailedCallback onRegisterFailed_;
    LoginSuccessCallback onLoginSuccess_;
    LoginFailedCallback onLoginFailed_;
    TokenValidCallback onTokenValid_;
    TokenInvalidCallback onTokenInvalid_;
    LogoutCallback onLogout_;
};

} // namespace auth
