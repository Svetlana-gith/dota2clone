#pragma once

#include "server/ServerWorld.h"

namespace WorldEditor {

// Adapter class to maintain backward compatibility with existing editor code
// This wraps ServerWorld and provides the old World interface
class World {
public:
    World();
#ifdef DIRECTX_RENDERER
    World(ID3D12Device* device);
#endif
    ~World();
    
    // Entity management (forwarded to ServerWorld)
    Entity createEntity(const String& name = "Entity") {
        return serverWorld_->createEntity(name);
    }
    
    void destroyEntity(Entity entity) {
        serverWorld_->destroyEntity(entity);
    }
    
    bool isValid(Entity entity) const {
        return serverWorld_->isValid(entity);
    }
    
    // Component management
    template<typename Component, typename... Args>
    Component& addComponent(Entity entity, Args&&... args) {
        return serverWorld_->addComponent<Component>(entity, std::forward<Args>(args)...);
    }
    
    template<typename Component>
    void removeComponent(Entity entity) {
        serverWorld_->removeComponent<Component>(entity);
    }
    
    template<typename Component>
    bool hasComponent(Entity entity) const {
        return serverWorld_->hasComponent<Component>(entity);
    }
    
    template<typename Component>
    Component& getComponent(Entity entity) {
        return serverWorld_->getComponent<Component>(entity);
    }
    
    template<typename Component>
    const Component& getComponent(Entity entity) const {
        return serverWorld_->getComponent<Component>(entity);
    }
    
    // System management
    void addSystem(UniquePtr<System> system) {
        serverWorld_->addSystem(std::move(system));
    }
    
    void removeSystem(const String& name) {
        serverWorld_->removeSystem(name);
    }
    
    System* getSystem(const String& name) {
        return serverWorld_->getSystem(name);
    }
    
    const System* getSystem(const String& name) const {
        return serverWorld_->getSystem(name);
    }
    
    // Update (with optional game mode flag)
    void update(f32 deltaTime, bool gameModeActive = false) {
        if (gameModeActive && !serverWorld_->isGameActive()) {
            serverWorld_->startGame();
        }
        serverWorld_->update(deltaTime);
    }
    
    // Rendering
#ifdef DIRECTX_RENDERER
    void render(ID3D12GraphicsCommandList* commandList,
                const Mat4& viewProjMatrix,
                const Vec3& cameraPosition,
                bool showPathLines = true) {
        serverWorld_->render(commandList, viewProjMatrix, cameraPosition, showPathLines);
    }
#endif
    
    // World state
    void clear() {
        serverWorld_->clear();
    }
    
    void clearEntities() {
        serverWorld_->clear();
    }
    
    size_t getEntityCount() const {
        return serverWorld_->getEntityCount();
    }
    
    EntityManager& getEntityManager() {
        return serverWorld_->getEntityManager();
    }
    
    const EntityManager& getEntityManager() const {
        return serverWorld_->getEntityManager();
    }
    
    // MOBA Game Management
    void startGame() {
        serverWorld_->startGame();
    }
    
    // Set game active without creating default heroes (for multiplayer clients)
    void setGameActive(bool active) {
        serverWorld_->setGameActive(active);
    }
    
    void pauseGame() {
        serverWorld_->pauseGame();
    }
    
    void resetGame() {
        serverWorld_->resetGame();
    }
    
    bool isGameActive() const {
        return serverWorld_->isGameActive();
    }
    
    f32 getGameTime() const {
        return serverWorld_->getGameTime();
    }
    
    i32 getCurrentWave() const {
        // TODO: Expose from ServerWorld
        return 0;
    }
    
    f32 getTimeToNextWave() const {
        // TODO: Expose from ServerWorld
        return 0.0f;
    }
    
    // Access to underlying ServerWorld (for advanced usage)
    ServerWorld* getServerWorld() { return serverWorld_.get(); }
    const ServerWorld* getServerWorld() const { return serverWorld_.get(); }
    
private:
    UniquePtr<ServerWorld> serverWorld_;
};

} // namespace WorldEditor
