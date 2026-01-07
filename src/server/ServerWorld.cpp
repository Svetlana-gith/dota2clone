#include "ServerWorld.h"
#include "world/Components.h"
#include "world/System.h"
#include "world/WorldLegacy.h"
#include "world/HeroSystem.h"
#include "world/CreepSpawnSystem.h"
#include <algorithm>

namespace WorldEditor {

ServerWorld::ServerWorld() {
    entityManager_.setWorld(nullptr); // Will be set properly when needed
}

#ifdef DIRECTX_RENDERER
ServerWorld::ServerWorld(ID3D12Device* device) : device_(device) {
    entityManager_.setWorld(nullptr);
}
#endif

ServerWorld::~ServerWorld() {
    clear();
}

void ServerWorld::update(f32 deltaTime) {
    if (!gameActive_ || gamePaused_) {
        return;
    }
    
    // Fixed timestep simulation
    tickAccumulator_ += deltaTime;
    const f32 tickInterval = 1.0f / static_cast<f32>(tickRate_);
    
    while (tickAccumulator_ >= tickInterval) {
        // Update systems
        updateSystems(tickInterval);
        
        // Update game state
        updateGameState(tickInterval);
        
        tickAccumulator_ -= tickInterval;
        currentTick_++;
    }
}

void ServerWorld::updateSystems(f32 deltaTime) {
    // Update all systems in order
    for (auto& [name, system] : systems_) {
        system->update(deltaTime);
    }
}

void ServerWorld::updateGameState(f32 deltaTime) {
    gameTime_ += deltaTime;
    
    // Wave spawning logic (placeholder)
    timeToNextWave_ -= deltaTime;
    if (timeToNextWave_ <= 0.0f) {
        currentWave_++;
        timeToNextWave_ = 30.0f; // Next wave in 30 seconds
    }
}

Entity ServerWorld::createEntity(const String& name) {
    Entity entity = entityManager_.createEntity(name);
    assignNetworkId(entity);
    return entity;
}

void ServerWorld::destroyEntity(Entity entity) {
    removeNetworkId(entity);
    entityManager_.destroyEntity(entity);
}

bool ServerWorld::isValid(Entity entity) const {
    return entityManager_.isValid(entity);
}

void ServerWorld::clear() {
    entityManager_.clear();
    entityToNetworkId_.clear();
    networkIdToEntity_.clear();
    clientToEntity_.clear();
    nextNetworkId_ = 1;
    currentTick_ = 0;
    gameTime_ = 0.0f;
    currentWave_ = 0;
    timeToNextWave_ = 30.0f;
}

size_t ServerWorld::getEntityCount() const {
    return entityManager_.getEntityCount();
}

bool ServerWorld::isGameActive() const {
    return gameActive_;
}

f32 ServerWorld::getGameTime() const {
    return gameTime_;
}

NetworkId ServerWorld::getNetworkId(Entity entity) const {
    auto it = entityToNetworkId_.find(entity);
    return (it != entityToNetworkId_.end()) ? it->second : INVALID_NETWORK_ID;
}

Entity ServerWorld::getEntityByNetworkId(NetworkId networkId) const {
    auto it = networkIdToEntity_.find(networkId);
    return (it != networkIdToEntity_.end()) ? it->second : INVALID_ENTITY;
}

NetworkId ServerWorld::assignNetworkId(Entity entity) {
    NetworkId id = nextNetworkId_++;
    entityToNetworkId_[entity] = id;
    networkIdToEntity_[id] = entity;
    return id;
}

void ServerWorld::removeNetworkId(Entity entity) {
    auto it = entityToNetworkId_.find(entity);
    if (it != entityToNetworkId_.end()) {
        networkIdToEntity_.erase(it->second);
        entityToNetworkId_.erase(it);
    }
}

void ServerWorld::processInput(ClientId clientId, const PlayerInput& input) {
    // Find client's controlled entity
    auto it = clientToEntity_.find(clientId);
    if (it == clientToEntity_.end()) {
        return; // Client has no controlled entity
    }
    
    Entity heroEntity = it->second;
    if (!isValid(heroEntity)) {
        return;
    }
    
    // Process input based on command type
    switch (input.commandType) {
        case InputCommandType::Move:
            // TODO: Issue move command to hero
            break;
            
        case InputCommandType::AttackTarget:
            // TODO: Issue attack command to hero
            break;
            
        case InputCommandType::CastAbility:
            // TODO: Cast ability
            break;
            
        case InputCommandType::Stop:
            // TODO: Stop hero
            break;
            
        default:
            break;
    }
}

WorldSnapshot ServerWorld::createSnapshot() const {
    WorldSnapshot snapshot;
    snapshot.tick = currentTick_;
    snapshot.serverTime = gameTime_;
    snapshot.gameTime = gameTime_;
    snapshot.currentWave = currentWave_;
    snapshot.timeToNextWave = timeToNextWave_;
    
    // Create snapshots for all networked entities
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<TransformComponent>();
    
    for (auto entity : view) {
        EntitySnapshot entitySnap = createEntitySnapshot(entity);
        if (entitySnap.networkId != INVALID_NETWORK_ID) {
            snapshot.entities.push_back(entitySnap);
        }
    }
    
    // Debug: log snapshot contents periodically
    static int snapshotCount = 0;
    if (++snapshotCount % 300 == 1) { // Every ~10 seconds at 30 tick rate
        LOG_INFO("Snapshot: tick={}, entities={}", snapshot.tick, snapshot.entities.size());
        for (const auto& e : snapshot.entities) {
            LOG_INFO("  Entity: netId={}, type={}, owner={}, team={}, pos=({:.0f},{:.0f},{:.0f})", 
                     e.networkId, e.entityType, e.ownerClientId, e.teamId,
                     e.position.x, e.position.y, e.position.z);
        }
    }
    
    return snapshot;
}

EntitySnapshot ServerWorld::createEntitySnapshot(Entity entity) const {
    EntitySnapshot snapshot;
    
    // Network ID
    snapshot.networkId = getNetworkId(entity);
    if (snapshot.networkId == INVALID_NETWORK_ID) {
        return snapshot; // Not a networked entity
    }
    
    snapshot.tick = currentTick_;
    
    // Transform
    if (entityManager_.hasComponent<TransformComponent>(entity)) {
        const auto& transform = entityManager_.getComponent<TransformComponent>(entity);
        snapshot.position = transform.position;
        snapshot.rotation = transform.rotation;
    }
    
    // Health
    if (entityManager_.hasComponent<HealthComponent>(entity)) {
        const auto& health = entityManager_.getComponent<HealthComponent>(entity);
        snapshot.health = health.currentHealth;
        snapshot.maxHealth = health.maxHealth;
    }
    
    // Hero - check first as heroes are most important for multiplayer sync
    if (entityManager_.hasComponent<HeroComponent>(entity)) {
        const auto& hero = entityManager_.getComponent<HeroComponent>(entity);
        snapshot.teamId = hero.teamId;
        snapshot.health = hero.currentHealth;
        snapshot.maxHealth = hero.maxHealth;
        snapshot.mana = hero.currentMana;
        snapshot.maxMana = hero.maxMana;
        snapshot.entityType = 1; // Hero
        
        // Find owner client ID for this hero
        for (const auto& [clientId, heroEntity] : clientToEntity_) {
            if (heroEntity == entity) {
                snapshot.ownerClientId = clientId;
                break;
            }
        }
    }
    // Team from other components
    else if (entityManager_.hasComponent<ObjectComponent>(entity)) {
        const auto& obj = entityManager_.getComponent<ObjectComponent>(entity);
        snapshot.teamId = obj.teamId;
    } else if (entityManager_.hasComponent<CreepComponent>(entity)) {
        const auto& creep = entityManager_.getComponent<CreepComponent>(entity);
        snapshot.teamId = creep.teamId;
    }
    
    // Entity type (for client-side rendering)
    if (entityManager_.hasComponent<CreepComponent>(entity)) {
        snapshot.entityType = 2; // Creep
    } else if (entityManager_.hasComponent<ObjectComponent>(entity)) {
        const auto& obj = entityManager_.getComponent<ObjectComponent>(entity);
        if (obj.type == ObjectType::Tower) {
            snapshot.entityType = 3; // Tower
        }
    }
    
    return snapshot;
}

void ServerWorld::startGame() {
    gameActive_ = true;
    gamePaused_ = false;
    gameTime_ = 0.0f;
    currentWave_ = 0;
    timeToNextWave_ = 30.0f;
    
    // Start creep spawning
    if (auto* spawnSystem = static_cast<CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        spawnSystem->startGame();
    }
    
    // Only create default heroes if no clients are connected (local/editor mode)
    // In multiplayer, DedicatedServer creates heroes for each client before calling startGame()
    if (clientToEntity_.empty()) {
        if (auto* heroSystem = static_cast<HeroSystem*>(getSystem("HeroSystem"))) {
            if (heroSystem->getPlayerHero() == INVALID_ENTITY) {
                // Find Radiant base for spawn position
                Vec3 playerSpawnPos(50.0f, 1.0f, 50.0f);
                Vec3 enemySpawnPos(-50.0f, 1.0f, -50.0f);
                
                auto& registry = entityManager_.getRegistry();
                auto baseView = registry.view<ObjectComponent, TransformComponent>();
                for (auto entity : baseView) {
                    auto& obj = baseView.get<ObjectComponent>(entity);
                    auto& transform = baseView.get<TransformComponent>(entity);
                    if (obj.type == ObjectType::Base) {
                        if (obj.teamId == 1) {
                            playerSpawnPos = transform.position + Vec3(10.0f, 1.0f, 10.0f);
                        } else if (obj.teamId == 2) {
                            enemySpawnPos = transform.position + Vec3(-10.0f, 1.0f, -10.0f);
                        }
                    }
                }
                
                // Create Warrior hero for player (Team 1 - Radiant)
                Entity playerHero = heroSystem->createHeroByType("Warrior", 1, playerSpawnPos);
                heroSystem->setPlayerHero(playerHero);
                
                // Assign network ID for multiplayer sync
                assignNetworkId(playerHero);
                
                // Give starting items
                heroSystem->giveItem(playerHero, HeroSystem::createItem_Tango());
                heroSystem->giveItem(playerHero, HeroSystem::createItem_IronBranch());
                heroSystem->giveItem(playerHero, HeroSystem::createItem_IronBranch());
                
                // Learn first ability
                heroSystem->learnAbility(playerHero, 0);
                
                LOG_INFO("Player hero created at ({}, {}, {}) with networkId={}", 
                         playerSpawnPos.x, playerSpawnPos.y, playerSpawnPos.z, getNetworkId(playerHero));
                
                // Create enemy AI hero (Team 2 - Dire)
                Entity enemyHero = heroSystem->createHeroByType("Mage", 2, enemySpawnPos);
                
                // Assign network ID for multiplayer sync
                assignNetworkId(enemyHero);
                
                if (entityManager_.hasComponent<HeroComponent>(enemyHero)) {
                    auto& enemyComp = entityManager_.getComponent<HeroComponent>(enemyHero);
                    enemyComp.isPlayerControlled = false;
                    enemyComp.heroName = "Enemy Mage";
                    
                    heroSystem->giveItem(enemyHero, HeroSystem::createItem_IronBranch());
                    heroSystem->giveItem(enemyHero, HeroSystem::createItem_IronBranch());
                    
                    heroSystem->learnAbility(enemyHero, 0);
                    heroSystem->learnAbility(enemyHero, 1);
                }
                
                LOG_INFO("Enemy AI hero created at ({}, {}, {}) with networkId={}", 
                         enemySpawnPos.x, enemySpawnPos.y, enemySpawnPos.z, getNetworkId(enemyHero));
            }
        }
    } else {
        LOG_INFO("Multiplayer mode: {} client heroes already spawned", clientToEntity_.size());
    }
}

void ServerWorld::pauseGame() {
    gamePaused_ = !gamePaused_;
    
    if (auto* spawnSystem = static_cast<CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        spawnSystem->pauseGame();
    }
}

void ServerWorld::resetGame() {
    gameActive_ = false;
    gamePaused_ = false;
    gameTime_ = 0.0f;
    currentWave_ = 0;
    timeToNextWave_ = 30.0f;
    currentTick_ = 0;
    
    if (auto* spawnSystem = static_cast<CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        spawnSystem->resetGame();
    }
}

void ServerWorld::addClient(ClientId clientId) {
    // Client added - hero will be created by DedicatedServer::spawnHeroesForClients()
    LOG_INFO("Client {} added to server world", clientId);
}

void ServerWorld::setClientHero(ClientId clientId, Entity heroEntity) {
    clientToEntity_[clientId] = heroEntity;
    LOG_INFO("Client {} mapped to hero entity {}", clientId, static_cast<u32>(heroEntity));
}

void ServerWorld::removeClient(ClientId clientId) {
    clientToEntity_.erase(clientId);
}

Entity ServerWorld::getClientControlledEntity(ClientId clientId) const {
    auto it = clientToEntity_.find(clientId);
    return (it != clientToEntity_.end()) ? it->second : INVALID_ENTITY;
}

void ServerWorld::addSystem(UniquePtr<System> system) {
    String name = system->getName();
    systems_[name] = std::move(system);
}

void ServerWorld::removeSystem(const String& name) {
    systems_.erase(name);
}

System* ServerWorld::getSystem(const String& name) {
    auto it = systems_.find(name);
    return (it != systems_.end()) ? it->second.get() : nullptr;
}

const System* ServerWorld::getSystem(const String& name) const {
    auto it = systems_.find(name);
    return (it != systems_.end()) ? it->second.get() : nullptr;
}

#ifdef DIRECTX_RENDERER
void ServerWorld::render(ID3D12GraphicsCommandList* commandList,
                        const Mat4& viewProjMatrix,
                        const Vec3& cameraPosition,
                        bool showPathLines) {
    // Delegate to RenderSystem if available
    auto* renderSystem = static_cast<RenderSystem*>(getSystem("RenderSystem"));
    if (renderSystem) {
        renderSystem->render(commandList, viewProjMatrix, cameraPosition, showPathLines);
    }
}
#endif

} // namespace WorldEditor
