#pragma once

#include "System.h"
#include "Components.h"
#include "core/Types.h"

namespace WorldEditor {

class EntityManager;
class World;

// Tower combat system (attack range + projectile).
// Runs in game mode only (gated by World::update).
class TowerSystem : public System {
public:
    explicit TowerSystem(EntityManager& entityManager);
    ~TowerSystem() override;

    void setWorld(World* world) { world_ = world; }

    void update(f32 deltaTime) override;
    String getName() const override { return "TowerSystem"; }

private:
    EntityManager& entityManager_;
    World* world_ = nullptr;

    Entity findTargetForTower(i32 towerTeamId, const Vec3& towerPos, f32 range) const;
    void fireProjectile(Entity towerEntity, const ObjectComponent& towerObj, Entity targetEntity);
};

} // namespace WorldEditor
