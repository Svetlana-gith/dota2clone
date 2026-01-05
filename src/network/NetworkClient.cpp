#include "NetworkClient.h"
#include <cstring>

namespace WorldEditor {
namespace Network {

constexpr f32 CONNECTION_TIMEOUT = 5.0f;
constexpr f32 PING_INTERVAL = 1.0f;

NetworkClient::NetworkClient()
    : state_(ConnectionState::Disconnected)
    , clientId_(INVALID_CLIENT_ID)
    , accountId_(0)
    , connectionTimeout_(0.0f)
    , pingTimer_(0.0f)
    , lastPingTime_(0.0f)
    , rtt_(0.0f)
    , hasNewSnapshot_(false)
    , nextInputSequence_(1)
    , packetLoss_(0)
    , totalPacketsSent_(0)
    , totalPacketsReceived_(0) {
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const char* serverIP, u16 serverPort) {
    if (state_ != ConnectionState::Disconnected) {
        LOG_WARN("Already connected or connecting");
        return false;
    }
    
    LOG_INFO("Creating socket...");
    if (!socket_.create()) {
        LOG_ERROR("Failed to create socket");
        return false;
    }
    
    // Bind to any local port
    LOG_INFO("Binding socket to local port...");
    if (!socket_.bind(0)) {
        LOG_ERROR("Failed to bind socket");
        socket_.close();
        return false;
    }
    
    serverAddress_ = NetworkAddress(serverIP, serverPort);
    state_ = ConnectionState::Connecting;
    connectionTimeout_ = CONNECTION_TIMEOUT;
    
    // Send connection request with username and accountId
    ConnectionRequestPayload payload;
    memset(&payload, 0, sizeof(payload));
    strncpy(payload.username, username_.c_str(), sizeof(payload.username) - 1);
    payload.accountId = accountId_;
    
    PacketHeader header;
    header.type = PacketType::ConnectionRequest;
    header.sequence = 0;
    header.payloadSize = sizeof(ConnectionRequestPayload);
    
    u8 packet[PacketHeader::SIZE + sizeof(ConnectionRequestPayload)];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &payload, sizeof(ConnectionRequestPayload));
    
    socket_.sendTo(packet, sizeof(packet), serverAddress_);
    totalPacketsSent_++;
    
