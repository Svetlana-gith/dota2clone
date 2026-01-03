#pragma once

#include "System.h"
#include "Components.h"
#include "EntityManager.h"
#include "core/Types.h"

namespace WorldEditor {

class World; // Forward declaration

class TowerSystem : public System {
public:
    TowerSystem(EntityManager& entityManager);
    ~TowerSystem() override = default;

    void update(f32 deltaTime) override;
    String getName() const override { return "TowerSystem"; }
    
    // Set world reference
    void setWorld(World* world) { world_ = world; }

    // Tower management
    void initializeTower(Entity tower);
    Entity findNearestEnemyInRange(Entity tower, const Vec3& position, f32 range, i32 teamId);

private:
    EntityManager& entityManager_;
    World* world_ = nullptr;
    
    // Tower AI
    void updateTowerAI(Entity entity, ObjectComponent& tower, TransformComponent& transform, f32 deltaTime);
    void updateTowerCombat(Entity entity, ObjectComponent& tower, const Vec3& position, f32 deltaTime);
    
    // Target acquisition
    Entity findBestTarget(const Vec3& towerPos, f32 range, i32 teamId);
    f32 calculateTargetPriority(Entity target, const Vec3& towerPos);
    
    // Combat
    void fireTowerProjectile(Entity tower, Entity target, const ObjectComponent& towerComp);
    
    // Performance optimization
    f32 lastFullUpdate_ = 0.0f;
    static constexpr f32 FULL_UPDATE_INTERVAL = 0.2f; // 5 Hz for expensive operations
};

} // namespace WorldEditor