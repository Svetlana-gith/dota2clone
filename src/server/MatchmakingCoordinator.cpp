#define NOMINMAX

#include "network/NetworkCommon.h"
#include "network/MatchmakingTypes.h"
#include "network/MatchmakingProtocol.h"
#include "auth/AuthProtocol.h"
#include "core/Timer.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <optional>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace WorldEditor;
using namespace WorldEditor::Network;
using namespace WorldEditor::Matchmaking;
using namespace WorldEditor::Matchmaking::Wire;

namespace {

static u64 RandomU64() {
    static std::mt19937_64 rng{ std::random_device{}() };
    return rng();
}

static std::string ReadFixedString(const char* s, size_t maxLen) {
    if (!s || maxLen == 0) return {};
    const size_t n = strnlen(s, maxLen);
    return std::string(s, s + n);
}

struct QueuedPlayer {
    u64 playerId = 0;
    u64 accountId = 0;  // Auth account ID
    MatchMode mode = MatchMode::AllPick;
    std::string region = "auto";
    std::string sessionToken;
};

// Pending auth validation request
struct PendingAuthValidation {
    u64 playerId = 0;
    NetworkAddress playerAddr;
    MatchMode mode = MatchMode::AllPick;
    std::string region = "auto";
    std::string sessionToken;
    u32 requestId = 0;
    f32 timeSinceRequest = 0.0f;
    static constexpr f32 kTimeoutSeconds = 5.0f;
};

struct Lobby {
    u64 lobbyId = 0;
    MatchMode mode = MatchMode::AllPick;
    std::string region = "auto";
    std::vector<u64> players;
    std::unordered_map<u64, bool> accepted;
    f32 acceptTimeoutSeconds = 20.0f;
    f32 timeSinceFound = 0.0f;
};

struct ServerEntry {
    u64 serverId = 0;
    std::string ip;
    u16 gamePort = 0;
    u16 capacity = 0;
    u16 currentPlayers = 0;
    f32 uptimeSeconds = 0.0f;
    f32 timeSinceHeartbeat = 0.0f;
    bool reserved = false;
    NetworkAddress controlAddr; // address we received ServerRegister from
};

class CoordinatorApp {
public:
    bool initialize(u16 port, const std::string& authServerIP = "127.0.0.1", u16 authServerPort = auth::kAuthServerPort) {
        if (!NetworkSystem::Initialize()) {
            LOG_ERROR("Failed to init network system");
            return false;
        }
        if (!socket_.create()) {
            LOG_ERROR("Failed to create UDP socket");
            return false;
        }
        if (!socket_.bind(port)) {
            LOG_ERROR("Failed to bind coordinator port {}", port);
            return false;
        }
        
        // Create auth socket for communicating with Auth Server
        if (!authSocket_.create()) {
            LOG_ERROR("Failed to create auth socket");
            return false;
        }
        
        authServerIP_ = authServerIP;
        authServerPort_ = authServerPort;
        authServerAddr_ = NetworkAddress(authServerIP.c_str(), authServerPort);

        listenPort_ = port;
        LOG_INFO("=== MatchmakingCoordinator Ready ===");
        LOG_INFO("Listening UDP {}", listenPort_);
        LOG_INFO("Auth Server: {}:{}", authServerIP_, authServerPort_);
        return true;
    }

    void shutdown() {
        socket_.close();
        authSocket_.close();
        NetworkSystem::Shutdown();
    }

