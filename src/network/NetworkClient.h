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
    
    // Input sending
    void sendInput(const PlayerInput& input);
    
    // Snapshot receiving
    bool hasNewSnapshot() const { return hasNewSnapshot_; }
    const WorldSnapshot& getLatestSnapshot() const { return latestSnapshot_; }
    void clearNewSnapshotFlag() { hasNewSnapshot_ = false; }
    
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
    void sendPing();
    void handlePong();
    
    UDPSocket socket_;
    ConnectionState state_;
    NetworkAddress serverAddress_;
    ClientId clientId_;
    
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
};

} // namespace Network
} // namespace WorldEditor
