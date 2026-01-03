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

using namespace WorldEditor;
using namespace WorldEditor::Network;
using namespace WorldEditor::Matchmaking;
using namespace WorldEditor::Matchmaking::Wire;

// ============ Dedicated Server Application ============

class DedicatedServerApp {
public:
    DedicatedServerApp() 
        : running_(false)
        , tickRate_(NetworkConfig::SERVER_TICK_RATE) {
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
        
        // Add client to server world
        serverWorld_->addClient(clientId);
        
        // Start game if not already started
        if (!serverWorld_->isGameActive()) {
            LOG_INFO("Starting game...");
            serverWorld_->startGame();
        }
    }
    
    void onClientDisconnected(ClientId clientId) {
        LOG_INFO("<<< Client {} disconnected", clientId);
        serverWorld_->removeClient(clientId);
        
        // Stop game if no clients
        if (networkServer_->getClientCount() == 0) {
            LOG_INFO("No clients connected, pausing game");
            serverWorld_->pauseGame();
        }
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
                LOG_INFO("MM: Assigned lobby {} (expectedPlayers={})", p->lobbyId, p->expectedPlayers);
            }
        }
    }
    
    std::unique_ptr<ServerWorld> serverWorld_;
    std::unique_ptr<NetworkServer> networkServer_;
    std::atomic<bool> running_;
    u32 tickRate_;

    // Matchmaking (server pool)
    UDPSocket mmSocket_;
    std::string coordinatorIP_ = "127.0.0.1";
    u16 coordinatorPort_ = kCoordinatorPort;
    u64 serverId_ = 0;
    f32 heartbeatTimer_ = 0.0f;
    f32 heartbeatInterval_ = 2.0f;
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
