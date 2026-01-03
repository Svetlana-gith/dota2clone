#pragma once

#define NOMINMAX  // Prevent Windows min/max macros
#include "core/Types.h"
#include "common/NetworkTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace WorldEditor {
namespace Network {

// ============ Network Configuration ============

constexpr u16 DEFAULT_SERVER_PORT = 27015;
constexpr u32 MAX_PACKET_SIZE = 1400;  // Safe UDP packet size
constexpr u32 MAX_CLIENTS = 10;
constexpr f32 CLIENT_TIMEOUT = 10.0f;  // Seconds before client is considered disconnected

// ============ Packet Types ============

enum class PacketType : u8 {
    // Connection
    ConnectionRequest = 0,
    ConnectionAccepted = 1,
    ConnectionRejected = 2,
    Disconnect = 3,
    
    // Gameplay
    ClientInput = 10,
    WorldSnapshot = 11,
    
    // Reliability
    Ping = 20,
    Pong = 21,
    
    // Game events
    GameEvent = 30
};

// ============ Packet Header ============

struct PacketHeader {
    PacketType type;
    SequenceNumber sequence;
    u16 payloadSize;
    
    static constexpr size_t SIZE = sizeof(PacketType) + sizeof(SequenceNumber) + sizeof(u16);
};

// ============ Network Address ============

struct NetworkAddress {
    sockaddr_in addr;
    
    NetworkAddress() {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
    }
    
    NetworkAddress(const char* ip, u16 port) : NetworkAddress() {
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);
    }
    
    bool operator==(const NetworkAddress& other) const {
        return addr.sin_addr.s_addr == other.addr.sin_addr.s_addr &&
               addr.sin_port == other.addr.sin_port;
    }
    
    String toString() const {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
        return String(ip) + ":" + std::to_string(ntohs(addr.sin_port));
    }
};

// ============ Network Initialization ============

class NetworkSystem {
public:
    static bool Initialize() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            LOG_ERROR("WSAStartup failed: {}", result);
            return false;
        }
        LOG_INFO("Network system initialized");
        return true;
    }
    
    static void Shutdown() {
        WSACleanup();
        LOG_INFO("Network system shutdown");
    }
};

// ============ UDP Socket Wrapper ============

class UDPSocket {
public:
    UDPSocket() : socket_(INVALID_SOCKET) {}
    
    ~UDPSocket() {
        close();
    }
    
    bool create() {
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            int error = WSAGetLastError();
            LOG_ERROR("Failed to create socket: {}", error);
            
            // If error is WSANOTINITIALISED (10093), try to initialize Winsock
            if (error == WSANOTINITIALISED) {
                LOG_WARN("Winsock not initialized, attempting to initialize...");
                if (NetworkSystem::Initialize()) {
                    LOG_INFO("Winsock initialized successfully, retrying socket creation...");
                    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    if (socket_ == INVALID_SOCKET) {
                        LOG_ERROR("Failed to create socket after Winsock init: {}", WSAGetLastError());
                        return false;
                    }
                } else {
                    LOG_ERROR("Failed to initialize Winsock");
                    return false;
                }
            } else {
                return false;
            }
        }
        
        // Set non-blocking mode
        u_long mode = 1;
        ioctlsocket(socket_, FIONBIO, &mode);
        
        return true;
    }
    
    bool bind(u16 port) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (::bind(socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            LOG_ERROR("Failed to bind socket to port {}: {}", port, WSAGetLastError());
            return false;
        }
        
        LOG_INFO("Socket bound to port {}", port);
        return true;
    }
    
    i32 sendTo(const void* data, size_t size, const NetworkAddress& dest) {
        return sendto(socket_, (const char*)data, (int)size, 0, 
                     (const sockaddr*)&dest.addr, sizeof(dest.addr));
    }
    
    i32 receiveFrom(void* buffer, size_t bufferSize, NetworkAddress& sender) {
        int senderSize = sizeof(sender.addr);
        return recvfrom(socket_, (char*)buffer, (int)bufferSize, 0,
                       (sockaddr*)&sender.addr, &senderSize);
    }
    
    void close() {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }
    }
    
    bool isValid() const { return socket_ != INVALID_SOCKET; }
    
private:
    SOCKET socket_;
};

} // namespace Network
} // namespace WorldEditor
