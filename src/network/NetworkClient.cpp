#include "NetworkClient.h"
#include <cstring>

namespace WorldEditor {
namespace Network {

constexpr f32 CONNECTION_TIMEOUT = 5.0f;
constexpr f32 PING_INTERVAL = 1.0f;

NetworkClient::NetworkClient()
    : state_(ConnectionState::Disconnected)
    , clientId_(INVALID_CLIENT_ID)
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
    
    // Send connection request
    PacketHeader header;
    header.type = PacketType::ConnectionRequest;
    header.sequence = 0;
    header.payloadSize = 0;
    
    socket_.sendTo(&header, PacketHeader::SIZE, serverAddress_);
    totalPacketsSent_++;
    
    LOG_INFO("Connection request sent to {}", serverAddress_.toString());
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

} // namespace Network
} // namespace WorldEditor
