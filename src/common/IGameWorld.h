#pragma once

#include "core/Types.h"
#include "NetworkTypes.h"
#include "GameInput.h"
#include "GameSnapshot.h"

namespace WorldEditor {

// Forward declarations
class EntityManager;

// Abstract game world interface (implemented by ServerWorld and ClientWorld)
class IGameWorld {
public:
    virtual ~IGameWorld() = default;
    
    // Core update loop
    virtual void update(f32 deltaTime) = 0;
    
    // Entity management
    virtual Entity createEntity(const String& name = "Entity") = 0;
    virtual void destroyEntity(Entity entity) = 0;
    virtual bool isValid(Entity entity) const = 0;
    
    // World state
    virtual void clear() = 0;
    virtual size_t getEntityCount() const = 0;
    
    // Game state
    virtual bool isGameActive() const = 0;
    virtual f32 getGameTime() const = 0;
    
    // Entity manager access
    virtual EntityManager& getEntityManager() = 0;
    virtual const EntityManager& getEntityManager() const = 0;
    
    // Network ID mapping (for client/server sync)
    virtual NetworkId getNetworkId(Entity entity) const = 0;
    virtual Entity getEntityByNetworkId(NetworkId networkId) const = 0;
};

// Server-side game world (authoritative simulation)
class IServerWorld : public IGameWorld {
public:
    virtual ~IServerWorld() = default;
    
    // Input processing (from clients)
    virtual void processInput(ClientId clientId, const PlayerInput& input) = 0;
    
    // Snapshot generation (for clients)
    virtual WorldSnapshot createSnapshot() const = 0;
    
    // Game management
    virtual void startGame() = 0;
    virtual void pauseGame() = 0;
    virtual void resetGame() = 0;
    
    // Client management
    virtual void addClient(ClientId clientId) = 0;
    virtual void removeClient(ClientId clientId) = 0;
    virtual Entity getClientControlledEntity(ClientId clientId) const = 0;
};

// Client-side game world (prediction + interpolation)
class IClientWorld : public IGameWorld {
public:
    virtual ~IClientWorld() = default;
    
    // Input generation (local player)
    virtual PlayerInput generateInput() = 0;
    
    // Snapshot application (from server)
    virtual void applySnapshot(const WorldSnapshot& snapshot) = 0;
    
    // Prediction & reconciliation
    virtual void predictLocalPlayer(f32 deltaTime) = 0;
    virtual void reconcile(const WorldSnapshot& snapshot) = 0;
    
    // Interpolation (remote entities)
    virtual void interpolateRemoteEntities(f32 deltaTime) = 0;
    
    // Local player
    virtual void setLocalPlayer(Entity entity) = 0;
    virtual Entity getLocalPlayer() const = 0;
};

} // namespace WorldEditor