    LOG_INFO("Connection request sent to {} (username: {}, accountId: {})", serverAddress_.toString(), username_, accountId_);
    return true;
}

void NetworkClient::disconnect() {
    if (state_ == ConnectionState::Disconnected) return;
    
    if (state_ == ConnectionState::Connected) {
        // Send disconnect packet
        PacketHeader header;
        header.type = PacketType::Disconnect;
        header.sequence = 0;
        header.payloadSize = 0;
        
        socket_.sendTo(&header, PacketHeader::SIZE, serverAddress_);
    }
    
    socket_.close();
    state_ = ConnectionState::Disconnected;
    clientId_ = INVALID_CLIENT_ID;
    
    LOG_INFO("Disconnected from server");
}

void NetworkClient::update(f32 deltaTime) {
    if (state_ == ConnectionState::Disconnected) return;
    
    receivePackets();
    
    // Handle connection timeout
    if (state_ == ConnectionState::Connecting) {
        connectionTimeout_ -= deltaTime;
        if (connectionTimeout_ <= 0.0f) {
            LOG_ERROR("Connection timeout");
            disconnect();
            return;
        }
    }
    
    // Send periodic pings
    if (state_ == ConnectionState::Connected) {
        pingTimer_ += deltaTime;
        if (pingTimer_ >= PING_INTERVAL) {
            sendPing();
            pingTimer_ = 0.0f;
        }
    }
}

void NetworkClient::receivePackets() {
    u8 buffer[MAX_PACKET_SIZE];
    NetworkAddress sender;
    
    while (true) {
        i32 bytesReceived = socket_.receiveFrom(buffer, MAX_PACKET_SIZE, sender);
        
        if (bytesReceived <= 0) {
            break;
        }
        
        // Verify sender is the server
        if (!(sender == serverAddress_)) {
            LOG_WARN("Received packet from unknown sender: {}", sender.toString());
            continue;
        }
        
        totalPacketsReceived_++;
        handlePacket(buffer, bytesReceived);
    }
}

void NetworkClient::handlePacket(const u8* data, size_t size) {
    if (size < PacketHeader::SIZE) {
        LOG_WARN("Received packet too small");
        return;
    }
    
    PacketHeader header;
    memcpy(&header, data, PacketHeader::SIZE);
    
    const u8* payload = data + PacketHeader::SIZE;
    size_t payloadSize = size - PacketHeader::SIZE;
    
    switch (header.type) {
        case PacketType::ConnectionAccepted:
            handleConnectionAccepted(payload, payloadSize);
            break;
            
        case PacketType::ConnectionRejected:
            handleConnectionRejected();
            break;
            
        case PacketType::WorldSnapshot:
            handleWorldSnapshot(payload, payloadSize);
            break;
            
        case PacketType::HeroPickBroadcast:
            handleHeroPickBroadcast(payload, payloadSize);
            break;
            
        case PacketType::AllHeroesPicked:
            handleAllHeroesPicked(payload, payloadSize);
            break;
            
        case PacketType::HeroPickTimer:
            handleHeroPickTimer(payload, payloadSize);
            break;
            
        case PacketType::TeamAssignment:
            handleTeamAssignment(payload, payloadSize);
            break;
            
        case PacketType::PlayerInfo:
            handlePlayerInfo(payload, payloadSize);
            break;
            
        case PacketType::Pong:
            handlePong();
            break;
            
        default:
            LOG_WARN("Unknown packet type: {}", (int)header.type);
            break;
    }
}

void NetworkClient::handleConnectionAccepted(const u8* data, size_t size) {
    if (state_ != ConnectionState::Connecting) return;
    
    struct AcceptPayload {
        ClientId assignedId;
    };
    
    if (size < sizeof(AcceptPayload)) {
        LOG_ERROR("Invalid connection accepted payload");
        disconnect();
        return;
    }
    
    AcceptPayload payload;
    memcpy(&payload, data, sizeof(AcceptPayload));
    
    clientId_ = payload.assignedId;
    state_ = ConnectionState::Connected;
    
    LOG_INFO("Connected to server! Assigned client ID: {}", clientId_);
}

void NetworkClient::handleConnectionRejected() {
    LOG_ERROR("Connection rejected by server");
    disconnect();
}

void NetworkClient::handleWorldSnapshot(const u8* data, size_t size) {
    if (state_ != ConnectionState::Connected) return;
    
    if (size < sizeof(WorldSnapshot)) {
        LOG_WARN("Invalid snapshot size");
        return;
    }
    
    memcpy(&latestSnapshot_, data, sizeof(WorldSnapshot));
    hasNewSnapshot_ = true;
}

void NetworkClient::sendInput(const PlayerInput& input) {
    if (state_ != ConnectionState::Connected) return;
    
    PacketHeader header;
    header.type = PacketType::ClientInput;
    header.sequence = nextInputSequence_++;
    header.payloadSize = sizeof(PlayerInput);
    
    u8 packet[PacketHeader::SIZE + sizeof(PlayerInput)];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &input, sizeof(PlayerInput));
    
    socket_.sendTo(packet, sizeof(packet), serverAddress_);
    totalPacketsSent_++;
}

void NetworkClient::sendPing() {
    PacketHeader header;
    header.type = PacketType::Ping;
    header.sequence = 0;
    header.payloadSize = 0;
    
    socket_.sendTo(&header, PacketHeader::SIZE, serverAddress_);
    lastPingTime_ = 0.0f; // Would use actual timer here
}

void NetworkClient::handlePong() {
    // Calculate RTT
    // rtt_ = currentTime - lastPingTime_;
    // For now, just acknowledge
}

