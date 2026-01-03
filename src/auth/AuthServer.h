#pragma once
/**
 * AuthServer - Standalone Authentication Server
 * 
 * Handles:
 * - User registration
 * - User login
 * - Session token validation
 * - Logout
 * 
 * Uses UDP binary protocol (AuthProtocol.h)
 */

#include "auth/AuthProtocol.h"
#include "auth/DatabaseManager.h"
#include "auth/SecurityManager.h"
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

// Forward declare Windows types to avoid including winsock2.h in header
struct sockaddr_in;

namespace auth {

// Network address wrapper for auth server
struct AuthNetworkAddress {
    uint32_t ip = 0;
    uint16_t port = 0;
    
    bool operator==(const AuthNetworkAddress& other) const {
        return ip == other.ip && port == other.port;
    }
    
    std::string toString() const;
};

/**
 * Authentication Server
 * 
 * Standalone server process that handles all authentication operations.
 * Uses UDP for low-latency communication.
 */
class AuthServer {
public:
    AuthServer();
    ~AuthServer();
    
    // Disable copy
    AuthServer(const AuthServer&) = delete;
    AuthServer& operator=(const AuthServer&) = delete;
    
    /**
     * Initialize the auth server.
     * @param port UDP port to listen on
     * @param dbPath Path to SQLite database
     * @return true if successful
     */
    bool Initialize(u16 port, const std::string& dbPath);
    
    /**
     * Start the server (begins processing requests).
     * Can be called in blocking or non-blocking mode.
     * @param blocking If true, blocks until Shutdown() is called
     */
    void Run(bool blocking = true);
    
    /**
     * Process pending requests (call in game loop if non-blocking).
     * @param maxPackets Maximum packets to process per call
     */
    void Update(int maxPackets = 100);
    
    /**
     * Shutdown the server gracefully.
     */
    void Shutdown();
    
    /**
     * Check if server is running.
     */
    bool IsRunning() const { return running_.load(); }
    
    /**
     * Get server statistics.
     */
    struct Stats {
        u64 totalRequests = 0;
        u64 successfulLogins = 0;
        u64 failedLogins = 0;
        u64 registrations = 0;
        u64 tokenValidations = 0;
    };
    Stats GetStats() const;
    
private:
    // Packet handling
    void ReceivePackets(int maxPackets);
    void HandlePacket(const AuthNetworkAddress& sender, const u8* data, size_t size);
    
    // Request handlers
    void HandleRegisterRequest(const AuthNetworkAddress& sender, 
                               const AuthHeader& header,
                               const RegisterRequestPayload& payload);
    
    void HandleLoginRequest(const AuthNetworkAddress& sender,
                            const AuthHeader& header,
                            const LoginRequestPayload& payload);
    
    void HandleValidateTokenRequest(const AuthNetworkAddress& sender,
                                    const AuthHeader& header,
                                    const ValidateTokenRequestPayload& payload);
    
    void HandleLogoutRequest(const AuthNetworkAddress& sender,
                             const AuthHeader& header,
                             const LogoutRequestPayload& payload);
    
    void HandleChangePasswordRequest(const AuthNetworkAddress& sender,
                                     const AuthHeader& header,
                                     const ChangePasswordRequestPayload& payload);
    
    // Response sending
    void SendResponse(const AuthNetworkAddress& dest,
                      AuthMessageType type,
                      u64 accountId,
                      u32 requestId,
                      const void* payload,
                      u32 payloadSize);
    
    void SendError(const AuthNetworkAddress& dest,
                   u32 requestId,
                   AuthResult errorCode,
                   const std::string& message);
    
    // Helpers
    std::string GetClientIP(const AuthNetworkAddress& addr) const;
    
    // Components
    DatabaseManager db_;
    SecurityManager security_;
    
    // Network (using raw socket to avoid header dependencies)
    void* socket_ = nullptr;  // SOCKET handle
    u16 port_ = 0;
    
    // State
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // Statistics
    mutable std::mutex statsMutex_;
    Stats stats_;
};

} // namespace auth