    void run() {
        running_ = true;

        Timer timer;
        f64 last = timer.elapsed();

        while (running_) {
            const f64 now = timer.elapsed();
            const f32 dt = static_cast<f32>(now - last);
            last = now;

            pumpNetwork();
            tick(dt);

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void stop() { running_ = false; }

private:
    void pumpNetwork() {
        u8 buffer[2048];
        NetworkAddress from;

        // Receive matchmaking messages
        while (true) {
            const i32 bytes = socket_.receiveFrom(buffer, sizeof(buffer), from);
            if (bytes <= 0) break;

            MMHeader h{};
            const void* payload = nullptr;
            u32 payloadSize = 0;
            if (!ParsePacket(buffer, static_cast<size_t>(bytes), h, payload, payloadSize)) {
                // Ignore unknown packets.
                continue;
            }

            const auto type = static_cast<MatchmakingMessageType>(h.type);
            handleMessage(type, h.playerId, h.lobbyId, payload, payloadSize, from);
        }
        
        // Receive auth validation responses
        while (true) {
            const i32 bytes = authSocket_.receiveFrom(buffer, sizeof(buffer), from);
            if (bytes <= 0) break;
            
            auth::AuthHeader ah{};
            const void* payload = nullptr;
            u32 payloadSize = 0;
            if (!auth::ParsePacket(buffer, static_cast<size_t>(bytes), ah, payload, payloadSize)) {
                continue;
            }
            
            const auto type = static_cast<auth::AuthMessageType>(ah.type);
            handleAuthResponse(type, ah.requestId, payload, payloadSize);
        }
    }

    void tick(f32 dt) {
        // Server pool TTL.
        for (auto it = servers_.begin(); it != servers_.end();) {
            it->second.timeSinceHeartbeat += dt;
            // TTL 15s for now.
            if (it->second.timeSinceHeartbeat > 15.0f) {
                LOG_WARN("Server {} timed out (no heartbeat)", it->second.serverId);
                it = servers_.erase(it);
                continue;
            }
            ++it;
        }
        
        // Pending auth validation timeouts
        for (auto it = pendingValidations_.begin(); it != pendingValidations_.end();) {
            it->second.timeSinceRequest += dt;
            if (it->second.timeSinceRequest >= PendingAuthValidation::kTimeoutSeconds) {
                LOG_WARN("Auth validation timeout for player {}", it->second.playerId);
                // Send rejection to player
                QueueRejectedPayload p{};
                CopyCString(p.reason, sizeof(p.reason), "Authentication server timeout");
                p.authFailed = 1;
                p.isBanned = 0;
                sendToPlayer(it->second.playerId, MatchmakingMessageType::QueueRejected, 
                            it->second.playerId, 0, &p, sizeof(p));
                it = pendingValidations_.erase(it);
                continue;
            }
            ++it;
        }

        // Lobby accept timeouts.
        for (auto it = lobbies_.begin(); it != lobbies_.end();) {
            Lobby& l = it->second;
            l.timeSinceFound += dt;
            if (l.timeSinceFound >= l.acceptTimeoutSeconds) {
                LOG_WARN("Lobby {} accept timed out -> cancelled", l.lobbyId);
                // Find first player who didn't accept (they caused the timeout)
                u64 timedOutPlayer = 0;
                for (const auto& kv : l.accepted) {
                    if (!kv.second) {
                        timedOutPlayer = kv.first;
                        break;
                    }
                }
                notifyMatchCancelledWithRequeue(l, "Accept timeout", timedOutPlayer);
                it = lobbies_.erase(it);
                continue;
            }
            ++it;
        }

        // Try create lobbies from queue (dev mode: 2 players).
        tryCreateLobby();
    }

    void tryCreateLobby() {
        if (queue_.size() < requiredPlayers_) return;

        // Very simple: take first N.
        Lobby lobby;
        lobby.lobbyId = RandomU64();
        lobby.acceptTimeoutSeconds = 20.0f;

        // Use first player's mode/region as lobby key for now.
        lobby.mode = queue_[0].mode;
        lobby.region = queue_[0].region;

        for (u16 i = 0; i < requiredPlayers_; ++i) {
            lobby.players.push_back(queue_[i].playerId);
            lobby.accepted[queue_[i].playerId] = false;
        }
        queue_.erase(queue_.begin(), queue_.begin() + requiredPlayers_);

        lobbies_[lobby.lobbyId] = lobby;

        LOG_INFO("Lobby found: lobbyId={} players={}", lobby.lobbyId, lobby.players.size());

        // Notify clients.
        MatchFoundPayload p{};
        p.requiredPlayers = requiredPlayers_;
        p.acceptTimeoutSeconds = static_cast<u16>(lobby.acceptTimeoutSeconds);
        for (u64 pid : lobby.players) {
            sendToPlayer(pid, MatchmakingMessageType::MatchFound, pid, lobby.lobbyId, &p, sizeof(p));
        }

        // Send initial accept status (all not accepted).
        broadcastAcceptStatus(lobby);
    }

    void handleMessage(MatchmakingMessageType type,
                       u64 playerId,
                       u64 lobbyId,
                       const void* payload,
                       u32 payloadSize,
                       const NetworkAddress& from) {
        switch (type) {
            case MatchmakingMessageType::QueueRequest:
                onQueueRequest(playerId, payload, payloadSize, from);
                break;
            case MatchmakingMessageType::QueueCancel:
                onQueueCancel(playerId);
                break;
            case MatchmakingMessageType::MatchAccept:
                onMatchAccept(playerId, lobbyId);
                break;
            case MatchmakingMessageType::MatchDecline:
                onMatchDecline(playerId, lobbyId);
                break;

            case MatchmakingMessageType::ServerRegister:
                onServerRegister(payload, payloadSize, from);
                break;
            case MatchmakingMessageType::ServerHeartbeat:
                onServerHeartbeat(payload, payloadSize);
                break;

            default:
                // ignore for now
                break;
        }
    }

    void onQueueRequest(u64 playerId, const void* payload, u32 payloadSize, const NetworkAddress& from) {
        if (playerId == 0) return;
        players_[playerId] = from;

        MatchMode mode = MatchMode::AllPick;
        std::string region = "auto";
        std::string sessionToken;

        if (payload && payloadSize >= sizeof(QueueRequestPayload)) {
            const auto* p = static_cast<const QueueRequestPayload*>(payload);
            mode = static_cast<MatchMode>(p->mode);
            region = ReadFixedString(p->region, sizeof(p->region));
            sessionToken = ReadFixedString(p->sessionToken, sizeof(p->sessionToken));
            if (region.empty()) region = "auto";
        }
        
        // Check if session token is provided
        if (sessionToken.empty()) {
            LOG_WARN("Player {} queue request rejected: no session token", playerId);
            QueueRejectedPayload rp{};
            CopyCString(rp.reason, sizeof(rp.reason), "Authentication required");
            rp.authFailed = 1;
            rp.isBanned = 0;
            sendToPlayer(playerId, MatchmakingMessageType::QueueRejected, playerId, 0, &rp, sizeof(rp));
            return;
        }
        
        // Check if already in queue or pending validation
        if (std::any_of(queue_.begin(), queue_.end(), [&](const QueuedPlayer& q) { return q.playerId == playerId; })) {
            LOG_WARN("Player {} already in queue", playerId);
            return;
        }
        
        if (pendingValidations_.find(playerId) != pendingValidations_.end()) {
            LOG_WARN("Player {} already has pending validation", playerId);
            return;
        }
        
        // Start async token validation with Auth Server
        PendingAuthValidation pv;
        pv.playerId = playerId;
        pv.playerAddr = from;
        pv.mode = mode;
        pv.region = region;
        pv.sessionToken = sessionToken;
        pv.requestId = nextAuthRequestId_++;
        pv.timeSinceRequest = 0.0f;
        
        pendingValidations_[playerId] = pv;
        
        // Send validation request to Auth Server
        auth::ValidateTokenRequestPayload vp{};
        auth::CopyString(vp.sessionToken, sizeof(vp.sessionToken), sessionToken);
        auth::CopyString(vp.ipAddress, sizeof(vp.ipAddress), from.toString());
        
        std::vector<u8> pkt;
        if (auth::BuildPacket(pkt, auth::AuthMessageType::ValidateTokenRequest, 0, pv.requestId, &vp, sizeof(vp))) {
            authSocket_.sendTo(pkt.data(), pkt.size(), authServerAddr_);
            LOG_INFO("Player {} queue request - validating token (reqId={})", playerId, pv.requestId);
        } else {
            LOG_ERROR("Failed to build auth validation packet for player {}", playerId);
            pendingValidations_.erase(playerId);
            
            QueueRejectedPayload rp{};
            CopyCString(rp.reason, sizeof(rp.reason), "Internal error");
            rp.authFailed = 1;
            rp.isBanned = 0;
            sendToPlayer(playerId, MatchmakingMessageType::QueueRejected, playerId, 0, &rp, sizeof(rp));
        }
    }

    void onQueueCancel(u64 playerId) {
        if (playerId == 0) return;
        const auto before = queue_.size();
        queue_.erase(std::remove_if(queue_.begin(), queue_.end(),
                                    [&](const QueuedPlayer& q) { return q.playerId == playerId; }),
                     queue_.end());
        if (queue_.size() != before) {
            LOG_INFO("Player {} cancelled queue", playerId);
        }
    }

    void onMatchAccept(u64 playerId, u64 lobbyId) {
        auto it = lobbies_.find(lobbyId);
        if (it == lobbies_.end()) return;
        Lobby& l = it->second;
        if (l.accepted.find(playerId) == l.accepted.end()) return;
        l.accepted[playerId] = true;
        LOG_INFO("Player {} accepted lobby {}", playerId, lobbyId);

        broadcastAcceptStatus(l);

        if (allAccepted(l)) {
            startMatch(l);
            lobbies_.erase(it);
        }
    }

    void onMatchDecline(u64 playerId, u64 lobbyId) {
        auto it = lobbies_.find(lobbyId);
        if (it == lobbies_.end()) return;
        Lobby& l = it->second;
        LOG_WARN("Player {} declined lobby {} -> cancelled", playerId, lobbyId);
        notifyMatchCancelledWithRequeue(l, "Player declined", playerId);
        lobbies_.erase(it);
    }

    void broadcastAcceptStatus(const Lobby& l) {
        MatchAcceptStatusPayload p{};
        p.playerCount = (u16)std::min<size_t>(l.players.size(), kMaxLobbyPlayers);
        p.requiredPlayers = (u16)std::min<size_t>((size_t)requiredPlayers_, kMaxLobbyPlayers);
        for (u16 i = 0; i < p.playerCount; ++i) {
            const u64 pid = l.players[i];
            p.playerIds[i] = pid;
            auto it = l.accepted.find(pid);
            p.accepted[i] = (it != l.accepted.end() && it->second) ? 1 : 0;
        }
        for (u64 pid : l.players) {
            sendToPlayer(pid, MatchmakingMessageType::MatchAcceptStatus, pid, l.lobbyId, &p, sizeof(p));
        }
    }

    bool allAccepted(const Lobby& l) const {
        for (const auto& kv : l.accepted) {
            if (!kv.second) return false;
        }
        return !l.players.empty();
    }

    void notifyMatchCancelled(const Lobby& l, const std::string& reason) {
        // Legacy version - no requeue, used for server errors
        MatchCancelledPayload p{};
        CopyCString(p.reason, sizeof(p.reason), reason);
        p.declinedByPlayerId = 0;
        p.shouldRequeue = 0;
        for (u64 pid : l.players) {
            sendToPlayer(pid, MatchmakingMessageType::MatchCancelled, pid, l.lobbyId, &p, sizeof(p));
        }
    }

    void notifyMatchCancelledWithRequeue(const Lobby& l, const std::string& reason, u64 declinedByPlayerId) {
        // Find players who accepted - they should be requeued
        std::vector<u64> playersToRequeue;
        for (const auto& kv : l.accepted) {
            if (kv.second && kv.first != declinedByPlayerId) {
                playersToRequeue.push_back(kv.first);
            }
        }

        // Notify each player with appropriate requeue flag
        for (u64 pid : l.players) {
            MatchCancelledPayload p{};
            CopyCString(p.reason, sizeof(p.reason), reason);
            p.declinedByPlayerId = declinedByPlayerId;
            
            // Player who declined/timed out should NOT requeue
            // Players who accepted SHOULD requeue
            bool shouldRequeue = (pid != declinedByPlayerId) && 
                                 (l.accepted.find(pid) != l.accepted.end() && l.accepted.at(pid));
            p.shouldRequeue = shouldRequeue ? 1 : 0;
            
            sendToPlayer(pid, MatchmakingMessageType::MatchCancelled, pid, l.lobbyId, &p, sizeof(p));
            
            // Re-add accepted players to queue
            if (shouldRequeue) {
                QueuedPlayer qp;
                qp.playerId = pid;
                qp.mode = l.mode;
                qp.region = l.region;
                queue_.push_back(qp);
                LOG_INFO("Player {} re-queued after match cancelled", pid);
            }
        }
    }

    void startMatch(const Lobby& l) {
        auto serverOpt = pickServer();
        if (!serverOpt.has_value()) {
            LOG_ERROR("No available servers in pool; cancelling lobby {}", l.lobbyId);
            notifyMatchCancelled(l, "No servers available");
            return;
        }

        ServerEntry& s = servers_.at(serverOpt.value());
        s.reserved = true;

        LOG_INFO("Lobby {} assigned to server {} {}:{}",
                 l.lobbyId, s.serverId, s.ip, s.gamePort);

        // Tell server (best-effort).
        AssignLobbyPayload ap{};
        ap.serverId = s.serverId;
        ap.lobbyId = l.lobbyId;
        ap.expectedPlayers = static_cast<u16>(l.players.size());
        sendRaw(s.controlAddr, MatchmakingMessageType::AssignLobby, 0, l.lobbyId, &ap, sizeof(ap));

        // Tell clients where to connect.
        MatchReadyPayload rp{};
        CopyCString(rp.serverIp, sizeof(rp.serverIp), s.ip);
        rp.serverPort = s.gamePort;

        for (u64 pid : l.players) {
            sendToPlayer(pid, MatchmakingMessageType::MatchReady, pid, l.lobbyId, &rp, sizeof(rp));
        }
    }

    std::optional<u64> pickServer() {
        if (servers_.empty()) return std::nullopt;

        u64 bestId = 0;
        int bestScore = INT32_MAX;

        for (auto& kv : servers_) {
            ServerEntry& s = kv.second;
            if (s.reserved) continue;
            if (s.capacity > 0 && s.currentPlayers >= s.capacity) continue;

            // Prefer least loaded.
            const int score = static_cast<int>(s.currentPlayers);
            if (score < bestScore) {
                bestScore = score;
                bestId = s.serverId;
            }
        }

        if (bestId == 0) return std::nullopt;
        return bestId;
    }

    void onServerRegister(const void* payload, u32 payloadSize, const NetworkAddress& from) {
        if (!payload || payloadSize < sizeof(ServerRegisterPayload)) return;
        const auto* p = static_cast<const ServerRegisterPayload*>(payload);

        ServerEntry s;
        s.serverId = p->serverId;
        s.ip = ReadFixedString(p->serverIp, sizeof(p->serverIp));
        s.gamePort = p->gamePort;
        s.capacity = p->capacity;
        s.currentPlayers = 0;
        s.uptimeSeconds = 0.0f;
        s.timeSinceHeartbeat = 0.0f;
        s.reserved = false;
        s.controlAddr = from;

        if (s.serverId == 0 || s.ip.empty() || s.gamePort == 0) {
            return;
        }

        servers_[s.serverId] = s;
        LOG_INFO("Server registered: id={} {}:{} cap={}", s.serverId, s.ip, s.gamePort, s.capacity);
    }

    void onServerHeartbeat(const void* payload, u32 payloadSize) {
        if (!payload || payloadSize < sizeof(ServerHeartbeatPayload)) return;
        const auto* p = static_cast<const ServerHeartbeatPayload*>(payload);
        auto it = servers_.find(p->serverId);
        if (it == servers_.end()) return;

        ServerEntry& s = it->second;
        s.currentPlayers = p->currentPlayers;
        s.capacity = p->capacity;
        s.uptimeSeconds = p->uptimeSeconds;
        s.timeSinceHeartbeat = 0.0f;
        if (s.reserved && s.currentPlayers == 0) {
            // allow reuse after match ends (simple heuristic)
            s.reserved = false;
        }
    }
    
    void handleAuthResponse(auth::AuthMessageType type, u32 requestId, const void* payload, u32 payloadSize) {
        if (type != auth::AuthMessageType::ValidateTokenResponse) {
            return;
        }
        
        // Find pending validation by requestId
        u64 playerId = 0;
        for (const auto& kv : pendingValidations_) {
            if (kv.second.requestId == requestId) {
                playerId = kv.first;
                break;
            }
        }
        
        if (playerId == 0) {
            LOG_WARN("Received auth response for unknown requestId {}", requestId);
            return;
        }
        
        auto it = pendingValidations_.find(playerId);
        if (it == pendingValidations_.end()) {
            return;
        }
        
        const PendingAuthValidation& pv = it->second;
        
        if (!payload || payloadSize < sizeof(auth::ValidateTokenResponsePayload)) {
            LOG_ERROR("Invalid auth response payload for player {}", playerId);
            QueueRejectedPayload rp{};
            CopyCString(rp.reason, sizeof(rp.reason), "Authentication error");
            rp.authFailed = 1;
            rp.isBanned = 0;
            sendToPlayer(playerId, MatchmakingMessageType::QueueRejected, playerId, 0, &rp, sizeof(rp));
            pendingValidations_.erase(it);
            return;
        }
        
        const auto* resp = static_cast<const auth::ValidateTokenResponsePayload*>(payload);
        const auto result = static_cast<auth::AuthResult>(resp->result);
        
        if (result != auth::AuthResult::Success) {
            // Token validation failed
            std::string reason;
            if (resp->isBanned) {
                reason = "Account is banned";
            } else if (result == auth::AuthResult::TokenExpired) {
                reason = "Session expired - please login again";
            } else if (result == auth::AuthResult::TokenInvalid) {
                reason = "Invalid session token";
            } else {
                reason = ReadFixedString(resp->errorMessage, sizeof(resp->errorMessage));
                if (reason.empty()) reason = "Authentication failed";
            }
            
            LOG_WARN("Player {} auth validation failed: {} (banned={})", playerId, reason, resp->isBanned);
            
            QueueRejectedPayload rp{};
            CopyCString(rp.reason, sizeof(rp.reason), reason);
            rp.authFailed = 1;
            rp.isBanned = resp->isBanned;
            sendToPlayer(playerId, MatchmakingMessageType::QueueRejected, playerId, 0, &rp, sizeof(rp));
            pendingValidations_.erase(it);
            return;
        }
        
        // Token is valid - add player to queue
        QueuedPlayer qp;
        qp.playerId = playerId;
        qp.accountId = resp->accountId;
        qp.mode = pv.mode;
        qp.region = pv.region;
        qp.sessionToken = pv.sessionToken;
        
        queue_.push_back(qp);
        pendingValidations_.erase(it);
        
        LOG_INFO("Player {} queued (accountId={}, mode={}, region={})", 
                 playerId, resp->accountId, static_cast<int>(qp.mode), qp.region);
        
        // Confirm queue
        sendToPlayer(playerId, MatchmakingMessageType::QueueConfirm, playerId, 0, nullptr, 0);
    }

    void sendToPlayer(u64 playerId,
                      MatchmakingMessageType type,
                      u64 headerPlayerId,
                      u64 lobbyId,
                      const void* payload,
                      u32 payloadSize) {
        auto it = players_.find(playerId);
        if (it == players_.end()) return;
        sendRaw(it->second, type, headerPlayerId, lobbyId, payload, payloadSize);
    }

    void sendRaw(const NetworkAddress& addr,
                 MatchmakingMessageType type,
                 u64 playerId,
                 u64 lobbyId,
                 const void* payload,
                 u32 payloadSize) {
        std::vector<u8> pkt;
        if (!BuildPacket(pkt, type, playerId, lobbyId, payload, payloadSize)) return;
        socket_.sendTo(pkt.data(), pkt.size(), addr);
    }

private:
    UDPSocket socket_;
    UDPSocket authSocket_;
    u16 listenPort_ = kCoordinatorPort;
    std::atomic<bool> running_{false};
    
    // Auth Server connection
    std::string authServerIP_ = "127.0.0.1";
    u16 authServerPort_ = auth::kAuthServerPort;
    NetworkAddress authServerAddr_;
    std::atomic<u32> nextAuthRequestId_{1};

    // Dev mode knobs
    u16 requiredPlayers_ = 2;

    // State
    std::unordered_map<u64, NetworkAddress> players_;
    std::vector<QueuedPlayer> queue_;
    std::unordered_map<u64, Lobby> lobbies_;
    std::unordered_map<u64, ServerEntry> servers_;
    std::unordered_map<u64, PendingAuthValidation> pendingValidations_;
};

} // namespace

static CoordinatorApp* g_app = nullptr;

#ifdef _WIN32
#include <windows.h>
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        LOG_INFO("Shutdown signal received");
        if (g_app) g_app->stop();
        return TRUE;
    }
    return FALSE;
}
#endif

int main(int argc, char** argv) {
    auto console = spdlog::stdout_color_mt("mmc");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    u16 port = kCoordinatorPort;
    std::string authServerIP = "127.0.0.1";
    u16 authServerPort = auth::kAuthServerPort;
    
    // Parse command line args
    // Usage: MatchmakingCoordinator [port] [auth_server_ip] [auth_server_port]
    if (argc > 1) {
        port = static_cast<u16>(std::atoi(argv[1]));
    }
    if (argc > 2) {
        authServerIP = argv[2];
    }
    if (argc > 3) {
        authServerPort = static_cast<u16>(std::atoi(argv[3]));
    }

    CoordinatorApp app;
    g_app = &app;

#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#endif
    if (!app.initialize(port, authServerIP, authServerPort)) {
        return 1;
    }

    app.run();
    app.shutdown();
    g_app = nullptr;
    return 0;
}

