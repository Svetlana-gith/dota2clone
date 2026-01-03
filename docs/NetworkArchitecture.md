# Network Architecture

## 🏗️ Overview

Наша MOBA использует **Client-Server архитектуру** с **authoritative server** моделью.

```
┌─────────────────────────────────────────────────────────────┐
│                     GAME CLIENT (Game.exe)                  │
├─────────────────────────────────────────────────────────────┤
│  GameState System                                           │
│    ├─ MainMenuState                                         │
│    ├─ LoadingState                                          │
│    └─ InGameState                                           │
│         ├─ ClientWorld (Prediction + Interpolation)        │
│         └─ NetworkClient (UDP Socket)                      │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       │ UDP Packets
                       │ ↑ PlayerInput (30 Hz)
                       │ ↓ WorldSnapshot (30 Hz)
                       │
┌──────────────────────┴──────────────────────────────────────┐
│           DEDICATED SERVER (DedicatedServer.exe)            │
├─────────────────────────────────────────────────────────────┤
│  NetworkServer (UDP Socket, Client Management)              │
│       ↓                                                      │
│  ServerWorld (Authoritative Game Simulation)                │
│       ├─ HeroSystem                                         │
│       ├─ CreepSystem                                        │
│       ├─ TowerSystem                                        │
│       ├─ CombatSystem                                       │
│       └─ ... other systems                                  │
└─────────────────────────────────────────────────────────────┘
```

## 📦 Components

### 1. Network Layer (`src/network/`)

#### NetworkCommon.h
- Packet types and headers
- UDP socket wrapper
- Network address handling
- Winsock2 initialization

#### NetworkServer
- UDP server socket
- Client connection management
- Packet routing
- Snapshot broadcasting

#### NetworkClient
- UDP client socket
- Connection state machine
- Input sending
- Snapshot receiving

### 2. Server (`src/server/`)

#### ServerWorld
- **Authoritative game simulation**
- Runs at fixed 30 Hz tick rate
- Processes client inputs
- Generates world snapshots
- Manages game state (heroes, creeps, towers)

#### DedicatedServer.exe
- Standalone server executable
- No rendering, headless
- Console application
- Can run on Linux/Windows server

### 3. Client (`src/client/`)

#### ClientWorld
- **Client-side prediction**
- **Server reconciliation**
- **Entity interpolation**
- Rendering and effects

### 4. Common (`src/common/`)

#### NetworkTypes.h
- Shared types (ClientId, NetworkId, TickNumber)
- Network configuration constants

#### GameSnapshot.h
- WorldSnapshot structure
- EntitySnapshot structure
- Snapshot buffer for interpolation

#### GameInput.h
- PlayerInput structure
- Input commands (Move, Attack, Ability, etc.)
- Input buffer for prediction

## 🔄 Network Flow

### Client → Server (Input)

```cpp
// Client sends input every frame
PlayerInput input;
input.commandType = InputCommandType::Move;
input.targetPosition = clickPosition;
input.sequenceNumber = nextSequence++;

networkClient->sendInput(input);
```

**Packet Structure:**
```
[PacketHeader] [PlayerInput]
  - type: ClientInput
  - sequence: 1234
  - payloadSize: sizeof(PlayerInput)
```

### Server → Client (Snapshot)

```cpp
// Server sends snapshot every tick (30 Hz)
WorldSnapshot snapshot = serverWorld->createSnapshot();
networkServer->sendSnapshotToAll(snapshot);
```

**Packet Structure:**
```
[PacketHeader] [WorldSnapshot]
  - type: WorldSnapshot
  - sequence: 5678
  - entities: [EntitySnapshot, EntitySnapshot, ...]
  - gameTime: 123.45
  - currentWave: 3
```

## 🎮 Client-Side Prediction

Клиент **предсказывает** результат своих действий локально, не дожидаясь ответа сервера:

```cpp
// 1. Client sends input
networkClient->sendInput(input);

// 2. Client immediately applies input locally (prediction)
clientWorld->applyInput(input);

// 3. Server processes input and sends snapshot
// 4. Client receives snapshot and reconciles
if (snapshot.lastProcessedInput < lastSentInput) {
    // Re-simulate unacknowledged inputs
    clientWorld->reconcile(snapshot);
}
```

## 🔄 Entity Interpolation

Клиент **интерполирует** позиции других сущностей между снапшотами:

```cpp
// Render time is slightly behind server time
f32 renderTime = serverTime - INTERPOLATION_DELAY;

// Find two snapshots that bracket render time
WorldSnapshot from, to;
f32 t;
if (snapshotBuffer.getInterpolationSnapshots(renderTime, from, to, t)) {
    // Interpolate entity positions
    Vec3 position = lerp(from.position, to.position, t);
}
```

