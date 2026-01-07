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
    , inHeroPickPhase_(false)
    , heroPickTimer_(0.0f)
    , heroPickTimerBroadcastInterval_(0.0f)
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
    updateHeroPickPhase(deltaTime);
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
            handleConnectionRequest(sender, payload, payloadSize);
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
        
        case PacketType::HeroPick: {
            ClientId clientId = findClientByAddress(sender);
            if (clientId != INVALID_CLIENT_ID) {
                handleHeroPick(clientId, payload, payloadSize);
            }
            break;
        }
            
        default:
            LOG_WARN("Unknown packet type {} from {}", (int)header.type, sender.toString());
            break;
    }
}

void NetworkServer::handleConnectionRequest(const NetworkAddress& sender, const u8* data, size_t size) {
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
    
    // Parse username and accountId from payload
    std::string username = "Player";
    u64 accountId = 0;
    if (size >= sizeof(ConnectionRequestPayload)) {
        ConnectionRequestPayload reqPayload;
        memcpy(&reqPayload, data, sizeof(ConnectionRequestPayload));
        username = std::string(reqPayload.username, strnlen(reqPayload.username, sizeof(reqPayload.username)));
        accountId = reqPayload.accountId;
        if (username.empty()) {
            username = "Player";
        }
    }
    
    // Accept connection
    ClientId newClientId = allocateClientId();
    
    ConnectedClient& client = clients_[newClientId];
    client.clientId = newClientId;
    client.address = sender;
    client.lastHeartbeat = 0.0f;
    client.username = username;
    client.accountId = accountId;
    
    LOG_INFO("Client {} ({}) connected from {} (ID: {})", username, clients_.size(), sender.toString(), newClientId);
    
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
    
    // Serialize snapshot to buffer
    u8 snapshotBuffer[MAX_PACKET_SIZE - PacketHeader::SIZE];
    size_t snapshotSize = snapshot.serialize(snapshotBuffer, sizeof(snapshotBuffer));
    
    if (snapshotSize == 0) {
        LOG_WARN("Failed to serialize snapshot (too many entities?)");
        return;
    }
    
    PacketHeader header;
    header.type = PacketType::WorldSnapshot;
    header.sequence = nextSequence_++;
    header.payloadSize = static_cast<u16>(snapshotSize);
    
    u8 packet[MAX_PACKET_SIZE];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, snapshotBuffer, snapshotSize);
    
    size_t packetSize = PacketHeader::SIZE + snapshotSize;
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

// ============ Hero Pick Phase ============

void NetworkServer::startHeroPickPhase(f32 pickTime) {
    inHeroPickPhase_ = true;
    heroPickTimer_ = pickTime;
    heroPickTimerBroadcastInterval_ = 0.0f;
    
    // Reset all client pick states and assign teams
    // Radiant: slots 0-4, Dire: slots 5-9
    // Alternate teams for fair distribution
    u8 radiantCount = 0;
    u8 direCount = 0;
    
    for (auto& [clientId, client] : clients_) {
        client.pickedHero.clear();
        client.hasConfirmedPick = false;
        
        // Alternate between teams
        if (radiantCount <= direCount && radiantCount < 5) {
            client.teamSlot = radiantCount;  // Radiant: 0-4
            radiantCount++;
        } else if (direCount < 5) {
            client.teamSlot = 5 + direCount;  // Dire: 5-9
            direCount++;
        } else {
            // Overflow - shouldn't happen in 5v5
            client.teamSlot = radiantCount + direCount;
        }
        
        LOG_INFO("Client {} assigned to team slot {} ({})", 
                 clientId, client.teamSlot, client.teamSlot < 5 ? "Radiant" : "Dire");
        
        // Send team assignment to this client
        sendTeamAssignment(clientId, client.teamSlot);
    }
    
    LOG_INFO("Hero pick phase started with {} seconds. Radiant: {}, Dire: {}", 
             pickTime, radiantCount, direCount);
    
    // Broadcast all player info so everyone knows who's in the game
    broadcastAllPlayerInfo();
    
    // Broadcast initial timer
    broadcastPickTimer(heroPickTimer_, 0);
}

