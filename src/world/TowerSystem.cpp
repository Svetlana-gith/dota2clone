#include "TowerSystem.h"
#include "ProjectileSystem.h"
#include "HeroSystem.h"
#include <algorithm>

namespace WorldEditor {

TowerSystem::TowerSystem(EntityManager& entityManager) 
    : entityManager_(entityManager) {
}

void TowerSystem::update(f32 deltaTime) {
    auto& registry = entityManager_.getRegistry();
    
    // Update tower runtime components
    auto runtimeView = registry.view<TowerRuntimeComponent>();
    for (auto entity : runtimeView) {
        auto& runtime = runtimeView.get<TowerRuntimeComponent>(entity);
        runtime.attackCooldown = std::max(0.0f, runtime.attackCooldown - deltaTime);
    }
    
    // Update tower AI (throttled for performance)
    lastFullUpdate_ += deltaTime;
    if (lastFullUpdate_ >= FULL_UPDATE_INTERVAL) {
        auto towerView = registry.view<ObjectComponent, TransformComponent>();
        for (auto entity : towerView) {
            auto& obj = towerView.get<ObjectComponent>(entity);
            auto& transform = towerView.get<TransformComponent>(entity);
            
            if (obj.type == ObjectType::Tower) {
                updateTowerAI(entity, obj, transform, deltaTime);
            }
        }
        lastFullUpdate_ = 0.0f;
    }
}

void TowerSystem::updateTowerAI(Entity entity, ObjectComponent& tower, TransformComponent& transform, f32 deltaTime) {
    // Skip if tower is dead
    if (entityManager_.hasComponent<HealthComponent>(entity)) {
        auto& health = entityManager_.getComponent<HealthComponent>(entity);
        if (health.isDead) {
            return;
        }
    }
    
    // Ensure tower has runtime component
    if (!entityManager_.hasComponent<TowerRuntimeComponent>(entity)) {
        entityManager_.addComponent<TowerRuntimeComponent>(entity);
    }
    
    auto& runtime = entityManager_.getComponent<TowerRuntimeComponent>(entity);
    
    // Skip if on cooldown
    if (runtime.attackCooldown > 0.0f) {
        return;
    }
    
    // Find target
    Entity target = findBestTarget(transform.position, tower.attackRange, tower.teamId);
    if (target != INVALID_ENTITY) {
        // Fire at target
        fireTowerProjectile(entity, target, tower);
        runtime.attackCooldown = 1.0f / tower.attackSpeed;
    }
}

Entity TowerSystem::findBestTarget(const Vec3& towerPos, f32 range, i32 teamId) {
    auto& registry = entityManager_.getRegistry();
    
    Entity bestTarget = INVALID_ENTITY;
    f32 bestPriority = -1.0f;
    
    // Priority 1: Check enemy creeps (towers prioritize creeps over heroes)
    auto creepView = registry.view<CreepComponent, TransformComponent>();
    for (auto entity : creepView) {
        auto& creep = creepView.get<CreepComponent>(entity);
        auto& transform = creepView.get<TransformComponent>(entity);
        
        // Skip same team or dead creeps
        if (creep.teamId == teamId || creep.state == CreepState::Dead) {
            continue;
        }
        
        // Check range
        f32 distance = glm::length(transform.position - towerPos);
        if (distance > range) {
            continue;
        }
        
        // Calculate priority
        f32 priority = calculateTargetPriority(entity, towerPos);
        if (priority > bestPriority) {
            bestTarget = entity;
            bestPriority = priority;
        }
    }
    
    // Priority 2: Check enemy heroes (if no creeps in range or hero is attacking tower)
    auto heroView = registry.view<HeroComponent, TransformComponent>();
    for (auto entity : heroView) {
        auto& hero = heroView.get<HeroComponent>(entity);
        auto& transform = heroView.get<TransformComponent>(entity);
        
        // Skip same team or dead heroes
        if (hero.teamId == teamId || hero.state == HeroState::Dead) {
            continue;
        }
        
        // Skip invisible heroes
        if (hero.isInvisible()) {
            continue;
        }
        
        // Check range
        f32 distance = glm::length(transform.position - towerPos);
        if (distance > range) {
            continue;
        }
        
        // Calculate priority for hero (lower than creeps, but still valid)
        f32 priority = calculateTargetPriority(entity, towerPos);
        // Heroes get lower base priority than creeps, but can still be targeted
        priority -= 20.0f; // Slight penalty to prefer creeps
        
        if (priority > bestPriority) {
            bestTarget = entity;
            bestPriority = priority;
        }
    }
    
    return bestTarget;
}

f32 TowerSystem::calculateTargetPriority(Entity target, const Vec3& towerPos) {
    f32 priority = 0.0f;
    
    if (!entityManager_.hasComponent<TransformComponent>(target)) {
        return priority;
    }
    
    auto& transform = entityManager_.getComponent<TransformComponent>(target);
    f32 distance = glm::length(transform.position - towerPos);
    
    // Base priority: closer targets are higher priority
    priority = 100.0f - distance;
    
    // Creep-specific priorities
    if (entityManager_.hasComponent<CreepComponent>(target)) {
        auto& creep = entityManager_.getComponent<CreepComponent>(target);
        
        // Prioritize by creep type
        switch (creep.type) {
            case CreepType::Siege:
            case CreepType::LargeSiege:
            case CreepType::MegaSiege:
                priority += 50.0f; // Siege units are high priority
                break;
            case CreepType::Ranged:
            case CreepType::LargeRanged:
            case CreepType::MegaRanged:
                priority += 20.0f; // Ranged units are medium priority
                break;
            case CreepType::Melee:
            case CreepType::LargeMelee:
            case CreepType::MegaMelee:
                priority += 10.0f; // Melee units are lower priority
                break;
        }
        
        // Prioritize low health targets (easier kills)
        f32 healthPercent = creep.currentHealth / creep.maxHealth;
        priority += (1.0f - healthPercent) * 30.0f;
    }
    // Hero-specific priorities
    else if (entityManager_.hasComponent<HeroComponent>(target)) {
        auto& hero = entityManager_.getComponent<HeroComponent>(target);
        
        // Heroes are valid targets but have lower priority than creeps
        // Prioritize low health heroes
        f32 healthPercent = hero.maxHealth > 0.0f ? hero.currentHealth / hero.maxHealth : 1.0f;
        priority += (1.0f - healthPercent) * 20.0f;
    }
    
    return priority;
}

void TowerSystem::fireTowerProjectile(Entity tower, Entity target, const ObjectComponent& towerComp) {
    // Find or create ProjectileSystem to handle projectile creation
    // For now, create projectile directly (in full implementation, would use system)
    
    if (!entityManager_.hasComponent<TransformComponent>(tower) || 
        !entityManager_.hasComponent<TransformComponent>(target)) {
        return;
    }
    
    auto& towerTransform = entityManager_.getComponent<TransformComponent>(tower);
    
    // Create projectile entity
    Entity projectile = entityManager_.createEntity("TowerProjectile");
    auto& projComp = entityManager_.addComponent<ProjectileComponent>(projectile);
    auto& transform = entityManager_.addComponent<TransformComponent>(projectile);
    
    // Set projectile properties
    projComp.attacker = tower;
    projComp.target = target;
    projComp.teamId = towerComp.teamId;
    projComp.baseDamage = towerComp.attackDamage;
    projComp.active = true;
    projComp.isTower = true;
    projComp.speed = 60.0f; // Tower projectiles are slower but more visible
    projComp.hitRadius = 1.5f; // Larger hit radius for tower projectiles
    
    // Set starting position (above tower)
    transform.position = towerTransform.position + Vec3(0, 2, 0);
    
    // Create visual mesh (larger than creep projectiles)
    auto& mesh = entityManager_.addComponent<MeshComponent>(projectile, "TowerProjectileMesh");
    // Use simple sphere for now - in full implementation would use proper projectile model
    Vector<Vec3> vertices, normals;
    Vector<Vec2> texCoords;
    Vector<u32> indices;
    
    // Generate sphere mesh
    const f32 radius = 0.2f;
    const i32 segments = 8;
    const i32 rings = 8;
    
    // Simple sphere generation
    for (i32 ring = 0; ring <= rings; ++ring) {
        f32 phi = static_cast<f32>(ring) / static_cast<f32>(rings) * 3.14159f;
        for (i32 seg = 0; seg <= segments; ++seg) {
            f32 theta = static_cast<f32>(seg) / static_cast<f32>(segments) * 2.0f * 3.14159f;
            
            Vec3 pos;
            pos.x = radius * std::sin(phi) * std::cos(theta);
            pos.y = radius * std::cos(phi);
            pos.z = radius * std::sin(phi) * std::sin(theta);
            
            vertices.push_back(pos);
            normals.push_back(glm::normalize(pos));
            texCoords.push_back(Vec2(static_cast<f32>(seg) / segments, static_cast<f32>(ring) / rings));
        }
    }
    
    // Generate indices
    for (i32 ring = 0; ring < rings; ++ring) {
        for (i32 seg = 0; seg < segments; ++seg) {
            i32 current = ring * (segments + 1) + seg;
            i32 next = current + segments + 1;
            
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
    
    mesh.vertices = std::move(vertices);
    mesh.normals = std::move(normals);
    mesh.texCoords = std::move(texCoords);
    mesh.indices = std::move(indices);
    mesh.gpuUploadNeeded = true;
    
    // Create material
    Entity materialEntity = entityManager_.createEntity("TowerProjectileMaterial");
    auto& material = entityManager_.addComponent<MaterialComponent>(materialEntity, "TowerProjectileMaterial");
    
    // Team-based colors (brighter for tower projectiles)
    if (towerComp.teamId == 1) {
        material.baseColor = Vec3(0.2f, 1.0f, 0.2f); // Bright green for Radiant
        material.emissiveColor = Vec3(0.1f, 0.4f, 0.1f);
    } else {
        material.baseColor = Vec3(1.0f, 0.2f, 0.2f); // Bright red for Dire
        material.emissiveColor = Vec3(0.4f, 0.1f, 0.1f);
    }
    
    mesh.materialEntity = materialEntity;
}

void TowerSystem::initializeTower(Entity tower) {
    if (!entityManager_.isValid(tower)) {
        return;
    }
    
    // Ensure tower has required components
    if (!entityManager_.hasComponent<ObjectComponent>(tower)) {
        return;
    }
    
    auto& obj = entityManager_.getComponent<ObjectComponent>(tower);
    if (obj.type != ObjectType::Tower) {
        return;
    }
    
    // Add runtime component if missing
    if (!entityManager_.hasComponent<TowerRuntimeComponent>(tower)) {
        entityManager_.addComponent<TowerRuntimeComponent>(tower);
    }
    
    // Add health component if missing
    if (!entityManager_.hasComponent<HealthComponent>(tower)) {
        auto& health = entityManager_.addComponent<HealthComponent>(tower, 1800.0f); // Standard tower HP
        health.armor = 5.0f;
    }
    
    // Add collision component if missing
    if (!entityManager_.hasComponent<CollisionComponent>(tower)) {
        auto& collision = entityManager_.addComponent<CollisionComponent>(tower, CollisionShape::Capsule);
        collision.capsuleRadius = 2.0f;
        collision.capsuleHeight = 4.0f;
        collision.isStatic = true;
        collision.blocksMovement = true;
    }
}

Entity TowerSystem::findNearestEnemyInRange(Entity tower, const Vec3& position, f32 range, i32 teamId) {
    return findBestTarget(position, range, teamId);
}

} // namespace WorldEditor