#define NOMINMAX
#include "ServerWorld.h"
#include "network/NetworkServer.h"
#include "network/NetworkCommon.h"
#include "network/MatchmakingTypes.h"
#include "network/MatchmakingProtocol.h"
#include "core/Timer.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <random>
#include <vector>
#include <unordered_map>

using namespace WorldEditor;
using namespace WorldEditor::Network;
using namespace WorldEditor::Matchmaking;
using namespace WorldEditor::Matchmaking::Wire;

// Helper to copy string safely
static void CopyString(char* dst, size_t dstSize, const std::string& src) {
    if (!dst || dstSize == 0) return;
    size_t len = std::min(src.size(), dstSize - 1);
    std::memcpy(dst, src.c_str(), len);
    dst[len] = '\0';
}

// ============ Dedicated Server Application ============

class DedicatedServerApp {
public:
    // Client info for tracking
    struct ClientInfo {
        ClientId clientId = 0;
        u64 accountId = 0;
        std::string username;
        std::string heroName;
        u8 teamSlot = 0;
        bool isConnected = false;
    };
    
    DedicatedServerApp() 
        : running_(false)
        , tickRate_(NetworkConfig::SERVER_TICK_RATE)
        , gameStarted_(false)
        , gameEnded_(false)
        , gameEndTimer_(0.0f)
        , gameStartDelay_(0.0f)
        , minPlayersToPlay_(1) {  // Minimum 1 for testing, should be 2+ in production
    }
    
    bool initialize(u16 port, const char* coordinatorIP = "127.0.0.1", u16 coordinatorPort = kCoordinatorPort) {
        LOG_INFO("=== Dedicated Server Initializing ===");
        
        // Initialize network system
        if (!NetworkSystem::Initialize()) {
            LOG_ERROR("Failed to initialize network system");
            return false;
        }
        
        // Create server world
        serverWorld_ = std::make_unique<ServerWorld>();
        LOG_INFO("Server world created");
        
        // Create network server
        networkServer_ = std::make_unique<NetworkServer>();
        
        // Setup callbacks
        networkServer_->setOnClientConnected([this](ClientId clientId) {
            onClientConnected(clientId);
        });
        
        networkServer_->setOnClientDisconnected([this](ClientId clientId) {
            onClientDisconnected(clientId);
        });
        
        networkServer_->setOnClientInput([this](ClientId clientId, const PlayerInput& input) {
            onClientInput(clientId, input);
        });
        
        // Start game when all heroes are picked (with delay)
        networkServer_->setOnAllPicked([this]() {
            LOG_INFO("All heroes picked! Game starting in 3 seconds...");
            gameStartDelay_ = 3.0f;  // 3 second delay before game starts
        });
        
        // Start network server
        if (!networkServer_->start(port)) {
            LOG_ERROR("Failed to start network server");
            return false;
        }

        // Register with matchmaking coordinator (server pool).
        coordinatorIP_ = coordinatorIP ? coordinatorIP : "127.0.0.1";
        coordinatorPort_ = coordinatorPort;
        serverId_ = generateServerId_();
        if (!mmSocket_.create()) {
            LOG_WARN("Failed to create MM socket (server pool disabled)");
        } else {
            // Bind ephemeral port (0) to receive AssignLobby.
            mmSocket_.bind(0);
            sendServerRegister_(port);
        }
        
        LOG_INFO("=== Dedicated Server Ready ===");
        LOG_INFO("Listening on port {}", port);
        LOG_INFO("Tick rate: {} Hz", tickRate_);
        LOG_INFO("Press Ctrl+C to stop");
        
        return true;
    }
    
    void shutdown() {
        LOG_INFO("=== Shutting down server ===");

        mmSocket_.close();
        
        if (networkServer_) {
            networkServer_->stop();
        }
        
        serverWorld_.reset();
        networkServer_.reset();
        
        NetworkSystem::Shutdown();
        
        LOG_INFO("Server shutdown complete");
    }
    