void NetworkServer::sendTeamAssignment(ClientId clientId, u8 teamSlot) {
    auto it = clients_.find(clientId);
    if (it == clients_.end()) return;
    
    TeamAssignmentPayload payload;
    payload.teamSlot = teamSlot;
    payload.teamId = teamSlot < 5 ? 0 : 1;  // 0 = Radiant, 1 = Dire
    memset(payload.username, 0, sizeof(payload.username));
    strncpy(payload.username, it->second.username.c_str(), sizeof(payload.username) - 1);
    
    PacketHeader header;
    header.type = PacketType::TeamAssignment;
    header.sequence = nextSequence_++;
    header.payloadSize = sizeof(TeamAssignmentPayload);
    
    u8 packet[PacketHeader::SIZE + sizeof(TeamAssignmentPayload)];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &payload, sizeof(TeamAssignmentPayload));
    
    socket_.sendTo(packet, sizeof(packet), it->second.address);
    totalPacketsSent_++;
    
    LOG_INFO("Sent team assignment to client {} ({}): slot={}, team={}", 
             clientId, it->second.username, teamSlot, teamSlot < 5 ? "Radiant" : "Dire");
}

void NetworkServer::broadcastPlayerInfo(ClientId playerId) {
    auto it = clients_.find(playerId);
    if (it == clients_.end()) return;
    
    PlayerInfoPayload payload;
    payload.playerId = playerId;
    payload.teamSlot = it->second.teamSlot;
    payload.teamId = it->second.teamSlot < 5 ? 0 : 1;
    memset(payload.username, 0, sizeof(payload.username));
    strncpy(payload.username, it->second.username.c_str(), sizeof(payload.username) - 1);
    
    PacketHeader header;
    header.type = PacketType::PlayerInfo;
    header.sequence = nextSequence_++;
    header.payloadSize = sizeof(PlayerInfoPayload);
    
    u8 packet[PacketHeader::SIZE + sizeof(PlayerInfoPayload)];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &payload, sizeof(PlayerInfoPayload));
    
    for (const auto& [clientId, client] : clients_) {
        socket_.sendTo(packet, sizeof(packet), client.address);
    }
    
    totalPacketsSent_ += clients_.size();
}

void NetworkServer::broadcastAllPlayerInfo() {
    for (const auto& [clientId, client] : clients_) {
        broadcastPlayerInfo(clientId);
    }
}

void NetworkServer::updateHeroPickPhase(f32 deltaTime) {
    if (!inHeroPickPhase_) return;
    
    heroPickTimer_ -= deltaTime;
    heroPickTimerBroadcastInterval_ += deltaTime;
    
    // Broadcast timer every second
    if (heroPickTimerBroadcastInterval_ >= 1.0f) {
        heroPickTimerBroadcastInterval_ = 0.0f;
        broadcastPickTimer(heroPickTimer_, 0);
    }
    
    // Check if all players have picked
    if (allPlayersHavePicked()) {
        LOG_INFO("All players have picked their heroes!");
        inHeroPickPhase_ = false;
        broadcastAllPicked(static_cast<u8>(clients_.size()), 3.0f);
        
        if (onAllPicked_) {
            onAllPicked_();
        }
    }
    // Timer expired - force random picks for those who haven't picked
    else if (heroPickTimer_ <= 0.0f) {
        LOG_INFO("Hero pick timer expired!");
        
        // Auto-pick for players who haven't picked
        const char* defaultHeroes[] = {"Axe", "Juggernaut", "Invoker", "Crystal Maiden", "Pudge"};
        int heroIdx = 0;
        
        for (auto& [clientId, client] : clients_) {
            if (!client.hasConfirmedPick) {
                client.pickedHero = defaultHeroes[heroIdx % 5];
                client.hasConfirmedPick = true;
                heroIdx++;
                
                broadcastHeroPick(clientId, client.pickedHero, client.teamSlot, true);
                
                if (onHeroPick_) {
                    onHeroPick_(clientId, client.pickedHero, client.teamSlot);
                }
            }
        }
        
        inHeroPickPhase_ = false;
        broadcastAllPicked(static_cast<u8>(clients_.size()), 3.0f);
        
        if (onAllPicked_) {
            onAllPicked_();
        }
    }
}

