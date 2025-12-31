#pragma once

#include "System.h"
#include "Components.h"
#include "core/Types.h"

namespace WorldEditor {

// Forward declaration
class EntityManager;
class World;

// Creep system for managing creep spawning, movement, and combat
class CreepSystem : public System {
public:
    CreepSystem(EntityManager& entityManager);
    ~CreepSystem() override;
    
    // Set world reference for accessing other systems
    void setWorld(World* world) { world_ = world; }

    void update(f32 deltaTime) override;
    String getName() const override { return "CreepSystem"; }

    // Spawn creeps from spawn points
    void spawnCreeps(f32 deltaTime);
    
    // Update creep movement and AI
    void updateCreeps(f32 deltaTime);
    
    // Find target for creep
    Entity findTarget(Entity creepEntity, const CreepComponent& creep, const TransformComponent& creepTransform);
    
    // Calculate lane direction for a creep
    Vec3 calculateLaneDirection(CreepLane lane, i32 teamId);
    
    // Cleanup dead creeps
    void cleanupDeadCreeps(f32 deltaTime);

    // Runtime projectile updates (ranged attacks)
    void updateProjectiles(f32 deltaTime);

    // TowerSystem uses this to spawn tower shots using our pooled projectile entities.
    void spawnTowerProjectile(Entity towerEntity, i32 teamId, f32 baseDamage, Entity targetEntity);

    // Reset runtime simulation state (used by GameMode Stop & Reset).
    void resetSimulation();
    
    // Waypoint system
    Vector<Vec3> buildPathForLane(i32 teamId, CreepLane lane) const;
    Entity findBaseForTeam(i32 teamId) const;
    Vec3 getNextWaypoint(const CreepComponent& creep, const TransformComponent& transform) const;

private:
    EntityManager& entityManager_;
    World* world_ = nullptr; // Reference to world for accessing systems

    // Auto-balance so 1 wave resolves within spawnInterval_.
    bool damageCalibrated_ = false;
    f32 damageMultiplier_ = 1.0f;
    void calibrateWaveDamage();
    
    // Spawn timing
    f32 spawnTimer_ = 0.0f;
    const f32 spawnInterval_ = 30.0f; // Spawn creeps every 30 seconds
    bool firstSpawnDone_ = false; // Track if first spawn has occurred
    
    // Creep limits
    const i32 maxCreepsPerTeam_ = 50; // Maximum creeps per team on map
    const i32 maxCreepsPerSpawn_ = 20; // Maximum creeps per spawn point
    
    // Helper methods
    Entity createCreepEntity(i32 teamId, CreepLane lane, CreepType type, const Vec3& position, Entity spawnPoint);
    void setupCreepStats(CreepComponent& creep, CreepType type);
    void moveCreep(Entity creepEntity, CreepComponent& creep, TransformComponent& transform, f32 deltaTime);
    void attackTarget(Entity creepEntity, CreepComponent& creep, Entity targetEntity, f32 deltaTime);
    bool isEnemy(Entity entity1, Entity entity2) const;
    bool isRangedType(CreepType type) const;
    void fireProjectile(Entity attacker, CreepComponent& creep, Entity targetEntity);
    void applyProjectileHit(const ProjectileComponent& proj);

    // Pooled projectile entities to avoid per-shot mesh/GPU buffer churn (big FPS killer over time).
    Vector<Entity> projectilePoolCreepTeam1_;
    Vector<Entity> projectilePoolCreepTeam2_;
    Vector<Entity> projectilePoolTowerTeam1_;
    Vector<Entity> projectilePoolTowerTeam2_;
    i32 projectileCreatedCreepTeam1_ = 0;
    i32 projectileCreatedCreepTeam2_ = 0;
    i32 projectileCreatedTowerTeam1_ = 0;
    i32 projectileCreatedTowerTeam2_ = 0;
    const i32 projectileMaxPerPool_ = 128;

    Entity projectileMatCreepTeam1_ = INVALID_ENTITY;
    Entity projectileMatCreepTeam2_ = INVALID_ENTITY;
    Entity projectileMatTowerTeam1_ = INVALID_ENTITY;
    Entity projectileMatTowerTeam2_ = INVALID_ENTITY;

    Entity acquireProjectileEntity(i32 teamId, bool isTower);
    void initProjectileVisual(Entity projE, i32 teamId, bool isTower);
    
    // Damage calculation
    f32 calculateDamage(f32 baseDamage, f32 targetArmor) const;
    
    // Count active creeps
    i32 countActiveCreeps(i32 teamId) const;
    i32 countCreepsFromSpawn(Entity spawnPoint) const;
    
    // Check if path to target is clear (no obstacles)
    bool isPathClear(const Vec3& from, const Vec3& to, f32 radius) const;
    
    // Find alternative path around obstacle
    Vec3 findPathAroundObstacle(const Vec3& from, const Vec3& to, f32 radius) const;

};

} // namespace WorldEditor
