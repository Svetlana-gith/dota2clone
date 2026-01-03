#include "auth/AuthClient.h"
#include "auth/SecurityManager.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstring>
#include <random>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace auth {

AuthClient::AuthClient() = default;

AuthClient::~AuthClient() {
    Disconnect();
}

bool AuthClient::Connect(const std::string& serverIP, u16 port) {
    if (connected_.load()) {
        spdlog::warn("AuthClient already connected");
        return true;
    }
    
    // Initialize Winsock
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        spdlog::error("WSAStartup failed: {}", result);
        return false;
    }
#endif
    
    // Create UDP socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        spdlog::error("Failed to create socket: {}", WSAGetLastError());
        return false;
    }
    
    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
    
    socket_ = reinterpret_cast<void*>(sock);
    serverIP_ = serverIP;
    serverPort_ = port;
    connected_.store(true);
    
    spdlog::info("AuthClient connected to {}:{}", serverIP, port);
    return true;
}

void AuthClient::Disconnect() {
    if (!connected_.load()) return;
    
    connected_.store(false);
    authenticated_.store(false);
    
    if (socket_) {
        SOCKET sock = reinterpret_cast<SOCKET>(socket_);
        closesocket(sock);
        socket_ = nullptr;
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    spdlog::info("AuthClient disconnected");
}

void AuthClient::Update() {
    if (!connected_.load()) return;
    ReceivePackets();
}

void AuthClient::Register(const std::string& username, const std::string& password) {
    if (!connected_.load()) {
        if (onRegisterFailed_) onRegisterFailed_("Not connected to auth server");
        return;
    }
    
    // Client-side validation
    if (username.length() < 3 || username.length() > 20) {
        if (onRegisterFailed_) onRegisterFailed_("Username must be 3-20 characters");
        return;
    }
    
    if (password.length() < 8) {
        if (onRegisterFailed_) onRegisterFailed_("Password must be at least 8 characters");
        return;
    }
    
    // Hash password with SHA256 before sending
    std::string passwordHash = HashPasswordSHA256(password);
    
    RegisterRequestPayload payload;
    CopyString(payload.username, sizeof(payload.username), username);
    CopyString(payload.passwordHashSHA256, sizeof(payload.passwordHashSHA256), passwordHash);
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        username_ = username;
    }
    
    SendPacket(AuthMessageType::RegisterRequest, &payload, sizeof(payload));
    spdlog::info("Registration request sent for user: {}", username);
}

void AuthClient::Login(const std::string& username, const std::string& password) {
    if (!connected_.load()) {
        if (onLoginFailed_) onLoginFailed_("Not connected to auth server");
        return;
    }
    
    // Hash password with SHA256 before sending
    std::string passwordHash = HashPasswordSHA256(password);
    
    LoginRequestPayload payload;
    CopyString(payload.username, sizeof(payload.username), username);
    CopyString(payload.passwordHashSHA256, sizeof(payload.passwordHashSHA256), passwordHash);
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        username_ = username;
    }
    
    SendPacket(AuthMessageType::LoginRequest, &payload, sizeof(payload));
    spdlog::info("Login request sent for user: {}", username);
}

void AuthClient::ValidateStoredToken() {
    if (!connected_.load()) {
        if (onTokenInvalid_) onTokenInvalid_();
        return;
    }
    
    std::string token, username;
    if (!LoadToken(token, username)) {
        spdlog::info("No stored token found");
        if (onTokenInvalid_) onTokenInvalid_();
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        sessionToken_ = token;
        username_ = username;
    }
    
    ValidateTokenRequestPayload payload;
    CopyString(payload.sessionToken, sizeof(payload.sessionToken), token);
    CopyString(payload.ipAddress, sizeof(payload.ipAddress), "127.0.0.1");
    
    SendPacket(AuthMessageType::ValidateTokenRequest, &payload, sizeof(payload));
    spdlog::info("Token validation request sent");
}

