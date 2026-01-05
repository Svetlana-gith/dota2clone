#pragma once
/**
 * Matchmaking Client - Connects to matchmaking coordinator
 * Handles queue, lobby, and game server assignment
 */

#include "MatchmakingTypes.h"
#include "MatchmakingProtocol.h"
#include "NetworkCommon.h"
#include <functional>
#include <memory>

namespace WorldEditor {
namespace Matchmaking {

// ============ Matchmaking Client Callbacks ============

using OnQueueConfirmedCallback = std::function<void()>;
using OnQueueUpdateCallback = std::function<void(const QueueStatus&)>;
using OnMatchFoundCallback = std::function<void(const LobbyInfo&)>;
using OnMatchAcceptStatusCallback = std::function<void(u16 requiredPlayers, const std::vector<u64>& playerIds, const std::vector<bool>& accepted)>;
using OnMatchReadyCallback = std::function<void(const std::string& serverIP, u16 port)>;
using OnMatchCancelledCallback = std::function<void(const std::string& reason, bool shouldRequeue)>;
using OnQueueRejectedCallback = std::function<void(const std::string& reason, bool authFailed, bool isBanned)>;
using OnErrorCallback = std::function<void(const std::string& error)>;
using OnActiveGameFoundCallback = std::function<void(const ActiveGameInfo& gameInfo)>;
using OnNoActiveGameCallback = std::function<void()>;
using OnReconnectApprovedCallback = std::function<void(const std::string& serverIP, u16 port, u8 teamSlot, const std::string& heroName)>;

// ============ Matchmaking Client ============

class MatchmakingClient {
public:
    MatchmakingClient();
    ~MatchmakingClient();
    
    // Connection
    bool connect(const char* coordinatorIP, u16 port = 27016);
    void disconnect();
    bool isConnected() const { return connected_; }
    
    // Queue management
    bool queueForMatch(const MatchPreferences& prefs);
    void cancelQueue();
    bool isInQueue() const { return inQueue_; }
    
    // Match acceptance
    void acceptMatch();
    void declineMatch();
    
    // Reconnect
    void checkForActiveGame(u64 accountId);
    void requestReconnect(u64 lobbyId);
    bool hasActiveGame() const { return hasActiveGame_; }
    const ActiveGameInfo& getActiveGameInfo() const { return activeGameInfo_; }
    
    // Update (call every frame)
    void update(f32 deltaTime);
    
    // Callbacks
    void setOnQueueConfirmed(OnQueueConfirmedCallback callback) { onQueueConfirmed_ = callback; }
    void setOnQueueUpdate(OnQueueUpdateCallback callback) { onQueueUpdate_ = callback; }
    void setOnMatchFound(OnMatchFoundCallback callback) { onMatchFound_ = callback; }
    void setOnMatchAcceptStatus(OnMatchAcceptStatusCallback callback) { onMatchAcceptStatus_ = callback; }
    void setOnMatchReady(OnMatchReadyCallback callback) { onMatchReady_ = callback; }
    void setOnMatchCancelled(OnMatchCancelledCallback callback) { onMatchCancelled_ = callback; }
    void setOnQueueRejected(OnQueueRejectedCallback callback) { onQueueRejected_ = callback; }
    void setOnError(OnErrorCallback callback) { onError_ = callback; }
    void setOnActiveGameFound(OnActiveGameFoundCallback callback) { onActiveGameFound_ = callback; }
    void setOnNoActiveGame(OnNoActiveGameCallback callback) { onNoActiveGame_ = callback; }
    void setOnReconnectApproved(OnReconnectApprovedCallback callback) { onReconnectApproved_ = callback; }
    
    // Status
    const QueueStatus& getQueueStatus() const { return queueStatus_; }
    const LobbyInfo& getCurrentLobby() const { return currentLobby_; }
    const PlayerInfo& getPlayerInfo() const { return playerInfo_; }
    void setPlayerInfo(const PlayerInfo& info) { playerInfo_ = info; }
    const std::vector<u64>& getAcceptPlayerIds() const { return acceptPlayerIds_; }
    const std::vector<bool>& getAcceptStates() const { return acceptStates_; }
    u16 getAcceptTimeoutSeconds() const { return acceptTimeoutSeconds_; }
    f32 getAcceptTimeRemainingSeconds() const { return std::max(0.0f, (f32)acceptTimeoutSeconds_ - acceptElapsedSeconds_); }
    
    // Authentication
    void setSessionToken(const std::string& token) { sessionToken_ = token; }
    const std::string& getSessionToken() const { return sessionToken_; }
    bool hasSessionToken() const { return !sessionToken_.empty(); }
    
private:
    void sendPacket(MatchmakingMessageType type, u64 lobbyId, const void* payload, u32 payloadSize);
    void handlePacket(MatchmakingMessageType type, u64 lobbyId, const void* payload, u32 payloadSize);
    void sendHeartbeat();
    
    // Network
    Network::UDPSocket socket_;
    bool connected_ = false;
    std::string coordinatorIP_;
    u16 coordinatorPort_ = 0;
    
    // State
    bool inQueue_ = false;
    PlayerInfo playerInfo_;
    QueueStatus queueStatus_;
    LobbyInfo currentLobby_;
    MatchPreferences currentPreferences_;
    std::string sessionToken_;  // Auth session token
    
    // Timing
    f32 heartbeatTimer_ = 0;
    f32 heartbeatInterval_ = 5.0f;
    
    // Callbacks
    OnQueueConfirmedCallback onQueueConfirmed_;
    OnQueueUpdateCallback onQueueUpdate_;
    OnMatchFoundCallback onMatchFound_;
    OnMatchAcceptStatusCallback onMatchAcceptStatus_;
    OnMatchReadyCallback onMatchReady_;
    OnMatchCancelledCallback onMatchCancelled_;
    OnQueueRejectedCallback onQueueRejected_;
    OnErrorCallback onError_;
    OnActiveGameFoundCallback onActiveGameFound_;
    OnNoActiveGameCallback onNoActiveGame_;
    OnReconnectApprovedCallback onReconnectApproved_;

    // Accept status state (Dota-like accept phase)
    std::vector<u64> acceptPlayerIds_;
    std::vector<bool> acceptStates_;
    u16 acceptTimeoutSeconds_ = 0;
    f32 acceptElapsedSeconds_ = 0.0f;
    
    // Reconnect state
    bool hasActiveGame_ = false;
    ActiveGameInfo activeGameInfo_;
};

} // namespace Matchmaking
} // namespace WorldEditor
