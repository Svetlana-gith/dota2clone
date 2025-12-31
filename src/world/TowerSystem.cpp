#include "TowerSystem.h"

#include "CreepSystem.h"
#include "World.h"
#include "EntityManager.h"

#include <algorithm>

namespace WorldEditor {

TowerSystem::TowerSystem(EntityManager& entityManager)
    : entityManager_(entityManager) {}

TowerSystem::~TowerSystem() = default;

Entity TowerSystem::findTargetForTower(i32 towerTeamId, const Vec3& towerPos, f32 range) const {
    auto& reg = entityManager_.getRegistry();
    auto creepView = reg.view<const CreepComponent, const TransformComponent>();

    Entity best = INVALID_ENTITY;
    float bestDist2 = std::numeric_limits<float>::max();

    const float range2 = range * range;
    for (auto e : creepView) {
        const auto& creep = creepView.get<const CreepComponent>(e);
        if (creep.state == CreepState::Dead) continue;
        if (creep.teamId == towerTeamId) continue;

        const auto& tr = creepView.get<const TransformComponent>(e);
        Vec3 d = tr.position - towerPos;
        d.y = 0.0f;
        const float dist2 = glm::dot(d, d);
        if (dist2 <= range2 && dist2 < bestDist2) {
            bestDist2 = dist2;
            best = e;
        }
    }

    return best;
}

void TowerSystem::fireProjectile(Entity towerEntity, const ObjectComponent& towerObj, Entity targetEntity) {
    if (!world_) {
        return;
    }
    auto* creepSystem = static_cast<CreepSystem*>(world_->getSystem("CreepSystem"));
    if (!creepSystem) {
        return;
    }
    creepSystem->spawnTowerProjectile(towerEntity, towerObj.teamId, towerObj.attackDamage, targetEntity);
}

void TowerSystem::update(f32 deltaTime) {
    auto& reg = entityManager_.getRegistry();
    auto towerView = reg.view<ObjectComponent, TransformComponent>();

    for (auto towerE : towerView) {
        auto& obj = towerView.get<ObjectComponent>(towerE);
        if (obj.type != ObjectType::Tower) continue;
        if (obj.teamId <= 0) continue;

        // If tower is dead, skip.
        if (reg.all_of<HealthComponent>(towerE)) {
            const auto& hp = reg.get<HealthComponent>(towerE);
            if (hp.isDead) continue;
        }

        // Ensure runtime state
        TowerRuntimeComponent* rt = reg.try_get<TowerRuntimeComponent>(towerE);
        if (!rt) {
            rt = &entityManager_.addComponent<TowerRuntimeComponent>(towerE);
            rt->attackCooldown = 0.0f;
        }

        rt->attackCooldown = std::max(0.0f, rt->attackCooldown - deltaTime);
        if (rt->attackCooldown > 0.0f) {
            continue;
        }

        const Vec3 towerPos = towerView.get<TransformComponent>(towerE).position;
        const f32 range = std::max(0.0f, obj.attackRange);
        if (range <= 0.01f) continue;

        Entity target = findTargetForTower(obj.teamId, towerPos, range);
        if (target == INVALID_ENTITY) {
            continue;
        }

        // Fire!
        fireProjectile(towerE, obj, target);
        const f32 atkSpeed = std::max(0.05f, obj.attackSpeed);
        rt->attackCooldown = 1.0f / atkSpeed;
    }
}

} // namespace WorldEditor
