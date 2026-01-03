#pragma once

#include "System.h"
#include "Components.h"
#include "EntityManager.h"
#include "core/Types.h"

namespace WorldEditor {

class CreepSpawnSystem : public System {
public:
    CreepSpawnSystem(EntityManager& entityManager);
    ~CreepSpawnSystem() override = default;

    void update(f32 deltaTime) override;
    String getName() const override { return "CreepSpawnSystem"; }

    // Spawn wave configuration
    struct CreepWave {
        f32 spawnTime = 0.0f;           // Time since game start to spawn this wave
        i32 meleeCount = 3;             // Number of melee creeps
        i32 rangedCount = 1;            // Number of ranged creeps  
        i32 siegeCount = 0;             // Number of siege creeps (every 7th wave)
        bool spawned = false;           // Whether this wave has been spawned
    };

    // Game timing
    void startGame();
    void pauseGame();
    void resetGame();
    bool isGameActive() const { return gameActive_; }
    f32 getGameTime() const { return gameTime_; }

    // Wave management
    void generateWaves(i32 maxWaves = 100);
    i32 getCurrentWave() const;
    f32 getTimeToNextWave() const;

private:
    EntityManager& entityManager_;
    
    // Game state
    bool gameActive_ = false;
    f32 gameTime_ = 0.0f;
    
    // Wave configuration
    Vector<CreepWave> waves_;
    i32 currentWaveIndex_ = 0;
    
    // Spawn timing (Dota-like)
    static constexpr f32 WAVE_INTERVAL = 30.0f;     // 30 seconds between waves
    static constexpr f32 FIRST_WAVE_DELAY = 0.0f;   // First wave spawns immediately
    static constexpr f32 CREEP_SPAWN_DELAY = 0.5f;  // Delay between individual creeps in wave
    
    // Spawn management
    void updateWaveSpawning(f32 deltaTime);
    void spawnWave(const CreepWave& wave);
    void spawnCreepsForLane(CreepLane lane, i32 meleeCount, i32 rangedCount, i32 siegeCount);
    
    // Spawn point management
    Vector<Entity> getSpawnPointsForLane(CreepLane lane, i32 teamId);
    Entity findBestSpawnPoint(const Vector<Entity>& spawnPoints);
    
    // Wave generation
    CreepWave generateWaveData(i32 waveNumber);
    bool isSiegeWave(i32 waveNumber) const { return (waveNumber % 7) == 0 && waveNumber > 0; }
    bool isMegaWave(i32 waveNumber) const { return waveNumber >= 50; } // After 25 minutes
};

} // namespace WorldEditor