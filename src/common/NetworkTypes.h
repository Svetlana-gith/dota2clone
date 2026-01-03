#pragma once

#include "core/Types.h"

namespace WorldEditor {

// Network entity ID (globally unique across client/server)
using NetworkId = u32;
constexpr NetworkId INVALID_NETWORK_ID = 0;

// Client ID for player identification
using ClientId = u32;
constexpr ClientId INVALID_CLIENT_ID = 0;

// Tick number for deterministic simulation
using TickNumber = u32;

// Sequence number for input/packets
using SequenceNumber = u32;

// Team ID
using TeamId = i32;
constexpr TeamId TEAM_NEUTRAL = 0;
constexpr TeamId TEAM_RADIANT = 1;
constexpr TeamId TEAM_DIRE = 2;

// Network configuration
namespace NetworkConfig {
    constexpr u32 SERVER_TICK_RATE = 30;        // Server updates per second
    constexpr f32 SERVER_TICK_INTERVAL = 1.0f / SERVER_TICK_RATE;
    
    constexpr u32 CLIENT_TICK_RATE = 60;        // Client updates per second
    constexpr f32 CLIENT_TICK_INTERVAL = 1.0f / CLIENT_TICK_RATE;
    
    constexpr f32 INTERPOLATION_DELAY = 0.1f;   // 100ms interpolation buffer
    constexpr u32 INPUT_BUFFER_SIZE = 128;      // Max buffered inputs
    constexpr u32 SNAPSHOT_BUFFER_SIZE = 64;    // Max buffered snapshots
}

} // namespace WorldEditor
