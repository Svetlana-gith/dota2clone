#pragma once

#include "NetworkCommon.h"
#include "common/GameInput.h"
#include "common/GameSnapshot.h"

namespace WorldEditor {
namespace Network {

// ============ Connection State ============

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting
};

// ============ Network Client ============

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();
    
    // Connection
    bool connect(const char* serverIP, u16 serverPort = DEFAULT_SERVER_PORT);
    void disconnect();
    void update(f32 deltaTime);
    
    // Set username before connecting
    void setUsername(const std::string& username) { username_ = username; }
    const std::string& getUsername() const { return username_; }
    
    // Set account ID before connecting (for reconnect support)
    void setAccountId(u64 accountId) { accountId_ = accountId; }
    u64 getAccountId() const { return accountId_; }
    
    // Input sending
    void sendInput(const PlayerInput& input);
    
    // Hero Pick
    void sendHeroPick(const std::string& heroName, u8 teamSlot, bool confirmed);
    
    // Callbacks for hero pick
    using OnHeroPickCallback = std::function<void(u64 playerId, const std::string& heroName, u8 teamSlot, bool confirmed)>;
    using OnAllPickedCallback = std::function<void(u8 playerCount, f32 startDelay)>;
    using OnPickTimerCallback = std::function<void(f32 timeRemaining, u8 phase)>;
    using OnTeamAssignmentCallback = std::function<void(u8 teamSlot, u8 teamId, const std::string& username)>;
    using OnPlayerInfoCallback = std::function<void(u64 playerId, u8 teamSlot, const std::string& username)>;
    
    void setOnHeroPick(OnHeroPickCallback cb) { onHeroPick_ = cb; }
    void setOnAllPicked(OnAllPickedCallback cb) { onAllPicked_ = cb; }
    void setOnPickTimer(OnPickTimerCallback cb) { onPickTimer_ = cb; }
    void setOnTeamAssignment(OnTeamAssignmentCallback cb) { onTeamAssignment_ = cb; }
    void setOnPlayerInfo(OnPlayerInfoCallback cb) { onPlayerInfo_ = cb; }
    
    // Snapshot receiving
    bool hasNewSnapshot() const { return hasNewSnapshot_; }
    const WorldSnapshot& getLatestSnapshot() const { return latestSnapshot_; }
    void clearNewSnapshotFlag() { hasNewSnapshot_ = false; }
    
    // Game time from server (from latest snapshot)
    f32 getServerGameTime() const { return latestSnapshot_.gameTime; }
    
    // State
    ConnectionState getState() const { return state_; }
    bool isConnected() const { return state_ == ConnectionState::Connected; }
    ClientId getClientId() const { return clientId_; }
    
    // Stats
    f32 getRoundTripTime() const { return rtt_; }
    u32 getPacketLoss() const { return packetLoss_; }
    
private:
    void receivePackets();
    void handlePacket(const u8* data, size_t size);
    void handleConnectionAccepted(const u8* data, size_t size);
    void handleConnectionRejected();
    void handleWorldSnapshot(const u8* data, size_t size);
    void handleHeroPickBroadcast(const u8* data, size_t size);
    void handleAllHeroesPicked(const u8* data, size_t size);
    void handleHeroPickTimer(const u8* data, size_t size);
    void handleTeamAssignment(const u8* data, size_t size);
    void handlePlayerInfo(const u8* data, size_t size);
    void sendPing();
    void handlePong();
    
    UDPSocket socket_;
    ConnectionState state_;
    NetworkAddress serverAddress_;
    ClientId clientId_;
    std::string username_;  // Player's username
    u64 accountId_;         // Auth account ID for reconnect support
    
    // Timing
    f32 connectionTimeout_;
    f32 pingTimer_;
    f32 lastPingTime_;
    f32 rtt_;  // Round-trip time
    
    // Snapshots
    WorldSnapshot latestSnapshot_;
    bool hasNewSnapshot_;
    
    // Sequence numbers
    SequenceNumber nextInputSequence_;
    
    // Stats
    u32 packetLoss_;
    u64 totalPacketsSent_;
    u64 totalPacketsReceived_;
    
    // Hero pick callbacks
    OnHeroPickCallback onHeroPick_;
    OnAllPickedCallback onAllPicked_;
    OnPickTimerCallback onPickTimer_;
    OnTeamAssignmentCallback onTeamAssignment_;
    OnPlayerInfoCallback onPlayerInfo_;
};

} // namespace Network
} // namespace WorldEditor
