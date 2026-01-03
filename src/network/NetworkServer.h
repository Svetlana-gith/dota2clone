#pragma once

#include "NetworkCommon.h"
#include "common/GameInput.h"
#include "common/GameSnapshot.h"
#include <unordered_map>

namespace WorldEditor {

// Forward declaration
class ServerWorld;

namespace Network {

// ============ Connected Client Info ============

struct ConnectedClient {
    ClientId clientId;
    NetworkAddress address;
    f32 lastHeartbeat;
    SequenceNumber lastReceivedInput;
    SequenceNumber lastSentSnapshot;
    
    ConnectedClient() 
        : clientId(INVALID_CLIENT_ID)
        , lastHeartbeat(0.0f)
        , lastReceivedInput(0)
        , lastSentSnapshot(0) {}
};

// ============ Network Server ============

class NetworkServer {
public:
    NetworkServer();
    ~NetworkServer();
    
    // Lifecycle
    bool start(u16 port = DEFAULT_SERVER_PORT);
    void stop();
    void update(f32 deltaTime);
    
    // Client management
    bool isClientConnected(ClientId clientId) const;
    size_t getClientCount() const { return clients_.size(); }
    
    // Packet sending
    void sendSnapshotToClient(ClientId clientId, const WorldSnapshot& snapshot);
    void sendSnapshotToAll(const WorldSnapshot& snapshot);
    void broadcastGameEvent(const void* eventData, size_t size);
    
    // Callbacks
    using OnClientConnectedCallback = std::function<void(ClientId)>;
    using OnClientDisconnectedCallback = std::function<void(ClientId)>;
    using OnClientInputCallback = std::function<void(ClientId, const PlayerInput&)>;
    
    void setOnClientConnected(OnClientConnectedCallback callback) { 
        onClientConnected_ = callback; 
    }
    void setOnClientDisconnected(OnClientDisconnectedCallback callback) { 
        onClientDisconnected_ = callback; 
    }
    void setOnClientInput(OnClientInputCallback callback) { 
        onClientInput_ = callback; 
    }
    
    bool isRunning() const { return running_; }
    
private:
    void receivePackets();
    void handlePacket(const NetworkAddress& sender, const u8* data, size_t size);
    void handleConnectionRequest(const NetworkAddress& sender);
    void handleClientInput(ClientId clientId, const u8* data, size_t size);
    void handleDisconnect(ClientId clientId);
    void checkClientTimeouts(f32 deltaTime);
    
    ClientId findClientByAddress(const NetworkAddress& addr) const;
    ClientId allocateClientId();
    
    UDPSocket socket_;
    bool running_;
    u16 port_;
    
    std::unordered_map<ClientId, ConnectedClient> clients_;
    ClientId nextClientId_;
    
    SequenceNumber nextSequence_;
    
    // Callbacks
    OnClientConnectedCallback onClientConnected_;
    OnClientDisconnectedCallback onClientDisconnected_;
    OnClientInputCallback onClientInput_;
    
    // Stats
    u64 totalPacketsSent_;
    u64 totalPacketsReceived_;
    u64 totalBytesSent_;
    u64 totalBytesReceived_;
};

} // namespace Network
} // namespace WorldEditor
