#include "MatchmakingClient.h"
#include "MatchmakingProtocol.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <vector>
#include <random>
#include <chrono>

namespace WorldEditor {
namespace Matchmaking {

MatchmakingClient::MatchmakingClient() {
    // Generate pseudo-SteamID for local dev.
    // IMPORTANT: don't use C rand() without seeding, otherwise multiple processes
    // launched at the same time get the same sequence and thus the same playerId.
    static thread_local std::mt19937_64 rng{
        [] {
            const auto t = (u64)std::chrono::high_resolution_clock::now().time_since_epoch().count();
            std::seed_seq seq{
                (u32)(t & 0xFFFFFFFFu),
                (u32)(t >> 32),
                (u32)std::random_device{}(),
                (u32)std::random_device{}()
            };
            return std::mt19937_64(seq);
        }()
    };

    do {
        playerInfo_.steamId = (u64)rng();
    } while (playerInfo_.steamId == 0);
    playerInfo_.playerName = "Player_" + std::to_string(playerInfo_.steamId % 10000);
    playerInfo_.mmr = 1000 + (u32)(rng() % 2000);
}

MatchmakingClient::~MatchmakingClient() {
    disconnect();
}

bool MatchmakingClient::connect(const char* coordinatorIP, u16 port) {
    LOG_INFO("Connecting to matchmaking coordinator {}:{}...", coordinatorIP, port);
    
    coordinatorIP_ = coordinatorIP;
    coordinatorPort_ = port;
    
    if (!socket_.create()) {
        LOG_ERROR("Failed to create socket");
        return false;
    }
    
    // For now, just mark as connected (UDP doesn't have connection state)
    // In real implementation, send handshake and wait for response
    connected_ = true;
    
    LOG_INFO("Connected to matchmaking coordinator");
    return true;
}

void MatchmakingClient::disconnect() {
    if (!connected_) return;
    
    if (inQueue_) {
        cancelQueue();
    }
    
    socket_.close();
    connected_ = false;
    
    LOG_INFO("Disconnected from matchmaking coordinator");
}

bool MatchmakingClient::queueForMatch(const MatchPreferences& prefs) {
    if (!connected_) {
        LOG_ERROR("Not connected to matchmaking coordinator");
        return false;
    }
    
    if (inQueue_) {
        LOG_WARN("Already in queue");
        return false;
    }
    
    if (sessionToken_.empty()) {
        LOG_ERROR("No session token set - authentication required before queueing");
        if (onQueueRejected_) {
            onQueueRejected_("Authentication required", true, false);
        }
        return false;
    }
    
    LOG_INFO("Queueing for match: mode={}, region={}", 
             static_cast<int>(prefs.mode), prefs.region);
    
    currentPreferences_ = prefs;
    
    // Send queue request with session token
    Wire::QueueRequestPayload p{};
    p.mode = static_cast<u8>(prefs.mode);
    Wire::CopyCString(p.region, sizeof(p.region), prefs.region);
    Wire::CopyCString(p.sessionToken, sizeof(p.sessionToken), sessionToken_);
    sendPacket(MatchmakingMessageType::QueueRequest, 0, &p, sizeof(p));
    
    inQueue_ = true;
    queueStatus_.inQueue = true;
    queueStatus_.searchTime = 0;
    queueStatus_.region = prefs.region;
    
    return true;
}

void MatchmakingClient::cancelQueue() {
    if (!inQueue_) return;
    
    LOG_INFO("Cancelling queue");
    
    sendPacket(MatchmakingMessageType::QueueCancel, 0, nullptr, 0);
    
    inQueue_ = false;
    queueStatus_.inQueue = false;
    queueStatus_.searchTime = 0;
}

void MatchmakingClient::acceptMatch() {
    LOG_INFO("Accepting match");
    sendPacket(MatchmakingMessageType::MatchAccept, currentLobby_.lobbyId, nullptr, 0);
    
    playerInfo_.isReady = true;
}

void MatchmakingClient::declineMatch() {
    LOG_INFO("Declining match");
    sendPacket(MatchmakingMessageType::MatchDecline, currentLobby_.lobbyId, nullptr, 0);
    
    inQueue_ = false;
    queueStatus_.inQueue = false;
    acceptTimeoutSeconds_ = 0;
    acceptElapsedSeconds_ = 0.0f;
}

void MatchmakingClient::update(f32 deltaTime) {
    if (!connected_) return;
    
    // Update queue time
    if (inQueue_) {
        queueStatus_.searchTime += deltaTime;
    }

    // Update accept countdown (MatchFound phase)
    if (currentLobby_.state == LobbyState::Found && acceptTimeoutSeconds_ > 0) {
        acceptElapsedSeconds_ += deltaTime;
        if (acceptElapsedSeconds_ > (f32)acceptTimeoutSeconds_) {
            acceptElapsedSeconds_ = (f32)acceptTimeoutSeconds_;
        }
    }
    
    // Send heartbeat
    heartbeatTimer_ += deltaTime;
    if (heartbeatTimer_ >= heartbeatInterval_) {
        sendHeartbeat();
        heartbeatTimer_ = 0;
    }
    
    // Receive messages
    u8 buffer[2048];
    Network::NetworkAddress from;

    while (true) {
        i32 bytesReceived = socket_.receiveFrom(buffer, sizeof(buffer), from);
        if (bytesReceived <= 0) break;

        Wire::MMHeader h{};
        const void* payload = nullptr;
        u32 payloadSize = 0;
        if (!Wire::ParsePacket(buffer, static_cast<size_t>(bytesReceived), h, payload, payloadSize)) {
            continue;
        }

        handlePacket(static_cast<MatchmakingMessageType>(h.type), h.lobbyId, payload, payloadSize);
    }
}

void MatchmakingClient::sendPacket(MatchmakingMessageType type, u64 lobbyId, const void* payload, u32 payloadSize) {
    if (!connected_) return;

    std::vector<u8> pkt;
    if (!Wire::BuildPacket(pkt, type, playerInfo_.steamId, lobbyId, payload, payloadSize)) {
        return;
    }

    Network::NetworkAddress addr(coordinatorIP_.c_str(), coordinatorPort_);
    socket_.sendTo(pkt.data(), pkt.size(), addr);
}

void MatchmakingClient::handlePacket(MatchmakingMessageType type, u64 lobbyId, const void* payload, u32 payloadSize) {
    switch (type) {
        case MatchmakingMessageType::QueueConfirm:
            LOG_INFO("Queue confirmed");
            if (onQueueConfirmed_) onQueueConfirmed_();
            break;
            
        case MatchmakingMessageType::QueueUpdate:
            if (payload && payloadSize >= sizeof(Wire::QueueUpdatePayload)) {
                const auto* p = static_cast<const Wire::QueueUpdatePayload*>(payload);
                queueStatus_.playersInQueue = p->playersInQueue;
                queueStatus_.estimatedWaitTime = p->estimatedWaitTime;
                // server may send authoritative search time; keep local too.
                queueStatus_.searchTime = std::max(queueStatus_.searchTime, p->searchTime);
                queueStatus_.region = std::string(p->region, strnlen(p->region, sizeof(p->region)));
            }
            LOG_INFO("Queue update: {} players in queue", queueStatus_.playersInQueue);
            if (onQueueUpdate_) onQueueUpdate_(queueStatus_);
            break;
            
        case MatchmakingMessageType::MatchFound:
            LOG_INFO("Match found! Lobby ID: {}", lobbyId);
            currentLobby_.lobbyId = lobbyId;
            currentLobby_.state = LobbyState::Found;
            // Initialize accept arrays using payload if provided.
            if (payload && payloadSize >= sizeof(Wire::MatchFoundPayload)) {
                const auto* p = static_cast<const Wire::MatchFoundPayload*>(payload);
                currentLobby_.players.clear();
                currentLobby_.players.resize(p->requiredPlayers);
                acceptPlayerIds_.clear();
                acceptStates_.clear();
                acceptPlayerIds_.resize(p->requiredPlayers, 0);
                acceptStates_.resize(p->requiredPlayers, false);

                acceptTimeoutSeconds_ = p->acceptTimeoutSeconds;
                acceptElapsedSeconds_ = 0.0f;
            }
            if (onMatchFound_) onMatchFound_(currentLobby_);
            break;

        case MatchmakingMessageType::MatchAcceptStatus:
            if (payload && payloadSize >= sizeof(Wire::MatchAcceptStatusPayload)) {
                const auto* p = static_cast<const Wire::MatchAcceptStatusPayload*>(payload);
                const u16 count = (u16)std::min<size_t>(p->playerCount, Wire::kMaxLobbyPlayers);
                acceptPlayerIds_.assign(p->playerIds, p->playerIds + count);
                acceptStates_.assign(count, false);
                for (u16 i = 0; i < count; ++i) {
                    acceptStates_[i] = (p->accepted[i] != 0);
                }
                if (onMatchAcceptStatus_) {
                    onMatchAcceptStatus_(p->requiredPlayers, acceptPlayerIds_, acceptStates_);
                }
            }
            break;
            
        case MatchmakingMessageType::MatchReady:
            if (payload && payloadSize >= sizeof(Wire::MatchReadyPayload)) {
                const auto* p = static_cast<const Wire::MatchReadyPayload*>(payload);
                currentLobby_.gameServerIP = std::string(p->serverIp, strnlen(p->serverIp, sizeof(p->serverIp)));
                currentLobby_.gameServerPort = p->serverPort;
            }
            LOG_INFO("Match ready! Server: {}:{}", currentLobby_.gameServerIP, currentLobby_.gameServerPort);
            if (onMatchReady_) {
                onMatchReady_(currentLobby_.gameServerIP, currentLobby_.gameServerPort);
            }
            inQueue_ = false;
            queueStatus_.inQueue = false;
            acceptTimeoutSeconds_ = 0;
            acceptElapsedSeconds_ = 0.0f;
            break;
            
        case MatchmakingMessageType::MatchCancelled:
        {
            std::string reason = "Match cancelled";
            bool shouldRequeue = false;
            if (payload && payloadSize >= sizeof(Wire::MatchCancelledPayload)) {
                const auto* p = static_cast<const Wire::MatchCancelledPayload*>(payload);
                reason = std::string(p->reason, strnlen(p->reason, sizeof(p->reason)));
                shouldRequeue = (p->shouldRequeue != 0);
            } else if (payload && payloadSize >= sizeof(Wire::ErrorPayload)) {
                // Legacy fallback
                const auto* p = static_cast<const Wire::ErrorPayload*>(payload);
                reason = std::string(p->message, strnlen(p->message, sizeof(p->message)));
            }
            LOG_WARN("Match cancelled: {} (requeue={})", reason, shouldRequeue);
            
            // Reset accept state
            acceptTimeoutSeconds_ = 0;
            acceptElapsedSeconds_ = 0.0f;
            currentLobby_.state = LobbyState::Searching;
            
            if (shouldRequeue) {
                // Stay in queue - server has re-added us
                inQueue_ = true;
                queueStatus_.inQueue = true;
                // Reset search time for new search
                queueStatus_.searchTime = 0;
            } else {
                // Don't requeue - this player caused the cancellation
                inQueue_ = false;
                queueStatus_.inQueue = false;
            }
            
            if (onMatchCancelled_) onMatchCancelled_(reason, shouldRequeue);
            break;
        }
            
        case MatchmakingMessageType::Error:
        {
            std::string err = "Matchmaking error";
            if (payload && payloadSize >= sizeof(Wire::ErrorPayload)) {
                const auto* p = static_cast<const Wire::ErrorPayload*>(payload);
                err = std::string(p->message, strnlen(p->message, sizeof(p->message)));
            }
            LOG_ERROR("Matchmaking error: {}", err);
            if (onError_) onError_(err);
            break;
        }
        
        case MatchmakingMessageType::QueueRejected:
        {
            std::string reason = "Queue rejected";
            bool authFailed = false;
            bool isBanned = false;
            if (payload && payloadSize >= sizeof(Wire::QueueRejectedPayload)) {
                const auto* p = static_cast<const Wire::QueueRejectedPayload*>(payload);
                reason = std::string(p->reason, strnlen(p->reason, sizeof(p->reason)));
                authFailed = (p->authFailed != 0);
                isBanned = (p->isBanned != 0);
            }
            LOG_WARN("Queue rejected: {} (authFailed={}, banned={})", reason, authFailed, isBanned);
            
            inQueue_ = false;
            queueStatus_.inQueue = false;
            
            if (onQueueRejected_) onQueueRejected_(reason, authFailed, isBanned);
            break;
        }
            
        default:
            // ignore
            break;
    }
}

void MatchmakingClient::sendHeartbeat() {
    sendPacket(MatchmakingMessageType::Heartbeat, currentLobby_.lobbyId, nullptr, 0);
}

} // namespace Matchmaking
} // namespace WorldEditor
