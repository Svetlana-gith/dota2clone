#include "ProjectileSystem.h"
#include "MeshGenerators.h"
#include "HeroSystem.h"
#include <algorithm>

namespace WorldEditor {

ProjectileSystem::ProjectileSystem(EntityManager& entityManager) 
    : entityManager_(entityManager) {
}

void ProjectileSystem::update(f32 deltaTime) {
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<ProjectileComponent, TransformComponent>();
    
    for (auto entity : view) {
        auto& projectile = view.get<ProjectileComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        if (!projectile.active) {
            continue;
        }
        
        // Update lifetime
        projectile.life += deltaTime;
        if (projectile.life >= projectile.maxLife) {
            projectile.active = false;
            continue;
        }
        
        // Update movement
        updateProjectileMovement(entity, projectile, transform, deltaTime);
        
        // Check for hit
        checkProjectileHit(entity, projectile, transform.position);
    }
    
    // Cleanup expired projectiles
    cleanupExpiredProjectiles();
}

void ProjectileSystem::updateProjectileMovement(Entity entity, ProjectileComponent& projectile, TransformComponent& transform, f32 deltaTime) {
    if (projectile.target == INVALID_ENTITY || !entityManager_.isValid(projectile.target)) {
        projectile.active = false;
        return;
    }
    
    if (!entityManager_.hasComponent<TransformComponent>(projectile.target)) {
        projectile.active = false;
        return;
    }
    
    auto& targetTransform = entityManager_.getComponent<TransformComponent>(projectile.target);
    
    // Calculate direction to target
    Vec3 direction = targetTransform.position - transform.position;
    f32 distance = glm::length(direction);
    
    if (distance < projectile.hitRadius) {
        // Hit target
        applyProjectileDamage(entity, projectile.target, projectile.baseDamage);
        createHitEffect(transform.position, projectile.teamId);
        projectile.active = false;
        return;
    }
    
    // Move towards target
    if (distance > 0.001f) {
        direction = glm::normalize(direction);
        Vec3 movement = direction * projectile.speed * deltaTime;
        
        // Don't overshoot target
        if (glm::length(movement) > distance) {
            transform.position = targetTransform.position;
        } else {
            transform.position += movement;
        }
        
        // Rotate to face movement direction
        f32 yaw = std::atan2(direction.x, direction.z);
        transform.rotation = glm::angleAxis(yaw, Vec3(0, 1, 0));
    }
}

void ProjectileSystem::checkProjectileHit(Entity entity, ProjectileComponent& projectile, const Vec3& position) {
    if (projectile.target == INVALID_ENTITY) {
        return;
    }
    
    if (!entityManager_.hasComponent<TransformComponent>(projectile.target)) {
        projectile.active = false;
        return;
    }
    
    auto& targetTransform = entityManager_.getComponent<TransformComponent>(projectile.target);
    f32 distance = glm::length(targetTransform.position - position);
    
    if (distance <= projectile.hitRadius) {
        // Hit!
        applyProjectileDamage(entity, projectile.target, projectile.baseDamage);
        createHitEffect(position, projectile.teamId);
        projectile.active = false;
    }
}

void ProjectileSystem::applyProjectileDamage(Entity projectile, Entity target, f32 damage) {
    // Deal damage to creep
    if (entityManager_.hasComponent<CreepComponent>(target)) {
        auto& creep = entityManager_.getComponent<CreepComponent>(target);
        creep.currentHealth -= damage;
        
        if (creep.currentHealth <= 0.0f) {
            creep.currentHealth = 0.0f;
            creep.state = CreepState::Dead;
            creep.deathTime = 0.0f;
        }
    }
    
    // Deal damage to hero
    if (entityManager_.hasComponent<HeroComponent>(target)) {
        auto& hero = entityManager_.getComponent<HeroComponent>(target);
        
        // Check invulnerability
        if (hero.isInvulnerable()) {
            return;
        }
        
        // Apply armor reduction (physical damage)
        f32 armor = hero.armor;
        f32 armorReduction = 1.0f - (0.06f * armor) / (1.0f + 0.06f * std::abs(armor));
        f32 actualDamage = damage * armorReduction;
        
        hero.currentHealth -= actualDamage;
        
        if (hero.currentHealth <= 0.0f) {
            hero.currentHealth = 0.0f;
            hero.state = HeroState::Dead;
            hero.deaths++;
            // Calculate respawn time based on level
            hero.respawnTimer = static_cast<f32>(hero.level) * 2.5f;
        }
    }
    
    // Deal damage to tower/building
    if (entityManager_.hasComponent<HealthComponent>(target)) {
        auto& health = entityManager_.getComponent<HealthComponent>(target);
        f32 actualDamage = damage; // TODO: Apply armor reduction
        health.currentHealth -= actualDamage;
        
        if (health.currentHealth <= 0.0f) {
            health.currentHealth = 0.0f;
            health.isDead = true;
        }
    }
}

void ProjectileSystem::cleanupExpiredProjectiles() {
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<ProjectileComponent>();
    
    Vector<Entity> toDestroy;
    
    for (auto entity : view) {
        auto& projectile = view.get<ProjectileComponent>(entity);
        
        if (!projectile.active || projectile.life >= projectile.maxLife) {
            toDestroy.push_back(entity);
        }
    }
    
    for (Entity entity : toDestroy) {
        entityManager_.destroyEntity(entity);
    }
}

Entity ProjectileSystem::createProjectile(Entity attacker, Entity target, f32 damage, bool isTower) {
    if (!entityManager_.isValid(attacker) || !entityManager_.isValid(target)) {
        return INVALID_ENTITY;
    }
    
    if (!entityManager_.hasComponent<TransformComponent>(attacker) || 
        !entityManager_.hasComponent<TransformComponent>(target)) {
        return INVALID_ENTITY;
    }
    
    auto& attackerTransform = entityManager_.getComponent<TransformComponent>(attacker);
    
    // Create projectile entity
    Entity projectile = entityManager_.createEntity("Projectile");
    auto& projComp = entityManager_.addComponent<ProjectileComponent>(projectile);
    auto& transform = entityManager_.addComponent<TransformComponent>(projectile);
    
    // Set projectile properties
    projComp.attacker = attacker;
    projComp.target = target;
    projComp.baseDamage = damage;
    projComp.active = true;
    projComp.isTower = isTower;
    projComp.life = 0.0f;
    
    // Get team ID from attacker
    if (entityManager_.hasComponent<CreepComponent>(attacker)) {
        auto& creep = entityManager_.getComponent<CreepComponent>(attacker);
        projComp.teamId = creep.teamId;
    } else if (entityManager_.hasComponent<ObjectComponent>(attacker)) {
        auto& obj = entityManager_.getComponent<ObjectComponent>(attacker);
        projComp.teamId = obj.teamId;
    }
    
    // Set starting position (slightly above attacker)
    transform.position = attackerTransform.position + Vec3(0, 1, 0);
    
    // Create visual mesh
    auto& mesh = entityManager_.addComponent<MeshComponent>(projectile, "ProjectileMesh");
    if (isTower) {
        // Tower projectiles are larger and more visible
        MeshGenerators::GenerateSphere(mesh, 0.15f, 8);
        projComp.speed = 60.0f; // Slower tower projectiles
    } else {
        // Creep projectiles are smaller and faster
        MeshGenerators::GenerateSphere(mesh, 0.08f, 6);
        projComp.speed = 80.0f; // Faster creep projectiles
    }
    mesh.gpuUploadNeeded = true;
    
    // Create material
    Entity materialEntity = entityManager_.createEntity("ProjectileMaterial");
    auto& material = entityManager_.addComponent<MaterialComponent>(materialEntity, "ProjectileMaterial");
    
    // Color based on team and type
    if (isTower) {
        if (projComp.teamId == 1) {
            material.baseColor = Vec3(0.2f, 1.0f, 0.2f); // Bright green for Radiant towers
            material.emissiveColor = Vec3(0.1f, 0.3f, 0.1f);
        } else {
            material.baseColor = Vec3(1.0f, 0.2f, 0.2f); // Bright red for Dire towers
            material.emissiveColor = Vec3(0.3f, 0.1f, 0.1f);
        }
    } else {
        // Creep projectiles are more subtle
        if (projComp.teamId == 1) {
            material.baseColor = Vec3(0.8f, 1.0f, 0.6f); // Light green
            material.emissiveColor = Vec3(0.05f, 0.1f, 0.05f);
        } else {
            material.baseColor = Vec3(1.0f, 0.6f, 0.6f); // Light red
            material.emissiveColor = Vec3(0.1f, 0.05f, 0.05f);
        }
    }
    
    mesh.materialEntity = materialEntity;
    
    return projectile;
}

void ProjectileSystem::createHitEffect(const Vec3& position, i32 teamId) {
    // TODO: Create particle effect or temporary visual effect at hit position
    // For now, this is a placeholder for future particle system integration
}

} // namespace WorldEditor