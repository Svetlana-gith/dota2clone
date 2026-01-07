#pragma once

#include "core/Types.h"
#include "NetworkTypes.h"

namespace WorldEditor {

// Entity state snapshot (replicated from server to clients)
struct EntitySnapshot {
    NetworkId networkId = INVALID_NETWORK_ID;
    TickNumber tick = 0;
    
    // Transform
    Vec3 position = Vec3(0.0f);
    Vec3 velocity = Vec3(0.0f);
    Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    
    // Combat state
    f32 health = 0.0f;
    f32 maxHealth = 0.0f;
    f32 mana = 0.0f;
    f32 maxMana = 0.0f;
    
    // State flags
    u32 stateFlags = 0;  // Bitfield for various states
    
    // Team
    TeamId teamId = TEAM_NEUTRAL;
    
    // Entity type (for client-side rendering)
    u8 entityType = 0;  // 0=Unknown, 1=Hero, 2=Creep, 3=Tower, etc.
    
    // Owner client ID (for heroes - identifies which client controls this entity)
    ClientId ownerClientId = INVALID_CLIENT_ID;
    
    EntitySnapshot() = default;
};

// Serialized snapshot header (fixed size, sent over network)
struct SnapshotHeader {
    TickNumber tick = 0;
    f32 serverTime = 0.0f;
    f32 gameTime = 0.0f;
    i32 currentWave = 0;
    f32 timeToNextWave = 0.0f;
    SequenceNumber lastProcessedInput = 0;
    u16 entityCount = 0;  // Number of EntitySnapshot following this header
};

// Maximum entities per snapshot packet (to fit in UDP packet)
constexpr u32 MAX_ENTITIES_PER_SNAPSHOT = 16;

// World snapshot (sent from server to client each tick)
struct WorldSnapshot {
    TickNumber tick = 0;
    f32 serverTime = 0.0f;
    
    // Entity snapshots
    Vector<EntitySnapshot> entities;
    
    // Game state
    f32 gameTime = 0.0f;
    i32 currentWave = 0;
    f32 timeToNextWave = 0.0f;
    
    // Acknowledged input (for reconciliation)
    SequenceNumber lastProcessedInput = 0;
    
    WorldSnapshot() = default;
    
    void clear() {
        entities.clear();
    }
    
    const EntitySnapshot* findEntity(NetworkId id) const {
        for (const auto& entity : entities) {
            if (entity.networkId == id) {
                return &entity;
            }
        }
        return nullptr;
    }
    
    // Serialize to buffer for network transmission
    // Returns number of bytes written, or 0 on failure
    size_t serialize(u8* buffer, size_t bufferSize) const {
        size_t entityCount = std::min(entities.size(), (size_t)MAX_ENTITIES_PER_SNAPSHOT);
        size_t requiredSize = sizeof(SnapshotHeader) + entityCount * sizeof(EntitySnapshot);
        
        if (bufferSize < requiredSize) {
            return 0;
        }
        
        // Write header
        SnapshotHeader header;
        header.tick = tick;
        header.serverTime = serverTime;
        header.gameTime = gameTime;
        header.currentWave = currentWave;
        header.timeToNextWave = timeToNextWave;
        header.lastProcessedInput = lastProcessedInput;
        header.entityCount = static_cast<u16>(entityCount);
        
        memcpy(buffer, &header, sizeof(SnapshotHeader));
        
        // Write entities
        u8* entityPtr = buffer + sizeof(SnapshotHeader);
        for (size_t i = 0; i < entityCount; ++i) {
            memcpy(entityPtr, &entities[i], sizeof(EntitySnapshot));
            entityPtr += sizeof(EntitySnapshot);
        }
        
        return requiredSize;
    }
    
    // Deserialize from buffer
    // Returns true on success
    bool deserialize(const u8* buffer, size_t bufferSize) {
        if (bufferSize < sizeof(SnapshotHeader)) {
            return false;
        }
        
        // Read header
        SnapshotHeader header;
        memcpy(&header, buffer, sizeof(SnapshotHeader));
        
        size_t requiredSize = sizeof(SnapshotHeader) + header.entityCount * sizeof(EntitySnapshot);
        if (bufferSize < requiredSize) {
            return false;
        }
        
        tick = header.tick;
        serverTime = header.serverTime;
        gameTime = header.gameTime;
        currentWave = header.currentWave;
        timeToNextWave = header.timeToNextWave;
        lastProcessedInput = header.lastProcessedInput;
        
        // Read entities
        entities.clear();
        entities.resize(header.entityCount);
        
        const u8* entityPtr = buffer + sizeof(SnapshotHeader);
        for (u16 i = 0; i < header.entityCount; ++i) {
            memcpy(&entities[i], entityPtr, sizeof(EntitySnapshot));
            entityPtr += sizeof(EntitySnapshot);
        }
        
        return true;
    }
};

// Snapshot buffer for interpolation
class SnapshotBuffer {
public:
    void addSnapshot(const WorldSnapshot& snapshot) {
        snapshots_.push_back(snapshot);
        if (snapshots_.size() > NetworkConfig::SNAPSHOT_BUFFER_SIZE) {
            snapshots_.erase(snapshots_.begin());
        }
    }
    
    // Get two snapshots for interpolation at given time
    bool getInterpolationSnapshots(f32 renderTime, 
                                   WorldSnapshot& from, 
                                   WorldSnapshot& to,
                                   f32& t) const {
        if (snapshots_.size() < 2) {
            return false;
        }
        
        // Find snapshots that bracket the render time
        for (size_t i = 0; i < snapshots_.size() - 1; ++i) {
            const auto& snap1 = snapshots_[i];
            const auto& snap2 = snapshots_[i + 1];
            
            if (snap1.serverTime <= renderTime && renderTime <= snap2.serverTime) {
                from = snap1;
                to = snap2;
                
                f32 duration = snap2.serverTime - snap1.serverTime;
                if (duration > 0.0001f) {
                    t = (renderTime - snap1.serverTime) / duration;
                } else {
                    t = 0.0f;
                }
                return true;
            }
        }
        
        return false;
    }
    
    const WorldSnapshot* getLatestSnapshot() const {
        return snapshots_.empty() ? nullptr : &snapshots_.back();
    }
    
    void clear() { snapshots_.clear(); }
    
private:
    Vector<WorldSnapshot> snapshots_;
};

} // namespace WorldEditor
