#pragma once

#include "common/IGameWorld.h"
#include "world/EntityManager.h"
#include "world/System.h"
#include "core/Types.h"

#ifdef DIRECTX_RENDERER
#include <d3d12.h>
#endif

namespace WorldEditor {

// Server-side authoritative game world
class ServerWorld : public IServerWorld {
public:
    ServerWorld();
#ifdef DIRECTX_RENDERER
    ServerWorld(ID3D12Device* device);
#endif
    ~ServerWorld() override;
    
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
    
    // IServerWorld interface
    void processInput(ClientId clientId, const PlayerInput& input) override;
    WorldSnapshot createSnapshot() const override;
    void startGame() override;
    void pauseGame() override;
    void resetGame() override;
    void addClient(ClientId clientId) override;
    void removeClient(ClientId clientId) override;
    Entity getClientControlledEntity(ClientId clientId) const override;
    
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
    
    // System management
    void addSystem(UniquePtr<System> system);
    void removeSystem(const String& name);
    System* getSystem(const String& name);
    const System* getSystem(const String& name) const;
    
    // Rendering (for editor/standalone server with visualization)
#ifdef DIRECTX_RENDERER
    void render(ID3D12GraphicsCommandList* commandList,
                const Mat4& viewProjMatrix,
                const Vec3& cameraPosition,
                bool showPathLines = true);
#endif
    
    // Tick management
    TickNumber getCurrentTick() const { return currentTick_; }
    void setTickRate(u32 tickRate) { tickRate_ = tickRate; }
    
private:
    EntityManager entityManager_;
    Map<String, UniquePtr<System>> systems_;
    
    // Network ID mapping
    Map<Entity, NetworkId> entityToNetworkId_;
    Map<NetworkId, Entity> networkIdToEntity_;
    NetworkId nextNetworkId_ = 1;
    
    // Client management
    Map<ClientId, Entity> clientToEntity_;  // Client -> controlled hero
    
    // Simulation state
    TickNumber currentTick_ = 0;
    u32 tickRate_ = NetworkConfig::SERVER_TICK_RATE;
    f32 tickAccumulator_ = 0.0f;
    
    // Game state
    bool gameActive_ = false;
    bool gamePaused_ = false;
    f32 gameTime_ = 0.0f;
    i32 currentWave_ = 0;
    f32 timeToNextWave_ = 30.0f;
    
#ifdef DIRECTX_RENDERER
    ID3D12Device* device_ = nullptr;
#endif
    
    // Helper methods
    NetworkId assignNetworkId(Entity entity);
    void removeNetworkId(Entity entity);
    void updateSystems(f32 deltaTime);
    void updateGameState(f32 deltaTime);
    EntitySnapshot createEntitySnapshot(Entity entity) const;
};

} // namespace WorldEditor
