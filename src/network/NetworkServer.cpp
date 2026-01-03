#include "NetworkServer.h"
#include "server/ServerWorld.h"
#include <cstring>

namespace WorldEditor {
namespace Network {

NetworkServer::NetworkServer()
    : running_(false)
    , port_(0)
    , nextClientId_(1)
    , nextSequence_(1)
    , totalPacketsSent_(0)
    , totalPacketsReceived_(0)
    , totalBytesSent_(0)
    , totalBytesReceived_(0) {
}

NetworkServer::~NetworkServer() {
    stop();
}

bool NetworkServer::start(u16 port) {
    if (running_) {
        LOG_WARN("Server already running");
        return false;
    }
    
    if (!socket_.create()) {
        return false;
    }
    
    if (!socket_.bind(port)) {
        socket_.close();
        return false;
    }
    
    port_ = port;
    running_ = true;
    
    LOG_INFO("Network server started on port {}", port);
    return true;
}

void NetworkServer::stop() {
    if (!running_) return;
    
    // Disconnect all clients
    for (auto& [clientId, client] : clients_) {
        if (onClientDisconnected_) {
            onClientDisconnected_(clientId);
        }
    }
    clients_.clear();
    
    socket_.close();
    running_ = false;
    
    LOG_INFO("Network server stopped. Stats: Sent={} packets ({} bytes), Received={} packets ({} bytes)",
             totalPacketsSent_, totalBytesSent_, totalPacketsReceived_, totalBytesReceived_);
}

void NetworkServer::update(f32 deltaTime) {
    if (!running_) return;
    
    receivePackets();
    checkClientTimeouts(deltaTime);
}

void NetworkServer::receivePackets() {
    u8 buffer[MAX_PACKET_SIZE];
    NetworkAddress sender;
    
    // Process all pending packets
    while (true) {
        i32 bytesReceived = socket_.receiveFrom(buffer, MAX_PACKET_SIZE, sender);
        
        if (bytesReceived <= 0) {
            break; // No more packets
        }
        
        totalPacketsReceived_++;
        totalBytesReceived_ += bytesReceived;
        
        handlePacket(sender, buffer, bytesReceived);
    }
}

void NetworkServer::handlePacket(const NetworkAddress& sender, const u8* data, size_t size) {
    if (size < PacketHeader::SIZE) {
        LOG_WARN("Received packet too small from {}", sender.toString());
        return;
    }
    
    PacketHeader header;
    memcpy(&header, data, PacketHeader::SIZE);
    
    const u8* payload = data + PacketHeader::SIZE;
    size_t payloadSize = size - PacketHeader::SIZE;
    
    switch (header.type) {
        case PacketType::ConnectionRequest:
            handleConnectionRequest(sender);
            break;
            
        case PacketType::ClientInput: {
            ClientId clientId = findClientByAddress(sender);
            if (clientId != INVALID_CLIENT_ID) {
                handleClientInput(clientId, payload, payloadSize);
            }
            break;
        }
            
        case PacketType::Disconnect: {
            ClientId clientId = findClientByAddress(sender);
            if (clientId != INVALID_CLIENT_ID) {
                handleDisconnect(clientId);
            }
            break;
        }
            
        case PacketType::Ping: {
            // Respond with pong
            ClientId clientId = findClientByAddress(sender);
            if (clientId != INVALID_CLIENT_ID) {
                clients_[clientId].lastHeartbeat = 0.0f; // Reset timeout
                
                PacketHeader pongHeader;
                pongHeader.type = PacketType::Pong;
                pongHeader.sequence = nextSequence_++;
                pongHeader.payloadSize = 0;
                
                socket_.sendTo(&pongHeader, PacketHeader::SIZE, sender);
            }
            break;
        }
            
        default:
            LOG_WARN("Unknown packet type {} from {}", (int)header.type, sender.toString());
            break;
    }
}

void NetworkServer::handleConnectionRequest(const NetworkAddress& sender) {
    // Check if client already connected
    ClientId existingId = findClientByAddress(sender);
    if (existingId != INVALID_CLIENT_ID) {
        LOG_WARN("Client {} already connected", sender.toString());
        return;
    }
    
    // Check max clients
    if (clients_.size() >= MAX_CLIENTS) {
        LOG_WARN("Server full, rejecting connection from {}", sender.toString());
        
        PacketHeader rejectHeader;
        rejectHeader.type = PacketType::ConnectionRejected;
        rejectHeader.sequence = nextSequence_++;
        rejectHeader.payloadSize = 0;
        
        socket_.sendTo(&rejectHeader, PacketHeader::SIZE, sender);
        return;
    }
    
    // Accept connection
    ClientId newClientId = allocateClientId();
    
    ConnectedClient& client = clients_[newClientId];
    client.clientId = newClientId;
    client.address = sender;
    client.lastHeartbeat = 0.0f;
    
    LOG_INFO("Client {} connected from {} (ID: {})", clients_.size(), sender.toString(), newClientId);
    
    // Send acceptance
    struct AcceptPayload {
        ClientId assignedId;
    };
    
    AcceptPayload payload;
    payload.assignedId = newClientId;
    
    PacketHeader acceptHeader;
    acceptHeader.type = PacketType::ConnectionAccepted;
    acceptHeader.sequence = nextSequence_++;
    acceptHeader.payloadSize = sizeof(AcceptPayload);
    
    u8 packet[PacketHeader::SIZE + sizeof(AcceptPayload)];
    memcpy(packet, &acceptHeader, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &payload, sizeof(AcceptPayload));
    
    socket_.sendTo(packet, sizeof(packet), sender);
    totalPacketsSent_++;
    totalBytesSent_ += sizeof(packet);
    
    // Notify callback
    if (onClientConnected_) {
        onClientConnected_(newClientId);
    }
}

void NetworkServer::handleClientInput(ClientId clientId, const u8* data, size_t size) {
    if (size < sizeof(PlayerInput)) {
        LOG_WARN("Invalid input packet size from client {}", clientId);
        return;
    }
    
    PlayerInput input;
    memcpy(&input, data, sizeof(PlayerInput));
    
    // Update client state
    auto& client = clients_[clientId];
    client.lastHeartbeat = 0.0f;
    client.lastReceivedInput = input.sequenceNumber;
    
    // Forward to game logic
    if (onClientInput_) {
        onClientInput_(clientId, input);
    }
}

void NetworkServer::handleDisconnect(ClientId clientId) {
    auto it = clients_.find(clientId);
    if (it == clients_.end()) return;
    
    LOG_INFO("Client {} disconnected", clientId);
    
    if (onClientDisconnected_) {
        onClientDisconnected_(clientId);
    }
    
    clients_.erase(it);
}

void NetworkServer::checkClientTimeouts(f32 deltaTime) {
    Vector<ClientId> timedOutClients;
    
    for (auto& [clientId, client] : clients_) {
        client.lastHeartbeat += deltaTime;
        
        if (client.lastHeartbeat > CLIENT_TIMEOUT) {
            timedOutClients.push_back(clientId);
        }
    }
    
    for (ClientId clientId : timedOutClients) {
        LOG_WARN("Client {} timed out", clientId);
        handleDisconnect(clientId);
    }
}

ClientId NetworkServer::findClientByAddress(const NetworkAddress& addr) const {
    for (const auto& [clientId, client] : clients_) {
        if (client.address == addr) {
            return clientId;
        }
    }
    return INVALID_CLIENT_ID;
}

ClientId NetworkServer::allocateClientId() {
    return nextClientId_++;
}

bool NetworkServer::isClientConnected(ClientId clientId) const {
    return clients_.find(clientId) != clients_.end();
}

void NetworkServer::sendSnapshotToClient(ClientId clientId, const WorldSnapshot& snapshot) {
    auto it = clients_.find(clientId);
    if (it == clients_.end()) return;
    
    PacketHeader header;
    header.type = PacketType::WorldSnapshot;
    header.sequence = nextSequence_++;
    header.payloadSize = sizeof(WorldSnapshot);
    
    u8 packet[MAX_PACKET_SIZE];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &snapshot, sizeof(WorldSnapshot));
    
    size_t packetSize = PacketHeader::SIZE + sizeof(WorldSnapshot);
    socket_.sendTo(packet, packetSize, it->second.address);
    
    totalPacketsSent_++;
    totalBytesSent_ += packetSize;
}

void NetworkServer::sendSnapshotToAll(const WorldSnapshot& snapshot) {
    for (const auto& [clientId, client] : clients_) {
        sendSnapshotToClient(clientId, snapshot);
    }
}

void NetworkServer::broadcastGameEvent(const void* eventData, size_t size) {
    if (size > MAX_PACKET_SIZE - PacketHeader::SIZE) {
        LOG_ERROR("Game event too large: {} bytes", size);
        return;
    }
    
    PacketHeader header;
    header.type = PacketType::GameEvent;
    header.sequence = nextSequence_++;
    header.payloadSize = (u16)size;
    
    u8 packet[MAX_PACKET_SIZE];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, eventData, size);
    
    size_t packetSize = PacketHeader::SIZE + size;
    
    for (const auto& [clientId, client] : clients_) {
        socket_.sendTo(packet, packetSize, client.address);
    }
    
    totalPacketsSent_ += clients_.size();
    totalBytesSent_ += packetSize * clients_.size();
}

} // namespace Network
} // namespace WorldEditor