void NetworkClient::sendHeroPick(const std::string& heroName, u8 teamSlot, bool confirmed) {
    if (state_ != ConnectionState::Connected) return;
    
    HeroPickPayload payload;
    payload.playerId = clientId_;
    payload.teamSlot = teamSlot;
    memset(payload.heroName, 0, sizeof(payload.heroName));
    strncpy(payload.heroName, heroName.c_str(), sizeof(payload.heroName) - 1);
    
    PacketHeader header;
    header.type = PacketType::HeroPick;
    header.sequence = nextInputSequence_++;
    header.payloadSize = sizeof(HeroPickPayload);
    
    u8 packet[PacketHeader::SIZE + sizeof(HeroPickPayload)];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &payload, sizeof(HeroPickPayload));
    
    socket_.sendTo(packet, sizeof(packet), serverAddress_);
    totalPacketsSent_++;
    
    LOG_INFO("Sent hero pick: {} (slot {})", heroName, teamSlot);
}

void NetworkClient::handleHeroPickBroadcast(const u8* data, size_t size) {
    if (size < sizeof(HeroPickBroadcastPayload)) {
        LOG_WARN("Invalid hero pick broadcast size");
        return;
    }
    
    HeroPickBroadcastPayload payload;
    memcpy(&payload, data, sizeof(HeroPickBroadcastPayload));
    
    std::string heroName(payload.heroName, strnlen(payload.heroName, sizeof(payload.heroName)));
    bool confirmed = payload.isConfirmed != 0;
    
    LOG_INFO("Hero pick broadcast: player {} picked {} (slot {}, confirmed={})", 
             payload.playerId, heroName, payload.teamSlot, confirmed);
    
    if (onHeroPick_) {
        onHeroPick_(payload.playerId, heroName, payload.teamSlot, confirmed);
    }
}

void NetworkClient::handleAllHeroesPicked(const u8* data, size_t size) {
    if (size < sizeof(AllHeroesPickedPayload)) {
        LOG_WARN("Invalid all heroes picked size");
        return;
    }
    
    AllHeroesPickedPayload payload;
    memcpy(&payload, data, sizeof(AllHeroesPickedPayload));
    
    LOG_INFO("All heroes picked! {} players, starting in {} seconds", 
             payload.playerCount, payload.gameStartDelay);
    
    if (onAllPicked_) {
        onAllPicked_(payload.playerCount, payload.gameStartDelay);
    }
}

void NetworkClient::handleHeroPickTimer(const u8* data, size_t size) {
    if (size < sizeof(HeroPickTimerPayload)) {
        LOG_WARN("Invalid hero pick timer size");
        return;
    }
    
    HeroPickTimerPayload payload;
    memcpy(&payload, data, sizeof(HeroPickTimerPayload));
    
    if (onPickTimer_) {
        onPickTimer_(payload.timeRemaining, payload.currentPhase);
    }
}

void NetworkClient::handleTeamAssignment(const u8* data, size_t size) {
    if (size < sizeof(TeamAssignmentPayload)) {
        LOG_WARN("Invalid team assignment size");
        return;
    }
    
    TeamAssignmentPayload payload;
    memcpy(&payload, data, sizeof(TeamAssignmentPayload));
    
    std::string username(payload.username, strnlen(payload.username, sizeof(payload.username)));
    
    LOG_INFO("Team assignment received: slot={}, team={}, username={}", 
             payload.teamSlot, payload.teamId == 0 ? "Radiant" : "Dire", username);
    
    if (onTeamAssignment_) {
        onTeamAssignment_(payload.teamSlot, payload.teamId, username);
    }
}

void NetworkClient::handlePlayerInfo(const u8* data, size_t size) {
    if (size < sizeof(PlayerInfoPayload)) {
        LOG_WARN("Invalid player info size");
        return;
    }
    
    PlayerInfoPayload payload;
    memcpy(&payload, data, sizeof(PlayerInfoPayload));
    
    std::string username(payload.username, strnlen(payload.username, sizeof(payload.username)));
    
    LOG_INFO("Player info: id={}, slot={}, username={}", 
             payload.playerId, payload.teamSlot, username);
    
    if (onPlayerInfo_) {
        onPlayerInfo_(payload.playerId, payload.teamSlot, username);
    }
}

} // namespace Network
} // namespace WorldEditor
