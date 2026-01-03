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
    
    // Matchmaking Server -> Client
    QueueConfirm,           // Queue request accepted
    QueueUpdate,            // Update on search progress
    MatchFound,             // Match found, waiting for accept
    MatchAcceptStatus,      // Accept status update (who accepted)
    MatchReady,             // All players accepted, here's the server
    MatchCancelled,         // Match cancelled (someone declined)
    QueueRejected,          // Queue request rejected (auth failed, banned, etc.)
    
    // Status
    Heartbeat,
    Error,

    // DedicatedServer -> Coordinator (server pool)
    ServerRegister = 100,
    ServerHeartbeat = 101,

    // Coordinator -> DedicatedServer
    AssignLobby = 102
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

} // namespace Matchmaking
} // namespace WorldEditor
