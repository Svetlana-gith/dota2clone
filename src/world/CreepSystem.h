#pragma once

#include "System.h"
#include "Components.h"
#include "EntityManager.h"
#include "core/Types.h"

namespace WorldEditor {

class World; // Forward declaration

class CreepSystem : public System {
public:
    CreepSystem(EntityManager& entityManager);
    ~CreepSystem() override = default;

    void update(f32 deltaTime) override;
    String getName() const override { return "CreepSystem"; }
    
    // Set world reference
    void setWorld(World* world) { world_ = world; }

    // Spawn creep at spawn point
    Entity spawnCreep(Entity spawnPoint, CreepType type, i32 teamId, CreepLane lane);
    
    // Get lane waypoints for pathfinding
    Vector<Vec3> getLaneWaypoints(CreepLane lane, i32 teamId) const;

private:
    EntityManager& entityManager_;
    World* world_ = nullptr;
    
    // Creep AI behavior
    void updateCreepAI(Entity entity, CreepComponent& creep, TransformComponent& transform, f32 deltaTime);
    void updateCreepMovement(Entity entity, CreepComponent& creep, TransformComponent& transform, f32 deltaTime);
    void updateCreepCombat(Entity entity, CreepComponent& creep, TransformComponent& transform, f32 deltaTime);
    
    // Target acquisition
    Entity findNearestEnemy(Entity creepEntity, const CreepComponent& creep, const Vec3& position);
    Entity findNearestEnemyCreep(const Vec3& position, i32 teamId, f32 searchRadius);
    Entity findNearestEnemyTower(const Vec3& position, i32 teamId, f32 searchRadius);
    Entity findNearestEnemyHero(const Vec3& position, i32 teamId, f32 searchRadius);
    
    // Pathfinding helpers
    bool hasLineOfSight(const Vec3& from, const Vec3& to) const;
    Vec3 getNextWaypointPosition(const CreepComponent& creep, const Vec3& currentPos) const;
    
    // Combat helpers
    bool isInAttackRange(const Vec3& attackerPos, const Vec3& targetPos, f32 range) const;
    void dealDamage(Entity attacker, Entity target, f32 damage);
    
    // Spawn management
    void cleanupDeadCreeps(f32 deltaTime);
    
    // Performance optimization
    f32 lastFullUpdate_ = 0.0f;
    static constexpr f32 FULL_UPDATE_INTERVAL = 0.1f; // 10 Hz for expensive operations
};

} // namespace WorldEditor