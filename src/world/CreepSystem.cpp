#include "CreepSystem.h"
#include "World.h"
#include "MeshGenerators.h"
#include "HeroSystem.h"
#include <algorithm>
#include <cmath>

namespace WorldEditor {

CreepSystem::CreepSystem(EntityManager& entityManager) 
    : entityManager_(entityManager) {
}

void CreepSystem::update(f32 deltaTime) {
    auto& registry = entityManager_.getRegistry();
    
    // Update all creeps
    auto view = registry.view<CreepComponent, TransformComponent>();
    for (auto entity : view) {
        auto& creep = view.get<CreepComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        updateCreepAI(entity, creep, transform, deltaTime);
    }
    
    // Cleanup dead creeps
    cleanupDeadCreeps(deltaTime);
    
    lastFullUpdate_ += deltaTime;
}

void CreepSystem::updateCreepAI(Entity entity, CreepComponent& creep, TransformComponent& transform, f32 deltaTime) {
    // Update cooldowns
    creep.attackCooldown = std::max(0.0f, creep.attackCooldown - deltaTime);
    creep.targetSearchCooldown = std::max(0.0f, creep.targetSearchCooldown - deltaTime);
    creep.pathCheckCooldown = std::max(0.0f, creep.pathCheckCooldown - deltaTime);
    
    // Skip if dead
    if (creep.state == CreepState::Dead) {
        creep.deathTime += deltaTime;
        return;
    }
    
    // Check if current target is still valid
    if (creep.targetEntity != INVALID_ENTITY) {
        if (!entityManager_.isValid(creep.targetEntity)) {
            creep.targetEntity = INVALID_ENTITY;
            creep.state = CreepState::Moving;
        } else {
            // Check if target is still alive
            if (entityManager_.hasComponent<HealthComponent>(creep.targetEntity)) {
                auto& health = entityManager_.getComponent<HealthComponent>(creep.targetEntity);
                if (health.isDead) {
                    creep.targetEntity = INVALID_ENTITY;
                    creep.state = CreepState::Moving;
                }
            }
            if (entityManager_.hasComponent<CreepComponent>(creep.targetEntity)) {
                auto& targetCreep = entityManager_.getComponent<CreepComponent>(creep.targetEntity);
                if (targetCreep.state == CreepState::Dead) {
                    creep.targetEntity = INVALID_ENTITY;
                    creep.state = CreepState::Moving;
                }
            }
        }
    }
    
    // State machine
    switch (creep.state) {
        case CreepState::Moving:
            updateCreepMovement(entity, creep, transform, deltaTime);
            
            // Look for enemies to attack (throttled)
            if (creep.targetSearchCooldown <= 0.0f) {
                Entity enemy = findNearestEnemy(entity, creep, transform.position);
                if (enemy != INVALID_ENTITY) {
                    creep.targetEntity = enemy;
                    creep.state = CreepState::Attacking;
                }
                creep.targetSearchCooldown = 0.5f; // Search every 0.5 seconds
            }
            break;
            
        case CreepState::Attacking:
            updateCreepCombat(entity, creep, transform, deltaTime);
            break;
            
        case CreepState::Idle:
            // Look for enemies or resume movement
            if (creep.targetSearchCooldown <= 0.0f) {
                Entity enemy = findNearestEnemy(entity, creep, transform.position);
                if (enemy != INVALID_ENTITY) {
                    creep.targetEntity = enemy;
                    creep.state = CreepState::Attacking;
                } else {
                    creep.state = CreepState::Moving;
                }
                creep.targetSearchCooldown = 0.5f;
            }
            break;
            
        case CreepState::Dead:
            // Handled above
            break;
    }
}

void CreepSystem::updateCreepMovement(Entity entity, CreepComponent& creep, TransformComponent& transform, f32 deltaTime) {
    // Get next waypoint position
    Vec3 targetPos = getNextWaypointPosition(creep, transform.position);
    
    // Check distance to actual waypoint (not formation target)
    Vec3 toWaypoint = targetPos - transform.position;
    f32 distanceToWaypoint = glm::length(toWaypoint);
    
    // If very close to waypoint, advance to next one
    const f32 waypointReachDistance = 3.0f; // Increased threshold to prevent circling
    if (distanceToWaypoint < waypointReachDistance) {
        // Reached waypoint, advance to next
        if (creep.currentWaypointIndex < static_cast<i32>(creep.path.size()) - 1) {
            creep.currentWaypointIndex++;
            // Reset stuck timer when advancing
            creep.waypointStuckTime = 0.0f;
        }
        // If at last waypoint, continue moving towards it
        targetPos = getNextWaypointPosition(creep, transform.position);
        toWaypoint = targetPos - transform.position;
        distanceToWaypoint = glm::length(toWaypoint);
    }
    
    // Track time stuck at current waypoint
    static const f32 stuckThreshold = 5.0f; // 5 seconds
    if (distanceToWaypoint < waypointReachDistance * 2.0f) {
        creep.waypointStuckTime += deltaTime;
        if (creep.waypointStuckTime > stuckThreshold) {
            // Force advance to next waypoint if stuck too long
            if (creep.currentWaypointIndex < static_cast<i32>(creep.path.size()) - 1) {
                creep.currentWaypointIndex++;
                creep.waypointStuckTime = 0.0f;
                targetPos = getNextWaypointPosition(creep, transform.position);
                toWaypoint = targetPos - transform.position;
                distanceToWaypoint = glm::length(toWaypoint);
            }
        }
    } else {
        creep.waypointStuckTime = 0.0f;
    }
    
    // Calculate formation offset to maintain spacing
    Vec3 laneDir = glm::normalize(toWaypoint);
    if (glm::length(laneDir) < 0.01f) {
        laneDir = creep.laneDirection;
    }
    Vec3 perpDir = glm::normalize(Vec3(-laneDir.z, 0, laneDir.x));
    
    // Formation offset based on index
    f32 spacing = 2.0f;
    i32 row = creep.formationIndex / 3;
    i32 col = creep.formationIndex % 3;
    f32 lateralOffset = (col - 1) * spacing;
    
    // Reduce formation offset when close to waypoint to prevent circling
    f32 formationScale = 1.0f;
    if (distanceToWaypoint < 10.0f) {
        formationScale = distanceToWaypoint / 10.0f; // Gradually reduce offset
        formationScale = std::max(0.1f, formationScale); // Minimum 10% offset
    }
    
    // Apply formation offset to target
    Vec3 formationTarget = targetPos + perpDir * lateralOffset * formationScale;
    
    // Separation from nearby creeps (avoid clumping)
    Vec3 separation(0.0f);
    i32 nearbyCount = 0;
    const f32 separationRadius = 3.0f;
    const f32 separationStrength = 2.0f;
    
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<CreepComponent, TransformComponent>();
    for (auto other : view) {
        if (other == entity) continue;
        
        auto& otherCreep = view.get<CreepComponent>(other);
        auto& otherTransform = view.get<TransformComponent>(other);
        
        if (otherCreep.state == CreepState::Dead) continue;
        
        Vec3 diff = transform.position - otherTransform.position;
        f32 dist = glm::length(diff);
        
        if (dist > 0.1f && dist < separationRadius) {
            // Push away from nearby creeps
            separation += glm::normalize(diff) * (1.0f - dist / separationRadius);
            nearbyCount++;
        }
    }
    
    if (nearbyCount > 0) {
        separation = separation / static_cast<f32>(nearbyCount) * separationStrength;
    }
    
    // Move towards target with separation
    Vec3 direction = formationTarget - transform.position;
    f32 distance = glm::length(direction);
    
    if (distance > 0.1f) {
        direction = glm::normalize(direction);
        
        // Combine movement direction with separation (reduce separation influence when close to waypoint)
        f32 separationWeight = distanceToWaypoint > 5.0f ? 0.5f : 0.2f;
        Vec3 moveDir = glm::normalize(direction + separation * separationWeight);
        Vec3 movement = moveDir * creep.moveSpeed * deltaTime;
        
        transform.position += movement;
        
        // Update rotation to face movement direction
        if (glm::length(direction) > 0.001f) {
            f32 yaw = std::atan2(direction.x, direction.z);
            transform.rotation = glm::angleAxis(yaw, Vec3(0, 1, 0));
        }
    }
}

void CreepSystem::updateCreepCombat(Entity entity, CreepComponent& creep, TransformComponent& transform, f32 deltaTime) {
    if (creep.targetEntity == INVALID_ENTITY) {
        creep.state = CreepState::Moving;
        return;
    }
    
    // Get target position
    if (!entityManager_.hasComponent<TransformComponent>(creep.targetEntity)) {
        creep.targetEntity = INVALID_ENTITY;
        creep.state = CreepState::Moving;
        return;
    }
    
    auto& targetTransform = entityManager_.getComponent<TransformComponent>(creep.targetEntity);
    f32 distance = glm::length(targetTransform.position - transform.position);
    
    // Check if target is in range
    if (distance <= creep.attackRange) {
        // Face target
        Vec3 direction = glm::normalize(targetTransform.position - transform.position);
        if (glm::length(direction) > 0.001f) {
            f32 yaw = std::atan2(direction.x, direction.z);
            transform.rotation = glm::angleAxis(yaw, Vec3(0, 1, 0));
        }
        
        // Attack if cooldown is ready
        if (creep.attackCooldown <= 0.0f) {
            // For ranged creeps, create projectile
            if (creep.type == CreepType::Ranged || creep.type == CreepType::LargeRanged || creep.type == CreepType::MegaRanged) {
                // Create projectile entity
                Entity projectile = entityManager_.createEntity("Projectile");
                auto& projComp = entityManager_.addComponent<ProjectileComponent>(projectile);
                auto& projTransform = entityManager_.addComponent<TransformComponent>(projectile);
                
                projComp.attacker = entity;
                projComp.target = creep.targetEntity;
                projComp.teamId = creep.teamId;
                projComp.baseDamage = creep.damage;
                projComp.active = true;
                projComp.isTower = false;
                
                projTransform.position = transform.position + Vec3(0, 1, 0); // Slightly above creep
                
                // Add simple mesh for projectile visualization
                auto& mesh = entityManager_.addComponent<MeshComponent>(projectile, "Projectile");
                MeshGenerators::GenerateSphere(mesh, 0.1f, 8);
                mesh.gpuUploadNeeded = true;
                
                // Add material
                Entity materialEntity = entityManager_.createEntity("ProjectileMaterial");
                auto& material = entityManager_.addComponent<MaterialComponent>(materialEntity, "ProjectileMaterial");
                material.baseColor = Vec3(1.0f, 0.8f, 0.2f); // Yellow projectile
                material.emissiveColor = Vec3(0.2f, 0.1f, 0.0f);
                mesh.materialEntity = materialEntity;
            } else {
                // Melee attack - deal damage directly
                dealDamage(entity, creep.targetEntity, creep.damage);
            }
            
            creep.attackCooldown = 1.0f / creep.attackSpeed;
        }
    } else {
        // Target out of range, move closer or resume lane movement
        if (distance > creep.attackRange * 2.0f) {
            // Target too far, resume lane movement
            creep.targetEntity = INVALID_ENTITY;
            creep.state = CreepState::Moving;
        } else {
            // Move towards target
            Vec3 direction = glm::normalize(targetTransform.position - transform.position);
            Vec3 movement = direction * creep.moveSpeed * deltaTime;
            transform.position += movement;
            
            // Face target
            if (glm::length(direction) > 0.001f) {
                f32 yaw = std::atan2(direction.x, direction.z);
                transform.rotation = glm::angleAxis(yaw, Vec3(0, 1, 0));
            }
        }
    }
}

Entity CreepSystem::findNearestEnemy(Entity creepEntity, const CreepComponent& creep, const Vec3& position) {
    Entity nearestEnemy = INVALID_ENTITY;
    f32 nearestDistance = 9999.0f;
    
    // Aggro radius is larger than attack range - creeps will chase enemies
    const f32 aggroRadius = 15.0f;  // Distance to detect and aggro on enemies
    
    // First priority: enemy heroes (highest threat) - larger aggro radius for heroes
    Entity enemyHero = findNearestEnemyHero(position, creep.teamId, aggroRadius);
    if (enemyHero != INVALID_ENTITY) {
        if (entityManager_.hasComponent<TransformComponent>(enemyHero)) {
            auto& transform = entityManager_.getComponent<TransformComponent>(enemyHero);
            f32 distance = glm::length(transform.position - position);
            if (distance < nearestDistance) {
                nearestEnemy = enemyHero;
                nearestDistance = distance;
            }
        }
    }
    
    // Second priority: enemy creeps (only if no hero in range or creep is closer)
    Entity enemyCreep = findNearestEnemyCreep(position, creep.teamId, aggroRadius);
    if (enemyCreep != INVALID_ENTITY && enemyCreep != creepEntity) {
        if (entityManager_.hasComponent<TransformComponent>(enemyCreep)) {
            auto& transform = entityManager_.getComponent<TransformComponent>(enemyCreep);
            f32 distance = glm::length(transform.position - position);
            // Only switch to creep if it's significantly closer than hero
            if (distance < nearestDistance * 0.7f || nearestEnemy == INVALID_ENTITY) {
                nearestEnemy = enemyCreep;
                nearestDistance = distance;
            }
        }
    }
    
    // Third priority: enemy towers (only attack if no other targets or very close)
    Entity enemyTower = findNearestEnemyTower(position, creep.teamId, aggroRadius);
    if (enemyTower != INVALID_ENTITY) {
        if (entityManager_.hasComponent<TransformComponent>(enemyTower)) {
            auto& transform = entityManager_.getComponent<TransformComponent>(enemyTower);
            f32 distance = glm::length(transform.position - position);
            // Only attack tower if no other targets
            if (nearestEnemy == INVALID_ENTITY && distance < aggroRadius) {
                nearestEnemy = enemyTower;
                nearestDistance = distance;
            }
        }
    }
    
    return nearestEnemy;
}

Entity CreepSystem::findNearestEnemyCreep(const Vec3& position, i32 teamId, f32 searchRadius) {
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<CreepComponent, TransformComponent>();
    
    Entity nearest = INVALID_ENTITY;
    f32 nearestDistance = searchRadius;
    
    for (auto entity : view) {
        auto& creep = view.get<CreepComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        // Skip same team or dead creeps
        if (creep.teamId == teamId || creep.state == CreepState::Dead) {
            continue;
        }
        
        f32 distance = glm::length(transform.position - position);
        if (distance < nearestDistance) {
            nearest = entity;
            nearestDistance = distance;
        }
    }
    
    return nearest;
}

Entity CreepSystem::findNearestEnemyTower(const Vec3& position, i32 teamId, f32 searchRadius) {
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<ObjectComponent, TransformComponent>();
    
    Entity nearest = INVALID_ENTITY;
    f32 nearestDistance = searchRadius;
    
    for (auto entity : view) {
        auto& obj = view.get<ObjectComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        // Skip non-towers or same team
        if (obj.type != ObjectType::Tower || obj.teamId == teamId || obj.teamId == 0) {
            continue;
        }
        
        // Check if tower is alive
        if (entityManager_.hasComponent<HealthComponent>(entity)) {
            auto& health = entityManager_.getComponent<HealthComponent>(entity);
            if (health.isDead) {
                continue;
            }
        }
        
        f32 distance = glm::length(transform.position - position);
        if (distance < nearestDistance) {
            nearest = entity;
            nearestDistance = distance;
        }
    }
    
    return nearest;
}

Entity CreepSystem::findNearestEnemyHero(const Vec3& position, i32 teamId, f32 searchRadius) {
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<HeroComponent, TransformComponent>();
    
    Entity nearest = INVALID_ENTITY;
    f32 nearestDistance = searchRadius;
    
    for (auto entity : view) {
        auto& hero = view.get<HeroComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        // Skip same team or dead heroes
        if (hero.teamId == teamId || hero.state == HeroState::Dead) {
            continue;
        }
        
        // Skip invisible heroes
        if (hero.isInvisible()) {
            continue;
        }
        
        f32 distance = glm::length(transform.position - position);
        if (distance < nearestDistance) {
            nearest = entity;
            nearestDistance = distance;
        }
    }
    
    return nearest;
}

Vec3 CreepSystem::getNextWaypointPosition(const CreepComponent& creep, const Vec3& currentPos) const {
    if (creep.path.empty()) {
        // Fallback: move along lane direction
        return currentPos + creep.laneDirection * 10.0f;
    }
    
    if (creep.currentWaypointIndex >= static_cast<i32>(creep.path.size())) {
        // Reached end of path
        return creep.path.back();
    }
    
    return creep.path[creep.currentWaypointIndex];
}

void CreepSystem::dealDamage(Entity attacker, Entity target, f32 damage) {
    // Deal damage to creep
    if (entityManager_.hasComponent<CreepComponent>(target)) {
        auto& targetCreep = entityManager_.getComponent<CreepComponent>(target);
        targetCreep.currentHealth -= damage;
        
        if (targetCreep.currentHealth <= 0.0f) {
            targetCreep.currentHealth = 0.0f;
            targetCreep.state = CreepState::Dead;
            targetCreep.deathTime = 0.0f;
        }
    }
    
    // Deal damage to hero
    if (entityManager_.hasComponent<HeroComponent>(target)) {
        auto& targetHero = entityManager_.getComponent<HeroComponent>(target);
        
        // Check invulnerability
        if (targetHero.isInvulnerable()) {
            return;
        }
        
        // Apply armor reduction (physical damage)
        f32 armor = targetHero.armor;
        f32 armorReduction = 1.0f - (0.06f * armor) / (1.0f + 0.06f * std::abs(armor));
        f32 actualDamage = damage * armorReduction;
        
        targetHero.currentHealth -= actualDamage;
        
        if (targetHero.currentHealth <= 0.0f) {
            targetHero.currentHealth = 0.0f;
            targetHero.state = HeroState::Dead;
            targetHero.deaths++;
            // Calculate respawn time based on level
            targetHero.respawnTimer = static_cast<f32>(targetHero.level) * 2.5f;
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

void CreepSystem::cleanupDeadCreeps(f32 deltaTime) {
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<CreepComponent>();
    
    Vector<Entity> toDestroy;
    
    for (auto entity : view) {
        auto& creep = view.get<CreepComponent>(entity);
        
        if (creep.state == CreepState::Dead && creep.deathTime >= creep.deathDelay) {
            toDestroy.push_back(entity);
        }
    }
    
    for (Entity entity : toDestroy) {
        entityManager_.destroyEntity(entity);
    }
}

Entity CreepSystem::spawnCreep(Entity spawnPoint, CreepType type, i32 teamId, CreepLane lane) {
    if (!entityManager_.isValid(spawnPoint)) {
        return INVALID_ENTITY;
    }
    
    if (!entityManager_.hasComponent<TransformComponent>(spawnPoint)) {
        return INVALID_ENTITY;
    }
    
    auto& spawnTransform = entityManager_.getComponent<TransformComponent>(spawnPoint);
    
    // Create creep entity
    Entity creep = entityManager_.createEntity("Creep");
    auto& creepComp = entityManager_.addComponent<CreepComponent>(creep, teamId, lane);
    auto& transform = entityManager_.addComponent<TransformComponent>(creep);
    
    // Set creep type and stats (scaled for 16000x16000 map)
    creepComp.type = type;
    switch (type) {
        case CreepType::Melee:
            creepComp.maxHealth = 550.0f;
            creepComp.damage = 19.0f;
            creepComp.attackRange = 100.0f;   // Melee range (~100 units)
            creepComp.moveSpeed = 325.0f;     // Movement speed in units/sec
            break;
        case CreepType::Ranged:
            creepComp.maxHealth = 300.0f;
            creepComp.damage = 21.0f;
            creepComp.attackRange = 500.0f;   // Ranged attack range
            creepComp.moveSpeed = 325.0f;
            break;
        case CreepType::Siege:
            creepComp.maxHealth = 550.0f;
            creepComp.damage = 39.0f;
            creepComp.attackRange = 700.0f;   // Siege range
            creepComp.moveSpeed = 280.0f;     // Slower
            break;
        default:
            break;
    }
    
    creepComp.currentHealth = creepComp.maxHealth;
    creepComp.spawnPoint = spawnPoint;
    
    // Count existing creeps of same team/lane to determine formation position
    i32 creepIndex = 0;
    {
        auto& registry = entityManager_.getRegistry();
        auto view = registry.view<CreepComponent, TransformComponent>();
        for (auto entity : view) {
            auto& c = view.get<CreepComponent>(entity);
            auto& t = view.get<TransformComponent>(entity);
            if (c.teamId == teamId && c.lane == lane && c.state != CreepState::Dead) {
                // Count creeps near spawn point
                f32 dist = glm::length(t.position - spawnTransform.position);
                if (dist < 15.0f) {
                    creepIndex++;
                }
            }
        }
    }
    
    // Calculate formation offset (line formation perpendicular to lane direction)
    Vec3 laneDir = creepComp.laneDirection;
    if (glm::length(laneDir) < 0.1f) {
        laneDir = Vec3(1, 0, 0); // Default direction
    }
    Vec3 perpDir = glm::normalize(Vec3(-laneDir.z, 0, laneDir.x)); // Perpendicular to lane
    
    // Formation: spread creeps in a line with some depth (scaled for 16000 map)
    f32 spacing = 80.0f;  // Space between creeps (~80 units)
    i32 row = creepIndex / 3;  // 3 creeps per row
    i32 col = creepIndex % 3;
    
    // Offset from center (-1, 0, 1 for columns)
    f32 lateralOffset = (col - 1) * spacing;
    f32 depthOffset = row * spacing * 1.5f;  // Rows behind each other
    
    // Add small random variation to avoid perfect grid
    f32 randX = ((rand() % 100) / 100.0f - 0.5f) * 20.0f;
    f32 randZ = ((rand() % 100) / 100.0f - 0.5f) * 20.0f;
    
    Vec3 formationOffset = perpDir * lateralOffset - laneDir * depthOffset + Vec3(randX, 0, randZ);
    
    // Set spawn position with formation offset
    transform.position = spawnTransform.position + formationOffset;
    
    // Get lane waypoints for pathfinding
    creepComp.path = getLaneWaypoints(lane, teamId);
    creepComp.currentWaypointIndex = 0;
    
    // Store formation index for maintaining formation during movement
    creepComp.formationIndex = creepIndex;
    
    // Create mesh based on type (sized for 16000x16000 map)
    // Creeps: ~30-50 units radius, ~60-100 units height
    auto& mesh = entityManager_.addComponent<MeshComponent>(creep, "CreepMesh");
    switch (type) {
        case CreepType::Melee:
            MeshGenerators::GenerateCylinder(mesh, 35.0f, 70.0f, 12);
            break;
        case CreepType::Ranged:
            MeshGenerators::GenerateSphere(mesh, 30.0f, 12);
            break;
        case CreepType::Siege:
            MeshGenerators::GenerateCylinder(mesh, 50.0f, 100.0f, 8);
            break;
        default:
            MeshGenerators::GenerateCylinder(mesh, 35.0f, 70.0f, 12);
            break;
    }
    mesh.gpuUploadNeeded = true;
    
    // Create material
    Entity materialEntity = entityManager_.createEntity("CreepMaterial");
    auto& material = entityManager_.addComponent<MaterialComponent>(materialEntity, "CreepMaterial");
    
    // Team colors
    if (teamId == 1) {
        material.baseColor = Vec3(0.2f, 0.8f, 0.2f); // Green for Radiant
    } else {
        material.baseColor = Vec3(0.8f, 0.2f, 0.2f); // Red for Dire
    }
    
    mesh.materialEntity = materialEntity;
    
    // Add collision
    auto& collision = entityManager_.addComponent<CollisionComponent>(creep, CollisionShape::Capsule);
    collision.capsuleRadius = 0.8f;
    collision.capsuleHeight = 1.5f;
    collision.blocksMovement = true;
    
    return creep;
}

Vector<Vec3> CreepSystem::getLaneWaypoints(CreepLane lane, i32 teamId) const {
    Vector<Vec3> waypoints;
    
    // Get waypoints from placed waypoint objects
    auto& registry = entityManager_.getRegistry();
    auto view = registry.view<ObjectComponent, TransformComponent>();
    
    Vector<std::pair<i32, Vec3>> orderedWaypoints;
    
    for (auto entity : view) {
        auto& obj = view.get<ObjectComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        if (obj.type == ObjectType::Waypoint) {
            // Check if waypoint matches lane and team
            if (obj.waypointLane == static_cast<i32>(lane) || obj.waypointLane == -1) {
                // For now, use all waypoints regardless of team
                // In full implementation, would filter by team direction
                orderedWaypoints.push_back({obj.waypointOrder, transform.position});
            }
        }
    }
    
    // Sort by order
    std::sort(orderedWaypoints.begin(), orderedWaypoints.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    // Extract positions
    for (const auto& wp : orderedWaypoints) {
        waypoints.push_back(wp.second);
    }
    
    // Fallback: create simple lane path if no waypoints found
    if (waypoints.empty()) {
        Vec3 start, end;
        switch (lane) {
            case CreepLane::Top:
                start = teamId == 1 ? Vec3(50, 0, 250) : Vec3(250, 0, 50);
                end = teamId == 1 ? Vec3(250, 0, 50) : Vec3(50, 0, 250);
                break;
            case CreepLane::Middle:
                start = teamId == 1 ? Vec3(50, 0, 50) : Vec3(250, 0, 250);
                end = teamId == 1 ? Vec3(250, 0, 250) : Vec3(50, 0, 50);
                break;
            case CreepLane::Bottom:
                start = teamId == 1 ? Vec3(250, 0, 50) : Vec3(50, 0, 250);
                end = teamId == 1 ? Vec3(50, 0, 250) : Vec3(250, 0, 50);
                break;
        }
        
        waypoints.push_back(start);
        waypoints.push_back(Vec3((start.x + end.x) * 0.5f, 0, (start.z + end.z) * 0.5f));
        waypoints.push_back(end);
    }
    
    return waypoints;
}

bool CreepSystem::hasLineOfSight(const Vec3& from, const Vec3& to) const {
    // Simple implementation - in full version would raycast against terrain/obstacles
    return true;
}

bool CreepSystem::isInAttackRange(const Vec3& attackerPos, const Vec3& targetPos, f32 range) const {
    return glm::length(targetPos - attackerPos) <= range;
}

} // namespace WorldEditor