void AuthClient::Logout(bool logoutAll) {
    std::string token;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        token = sessionToken_;
    }
    
    if (token.empty()) {
        spdlog::warn("No session to logout");
        return;
    }
    
    if (connected_.load()) {
        LogoutRequestPayload payload;
        CopyString(payload.sessionToken, sizeof(payload.sessionToken), token);
        payload.logoutAllSessions = logoutAll ? 1 : 0;
        
        SendPacket(AuthMessageType::LogoutRequest, &payload, sizeof(payload));
    }
    
    // Clear local state
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        sessionToken_.clear();
        username_.clear();
    }
    authenticated_.store(false);
    accountId_.store(0);
    
    ClearStoredToken();
    spdlog::info("Logged out");
}

u64 AuthClient::CreateGuestAccount() {
    // Generate random guest ID
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<u64> dis(1000000, 9999999);
    
    u64 guestId = dis(gen);
    
    accountId_.store(guestId);
    isGuest_.store(true);
    authenticated_.store(true);
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        username_ = "Guest_" + std::to_string(guestId);
        sessionToken_.clear();  // No token for guests
    }
    
    spdlog::info("Guest account created: {}", guestId);
    return guestId;
}

std::string AuthClient::GetSessionToken() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return sessionToken_;
}

std::string AuthClient::GetUsername() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return username_;
}


void AuthClient::SendPacket(AuthMessageType type, const void* payload, u32 payloadSize) {
    std::vector<u8> packet;
    BuildPacket(packet, type, accountId_.load(), NextRequestId(), payload, payloadSize);
    
    SOCKET sock = reinterpret_cast<SOCKET>(socket_);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort_);
    inet_pton(AF_INET, serverIP_.c_str(), &serverAddr.sin_addr);
    
    sendto(sock, (const char*)packet.data(), (int)packet.size(), 0,
           (sockaddr*)&serverAddr, sizeof(serverAddr));
}

void AuthClient::ReceivePackets() {
    SOCKET sock = reinterpret_cast<SOCKET>(socket_);
    u8 buffer[1400];
    
    while (true) {
        sockaddr_in senderAddr;
        int senderSize = sizeof(senderAddr);
        
        int bytesReceived = recvfrom(sock, (char*)buffer, sizeof(buffer), 0,
                                     (sockaddr*)&senderAddr, &senderSize);
        
        if (bytesReceived <= 0) break;
        
        HandlePacket(buffer, bytesReceived);
    }
}

void AuthClient::HandlePacket(const u8* data, size_t size) {
    AuthHeader header;
    const void* payload = nullptr;
    u32 payloadSize = 0;
    
    if (!ParsePacket(data, size, header, payload, payloadSize)) {
        spdlog::warn("Invalid packet received");
        return;
    }
    
    AuthMessageType type = static_cast<AuthMessageType>(header.type);
    
    switch (type) {
        case AuthMessageType::RegisterResponse:
            if (payloadSize >= sizeof(RegisterResponsePayload)) {
                HandleRegisterResponse(*static_cast<const RegisterResponsePayload*>(payload));
            }
            break;
            
        case AuthMessageType::LoginResponse:
            if (payloadSize >= sizeof(LoginResponsePayload)) {
                HandleLoginResponse(*static_cast<const LoginResponsePayload*>(payload));
            }
            break;
            
        case AuthMessageType::ValidateTokenResponse:
            if (payloadSize >= sizeof(ValidateTokenResponsePayload)) {
                HandleValidateTokenResponse(*static_cast<const ValidateTokenResponsePayload*>(payload));
            }
            break;
            
        case AuthMessageType::LogoutResponse:
            if (payloadSize >= sizeof(LogoutResponsePayload)) {
                HandleLogoutResponse(*static_cast<const LogoutResponsePayload*>(payload));
            }
            break;
            
        case AuthMessageType::Error:
            if (payloadSize >= sizeof(ErrorPayload)) {
                HandleError(*static_cast<const ErrorPayload*>(payload));
            }
            break;
            
        default:
            spdlog::warn("Unknown response type: {}", header.type);
            break;
    }
}

