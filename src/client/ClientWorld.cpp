#include "ClientWorld.h"
#include "world/Components.h"

namespace WorldEditor {

ClientWorld::ClientWorld() {
    entityManager_.setWorld(nullptr);
}

ClientWorld::~ClientWorld() {
    clear();
}

void ClientWorld::update(f32 deltaTime) {
    if (!gameActive_) {
        return;
    }
    
    // Update render time for interpolation
    renderTime_ += deltaTime;
    
    // Interpolate remote entities
    interpolateRemoteEntities(deltaTime);
    
    // Predict local player (if controlled)
    if (localPlayer_ != INVALID_ENTITY && isValid(localPlayer_)) {
        predictLocalPlayer(deltaTime);
    }
}

Entity ClientWorld::createEntity(const String& name) {
    return entityManager_.createEntity(name);
}

void ClientWorld::destroyEntity(Entity entity) {
    removeNetworkId(entity);
    entityManager_.destroyEntity(entity);
}

bool ClientWorld::isValid(Entity entity) const {
    return entityManager_.isValid(entity);
}

void ClientWorld::clear() {
    entityManager_.clear();
    entityToNetworkId_.clear();
    networkIdToEntity_.clear();
    inputBuffer_.clear();
    snapshotBuffer_.clear();
    localPlayer_ = INVALID_ENTITY;
    nextSequenceNumber_ = 1;
    lastAcknowledgedInput_ = 0;
    renderTime_ = 0.0f;
    gameTime_ = 0.0f;
    gameActive_ = false;
}

size_t ClientWorld::getEntityCount() const {
    return entityManager_.getEntityCount();
}

bool ClientWorld::isGameActive() const {
    return gameActive_;
}

f32 ClientWorld::getGameTime() const {
    return gameTime_;
}

NetworkId ClientWorld::getNetworkId(Entity entity) const {
    auto it = entityToNetworkId_.find(entity);
    return (it != entityToNetworkId_.end()) ? it->second : INVALID_NETWORK_ID;
}

Entity ClientWorld::getEntityByNetworkId(NetworkId networkId) const {
    auto it = networkIdToEntity_.find(networkId);
    return (it != networkIdToEntity_.end()) ? it->second : INVALID_ENTITY;
}

NetworkId ClientWorld::assignNetworkId(Entity entity, NetworkId networkId) {
    entityToNetworkId_[entity] = networkId;
    networkIdToEntity_[networkId] = entity;
    return networkId;
}

void ClientWorld::removeNetworkId(Entity entity) {
    auto it = entityToNetworkId_.find(entity);
    if (it != entityToNetworkId_.end()) {
        networkIdToEntity_.erase(it->second);
        entityToNetworkId_.erase(it);
    }
}

PlayerInput ClientWorld::generateInput() {
    // TODO: Generate input from user input (keyboard/mouse)
    // This will be implemented when integrating with input system
    PlayerInput input;
    input.sequenceNumber = getNextSequenceNumber();
    input.timestamp = renderTime_;
    return input;
}

void ClientWorld::applySnapshot(const WorldSnapshot& snapshot) {
    // Store snapshot for interpolation
    snapshotBuffer_.addSnapshot(snapshot);
    
    // Update game state
    gameTime_ = snapshot.gameTime;
    gameActive_ = true;
    currentWave_ = snapshot.currentWave;
    timeToNextWave_ = snapshot.timeToNextWave;
    
    // Update acknowledged input for reconciliation
    lastAcknowledgedInput_ = snapshot.lastProcessedInput;
    
    // Create or update entities from snapshot
    for (const auto& entitySnap : snapshot.entities) {
        createOrUpdateEntity(entitySnap);
    }
    
    // Reconcile local player if needed
    if (lastAcknowledgedInput_ > 0) {
        reconcile(snapshot);
    }
}

void ClientWorld::createOrUpdateEntity(const EntitySnapshot& snapshot) {
    Entity entity = getEntityByNetworkId(snapshot.networkId);
    
    // Create entity if it doesn't exist
    if (entity == INVALID_ENTITY) {
        entity = createEntity("NetworkedEntity");
        assignNetworkId(entity, snapshot.networkId);
        
        // Add required components based on entity type
        addComponent<TransformComponent>(entity);
        
        if (snapshot.entityType == 2) { // Creep
            addComponent<CreepComponent>(entity);
        }
        
        if (snapshot.maxHealth > 0.0f) {
            addComponent<HealthComponent>(entity);
        }
    }
    
    // Update transform (will be interpolated later)
    if (hasComponent<TransformComponent>(entity)) {
        auto& transform = getComponent<TransformComponent>(entity);
        // Store server position for interpolation
        // Actual interpolation happens in interpolateRemoteEntities()
    }
    
    // Update health
    if (hasComponent<HealthComponent>(entity)) {
        auto& health = getComponent<HealthComponent>(entity);
        health.currentHealth = snapshot.health;
        health.maxHealth = snapshot.maxHealth;
    }
}

void ClientWorld::predictLocalPlayer(f32 deltaTime) {
    // TODO: Apply pending inputs to local player for prediction
    // This will be implemented when integrating with movement system
}

void ClientWorld::reconcile(const WorldSnapshot& snapshot) {
    // Remove acknowledged inputs from buffer
    inputBuffer_.removeInputsUpTo(lastAcknowledgedInput_);
    
    // TODO: Reconcile local player position with server snapshot
    // Re-apply unacknowledged inputs on top of server state
}

void ClientWorld::interpolateRemoteEntities(f32 deltaTime) {
    // Calculate interpolation time (render time - interpolation delay)
    const f32 interpTime = renderTime_ - NetworkConfig::INTERPOLATION_DELAY;
    
    // Get two snapshots for interpolation
    WorldSnapshot from, to;
    f32 t = 0.0f;
    
    if (!snapshotBuffer_.getInterpolationSnapshots(interpTime, from, to, t)) {
        return; // Not enough snapshots yet
    }
    
    // Interpolate all entities (except local player)
    for (const auto& entitySnap : from.entities) {
        Entity entity = getEntityByNetworkId(entitySnap.networkId);
        
        // Skip local player (it's predicted, not interpolated)
        if (entity == localPlayer_) {
            continue;
        }
        
        if (entity == INVALID_ENTITY || !isValid(entity)) {
            continue;
        }
        
        // Find corresponding snapshot in 'to'
        const EntitySnapshot* toSnap = to.findEntity(entitySnap.networkId);
        if (!toSnap) {
            continue;
        }
        
        // Interpolate entity
        interpolateEntity(entity, entitySnap, *toSnap, t);
    }
}

void ClientWorld::interpolateEntity(Entity entity, 
                                    const EntitySnapshot& from, 
                                    const EntitySnapshot& to, 
                                    f32 t) {
    if (!hasComponent<TransformComponent>(entity)) {
        return;
    }
    
    auto& transform = getComponent<TransformComponent>(entity);
    
    // Interpolate position
    transform.position = glm::mix(from.position, to.position, t);
    
    // Interpolate rotation (slerp for quaternions)
    transform.rotation = glm::slerp(from.rotation, to.rotation, t);
}

} // namespace WorldEditor