    void run() {
        running_ = true;
        
        Timer frameTimer;
        Timer statsTimer;
        Timer mmTimer;
        f64 lastFrameTime = frameTimer.elapsed();
        
        const f32 tickInterval = 1.0f / static_cast<f32>(tickRate_);
        f32 accumulator = 0.0f;
        
        u64 tickCount = 0;
        
        while (running_) {
            f64 currentTime = frameTimer.elapsed();
            f32 deltaTime = static_cast<f32>(currentTime - lastFrameTime);
            lastFrameTime = currentTime;
            
            accumulator += deltaTime;
            
            // Fixed timestep simulation
            while (accumulator >= tickInterval) {
                tick(tickInterval);
                accumulator -= tickInterval;
                tickCount++;
            }
            
            // Network update (process packets)
            networkServer_->update(deltaTime);
            
            // Handle game end timer
            if (gameEnded_ && gameEndTimer_ > 0.0f) {
                gameEndTimer_ -= deltaTime;
                if (gameEndTimer_ <= 0.0f) {
                    LOG_INFO("Game end timer expired, shutting down server...");
                    running_ = false;
                    break;
                }
            }
            
            // Handle game start delay (after hero pick)
            if (gameStartDelay_ > 0.0f && !gameStarted_) {
                gameStartDelay_ -= deltaTime;
                if (gameStartDelay_ <= 0.0f) {
                    LOG_INFO("Game start delay expired, starting game!");
                    serverWorld_->startGame();
                    gameStarted_ = true;
                    gameStartDelay_ = 0.0f;
                }
            }

            // Matchmaking side-channel (register/heartbeat + AssignLobby)
            if (mmSocket_.isValid()) {
                pumpMatchmaking_();
                heartbeatTimer_ += deltaTime;
                if (heartbeatTimer_ >= heartbeatInterval_) {
                    sendServerHeartbeat_((f32)mmTimer.elapsed());
                    heartbeatTimer_ = 0.0f;
                }
            }
            
            // Print stats every 10 seconds
            if (statsTimer.elapsed() >= 10.0) {
                printStats(tickCount, static_cast<f32>(statsTimer.elapsed()));
                tickCount = 0;
                statsTimer.reset();
            }
            
            // Sleep to avoid busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void stop() {
        running_ = false;
    }
    
private:
    void tick(f32 deltaTime) {
        // Update game simulation
        serverWorld_->update(deltaTime);
        
        // Create and send snapshot to all clients
        if (networkServer_->getClientCount() > 0) {
            WorldSnapshot snapshot = serverWorld_->createSnapshot();
            networkServer_->sendSnapshotToAll(snapshot);
        }
    }
    
    void onClientConnected(ClientId clientId) {
        LOG_INFO(">>> Client {} connected", clientId);
        
        // Get username and accountId from NetworkServer
        std::string username = networkServer_->getClientUsername(clientId);
        u64 accountId = networkServer_->getClientAccountId(clientId);
        if (username.empty()) {
            username = "Player" + std::to_string(clientId);
        }
        
        // Create client info
        ClientInfo info;
        info.clientId = clientId;
        info.accountId = accountId;
        info.username = username;
        info.teamSlot = static_cast<u8>(clients_.size());  // Assign slot based on join order
        info.isConnected = true;
        clients_[clientId] = info;
        
        LOG_INFO(">>> Player '{}' connected (slot {}, accountId={})", username, info.teamSlot, accountId);
        
        // Add client to server world
        serverWorld_->addClient(clientId);
        
        // Check if we should start hero pick phase
        size_t clientCount = networkServer_->getClientCount();
        
        // Start hero pick when we have at least 2 players (for testing)
        // In production, wait for expected player count from matchmaking
        if (clientCount >= 2 && !networkServer_->isInHeroPickPhase() && !gameStarted_) {
            LOG_INFO("Starting hero pick phase with {} players...", clientCount);
            networkServer_->startHeroPickPhase(30.0f);  // 30 seconds to pick
        }
        
        // Game will start after hero pick phase completes (via onAllPicked callback)
    }
    
    void onClientDisconnected(ClientId clientId) {
        // Get client info before removing
        std::string username = "Unknown";
        u64 accountId = 0;
        u8 clientTeamSlot = 0;
        std::string heroName;
        
        auto it = clients_.find(clientId);
        if (it != clients_.end()) {
            username = it->second.username;
            accountId = it->second.accountId;
            clientTeamSlot = it->second.teamSlot;
            heroName = it->second.heroName;
            it->second.isConnected = false;
        }
        
        LOG_INFO("<<< Player '{}' disconnected (slot {}, accountId={})", username, clientTeamSlot, accountId);
        
        // Notify matchmaking coordinator about disconnect
        if (mmSocket_.isValid() && currentLobbyId_ != 0) {
            PlayerDisconnectedPayload p{};
            p.serverId = serverId_;
            p.lobbyId = currentLobbyId_;
            p.accountId = accountId;  // Use real accountId from auth
            p.teamSlot = clientTeamSlot;
            CopyString(p.heroName, sizeof(p.heroName), heroName);
            sendPacketToCoordinator_(MatchmakingMessageType::PlayerDisconnected, &p, sizeof(p), currentLobbyId_);
            LOG_INFO("Notified coordinator: player '{}' (accountId={}) disconnected", username, accountId);
        }
        
        serverWorld_->removeClient(clientId);
        
        size_t remainingClients = networkServer_->getClientCount();
        
        // Check if game should end
        if (remainingClients == 0) {
            LOG_INFO("=== ALL PLAYERS DISCONNECTED ===");
            LOG_INFO("Game ended - no players remaining");
            
            // Calculate winner based on game state
            calculateGameResult();
            
            // Pause game
            serverWorld_->pauseGame();
            
            // Schedule server shutdown after a delay
            LOG_INFO("Server will shutdown in 5 seconds...");
            gameEndTimer_ = 5.0f;
            gameEnded_ = true;
        }
        else if (gameStarted_ && remainingClients < minPlayersToPlay_) {
            // Not enough players to continue - end game
            LOG_INFO("=== NOT ENOUGH PLAYERS ===");
            LOG_INFO("Only {} players remaining, minimum {} required", remainingClients, minPlayersToPlay_);
            
            calculateGameResult();
            gameEnded_ = true;
            gameEndTimer_ = 10.0f;  // Give time for remaining players to see result
        }
    }
    
    void calculateGameResult() {
        // Count remaining players per team
        // Radiant: slots 0-4, Dire: slots 5-9
        int radiantPlayers = 0;
        int direPlayers = 0;
        
        // TODO: Get actual team counts from NetworkServer client info
        // For now, simple logic: team with more remaining players wins
        
        f32 gameTime = serverWorld_->getGameTime();
        
        LOG_INFO("=== GAME RESULT ===");
        LOG_INFO("  Game Duration: {:.1f} seconds", gameTime);
        
        if (radiantPlayers > direPlayers) {
            LOG_INFO("  Winner: RADIANT");
            LOG_INFO("  Radiant players: {}, Dire players: {}", radiantPlayers, direPlayers);
        } else if (direPlayers > radiantPlayers) {
            LOG_INFO("  Winner: DIRE");
            LOG_INFO("  Radiant players: {}, Dire players: {}", radiantPlayers, direPlayers);
        } else {
            // Both teams have same players (or 0) - check other conditions
            // For now, if all disconnected, it's a draw
            LOG_INFO("  Result: DRAW (all players disconnected)");
        }
        
        // TODO: Send game result to matchmaking coordinator for stats
        // TODO: Update player MMR based on result
    }
    
    void onClientInput(ClientId clientId, const PlayerInput& input) {
        // Process input in server world
        serverWorld_->processInput(clientId, input);
    }
    
    void printStats(u64 tickCount, f32 duration) {
        f32 avgTickRate = static_cast<f32>(tickCount) / duration;
        size_t entityCount = serverWorld_->getEntityCount();
        size_t clientCount = networkServer_->getClientCount();
        f32 gameTime = serverWorld_->getGameTime();
        
        LOG_INFO("=== Server Stats ===");
        LOG_INFO("  Tick Rate: {:.1f} Hz (target: {})", avgTickRate, tickRate_);
        LOG_INFO("  Clients: {}", clientCount);
        LOG_INFO("  Entities: {}", entityCount);
        LOG_INFO("  Game Time: {:.1f}s", gameTime);
    }

private:
    static u64 generateServerId_() {
        static std::mt19937_64 rng{ std::random_device{}() };
        return rng();
    }

    void sendPacketToCoordinator_(MatchmakingMessageType type, const void* payload, u32 payloadSize, u64 lobbyId = 0) {
        std::vector<u8> pkt;
        if (!BuildPacket(pkt, type, 0, lobbyId, payload, payloadSize)) return;
        NetworkAddress addr(coordinatorIP_.c_str(), coordinatorPort_);
        mmSocket_.sendTo(pkt.data(), pkt.size(), addr);
    }

    void sendServerRegister_(u16 gamePort) {
        ServerRegisterPayload p{};
        p.serverId = serverId_;
        // For local dev, advertise localhost. Later: detect LAN/public IP.
        CopyCString(p.serverIp, sizeof(p.serverIp), "127.0.0.1");
        p.gamePort = gamePort;
        p.capacity = (u16)MAX_CLIENTS;
        sendPacketToCoordinator_(MatchmakingMessageType::ServerRegister, &p, sizeof(p));
        LOG_INFO("MM: Registered server {} as 127.0.0.1:{} cap={}", serverId_, gamePort, p.capacity);
    }

    void sendServerHeartbeat_(f32 uptimeSeconds) {
        ServerHeartbeatPayload p{};
        p.serverId = serverId_;
        p.currentPlayers = (u16)networkServer_->getClientCount();
        p.capacity = (u16)MAX_CLIENTS;
        p.uptimeSeconds = uptimeSeconds;
        sendPacketToCoordinator_(MatchmakingMessageType::ServerHeartbeat, &p, sizeof(p));
    }

    void pumpMatchmaking_() {
        u8 buffer[2048];
        NetworkAddress from;
        while (true) {
            const i32 bytes = mmSocket_.receiveFrom(buffer, sizeof(buffer), from);
            if (bytes <= 0) break;

            MMHeader h{};
            const void* payload = nullptr;
            u32 payloadSize = 0;
            if (!ParsePacket(buffer, (size_t)bytes, h, payload, payloadSize)) {
                continue;
            }

            const auto type = (MatchmakingMessageType)h.type;
            if (type == MatchmakingMessageType::AssignLobby && payloadSize >= sizeof(AssignLobbyPayload)) {
                const auto* p = (const AssignLobbyPayload*)payload;
                currentLobbyId_ = p->lobbyId;
                LOG_INFO("MM: Assigned lobby {} (expectedPlayers={})", p->lobbyId, p->expectedPlayers);
            }
        }
    }
    
    std::unique_ptr<ServerWorld> serverWorld_;
    std::unique_ptr<NetworkServer> networkServer_;
    std::atomic<bool> running_;
    u32 tickRate_;
    
    // Game state
    bool gameStarted_ = false;
    bool gameEnded_ = false;
    f32 gameEndTimer_ = 0.0f;
    f32 gameStartDelay_ = 0.0f;  // Delay after hero pick before game starts
    size_t minPlayersToPlay_ = 1;  // Minimum players to continue game

    // Matchmaking (server pool)
    UDPSocket mmSocket_;
    std::string coordinatorIP_ = "127.0.0.1";
    u16 coordinatorPort_ = kCoordinatorPort;
    u64 serverId_ = 0;
    u64 currentLobbyId_ = 0;  // Current game lobby ID
    f32 heartbeatTimer_ = 0.0f;
    f32 heartbeatInterval_ = 2.0f;
    
    // Client tracking
    std::unordered_map<ClientId, ClientInfo> clients_;
};

// ============ Main Entry Point ============

static DedicatedServerApp* g_serverApp = nullptr;

#ifdef _WIN32
#include <windows.h>

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        LOG_INFO("Shutdown signal received");
        if (g_serverApp) {
            g_serverApp->stop();
        }
        return TRUE;
    }
    return FALSE;
}
#endif

int main(int argc, char** argv) {
    // Setup logging
    auto console = spdlog::stdout_color_mt("server");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    
    // Parse command line
    u16 port = DEFAULT_SERVER_PORT;
    const char* mmIP = "127.0.0.1";
    u16 mmPort = kCoordinatorPort;
    
    if (argc > 1) {
        port = static_cast<u16>(std::atoi(argv[1]));
    }
    if (argc > 2) {
        mmIP = argv[2];
    }
    if (argc > 3) {
        mmPort = static_cast<u16>(std::atoi(argv[3]));
    }
    
    // Create server app
    DedicatedServerApp serverApp;
    g_serverApp = &serverApp;
    
    // Setup signal handler
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#endif
    
    // Initialize
    if (!serverApp.initialize(port, mmIP, mmPort)) {
        LOG_ERROR("Failed to initialize server");
        return 1;
    }
    
    // Run server loop
    serverApp.run();
    
    // Cleanup
    serverApp.shutdown();
    g_serverApp = nullptr;
    
    return 0;
}
