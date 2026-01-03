#pragma once

#include "System.h"
#include "Components.h"
#include "EntityManager.h"
#include "core/Types.h"

namespace WorldEditor {

class ProjectileSystem : public System {
public:
    ProjectileSystem(EntityManager& entityManager);
    ~ProjectileSystem() override = default;

    void update(f32 deltaTime) override;
    String getName() const override { return "ProjectileSystem"; }

    // Create projectile for ranged attacks
    Entity createProjectile(Entity attacker, Entity target, f32 damage, bool isTower = false);

private:
    EntityManager& entityManager_;
    
    // Projectile movement and collision
    void updateProjectileMovement(Entity entity, ProjectileComponent& projectile, TransformComponent& transform, f32 deltaTime);
    void checkProjectileHit(Entity entity, ProjectileComponent& projectile, const Vec3& position);
    
    // Damage application
    void applyProjectileDamage(Entity projectile, Entity target, f32 damage);
    
    // Cleanup
    void cleanupExpiredProjectiles();
    
    // Visual effects
    void createHitEffect(const Vec3& position, i32 teamId);
};

} // namespace WorldEditor