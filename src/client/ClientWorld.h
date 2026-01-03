#pragma once

#include "common/IGameWorld.h"
#include "common/GameInput.h"
#include "common/GameSnapshot.h"
#include "world/EntityManager.h"
#include "core/Types.h"

namespace WorldEditor {

// Client-side game world (prediction + interpolation)
class ClientWorld : public IClientWorld {
public:
    ClientWorld();
    ~ClientWorld() override;
    
    // IGameWorld interface
    void update(f32 deltaTime) override;
    Entity createEntity(const String& name = "Entity") override;
    void destroyEntity(Entity entity) override;
    bool isValid(Entity entity) const override;
    void clear() override;
    size_t getEntityCount() const override;
    bool isGameActive() const override;
    f32 getGameTime() const override;
    EntityManager& getEntityManager() override { return entityManager_; }
    const EntityManager& getEntityManager() const override { return entityManager_; }
    
    // Network ID mapping
    NetworkId getNetworkId(Entity entity) const override;
    Entity getEntityByNetworkId(NetworkId networkId) const override;
    
    // IClientWorld interface
    PlayerInput generateInput() override;
    void applySnapshot(const WorldSnapshot& snapshot) override;
    void predictLocalPlayer(f32 deltaTime) override;
    void reconcile(const WorldSnapshot& snapshot) override;
    void interpolateRemoteEntities(f32 deltaTime) override;
    void setLocalPlayer(Entity entity) override { localPlayer_ = entity; }
    Entity getLocalPlayer() const override { return localPlayer_; }
    
    // Component management (forwarded to EntityManager)
    template<typename Component, typename... Args>
    Component& addComponent(Entity entity, Args&&... args) {
        return entityManager_.addComponent<Component>(entity, std::forward<Args>(args)...);
    }
    
    template<typename Component>
    void removeComponent(Entity entity) {
        entityManager_.removeComponent<Component>(entity);
    }
    
    template<typename Component>
    bool hasComponent(Entity entity) const {
        return entityManager_.hasComponent<Component>(entity);
    }
    
    template<typename Component>
    Component& getComponent(Entity entity) {
        return entityManager_.getComponent<Component>(entity);
    }
    
    template<typename Component>
    const Component& getComponent(Entity entity) const {
        return entityManager_.getComponent<Component>(entity);
    }
    
    // Input management
    SequenceNumber getNextSequenceNumber() { return nextSequenceNumber_++; }
    const InputBuffer& getInputBuffer() const { return inputBuffer_; }
    
    // Snapshot buffer
    const SnapshotBuffer& getSnapshotBuffer() const { return snapshotBuffer_; }
    
    // Render time (for interpolation)
    f32 getRenderTime() const { return renderTime_; }
    
private:
    EntityManager entityManager_;
    
    // Network ID mapping
    Map<Entity, NetworkId> entityToNetworkId_;
    Map<NetworkId, Entity> networkIdToEntity_;
    
    // Local player
    Entity localPlayer_ = INVALID_ENTITY;
    
    // Input management
    SequenceNumber nextSequenceNumber_ = 1;
    InputBuffer inputBuffer_;
    
    // Snapshot management
    SnapshotBuffer snapshotBuffer_;
    SequenceNumber lastAcknowledgedInput_ = 0;
    
    // Timing
    f32 renderTime_ = 0.0f;
    f32 gameTime_ = 0.0f;
    
    // Game state (from server)
    bool gameActive_ = false;
    i32 currentWave_ = 0;
    f32 timeToNextWave_ = 0.0f;
    
    // Helper methods
    void createOrUpdateEntity(const EntitySnapshot& snapshot);
    void interpolateEntity(Entity entity, const EntitySnapshot& from, const EntitySnapshot& to, f32 t);
    NetworkId assignNetworkId(Entity entity, NetworkId networkId);
    void removeNetworkId(Entity entity);
};

} // namespace WorldEditor
