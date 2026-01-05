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
    
    // Player info
    std::string username;
    u64 accountId;  // Auth account ID for reconnect support
    
    // Hero pick state
    std::string pickedHero;
    u8 teamSlot;
    bool hasConfirmedPick;
    
    ConnectedClient() 
        : clientId(INVALID_CLIENT_ID)
        , lastHeartbeat(0.0f)
        , lastReceivedInput(0)
        , lastSentSnapshot(0)
        , accountId(0)
        , teamSlot(0)
        , hasConfirmedPick(false) {}
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
    std::string getClientUsername(ClientId clientId) const {
        auto it = clients_.find(clientId);
        return (it != clients_.end()) ? it->second.username : "";
    }
    u64 getClientAccountId(ClientId clientId) const {
        auto it = clients_.find(clientId);
        return (it != clients_.end()) ? it->second.accountId : 0;
    }
    
    // Packet sending
    void sendSnapshotToClient(ClientId clientId, const WorldSnapshot& snapshot);
    void sendSnapshotToAll(const WorldSnapshot& snapshot);
    void broadcastGameEvent(const void* eventData, size_t size);
    
    // Hero pick phase
    void startHeroPickPhase(f32 pickTime = 30.0f);
    void sendTeamAssignment(ClientId clientId, u8 teamSlot);
    void broadcastPlayerInfo(ClientId playerId);
    void broadcastAllPlayerInfo();
    void broadcastHeroPick(ClientId playerId, const std::string& heroName, u8 teamSlot, bool confirmed);
    void broadcastPickTimer(f32 timeRemaining, u8 phase);
    void broadcastAllPicked(u8 playerCount, f32 startDelay);
    bool allPlayersHavePicked() const;
    
    // Callbacks
    using OnClientConnectedCallback = std::function<void(ClientId)>;
    using OnClientDisconnectedCallback = std::function<void(ClientId)>;
    using OnClientInputCallback = std::function<void(ClientId, const PlayerInput&)>;
    using OnHeroPickCallback = std::function<void(ClientId, const std::string& heroName, u8 teamSlot)>;
    using OnAllPickedCallback = std::function<void()>;
    
    void setOnClientConnected(OnClientConnectedCallback callback) { 
        onClientConnected_ = callback; 
    }
    void setOnClientDisconnected(OnClientDisconnectedCallback callback) { 
        onClientDisconnected_ = callback; 
    }
    void setOnClientInput(OnClientInputCallback callback) { 
        onClientInput_ = callback; 
    }
    void setOnHeroPick(OnHeroPickCallback callback) {
        onHeroPick_ = callback;
    }
    void setOnAllPicked(OnAllPickedCallback callback) {
        onAllPicked_ = callback;
    }
    
    bool isRunning() const { return running_; }
    bool isInHeroPickPhase() const { return inHeroPickPhase_; }
    
private:
    void receivePackets();
    void handlePacket(const NetworkAddress& sender, const u8* data, size_t size);
    void handleConnectionRequest(const NetworkAddress& sender, const u8* data, size_t size);
    void handleClientInput(ClientId clientId, const u8* data, size_t size);
    void handleHeroPick(ClientId clientId, const u8* data, size_t size);
    void handleDisconnect(ClientId clientId);
    void checkClientTimeouts(f32 deltaTime);
    void updateHeroPickPhase(f32 deltaTime);
    
    ClientId findClientByAddress(const NetworkAddress& addr) const;
    ClientId allocateClientId();
    
    UDPSocket socket_;
    bool running_;
    u16 port_;
    
    std::unordered_map<ClientId, ConnectedClient> clients_;
    ClientId nextClientId_;
    
    SequenceNumber nextSequence_;
    
    // Hero pick phase
    bool inHeroPickPhase_;
    f32 heroPickTimer_;
    f32 heroPickTimerBroadcastInterval_;
    
    // Callbacks
    OnClientConnectedCallback onClientConnected_;
    OnClientDisconnectedCallback onClientDisconnected_;
    OnClientInputCallback onClientInput_;
    OnHeroPickCallback onHeroPick_;
    OnAllPickedCallback onAllPicked_;
    
    // Stats
    u64 totalPacketsSent_;
    u64 totalPacketsReceived_;
    u64 totalBytesSent_;
    u64 totalBytesReceived_;
};

} // namespace Network
} // namespace WorldEditor
