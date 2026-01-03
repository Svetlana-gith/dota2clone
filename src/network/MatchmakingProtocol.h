#pragma once
/**
 * MatchmakingProtocol - binary wire protocol for MM coordinator
 *
 * Goal: deterministic, small UDP messages (no JSON) shared between:
 * - Game clients (MatchmakingClient)
 * - MatchmakingCoordinator.exe
 * - DedicatedServer.exe (server pool registration/heartbeat)
 */
#
// Keep this header lightweight: no Windows.h, no winsock includes.

#include "../core/Types.h"
#include "MatchmakingTypes.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace WorldEditor::Matchmaking::Wire {

static constexpr u32 kMagic = 0x4D4D5031; // 'MMP1'
static constexpr u16 kVersion = 1;
static constexpr u16 kCoordinatorPort = 27016;

// IPv6 string max is 45 chars + null.
static constexpr size_t kIpStringMax = 46;
static constexpr size_t kRegionMax = 16;
static constexpr size_t kErrorMax = 128;
static constexpr size_t kMaxLobbyPlayers = 10;
static constexpr size_t kSessionTokenMax = 65;  // 64 hex chars + null terminator

#pragma pack(push, 1)
struct MMHeader {
    u32 magic = kMagic;
    u16 version = kVersion;
    u16 type = 0;          // MatchmakingMessageType as u16
    u32 payloadSize = 0;
    u64 playerId = 0;
    u64 lobbyId = 0;
};
#pragma pack(pop)

static_assert(sizeof(MMHeader) == 28, "MMHeader size must be stable");

// ---- Payloads (fixed-size, POD) ----

#pragma pack(push, 1)
struct QueueRequestPayload {
    u8 mode = 0; // MatchMode as u8
    char region[kRegionMax]{};
    char sessionToken[kSessionTokenMax]{};  // Auth session token for validation
};

struct QueueUpdatePayload {
    u32 playersInQueue = 0;
    f32 estimatedWaitTime = 0.0f;
    f32 searchTime = 0.0f;
    char region[kRegionMax]{};
};

struct MatchFoundPayload {
    u16 requiredPlayers = 2;        // dev-mode default
    u16 acceptTimeoutSeconds = 20;  // Dota-like accept timer
};

struct MatchAcceptStatusPayload {
    u16 playerCount = 0;
    u16 requiredPlayers = 0;
    u64 playerIds[kMaxLobbyPlayers]{};
    u8 accepted[kMaxLobbyPlayers]{}; // 0 = not accepted, 1 = accepted
};

struct MatchReadyPayload {
    char serverIp[kIpStringMax]{};
    u16 serverPort = 0;
    u16 _reserved = 0;
};

// Server pool (DedicatedServer <-> Coordinator)
struct ServerRegisterPayload {
    u64 serverId = 0;
    char serverIp[kIpStringMax]{};
    u16 gamePort = 0;
    u16 controlPort = 0;
    u16 capacity = 0;
    u16 _reserved = 0;
};

struct ServerHeartbeatPayload {
    u64 serverId = 0;
    u16 currentPlayers = 0;
    u16 capacity = 0;
    f32 uptimeSeconds = 0.0f;
};

struct AssignLobbyPayload {
    u64 serverId = 0;
    u64 lobbyId = 0;
    u16 expectedPlayers = 0;
    u16 _reserved = 0;
};

struct ErrorPayload {
    char message[kErrorMax]{};
};

struct MatchCancelledPayload {
    char reason[kErrorMax]{};
    u64 declinedByPlayerId = 0;  // 0 = timeout, non-zero = specific player declined
    u8 shouldRequeue = 0;        // 1 = this player should auto-requeue, 0 = don't requeue
    u8 _reserved[7]{};
};

// Auth validation result (internal use by coordinator)
struct AuthValidationResult {
    u8 valid = 0;           // 1 = valid, 0 = invalid
    u64 accountId = 0;      // Account ID if valid
    u8 isBanned = 0;        // 1 = banned, 0 = not banned
    char errorMessage[kErrorMax]{};
};

// Queue rejection payload (sent to client when auth fails)
struct QueueRejectedPayload {
    char reason[kErrorMax]{};
    u8 authFailed = 0;      // 1 = auth failure, 0 = other reason
    u8 isBanned = 0;        // 1 = account is banned
    u8 _reserved[6]{};
};
#pragma pack(pop)

// ---- Helpers ----

// Build a packet: [MMHeader][payload bytes...]
bool BuildPacket(std::vector<u8>& out,
                 MatchmakingMessageType type,
                 u64 playerId,
                 u64 lobbyId,
                 const void* payload,
                 u32 payloadSize);

// Parse a packet and return pointer into the original buffer for payload.
bool ParsePacket(const void* data,
                 size_t size,
                 MMHeader& outHeader,
                 const void*& outPayload,
                 u32& outPayloadSize);

// Safe string copy into fixed char array (always null-terminated).
void CopyCString(char* dst, size_t dstSize, const std::string& src);

} // namespace WorldEditor::Matchmaking::Wire