## 🔐 Security

### Authoritative Server
- Сервер **всегда** имеет финальное слово
- Клиент не может:
  - Изменить HP
  - Телепортироваться
  - Использовать способности без cooldown
  - Видеть сквозь Fog of War

### Input Validation
```cpp
void ServerWorld::processInput(ClientId clientId, const PlayerInput& input) {
    Entity hero = getClientHero(clientId);
    
    // Validate input
    if (!isValidMovePosition(input.targetPosition)) {
        return; // Ignore invalid input
    }
    
    if (input.abilityIndex >= 0) {
        if (!canCastAbility(hero, input.abilityIndex)) {
            return; // Ability on cooldown or not enough mana
        }
    }
    
    // Apply validated input
    applyInput(hero, input);
}
```

## 📊 Network Stats

### Bandwidth Usage

**Per Client:**
- **Upstream (Client → Server):** ~5 KB/s
  - PlayerInput: 128 bytes × 30 Hz = 3.84 KB/s
  - Ping/Pong: ~100 bytes/s
  
- **Downstream (Server → Client):** ~30 KB/s
  - WorldSnapshot: ~1000 bytes × 30 Hz = 30 KB/s
  - (depends on entity count)

**Server (10 clients):**
- **Upstream:** 300 KB/s (snapshots to all clients)
- **Downstream:** 50 KB/s (inputs from all clients)

### Latency Handling

- **Interpolation Delay:** 100ms (3 snapshots buffer)
- **Prediction:** Instant local response
- **Reconciliation:** Smooth correction on mismatch

## 🚀 Usage

### Starting Dedicated Server

```bash
# Default port (27015)
DedicatedServer.exe

# Custom port
DedicatedServer.exe 7777
```

### Connecting Client

```cpp
// In LoadingState or MainMenuState
NetworkClient client;
if (client.connect("127.0.0.1", 27015)) {
    LOG_INFO("Connected to server!");
}
```

## 🔧 Configuration

### Network Config (`src/common/NetworkTypes.h`)

```cpp
namespace NetworkConfig {
    constexpr u32 SERVER_TICK_RATE = 30;        // Server Hz
    constexpr u32 CLIENT_TICK_RATE = 60;        // Client Hz
    constexpr f32 INTERPOLATION_DELAY = 0.1f;   // 100ms buffer
    constexpr u32 INPUT_BUFFER_SIZE = 128;
    constexpr u32 SNAPSHOT_BUFFER_SIZE = 64;
}
```

### Server Config (`src/network/NetworkCommon.h`)

```cpp
constexpr u16 DEFAULT_SERVER_PORT = 27015;
constexpr u32 MAX_PACKET_SIZE = 1400;  // MTU-safe
constexpr u32 MAX_CLIENTS = 10;
constexpr f32 CLIENT_TIMEOUT = 10.0f;
```

## 📝 Next Steps

### Phase 1: Basic Networking ✅
- [x] UDP socket wrapper
- [x] NetworkServer
- [x] NetworkClient
- [x] DedicatedServer executable
- [x] Packet serialization

### Phase 2: Integration
- [ ] Integrate NetworkClient into InGameState
- [ ] Implement client-side prediction in ClientWorld
- [ ] Implement entity interpolation
- [ ] Add connection UI (connecting, disconnected, lag)

### Phase 3: Optimization
- [ ] Delta compression (only send changed entities)
- [ ] Packet aggregation (multiple inputs per packet)
- [ ] Reliable packets (for important events)
- [ ] Lag compensation (server-side rewind)

### Phase 4: Advanced Features
- [ ] Reconnection support
- [ ] Spectator mode
- [ ] Replay recording
- [ ] Anti-cheat (input validation, rate limiting)

## 🐛 Debugging

### Network Logging

```cpp
// Enable verbose network logging
#define NETWORK_DEBUG 1

// In NetworkServer.cpp / NetworkClient.cpp
LOG_DEBUG("Sent packet: type={}, seq={}, size={}", 
          (int)header.type, header.sequence, packetSize);
```

### Packet Capture

Use **Wireshark** to inspect UDP packets:
- Filter: `udp.port == 27015`
- Look for packet loss, duplicates, reordering

### Simulating Network Conditions

```cpp
// Add artificial latency/packet loss (for testing)
class NetworkSimulator {
    void sendPacket(const Packet& packet) {
        if (rand() % 100 < packetLossPercent) {
            return; // Drop packet
        }
        
        f32 delay = latency + randomJitter();
        delayedPackets.push({packet, delay});
    }
};
```

---

**Архитектура готова к интеграции!** 🎉
