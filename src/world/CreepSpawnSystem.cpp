#include "CreepSpawnSystem.h"
#include "CreepSystem.h"
#include "World.h"

namespace WorldEditor {

CreepSpawnSystem::CreepSpawnSystem(EntityManager& entityManager) 
    : entityManager_(entityManager) {
    generateWaves();
}

void CreepSpawnSystem::update(f32 deltaTime) {
    if (!gameActive_) {
        return;
    }
    
    gameTime_ += deltaTime;
    updateWaveSpawning(deltaTime);
}

void CreepSpawnSystem::startGame() {
    gameActive_ = true;
    gameTime_ = 0.0f;
    currentWaveIndex_ = 0;
    
    // Reset all waves
    for (auto& wave : waves_) {
        wave.spawned = false;
    }
}

void CreepSpawnSystem::pauseGame() {
    gameActive_ = false;
}

void CreepSpawnSystem::resetGame() {
    gameActive_ = false;
    gameTime_ = 0.0f;
    currentWaveIndex_ = 0;
    
    // Reset all waves
    for (auto& wave : waves_) {
        wave.spawned = false;
    }
    
    // Clear existing creeps
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<CreepComponent>();
    Vector<Entity> toDestroy;
    
    for (auto entity : view) {
        toDestroy.push_back(entity);
    }
    
    for (Entity entity : toDestroy) {
        entityManager_.destroyEntity(entity);
    }
}

void CreepSpawnSystem::generateWaves(i32 maxWaves) {
    waves_.clear();
    waves_.reserve(maxWaves);
    
    for (i32 i = 0; i < maxWaves; ++i) {
        CreepWave wave = generateWaveData(i);
        wave.spawnTime = FIRST_WAVE_DELAY + (i * WAVE_INTERVAL);
        waves_.push_back(wave);
    }
}

CreepSpawnSystem::CreepWave CreepSpawnSystem::generateWaveData(i32 waveNumber) {
    CreepWave wave;
    
    // Base creep counts (Dota-like)
    wave.meleeCount = 3;
    wave.rangedCount = 1;
    wave.siegeCount = 0;
    
    // Every 7th wave has siege creeps
    if (isSiegeWave(waveNumber)) {
        wave.siegeCount = 1;
    }
    
    // Mega creeps after wave 50 (25+ minutes)
    if (isMegaWave(waveNumber)) {
        wave.meleeCount = 2; // Fewer but stronger
        wave.rangedCount = 1;
        wave.siegeCount = 1;
    }
    
    return wave;
}

void CreepSpawnSystem::updateWaveSpawning(f32 deltaTime) {
    // Check if it's time to spawn the next wave
    if (currentWaveIndex_ < static_cast<i32>(waves_.size())) {
        CreepWave& wave = waves_[currentWaveIndex_];
        
        if (!wave.spawned && gameTime_ >= wave.spawnTime) {
            spawnWave(wave);
            wave.spawned = true;
            currentWaveIndex_++;
        }
    }
}

void CreepSpawnSystem::spawnWave(const CreepWave& wave) {
    // Spawn creeps for all lanes
    spawnCreepsForLane(CreepLane::Top, wave.meleeCount, wave.rangedCount, wave.siegeCount);
    spawnCreepsForLane(CreepLane::Middle, wave.meleeCount, wave.rangedCount, wave.siegeCount);
    spawnCreepsForLane(CreepLane::Bottom, wave.meleeCount, wave.rangedCount, wave.siegeCount);
}

void CreepSpawnSystem::spawnCreepsForLane(CreepLane lane, i32 meleeCount, i32 rangedCount, i32 siegeCount) {
    // Get CreepSystem for spawning - use forward declaration to avoid circular dependency
    auto* world = entityManager_.getWorld();
    if (!world) {
        return;
    }
    
    auto* creepSystem = static_cast<CreepSystem*>(world->getSystem("CreepSystem"));
    if (!creepSystem) {
        return;
    }
    
    // Spawn for both teams
    for (i32 teamId = 1; teamId <= 2; ++teamId) {
        Vector<Entity> spawnPoints = getSpawnPointsForLane(lane, teamId);
        if (spawnPoints.empty()) {
            continue;
        }
        
        Entity spawnPoint = findBestSpawnPoint(spawnPoints);
        if (spawnPoint == INVALID_ENTITY) {
            continue;
        }
        
        // Spawn melee creeps
        for (i32 i = 0; i < meleeCount; ++i) {
            CreepType type = isMegaWave(currentWaveIndex_) ? CreepType::MegaMelee : CreepType::Melee;
            creepSystem->spawnCreep(spawnPoint, type, teamId, lane);
        }
        
        // Spawn ranged creeps
        for (i32 i = 0; i < rangedCount; ++i) {
            CreepType type = isMegaWave(currentWaveIndex_) ? CreepType::MegaRanged : CreepType::Ranged;
            creepSystem->spawnCreep(spawnPoint, type, teamId, lane);
        }
        
        // Spawn siege creeps
        for (i32 i = 0; i < siegeCount; ++i) {
            CreepType type = isMegaWave(currentWaveIndex_) ? CreepType::MegaSiege : CreepType::Siege;
            creepSystem->spawnCreep(spawnPoint, type, teamId, lane);
        }
    }
}

Vector<Entity> CreepSpawnSystem::getSpawnPointsForLane(CreepLane lane, i32 teamId) {
    Vector<Entity> spawnPoints;
    
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<ObjectComponent, TransformComponent>();
    
    for (auto entity : view) {
        auto& obj = view.get<ObjectComponent>(entity);
        
        if (obj.type == ObjectType::CreepSpawn && obj.teamId == teamId) {
            // Check if spawn point matches lane
            if (obj.spawnLane == static_cast<i32>(lane) || obj.spawnLane == -1) {
                spawnPoints.push_back(entity);
            }
        }
    }
    
    return spawnPoints;
}

Entity CreepSpawnSystem::findBestSpawnPoint(const Vector<Entity>& spawnPoints) {
    if (spawnPoints.empty()) {
        return INVALID_ENTITY;
    }
    
    // For now, just return the first spawn point
    // In full implementation, would consider factors like:
    // - Distance from existing creeps
    // - Spawn point capacity
    // - Lane balance
    return spawnPoints[0];
}

i32 CreepSpawnSystem::getCurrentWave() const {
    return currentWaveIndex_;
}

f32 CreepSpawnSystem::getTimeToNextWave() const {
    if (currentWaveIndex_ >= static_cast<i32>(waves_.size())) {
        return -1.0f; // No more waves
    }
    
    const CreepWave& nextWave = waves_[currentWaveIndex_];
    return std::max(0.0f, nextWave.spawnTime - gameTime_);
}

} // namespace WorldEditor