void NetworkServer::handleHeroPick(ClientId clientId, const u8* data, size_t size) {
    if (size < sizeof(HeroPickPayload)) {
        LOG_WARN("Invalid hero pick payload size");
        return;
    }
    
    HeroPickPayload payload;
    memcpy(&payload, data, sizeof(HeroPickPayload));
    
    auto it = clients_.find(clientId);
    if (it == clients_.end()) return;
    
    std::string heroName(payload.heroName, strnlen(payload.heroName, sizeof(payload.heroName)));
    
    // Update client state
    it->second.pickedHero = heroName;
    it->second.hasConfirmedPick = true;
    
    LOG_INFO("Client {} picked hero: {}", clientId, heroName);
    
    // Broadcast to all clients
    broadcastHeroPick(clientId, heroName, it->second.teamSlot, true);
    
    // Notify callback
    if (onHeroPick_) {
        onHeroPick_(clientId, heroName, it->second.teamSlot);
    }
}

void NetworkServer::broadcastHeroPick(ClientId playerId, const std::string& heroName, u8 teamSlot, bool confirmed) {
    HeroPickBroadcastPayload payload;
    payload.playerId = playerId;
    payload.teamSlot = teamSlot;
    payload.isConfirmed = confirmed ? 1 : 0;
    memset(payload.heroName, 0, sizeof(payload.heroName));
    strncpy(payload.heroName, heroName.c_str(), sizeof(payload.heroName) - 1);
    
    PacketHeader header;
    header.type = PacketType::HeroPickBroadcast;
    header.sequence = nextSequence_++;
    header.payloadSize = sizeof(HeroPickBroadcastPayload);
    
    u8 packet[PacketHeader::SIZE + sizeof(HeroPickBroadcastPayload)];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &payload, sizeof(HeroPickBroadcastPayload));
    
    for (const auto& [clientId, client] : clients_) {
        socket_.sendTo(packet, sizeof(packet), client.address);
    }
    
    totalPacketsSent_ += clients_.size();
}

void NetworkServer::broadcastPickTimer(f32 timeRemaining, u8 phase) {
    HeroPickTimerPayload payload;
    payload.timeRemaining = timeRemaining;
    payload.currentPhase = phase;
    
    PacketHeader header;
    header.type = PacketType::HeroPickTimer;
    header.sequence = nextSequence_++;
    header.payloadSize = sizeof(HeroPickTimerPayload);
    
    u8 packet[PacketHeader::SIZE + sizeof(HeroPickTimerPayload)];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &payload, sizeof(HeroPickTimerPayload));
    
    for (const auto& [clientId, client] : clients_) {
        socket_.sendTo(packet, sizeof(packet), client.address);
    }
}

void NetworkServer::broadcastAllPicked(u8 playerCount, f32 startDelay) {
    AllHeroesPickedPayload payload;
    payload.playerCount = playerCount;
    payload.gameStartDelay = startDelay;
    
    PacketHeader header;
    header.type = PacketType::AllHeroesPicked;
    header.sequence = nextSequence_++;
    header.payloadSize = sizeof(AllHeroesPickedPayload);
    
    u8 packet[PacketHeader::SIZE + sizeof(AllHeroesPickedPayload)];
    memcpy(packet, &header, PacketHeader::SIZE);
    memcpy(packet + PacketHeader::SIZE, &payload, sizeof(AllHeroesPickedPayload));
    
    for (const auto& [clientId, client] : clients_) {
        socket_.sendTo(packet, sizeof(packet), client.address);
    }
    
    LOG_INFO("Broadcasted AllHeroesPicked to {} clients", clients_.size());
}

bool NetworkServer::allPlayersHavePicked() const {
    if (clients_.empty()) return false;
    
    for (const auto& [clientId, client] : clients_) {
        if (!client.hasConfirmedPick) {
            return false;
        }
    }
    return true;
}

} // namespace Network
} // namespace WorldEditor
