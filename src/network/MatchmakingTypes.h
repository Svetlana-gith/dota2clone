#pragma once
/**
 * Matchmaking Types - Dota 2 style matchmaking system
 * Handles player queuing, lobby creation, and game server assignment
 */

#include "../core/Types.h"
#include <string>
#include <vector>

namespace WorldEditor {
namespace Matchmaking {

// ============ Player Info ============

struct PlayerInfo {
    u64 steamId = 0;           // Unique player ID (for now just random)
    std::string playerName;
    u32 mmr = 1000;            // Matchmaking rating
    u32 behaviorScore = 10000; // Behavior score (0-10000)
    bool isReady = false;
};

// ============ Match Info ============

enum class MatchMode {
    AllPick,
    CaptainsMode,
    RandomDraft,
    SingleDraft,
    AllRandom
};

struct MatchPreferences {
    MatchMode mode = MatchMode::AllPick;
    std::vector<std::string> preferredRoles; // "carry", "support", "mid", etc.
    std::string region = "auto";
};

// ============ Lobby State ============

enum class LobbyState {
    Searching,      // Looking for players
    Found,          // 10 players found, waiting for accept
    Ready,          // All players accepted, loading game
    InGame,         // Game in progress
    Finished        // Game ended
};

struct LobbyInfo {
    u64 lobbyId = 0;
    LobbyState state = LobbyState::Searching;
    MatchMode mode = MatchMode::AllPick;
    std::vector<PlayerInfo> players;
    std::string gameServerIP;
    u16 gameServerPort = 0;
    f32 averageMMR = 0;
    f32 searchTime = 0;
    
    bool isFull() const { return players.size() >= 10; }
    bool allPlayersReady() const {
        for (const auto& p : players) {
            if (!p.isReady) return false;
        }
        return !players.empty();
    }
};

// ============ Matchmaking Messages ============

enum class MatchmakingMessageType : u8 {
    // Client -> Matchmaking Server
    QueueRequest,           // Player wants to find a match
    QueueCancel,            // Player cancels search
    MatchAccept,            // Player accepts found match
    MatchDecline,           // Player declines found match
    CheckActiveGame,        // Check if player has an active game to reconnect
    ReconnectRequest,       // Player wants to reconnect to active game
    
    // Matchmaking Server -> Client
    QueueConfirm,           // Queue request accepted
    QueueUpdate,            // Update on search progress
    MatchFound,             // Match found, waiting for accept
    MatchAcceptStatus,      // Accept status update (who accepted)
    MatchReady,             // All players accepted, here's the server
    MatchCancelled,         // Match cancelled (someone declined)
    QueueRejected,          // Queue request rejected (auth failed, banned, etc.)
    ActiveGameInfo,         // Response to CheckActiveGame - has active game info
    NoActiveGame,           // Response to CheckActiveGame - no active game
    ReconnectApproved,      // Reconnect approved, here's the server info
    
    // Status
    Heartbeat,
    Error,

    // DedicatedServer -> Coordinator (server pool)
    ServerRegister = 100,
    ServerHeartbeat = 101,
    PlayerDisconnected = 103,  // Notify coordinator that player disconnected
    PlayerReconnected = 104,   // Notify coordinator that player reconnected
    GameEnded = 105,           // Game finished, clear active game records

    // Coordinator -> DedicatedServer
    AssignLobby = 102,
    ReconnectPlayer = 106      // Tell server to expect reconnecting player
};

struct MatchmakingMessage {
    MatchmakingMessageType type;
    u64 playerId = 0;
    u64 lobbyId = 0;
    std::string data;  // JSON or serialized data
    f32 timestamp = 0;
};

// ============ Queue Status ============

struct QueueStatus {
    bool inQueue = false;
    f32 searchTime = 0;
    u32 playersInQueue = 0;
    f32 estimatedWaitTime = 0;
    std::string region;
};

// ============ Active Game Info (for reconnect) ============

struct ActiveGameInfo {
    u64 lobbyId = 0;
    u64 accountId = 0;
    std::string serverIP;
    u16 serverPort = 0;
    u8 teamSlot = 0;           // Player's team slot (0-4 Radiant, 5-9 Dire)
    std::string heroName;       // Hero they were playing
    f32 gameTime = 0;           // How long the game has been running
    f32 disconnectTime = 0;     // How long they've been disconnected
    bool canReconnect = true;   // False if abandon timer expired
};

// ============ Disconnected Player Info ============

struct DisconnectedPlayer {
    u64 accountId = 0;
    u64 lobbyId = 0;
    u8 teamSlot = 0;
    std::string heroName;
    f32 disconnectTimestamp = 0;
    f32 abandonTimeout = 300.0f;  // 5 minutes to reconnect before abandon
    bool hasAbandoned = false;
};

} // namespace Matchmaking
} // namespace WorldEditor