void AuthClient::HandleRegisterResponse(const RegisterResponsePayload& payload) {
    AuthResult result = static_cast<AuthResult>(payload.result);
    
    if (result == AuthResult::Success) {
        std::string token(payload.sessionToken, strnlen(payload.sessionToken, kSessionTokenMax));
        
        accountId_.store(payload.accountId);
        authenticated_.store(true);
        isGuest_.store(false);
        
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            sessionToken_ = token;
        }
        
        // Store token for persistence
        std::string username;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            username = username_;
        }
        StoreToken(token, username);
        
        spdlog::info("Registration successful: account {}", payload.accountId);
        
        if (onRegisterSuccess_) {
            onRegisterSuccess_(payload.accountId, token);
        }
    } else {
        std::string error(payload.errorMessage, strnlen(payload.errorMessage, kErrorMessageMax));
        spdlog::warn("Registration failed: {}", error);
        
        if (onRegisterFailed_) {
            onRegisterFailed_(error);
        }
    }
}

void AuthClient::HandleLoginResponse(const LoginResponsePayload& payload) {
    AuthResult result = static_cast<AuthResult>(payload.result);
    
    if (result == AuthResult::Success) {
        std::string token(payload.sessionToken, strnlen(payload.sessionToken, kSessionTokenMax));
        
        accountId_.store(payload.accountId);
        authenticated_.store(true);
        isGuest_.store(false);
        
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            sessionToken_ = token;
        }
        
        // Store token for persistence
        std::string username;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            username = username_;
        }
        StoreToken(token, username);
        
        spdlog::info("Login successful: account {}", payload.accountId);
        
        if (onLoginSuccess_) {
            onLoginSuccess_(payload.accountId, token);
        }
    } else {
        std::string error(payload.errorMessage, strnlen(payload.errorMessage, kErrorMessageMax));
        spdlog::warn("Login failed: {}", error);
        
        if (onLoginFailed_) {
            onLoginFailed_(error);
        }
    }
}

void AuthClient::HandleValidateTokenResponse(const ValidateTokenResponsePayload& payload) {
    AuthResult result = static_cast<AuthResult>(payload.result);
    
    if (result == AuthResult::Success && !payload.isBanned) {
        accountId_.store(payload.accountId);
        authenticated_.store(true);
        isGuest_.store(false);
        
        spdlog::info("Token validated: account {}", payload.accountId);
        
        if (onTokenValid_) {
            onTokenValid_(payload.accountId);
        }
    } else {
        // Clear invalid token
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            sessionToken_.clear();
        }
        ClearStoredToken();
        
        spdlog::info("Token invalid or expired");
        
        if (onTokenInvalid_) {
            onTokenInvalid_();
        }
    }
}

void AuthClient::HandleLogoutResponse(const LogoutResponsePayload& payload) {
    spdlog::info("Logout complete: {} sessions invalidated", payload.sessionsInvalidated);
    
    if (onLogout_) {
        onLogout_(payload.sessionsInvalidated);
    }
}

void AuthClient::HandleError(const ErrorPayload& payload) {
    std::string message(payload.message, strnlen(payload.message, kErrorMessageMax));
    spdlog::error("Auth error: {}", message);
}

void AuthClient::StoreToken(const std::string& token, const std::string& username) {
    std::ofstream file(tokenStoragePath_, std::ios::binary);
    if (!file) {
        spdlog::warn("Failed to store token");
        return;
    }
    
    // Simple format: username\ntoken
    file << username << "\n" << token;
    file.close();
    
    spdlog::debug("Token stored to {}", tokenStoragePath_);
}

bool AuthClient::LoadToken(std::string& outToken, std::string& outUsername) {
    std::ifstream file(tokenStoragePath_);
    if (!file) {
        return false;
    }
    
    std::getline(file, outUsername);
    std::getline(file, outToken);
    file.close();
    
    if (outToken.empty()) {
        return false;
    }
    
    spdlog::debug("Token loaded from {}", tokenStoragePath_);
    return true;
}

void AuthClient::ClearStoredToken() {
    std::remove(tokenStoragePath_.c_str());
    spdlog::debug("Stored token cleared");
}

std::string AuthClient::HashPasswordSHA256(const std::string& password) {
    SecurityManager security;
    return security.SHA256Hash(password);
}

} // namespace auth
