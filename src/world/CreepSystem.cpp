#include "CreepSystem.h"
#include "World.h"
#include "CollisionSystem.h"
#include "MeshGenerators.h"
#include "TerrainRaycast.h"
#include "core/Types.h"
#include "core/MathUtils.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>

namespace WorldEditor {

CreepSystem::CreepSystem(EntityManager& entityManager)
    : entityManager_(entityManager) {
    spdlog::info("CreepSystem initialized");
}

CreepSystem::~CreepSystem() {
    spdlog::info("CreepSystem destroyed");
}

void CreepSystem::resetSimulation() {
    // Reset spawn timers so the next start feels like a new round.
    spawnTimer_ = 0.0f;
    firstSpawnDone_ = false;

    // Reset auto-balance.
    damageCalibrated_ = false;
    damageMultiplier_ = 1.0f;

    // Reset projectile pooling (projectile entities themselves are destroyed by GameMode reset).
    projectilePoolCreepTeam1_.clear();
    projectilePoolCreepTeam2_.clear();
    projectilePoolTowerTeam1_.clear();
    projectilePoolTowerTeam2_.clear();
    projectileCreatedCreepTeam1_ = 0;
    projectileCreatedCreepTeam2_ = 0;
    projectileCreatedTowerTeam1_ = 0;
    projectileCreatedTowerTeam2_ = 0;

    // Forget cached projectile materials (they may have been destroyed by reset as well).
    projectileMatCreepTeam1_ = INVALID_ENTITY;
    projectileMatCreepTeam2_ = INVALID_ENTITY;
    projectileMatTowerTeam1_ = INVALID_ENTITY;
    projectileMatTowerTeam2_ = INVALID_ENTITY;
}

void CreepSystem::update(f32 deltaTime) {
    // One-time balancing: pick a damage multiplier so that a wave can resolve
    // within the spawn interval, given current spawn point spacing + move speed.
    if (!damageCalibrated_) {
        calibrateWaveDamage();
    }
    spawnCreeps(deltaTime);
    updateCreeps(deltaTime);
    updateProjectiles(deltaTime);
    cleanupDeadCreeps(deltaTime);
}

void CreepSystem::calibrateWaveDamage() {
    damageCalibrated_ = true;

    // Find 1v1 spawn distance (worst-case per-lane) to estimate time-to-meet.
    // We use this to budget time for combat before the next wave.
    auto& reg = entityManager_.getRegistry();
    auto spawnView = reg.view<const ObjectComponent, const TransformComponent>();

    struct SpawnInfo { Vec3 pos; i32 teamId; i32 lane; };
    Vector<SpawnInfo> spawns;
    for (auto e : spawnView) {
        const auto& obj = spawnView.get<const ObjectComponent>(e);
        if (obj.type != ObjectType::CreepSpawn) continue;
        if (obj.teamId != 1 && obj.teamId != 2) continue;
        const auto& tr = spawnView.get<const TransformComponent>(e);
        spawns.push_back({tr.position, obj.teamId, obj.spawnLane});
    }
    if (spawns.empty()) {
        return;
    }

    // Base move speed (what setupCreepStats sets).
    const f32 moveSpeed = 5.0f;

    // Compute worst-case (largest) team1-team2 distance among lanes we can pair.
    f32 worstDist = 0.0f;
    auto considersLane = [](i32 spawnLane, i32 lane) -> bool {
        return spawnLane == -1 || spawnLane == lane;
    };
    for (i32 lane = 0; lane <= 2; ++lane) {
        Vector<Vec3> t1;
        Vector<Vec3> t2;
        for (const auto& s : spawns) {
            if (!considersLane(s.lane, lane)) continue;
            if (s.teamId == 1) t1.push_back(s.pos);
            else t2.push_back(s.pos);
        }
        if (t1.empty() || t2.empty()) continue;

        // Use farthest pairing to be safe.
        f32 laneWorst = 0.0f;
        for (const auto& a : t1) {
            for (const auto& b : t2) {
                Vec3 d = b - a;
                d.y = 0.0f;
                laneWorst = std::max(laneWorst, glm::length(d));
            }
        }
        worstDist = std::max(worstDist, laneWorst);
    }
    if (worstDist <= 0.01f) {
        return;
    }

    const f32 timeToMeet = worstDist / (2.0f * std::max(0.01f, moveSpeed));
    f32 combatBudget = spawnInterval_ - timeToMeet;
    // If spawns are extremely far (slow units), keep a minimum combat budget to avoid insane multipliers.
    combatBudget = std::clamp(combatBudget, 2.0f, spawnInterval_);

    // Estimate wave HP and DPS for the standard wave: 4 melee + 1 ranged.
    // We compute using our base stats (no armor, no misses).
    CreepComponent melee{};
    CreepComponent ranged{};
    const f32 prevMult = damageMultiplier_;
    damageMultiplier_ = 1.0f; // compute baseline
    setupCreepStats(melee, CreepType::Melee);
    setupCreepStats(ranged, CreepType::Ranged);
    damageMultiplier_ = prevMult;

    const f32 waveHP = 4.0f * melee.maxHealth + 1.0f * ranged.maxHealth;
    const f32 waveDPS = 4.0f * (melee.damage * melee.attackSpeed) + 1.0f * (ranged.damage * ranged.attackSpeed);
    if (waveHP <= 0.01f || waveDPS <= 0.01f) {
        return;
    }

    // Required multiplier so that waveHP / (waveDPS * mult) <= combatBudget.
    f32 required = waveHP / (waveDPS * combatBudget);
    // Safety margin for non-ideal targeting, projectile travel, and movement jitter.
    required *= 1.25f;
    required = std::max(1.0f, required);
    required = std::min(required, 10.0f);

    damageMultiplier_ = required;
    spdlog::info("Creep wave auto-balance: worstDist={:.1f}, meet={:.1f}s, combatBudget={:.1f}s, dmgMult={:.2f}",
                 worstDist, timeToMeet, combatBudget, damageMultiplier_);
}

void CreepSystem::spawnCreeps(f32 deltaTime) {
    // Spawn immediately on first update (at 0 seconds)
    if (!firstSpawnDone_) {
        firstSpawnDone_ = true;
        spawnTimer_ = spawnInterval_; // Set to interval to trigger spawn
    } else {
        spawnTimer_ += deltaTime;
    }
    
    if (spawnTimer_ >= spawnInterval_) {
        spawnTimer_ = 0.0f;
        
        // Find all CreepSpawn entities
        auto& reg = entityManager_.getRegistry();
        auto spawnView = reg.view<ObjectComponent, TransformComponent>();
        
        for (auto spawnEntity : spawnView) {
            auto& objComp = spawnView.get<ObjectComponent>(spawnEntity);
            auto& spawnTransform = spawnView.get<TransformComponent>(spawnEntity);
            
            if (objComp.type == ObjectType::CreepSpawn && objComp.teamId > 0) {
                // Check creep limits
                i32 teamCreepCount = countActiveCreeps(objComp.teamId);
                i32 spawnCreepCount = countCreepsFromSpawn(spawnEntity);
                
                if (teamCreepCount >= maxCreepsPerTeam_) {
                    continue; // Too many creeps for this team
                }
                
                if (spawnCreepCount >= maxCreepsPerSpawn_) {
                    continue; // Too many creeps from this spawn
                }
                
                // Spawn creeps for specified lane(s)
                const Vec3 spawnPos = spawnTransform.position;
                
                // Determine which lanes to spawn for
                // spawnLane = -1 means all lanes, 0-2 means specific lane
                int startLane = (objComp.spawnLane >= 0) ? objComp.spawnLane : 0;
                int endLane = (objComp.spawnLane >= 0) ? objComp.spawnLane : 2;
                
                for (int lane = startLane; lane <= endLane; ++lane) {
                    // Spawn 5 creeps: 4 melee + 1 ranged (like Dota 2)
                    for (int i = 0; i < 5; ++i) {
                        // Check limits before each spawn
                        if (teamCreepCount >= maxCreepsPerTeam_ || spawnCreepCount >= maxCreepsPerSpawn_) {
                            break;
                        }
                        
                        // Random offset for positioning
                        Vec3 offset = Vec3(
                            (static_cast<float>(rand() % 100) - 50.0f) * 0.1f,
                            0.0f,
                            (static_cast<float>(rand() % 100) - 50.0f) * 0.1f
                        );
                        
                        // First 4 are melee, last one is ranged
                        CreepType creepType = (i < 4) ? CreepType::Melee : CreepType::Ranged;
                        createCreepEntity(objComp.teamId, static_cast<CreepLane>(lane), creepType, spawnPos + offset, spawnEntity);
                        teamCreepCount++;
                        spawnCreepCount++;
                    }
                }
            }
        }
    }
}

Entity CreepSystem::createCreepEntity(i32 teamId, CreepLane lane, CreepType type, const Vec3& position, Entity spawnPoint) {
    Entity creepE = entityManager_.createEntity("Creep_" + std::to_string(teamId) + "_" + std::to_string(static_cast<int>(lane)));
    
    // Find terrain to get proper height
    Vec3 spawnPos = position;
    auto& reg = entityManager_.getRegistry();
    auto terrainView = reg.view<TerrainComponent, TransformComponent>();
    for (auto terrainEntity : terrainView) {
        const auto& terrain = terrainView.get<TerrainComponent>(terrainEntity);
        const auto& terrainTransform = terrainView.get<TransformComponent>(terrainEntity);
        
        // Sample terrain height at spawn position
        const Vec3 localPos = position - terrainTransform.position;
        const float clampedX = std::clamp(localPos.x, 0.0f, terrain.size);
        const float clampedZ = std::clamp(localPos.z, 0.0f, terrain.size);
        
        if (terrain.resolution.x > 1 && terrain.resolution.y > 1 && terrain.size > 0.0f) {
            const float cellSize = terrain.size / float(terrain.resolution.x - 1);
            if (cellSize > 0.0f) {
                const float gridX = clampedX / cellSize;
                const float gridZ = clampedZ / cellSize;
                const int x = std::clamp(static_cast<int>(std::round(gridX)), 0, terrain.resolution.x - 1);
                const int z = std::clamp(static_cast<int>(std::round(gridZ)), 0, terrain.resolution.y - 1);
                float height = terrain.getHeightAt(x, z);
                spawnPos = Vec3(terrainTransform.position.x + clampedX, height + 0.5f, terrainTransform.position.z + clampedZ);
                break; // Use first terrain found
            }
        }
    }
    
    // Transform - larger size for better visibility
    auto& transform = entityManager_.addComponent<TransformComponent>(creepE);
    transform.position = spawnPos;
    transform.scale = Vec3(2.0f, 2.0f, 2.0f); // Larger scale for visibility
    
    // Creep component
    auto& creep = entityManager_.addComponent<CreepComponent>(creepE);
    creep.teamId = teamId;
    creep.lane = lane;
    creep.type = type;
    creep.state = CreepState::Moving;
    creep.spawnPoint = spawnPoint;
    creep.spawnTime = 0.0f;
    creep.laneDirection = calculateLaneDirection(lane, teamId);

    // Apply per-type stats (attack range, hp, dmg, etc.)
    setupCreepStats(creep, type);
    
    // Build path from waypoints
    creep.path = buildPathForLane(teamId, lane);
    creep.currentWaypointIndex = 0;
    
    // Set initial target position
    if (!creep.path.empty()) {
        creep.targetPosition = creep.path[0];
    } else {
        // Fallback to lane direction if no waypoints
        creep.targetPosition = spawnPos + creep.laneDirection * 50.0f;
    }
    
    // Mesh - size based on creep type
    auto& mesh = entityManager_.addComponent<MeshComponent>(creepE);
    mesh.name = "Creep";
    mesh.visible = true;
    
    // Different sizes for different creep types
    Vec3 meshSize(1.5f, 2.0f, 1.5f);
    if (type == CreepType::Ranged || type == CreepType::LargeRanged || type == CreepType::MegaRanged) {
        meshSize = Vec3(1.2f, 1.8f, 1.2f); // Slightly smaller for ranged
    } else if (type == CreepType::Siege || type == CreepType::LargeSiege || type == CreepType::MegaSiege) {
        meshSize = Vec3(2.0f, 2.5f, 2.0f); // Larger for siege
    }
    
    // Scale for large and mega versions
    if (type == CreepType::LargeMelee || type == CreepType::LargeRanged || type == CreepType::LargeSiege) {
        meshSize *= 1.5f;
        transform.scale = Vec3(3.0f, 3.0f, 3.0f);
    } else if (type == CreepType::MegaMelee || type == CreepType::MegaRanged || type == CreepType::MegaSiege) {
        meshSize *= 2.0f;
        transform.scale = Vec3(4.0f, 4.0f, 4.0f);
    }
    
    MeshGenerators::GenerateCube(mesh, meshSize);
    
    // Material - bright, contrasting colors
    Entity matE = entityManager_.createEntity("CreepMaterial_" + std::to_string(static_cast<u32>(creepE)));
    auto& mat = entityManager_.addComponent<MaterialComponent>(matE);
    mat.name = "CreepMaterial";
    if (teamId == 1) {
        mat.baseColor = Vec3(0.1f, 1.0f, 0.1f); // Bright green for Radiant
        mat.emissiveColor = Vec3(0.0f, 0.5f, 0.0f); // Green glow
    } else {
        mat.baseColor = Vec3(1.0f, 0.1f, 0.1f); // Bright red for Dire
        mat.emissiveColor = Vec3(0.5f, 0.0f, 0.0f); // Red glow
    }
    mat.roughness = 0.3f; // Slightly shiny
    mat.metallic = 0.0f;
    mat.gpuBufferCreated = false;
    mesh.materialEntity = matE;
    
    // Add collision component
    auto& collision = entityManager_.addComponent<CollisionComponent>(creepE);
    collision.shape = CollisionShape::Sphere;
    collision.radius = std::max(meshSize.x, std::max(meshSize.y, meshSize.z)) * 0.5f;
    collision.isStatic = false;
    collision.isTrigger = false;
    collision.blocksMovement = true;
    collision.offset = Vec3(0.0f, meshSize.y * 0.5f, 0.0f); // Center at half height
    
    spdlog::debug("Created creep: Team {}, Lane {}, Position ({:.1f}, {:.1f}, {:.1f})", 
                  teamId, static_cast<int>(lane), spawnPos.x, spawnPos.y, spawnPos.z);
    
    return creepE;
}

Vec3 CreepSystem::calculateLaneDirection(CreepLane lane, i32 teamId) {
    // Calculate direction along the lane towards enemy base
    Vec3 direction(0.0f);
    
    if (teamId == 1) { // Radiant (bottom-left) -> Dire (top-right)
        switch (lane) {
            case CreepLane::Top:
                direction = Vec3(0.3f, 0.0f, 0.7f); // More Z, less X
                break;
            case CreepLane::Middle:
                direction = Vec3(1.0f, 0.0f, 1.0f); // Diagonal
                break;
            case CreepLane::Bottom:
                direction = Vec3(0.7f, 0.0f, 0.3f); // More X, less Z
                break;
        }
    } else { // Dire (top-right) -> Radiant (bottom-left)
        switch (lane) {
            case CreepLane::Top:
                direction = Vec3(-0.3f, 0.0f, -0.7f);
                break;
            case CreepLane::Middle:
                direction = Vec3(-1.0f, 0.0f, -1.0f);
                break;
            case CreepLane::Bottom:
                direction = Vec3(-0.7f, 0.0f, -0.3f);
                break;
        }
    }
    
    return glm::normalize(direction);
}

void CreepSystem::updateCreeps(f32 deltaTime) {
    auto& reg = entityManager_.getRegistry();
    auto creepView = reg.view<CreepComponent, TransformComponent>();

    // Very cheap "contact aggro" so creeps don't walk past each other.
    auto findNearbyEnemyCreep = [&](Entity self, const CreepComponent& creep, const TransformComponent& tr, float radius) -> Entity {
        const float r2 = radius * radius;
        Entity best = INVALID_ENTITY;
        float bestD2 = std::numeric_limits<float>::max();

        for (auto other : creepView) {
            if (other == self) continue;
            const auto& oc = creepView.get<CreepComponent>(other);
            if (oc.state == CreepState::Dead) continue;
            if (oc.teamId == creep.teamId) continue;

            const auto& ot = creepView.get<TransformComponent>(other);
            Vec3 d = ot.position - tr.position;
            d.y = 0.0f;
            const float d2 = glm::dot(d, d);
            if (d2 <= r2 && d2 < bestD2) {
                bestD2 = d2;
                best = other;
            }
        }
        return best;
    };
    
    for (auto creepEntity : creepView) {
        auto& creep = creepView.get<CreepComponent>(creepEntity);
        auto& transform = creepView.get<TransformComponent>(creepEntity);
        
        // Dead creeps still need their death timer to advance so cleanupDeadCreeps can remove them.
        if (creep.state == CreepState::Dead) {
            creep.deathTime += deltaTime;
            continue;
        }
        
        // Update timers
        creep.spawnTime += deltaTime;
        if (creep.attackCooldown > 0.0f) {
            creep.attackCooldown -= deltaTime;
        }
        if (creep.targetSearchCooldown > 0.0f) {
            creep.targetSearchCooldown -= deltaTime;
        }
        if (creep.pathCheckCooldown > 0.0f) {
            creep.pathCheckCooldown -= deltaTime;
        }
        
        // Find target if we don't have one
        if (creep.targetEntity == INVALID_ENTITY || !entityManager_.isValid(creep.targetEntity)) {
            // If an enemy creep is very close, aggro immediately (prevents "passing by").
            const float contactAggroRange = std::max(creep.attackRange * 1.5f, 8.0f);
            Entity closeEnemy = findNearbyEnemyCreep(creepEntity, creep, transform, contactAggroRange);
            if (closeEnemy != INVALID_ENTITY) {
                creep.targetEntity = closeEnemy;
                creep.targetSearchCooldown = 0.05f;
            }

            // Perf: don't scan every frame for every creep.
            if (creep.targetEntity == INVALID_ENTITY && creep.targetSearchCooldown <= 0.0f) {
                creep.targetEntity = findTarget(creepEntity, creep, transform);
                // If we found something, we can scan slightly more often; otherwise back off.
                creep.targetSearchCooldown = (creep.targetEntity != INVALID_ENTITY) ? 0.10f : 0.25f;
            }
        }

        // If we have a target but it's already dead, clear it so we don't "attack empty space"
        // while the dead entity is still alive in the registry (cleanup delay).
        if (creep.targetEntity != INVALID_ENTITY && entityManager_.isValid(creep.targetEntity)) {
            bool targetDead = false;
            if (entityManager_.hasComponent<CreepComponent>(creep.targetEntity)) {
                const auto& tc = entityManager_.getComponent<CreepComponent>(creep.targetEntity);
                targetDead = (tc.state == CreepState::Dead);
            } else if (entityManager_.hasComponent<ObjectComponent>(creep.targetEntity) &&
                       entityManager_.hasComponent<HealthComponent>(creep.targetEntity)) {
                const auto& hp = entityManager_.getComponent<HealthComponent>(creep.targetEntity);
                targetDead = hp.isDead;
            }
            if (targetDead) {
                creep.targetEntity = INVALID_ENTITY;
            }
        }
        
        // If we have a target, check if we should attack or move
        if (creep.targetEntity != INVALID_ENTITY && entityManager_.isValid(creep.targetEntity)) {
            if (entityManager_.hasComponent<TransformComponent>(creep.targetEntity)) {
                auto& targetTransform = entityManager_.getComponent<TransformComponent>(creep.targetEntity);
                Vec3 toTarget = targetTransform.position - transform.position;
                // Use horizontal distance for attack checks (ignore height differences).
                toTarget.y = 0.0f;
                float distance = glm::length(toTarget);
                
                if (distance <= creep.attackRange) {
                    // In range: hold position and attack when cooldown is ready.
                    // (Previously we moved while cooldown>0, which made ranged units "run past" and look like they don't shoot.)
                    creep.state = CreepState::Attacking;
                    if (creep.attackCooldown <= 0.0001f) {
                        attackTarget(creepEntity, creep, creep.targetEntity, deltaTime);
                    }
                } else {
                    // Out of range: move towards target
                    creep.state = CreepState::Moving;
                    moveCreep(creepEntity, creep, transform, deltaTime);
                }
            }
        } else {
            // No target, move along lane
            creep.state = CreepState::Moving;
            moveCreep(creepEntity, creep, transform, deltaTime);
        }
    }
}

void CreepSystem::moveCreep(Entity creepEntity, CreepComponent& creep, TransformComponent& transform, f32 deltaTime) {
    Vec3 direction(0.0f);
    Vec3 targetPos = transform.position;
    
    if (creep.targetEntity != INVALID_ENTITY && entityManager_.isValid(creep.targetEntity)) {
        // Move towards target
        if (entityManager_.hasComponent<TransformComponent>(creep.targetEntity)) {
            auto& targetTransform = entityManager_.getComponent<TransformComponent>(creep.targetEntity);
            targetPos = targetTransform.position;
            direction = targetPos - transform.position;
            direction.y = 0.0f; // Keep on ground
            float dist = glm::length(direction);
            if (dist > 0.001f) {
                direction = glm::normalize(direction);
            }
        }
    } else {
        // Move along lane using waypoint path
        Vec3 toTarget = creep.targetPosition - transform.position;
        toTarget.y = 0.0f;
        float distToTarget = glm::length(toTarget);
        
        // Check if we've reached current waypoint
        if (distToTarget < 5.0f) {
            // Move to next waypoint in path
            if (!creep.path.empty() && creep.currentWaypointIndex + 1 < static_cast<i32>(creep.path.size())) {
                creep.currentWaypointIndex++;
                creep.targetPosition = creep.path[creep.currentWaypointIndex];
                toTarget = creep.targetPosition - transform.position;
                toTarget.y = 0.0f;
                distToTarget = glm::length(toTarget);
            } else {
                // Reached end of path - stop or use fallback
                // Try to find enemy base as final destination
                Entity enemyBase = findBaseForTeam(creep.teamId == 1 ? 2 : 1);
                if (enemyBase != INVALID_ENTITY && entityManager_.hasComponent<TransformComponent>(enemyBase)) {
                    const auto& baseTransform = entityManager_.getComponent<TransformComponent>(enemyBase);
                    creep.targetPosition = baseTransform.position;
                    toTarget = creep.targetPosition - transform.position;
                    toTarget.y = 0.0f;
                    distToTarget = glm::length(toTarget);
                } else {
                    // No base found, use fallback lane direction
                    creep.targetPosition += creep.laneDirection * 50.0f;
                    toTarget = creep.targetPosition - transform.position;
                    toTarget.y = 0.0f;
                    distToTarget = glm::length(toTarget);
                }
            }
        }
        
        // Check if path to waypoint is clear, if not find alternative path
        if (distToTarget > 0.001f) {
            // Get creep collision radius
            f32 creepRadius = 1.0f;
            if (entityManager_.hasComponent<CollisionComponent>(creepEntity)) {
                const auto& creepCol = entityManager_.getComponent<CollisionComponent>(creepEntity);
                if (creepCol.shape == CollisionShape::Sphere) {
                    creepRadius = creepCol.radius;
                } else if (creepCol.shape == CollisionShape::Box) {
                    Vec3 halfSize = creepCol.boxSize * 0.5f;
                    creepRadius = std::max({halfSize.x, halfSize.y, halfSize.z});
                }
            }
            
            // Check if direct path is clear (throttled).
            if (creep.pathCheckCooldown <= 0.0f) {
                creep.lastPathClear = isPathClear(transform.position, creep.targetPosition, creepRadius);
                creep.pathCheckCooldown = 0.15f; // ~6 checks/sec per creep max
            }

            if (!creep.lastPathClear) {
                // Path blocked, find alternative path around obstacle
                Vec3 alternativeTarget = findPathAroundObstacle(transform.position, creep.targetPosition, creepRadius);
                if (glm::length(alternativeTarget - transform.position) > 0.1f) {
                    // Use alternative target
                    toTarget = alternativeTarget - transform.position;
                    toTarget.y = 0.0f;
                    distToTarget = glm::length(toTarget);
                    if (distToTarget > 0.001f) {
                        direction = glm::normalize(toTarget);
                        targetPos = alternativeTarget;
                    } else {
                        direction = creep.laneDirection;
                        targetPos = transform.position + direction * 50.0f;
                    }
                } else {
                    // Couldn't find alternative, try to go around
                    direction = creep.laneDirection;
                    targetPos = transform.position + direction * 50.0f;
                }
            } else {
                // Path is clear, go directly to waypoint
                direction = glm::normalize(toTarget);
                targetPos = creep.targetPosition;
            }
        } else {
            // Fallback to lane direction
            direction = creep.laneDirection;
            targetPos = transform.position + direction * 50.0f;
        }
    }
    
    // Calculate movement with max distance per frame to prevent teleporting
    f32 maxMoveDistance = creep.moveSpeed * deltaTime;
    f32 maxMovePerFrame = 5.0f; // Prevent huge jumps if deltaTime is large
    maxMoveDistance = std::min(maxMoveDistance, maxMovePerFrame);
    
    Vec3 movement = direction * maxMoveDistance;
    Vec3 newPosition = transform.position + movement;
    
    // Update height based on terrain
    auto& reg = entityManager_.getRegistry();
    auto terrainView = reg.view<TerrainComponent, TransformComponent>();
    for (auto terrainEntity : terrainView) {
        const auto& terrain = terrainView.get<TerrainComponent>(terrainEntity);
        const auto& terrainTransform = terrainView.get<TransformComponent>(terrainEntity);
        
        // Sample terrain height at new position
        const Vec3 localPos = newPosition - terrainTransform.position;
        const float clampedX = std::clamp(localPos.x, 0.0f, terrain.size);
        const float clampedZ = std::clamp(localPos.z, 0.0f, terrain.size);
        
        if (terrain.resolution.x > 1 && terrain.resolution.y > 1 && terrain.size > 0.0f) {
            const float cellSize = terrain.size / float(terrain.resolution.x - 1);
            if (cellSize > 0.0f) {
                const float gridX = clampedX / cellSize;
                const float gridZ = clampedZ / cellSize;
                const int x = std::clamp(static_cast<int>(std::round(gridX)), 0, terrain.resolution.x - 1);
                const int z = std::clamp(static_cast<int>(std::round(gridZ)), 0, terrain.resolution.y - 1);
                float height = terrain.getHeightAt(x, z);
                newPosition.y = height + 1.0f; // Keep creep above terrain
                break;
            }
        }
    }
    
    // Check collision and adjust position using CollisionSystem
    if (world_ && entityManager_.hasComponent<CollisionComponent>(creepEntity)) {
        auto* collisionSystem = static_cast<CollisionSystem*>(world_->getSystem("CollisionSystem"));
        if (collisionSystem) {
            // Get creep collision radius
            const auto& creepCol = entityManager_.getComponent<CollisionComponent>(creepEntity);
            f32 creepRadius = 1.0f;
            if (creepCol.shape == CollisionShape::Sphere) {
                creepRadius = creepCol.radius;
            } else if (creepCol.shape == CollisionShape::Box) {
                Vec3 halfSize = creepCol.boxSize * 0.5f;
                creepRadius = std::max({halfSize.x, halfSize.y, halfSize.z});
            }
            
            // Check if desired position has collisions
            Vec3 adjustedPosition = collisionSystem->checkMovementCollision(creepEntity, newPosition, creepRadius);
            
            // If position was adjusted due to collision, try to find a path around obstacle
            if (glm::length(adjustedPosition - newPosition) > 0.1f) {
                // Obstacle detected - try to steer around it
                Vec3 desiredDir = glm::normalize(newPosition - transform.position);
                Vec3 adjustedDir = glm::normalize(adjustedPosition - transform.position);
                
                // Calculate steering direction (perpendicular to obstacle)
                Vec3 obstacleNormal = glm::normalize(transform.position - adjustedPosition);
                obstacleNormal.y = 0.0f;
                if (glm::length(obstacleNormal) > 0.001f) {
                    obstacleNormal = glm::normalize(obstacleNormal);
                    
                    // Try to go around obstacle (steer perpendicular to obstacle normal)
                    Vec3 steerDir = glm::cross(obstacleNormal, Vec3(0.0f, 1.0f, 0.0f));
                    steerDir.y = 0.0f;
                    if (glm::length(steerDir) > 0.001f) {
                        steerDir = glm::normalize(steerDir);
                        
                        // Choose direction that's closer to desired direction
                        if (glm::dot(steerDir, desiredDir) < 0.0f) {
                            steerDir = -steerDir;
                        }
                        
                        // Try new position by steering around obstacle
                        Vec3 steerPosition = transform.position + steerDir * maxMoveDistance * 0.5f;
                        steerPosition.y = newPosition.y; // Keep same height
                        
                        // Check if steered position is valid
                        Vec3 finalSteerPos = collisionSystem->checkMovementCollision(creepEntity, steerPosition, creepRadius);
                        if (glm::length(finalSteerPos - steerPosition) < 0.1f) {
                            // Steered position is valid, use it
                            adjustedPosition = finalSteerPos;
                        } else {
                            // Try opposite direction
                            steerPosition = transform.position - steerDir * maxMoveDistance * 0.5f;
                            steerPosition.y = newPosition.y;
                            Vec3 finalSteerPos2 = collisionSystem->checkMovementCollision(creepEntity, steerPosition, creepRadius);
                            if (glm::length(finalSteerPos2 - steerPosition) < 0.1f) {
                                adjustedPosition = finalSteerPos2;
                            }
                        }
                    }
                }
                
                newPosition = adjustedPosition;
            } else {
                // No collision, use desired position
                newPosition = adjustedPosition;
            }
        }
    }
    
    transform.position = newPosition;
    
    // Face movement direction
    if (glm::length(movement) > 0.001f) {
        Vec3 forward = glm::normalize(movement);
        // Create rotation from forward direction
        // Use lookAt matrix and extract quaternion
        Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
        Vec3 right = glm::cross(up, forward);
        if (glm::length(right) < 0.001f) {
            right = Vec3(1.0f, 0.0f, 0.0f);
        } else {
            right = glm::normalize(right);
        }
        up = glm::cross(forward, right);
        
        Mat4 lookAtMat = Mat4(1.0f);
        lookAtMat[0] = Vec4(right, 0.0f);
        lookAtMat[1] = Vec4(up, 0.0f);
        lookAtMat[2] = Vec4(forward, 0.0f);
        transform.rotation = glm::quat_cast(lookAtMat);
    }
}

Entity CreepSystem::findTarget(Entity creepEntity, const CreepComponent& creep, const TransformComponent& creepTransform) {
    Entity bestTarget = INVALID_ENTITY;
    float bestScore = -1.0f; // Higher score = better target
    
    auto& reg = entityManager_.getRegistry();
    
    // Search for enemy creeps, towers, or buildings
    auto targetView = reg.view<TransformComponent>();
    
    for (auto targetEntity : targetView) {
        if (targetEntity == creepEntity) continue;
        
        // Check if it's an enemy
        bool isEnemy = false;
        bool isDead = false;
        f32 priority = 1.0f; // Base priority
        
        // Check if it's a creep
        if (reg.all_of<CreepComponent>(targetEntity)) {
            auto& targetCreep = reg.get<CreepComponent>(targetEntity);
            if (targetCreep.teamId != creep.teamId && targetCreep.state != CreepState::Dead) {
                isEnemy = true;
                // Prioritize creeps that are attacking us (aggression)
                if (targetCreep.targetEntity == creepEntity) {
                    priority = 2.0f; // Higher priority for aggressive enemies
                }
            } else if (targetCreep.state == CreepState::Dead) {
                isDead = true;
            }
        }
        // Check if it's a tower or building
        else if (reg.all_of<ObjectComponent>(targetEntity)) {
            auto& objComp = reg.get<ObjectComponent>(targetEntity);
            if ((objComp.type == ObjectType::Tower || objComp.type == ObjectType::Building) &&
                objComp.teamId != creep.teamId && objComp.teamId > 0) {
                isEnemy = true;
                // Prioritize towers over buildings
                if (objComp.type == ObjectType::Tower) {
                    priority = 1.5f;
                }
                
                // Check if building is dead
                if (reg.all_of<HealthComponent>(targetEntity)) {
                    auto& health = reg.get<HealthComponent>(targetEntity);
                    if (health.isDead) {
                        isDead = true;
                    }
                }
            }
        }
        
        if (isEnemy && !isDead) {
            auto& targetTransform = reg.get<TransformComponent>(targetEntity);
            Vec3 toTarget = targetTransform.position - creepTransform.position;
            // Horizontal aggro range.
            toTarget.y = 0.0f;
            float distance = glm::length(toTarget);
            
            // Only consider targets within aggro range (1.5x attack range)
            if (distance <= creep.attackRange * 1.5f) {
                // Score = priority / distance (closer and higher priority = better)
                float score = priority / (distance + 1.0f);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestTarget = targetEntity;
                }
            }
        }
    }
    
    return bestTarget;
}

void CreepSystem::attackTarget(Entity creepEntity, CreepComponent& creep, Entity targetEntity, f32 deltaTime) {
    if (!entityManager_.isValid(targetEntity)) {
        creep.targetEntity = INVALID_ENTITY;
        return;
    }
    
    // Check if attack cooldown is ready (discrete attacks, not continuous damage)
    if (creep.attackCooldown > 0.0f) {
        return; // Still on cooldown
    }

    // Ranged/siege: fire a projectile, apply damage on hit.
    // (Melee stays instant so it "looks like sword hit".)
    if (isRangedType(creep.type)) {
        // If target is already dead, drop it (prevents ranged from "shooting corpses").
        if (entityManager_.hasComponent<CreepComponent>(targetEntity)) {
            const auto& tc = entityManager_.getComponent<CreepComponent>(targetEntity);
            if (tc.state == CreepState::Dead) {
                creep.targetEntity = INVALID_ENTITY;
                return;
            }
        } else if (entityManager_.hasComponent<ObjectComponent>(targetEntity) &&
                   entityManager_.hasComponent<HealthComponent>(targetEntity)) {
            const auto& hp = entityManager_.getComponent<HealthComponent>(targetEntity);
            if (hp.isDead) {
                creep.targetEntity = INVALID_ENTITY;
                return;
            }
        }

        fireProjectile(creepEntity, creep, targetEntity);
        // Put attack on cooldown immediately so we don't spawn many projectiles per frame.
        creep.attackCooldown = 1.0f / std::max(0.01f, creep.attackSpeed);
        return;
    }
    
    // Perform attack
    f32 actualDamage = 0.0f;
    
    // Apply damage to creep target
    if (entityManager_.hasComponent<CreepComponent>(targetEntity)) {
        auto& targetCreep = entityManager_.getComponent<CreepComponent>(targetEntity);
        if (targetCreep.state == CreepState::Dead) {
            creep.targetEntity = INVALID_ENTITY;
            return;
        }
        
        // Calculate damage with armor reduction
        actualDamage = calculateDamage(creep.damage, targetCreep.armor);
        targetCreep.currentHealth -= actualDamage;
        
        if (targetCreep.currentHealth <= 0.0f) {
            targetCreep.currentHealth = 0.0f;
            targetCreep.state = CreepState::Dead;
            targetCreep.deathTime = 0.0f; // Will be updated in updateCreeps
            creep.targetEntity = INVALID_ENTITY;
            
            // Hide dead creep (will be removed after death delay)
            if (entityManager_.hasComponent<MeshComponent>(targetEntity)) {
                auto& mesh = entityManager_.getComponent<MeshComponent>(targetEntity);
                mesh.visible = false;
            }
        }
    }
    // Attack towers/buildings with health component
    else if (entityManager_.hasComponent<ObjectComponent>(targetEntity) && 
             entityManager_.hasComponent<HealthComponent>(targetEntity)) {
        auto& health = entityManager_.getComponent<HealthComponent>(targetEntity);
        if (health.isDead) {
            creep.targetEntity = INVALID_ENTITY;
            return;
        }
        
        // Calculate damage with armor reduction
        actualDamage = calculateDamage(creep.damage, health.armor);
        health.currentHealth -= actualDamage;
        
        if (health.currentHealth <= 0.0f) {
            health.currentHealth = 0.0f;
            health.isDead = true;
            creep.targetEntity = INVALID_ENTITY;
        }
    }
    // Attack towers/buildings without health component (legacy support)
    else if (entityManager_.hasComponent<ObjectComponent>(targetEntity)) {
        // Just track that we're attacking (no damage for now)
    }
    
    // Reset attack cooldown for next attack
    creep.attackCooldown = 1.0f / std::max(0.01f, creep.attackSpeed);
}

bool CreepSystem::isRangedType(CreepType type) const {
    switch (type) {
        case CreepType::Ranged:
        case CreepType::Siege:
        case CreepType::LargeRanged:
        case CreepType::LargeSiege:
        case CreepType::MegaRanged:
        case CreepType::MegaSiege:
            return true;
        default:
            return false;
    }
}

void CreepSystem::fireProjectile(Entity attacker, CreepComponent& creep, Entity targetEntity) {
    if (!entityManager_.isValid(targetEntity) || !entityManager_.hasComponent<TransformComponent>(targetEntity)) {
        creep.targetEntity = INVALID_ENTITY;
        return;
    }
    if (!entityManager_.hasComponent<TransformComponent>(attacker)) {
        return;
    }

    Entity projE = acquireProjectileEntity(creep.teamId, /*isTower*/ false);
    if (projE == INVALID_ENTITY) {
        return; // pool exhausted; skip shot
    }

    auto& tr = entityManager_.getComponent<TransformComponent>(projE);
    const Vec3 attackerPos = entityManager_.getComponent<TransformComponent>(attacker).position;
    tr.position = attackerPos + Vec3(0.0f, 2.0f, 0.0f);

    auto& proj = entityManager_.getComponent<ProjectileComponent>(projE);
    proj.attacker = attacker;
    proj.target = targetEntity;
    proj.teamId = creep.teamId;
    proj.active = true;
    proj.isTower = false;
    proj.baseDamage = creep.damage;
    proj.speed = 90.0f;
    proj.hitRadius = 1.2f;
    proj.life = 0.0f;
    proj.maxLife = 6.0f;
    if (entityManager_.hasComponent<MeshComponent>(projE)) {
        entityManager_.getComponent<MeshComponent>(projE).visible = true;
    }
}

void CreepSystem::spawnTowerProjectile(Entity towerEntity, i32 teamId, f32 baseDamage, Entity targetEntity) {
    if (!entityManager_.isValid(targetEntity) || !entityManager_.hasComponent<TransformComponent>(targetEntity)) {
        return;
    }
    if (!entityManager_.hasComponent<TransformComponent>(towerEntity)) {
        return;
    }
    if (teamId <= 0) {
        return;
    }

    Entity projE = acquireProjectileEntity(teamId, /*isTower*/ true);
    if (projE == INVALID_ENTITY) {
        return;
    }

    auto& tr = entityManager_.getComponent<TransformComponent>(projE);
    const Vec3 towerPos = entityManager_.getComponent<TransformComponent>(towerEntity).position;
    tr.position = towerPos + Vec3(0.0f, 10.0f, 0.0f);

    auto& proj = entityManager_.getComponent<ProjectileComponent>(projE);
    proj.attacker = towerEntity;
    proj.target = targetEntity;
    proj.teamId = teamId;
    proj.active = true;
    proj.isTower = true;
    proj.baseDamage = baseDamage;
    proj.speed = 160.0f;
    proj.hitRadius = 1.4f;
    proj.life = 0.0f;
    proj.maxLife = 4.0f;

    if (entityManager_.hasComponent<MeshComponent>(projE)) {
        entityManager_.getComponent<MeshComponent>(projE).visible = true;
    }
}

void CreepSystem::applyProjectileHit(const ProjectileComponent& proj) {
    if (!entityManager_.isValid(proj.target)) {
        return;
    }

    // Apply damage to creep target
    if (entityManager_.hasComponent<CreepComponent>(proj.target)) {
        auto& targetCreep = entityManager_.getComponent<CreepComponent>(proj.target);
        if (targetCreep.state == CreepState::Dead) {
            return;
        }

        const f32 actualDamage = calculateDamage(proj.baseDamage, targetCreep.armor);
        targetCreep.currentHealth -= actualDamage;
        if (targetCreep.currentHealth <= 0.0f) {
            targetCreep.currentHealth = 0.0f;
            targetCreep.state = CreepState::Dead;
            targetCreep.deathTime = 0.0f;
            if (entityManager_.hasComponent<MeshComponent>(proj.target)) {
                entityManager_.getComponent<MeshComponent>(proj.target).visible = false;
            }
        }
        return;
    }

    // Apply damage to towers/buildings with health
    if (entityManager_.hasComponent<ObjectComponent>(proj.target) &&
        entityManager_.hasComponent<HealthComponent>(proj.target)) {
        auto& health = entityManager_.getComponent<HealthComponent>(proj.target);
        if (health.isDead) {
            return;
        }
        const f32 actualDamage = calculateDamage(proj.baseDamage, health.armor);
        health.currentHealth -= actualDamage;
        if (health.currentHealth <= 0.0f) {
            health.currentHealth = 0.0f;
            health.isDead = true;
        }
    }
}

void CreepSystem::updateProjectiles(f32 deltaTime) {
    auto& reg = entityManager_.getRegistry();
    auto view = reg.view<ProjectileComponent, TransformComponent, MeshComponent>();

    for (auto e : view) {
        auto& proj = view.get<ProjectileComponent>(e);
        auto& tr = view.get<TransformComponent>(e);
        auto& mesh = view.get<MeshComponent>(e);

        if (!proj.active) {
            // Keep pooled projectiles alive but hidden.
            if (mesh.visible) {
                mesh.visible = false;
            }
            continue;
        }

        proj.life += deltaTime;
        if (proj.life >= proj.maxLife) {
            // Return to pool.
            proj.active = false;
            proj.attacker = INVALID_ENTITY;
            proj.target = INVALID_ENTITY;
            proj.life = 0.0f;
            mesh.visible = false;
            Vector<Entity>& pool =
                (proj.isTower
                    ? ((proj.teamId == 1) ? projectilePoolTowerTeam1_ : projectilePoolTowerTeam2_)
                    : ((proj.teamId == 1) ? projectilePoolCreepTeam1_ : projectilePoolCreepTeam2_));
            pool.push_back(e);
            continue;
        }

        if (!entityManager_.isValid(proj.target) || !entityManager_.hasComponent<TransformComponent>(proj.target)) {
            // Target disappeared; return to pool.
            proj.active = false;
            proj.attacker = INVALID_ENTITY;
            proj.target = INVALID_ENTITY;
            proj.life = 0.0f;
            mesh.visible = false;
            Vector<Entity>& pool =
                (proj.isTower
                    ? ((proj.teamId == 1) ? projectilePoolTowerTeam1_ : projectilePoolTowerTeam2_)
                    : ((proj.teamId == 1) ? projectilePoolCreepTeam1_ : projectilePoolCreepTeam2_));
            pool.push_back(e);
            continue;
        }

        Vec3 targetPos = entityManager_.getComponent<TransformComponent>(proj.target).position;
        // Aim a bit above ground so it doesn't clip into terrain.
        targetPos.y += entityManager_.hasComponent<CreepComponent>(proj.target) ? 1.5f : 4.0f;

        Vec3 to = targetPos - tr.position;
        const float dist = glm::length(to);
        if (dist <= std::max(0.01f, proj.hitRadius)) {
            applyProjectileHit(proj);
            // Hit -> return to pool.
            proj.active = false;
            proj.attacker = INVALID_ENTITY;
            proj.target = INVALID_ENTITY;
            proj.life = 0.0f;
            mesh.visible = false;
            Vector<Entity>& pool =
                (proj.isTower
                    ? ((proj.teamId == 1) ? projectilePoolTowerTeam1_ : projectilePoolTowerTeam2_)
                    : ((proj.teamId == 1) ? projectilePoolCreepTeam1_ : projectilePoolCreepTeam2_));
            pool.push_back(e);
            continue;
        }

        if (dist > 0.0001f) {
            const Vec3 dir = to / dist;
            const float step = proj.speed * deltaTime;
            tr.position += dir * std::min(step, dist);
        }
    }
}

Entity CreepSystem::acquireProjectileEntity(i32 teamId, bool isTower) {
    if (teamId != 1 && teamId != 2) {
        return INVALID_ENTITY;
    }

    Vector<Entity>& pool =
        (isTower
            ? ((teamId == 1) ? projectilePoolTowerTeam1_ : projectilePoolTowerTeam2_)
            : ((teamId == 1) ? projectilePoolCreepTeam1_ : projectilePoolCreepTeam2_));

    if (!pool.empty()) {
        Entity e = pool.back();
        pool.pop_back();
        // Ensure visual is correct if it was created long ago.
        initProjectileVisual(e, teamId, isTower);
        return e;
    }

    i32& created =
        (isTower
            ? ((teamId == 1) ? projectileCreatedTowerTeam1_ : projectileCreatedTowerTeam2_)
            : ((teamId == 1) ? projectileCreatedCreepTeam1_ : projectileCreatedCreepTeam2_));

    if (created >= projectileMaxPerPool_) {
        return INVALID_ENTITY;
    }
    created++;

    Entity projE = entityManager_.createEntity(isTower ? "TowerProjectile" : "Projectile");
    auto& tr = entityManager_.addComponent<TransformComponent>(projE);
    tr.scale = Vec3(1.0f);

    auto& proj = entityManager_.addComponent<ProjectileComponent>(projE);
    proj.teamId = teamId;
    proj.active = false;
    proj.isTower = isTower;

    // Visual
    auto& mesh = entityManager_.addComponent<MeshComponent>(projE);
    mesh.name = isTower ? "TowerProjectile" : "Projectile";
    mesh.visible = false;
    MeshGenerators::GenerateSphere(mesh, isTower ? 0.55f : 0.45f, 12);

    initProjectileVisual(projE, teamId, isTower);
    return projE;
}

void CreepSystem::initProjectileVisual(Entity projE, i32 teamId, bool isTower) {
    if (!entityManager_.hasComponent<MeshComponent>(projE)) {
        return;
    }
    auto& mesh = entityManager_.getComponent<MeshComponent>(projE);

    Entity& matRef =
        isTower
            ? ((teamId == 1) ? projectileMatTowerTeam1_ : projectileMatTowerTeam2_)
            : ((teamId == 1) ? projectileMatCreepTeam1_ : projectileMatCreepTeam2_);

    if (matRef == INVALID_ENTITY || !entityManager_.isValid(matRef)) {
        matRef = entityManager_.createEntity(isTower
            ? (teamId == 1 ? "TowerProjectileMaterial_Team1" : "TowerProjectileMaterial_Team2")
            : (teamId == 1 ? "ProjectileMaterial_Team1" : "ProjectileMaterial_Team2"));
        auto& mat = entityManager_.addComponent<MaterialComponent>(matRef);
        mat.name = isTower
            ? (teamId == 1 ? "TowerProjectileMaterial_Team1" : "TowerProjectileMaterial_Team2")
            : (teamId == 1 ? "ProjectileMaterial_Team1" : "ProjectileMaterial_Team2");

        if (teamId == 1) {
            mat.baseColor = isTower ? Vec3(0.35f, 1.0f, 0.35f) : Vec3(0.25f, 1.0f, 0.25f);
            mat.emissiveColor = isTower ? Vec3(0.15f, 0.85f, 0.15f) : Vec3(0.10f, 0.75f, 0.10f);
        } else {
            mat.baseColor = isTower ? Vec3(1.0f, 0.35f, 0.35f) : Vec3(1.0f, 0.25f, 0.25f);
            mat.emissiveColor = isTower ? Vec3(0.85f, 0.15f, 0.15f) : Vec3(0.75f, 0.10f, 0.10f);
        }
        mat.roughness = isTower ? 0.15f : 0.2f;
        mat.metallic = 0.0f;
        mat.gpuBufferCreated = false;
    }

    mesh.materialEntity = matRef;
}

bool CreepSystem::isEnemy(Entity entity1, Entity entity2) const {
    i32 team1 = 0, team2 = 0;
    
    if (entityManager_.hasComponent<CreepComponent>(entity1)) {
        team1 = entityManager_.getComponent<CreepComponent>(entity1).teamId;
    } else if (entityManager_.hasComponent<ObjectComponent>(entity1)) {
        team1 = entityManager_.getComponent<ObjectComponent>(entity1).teamId;
    }
    
    if (entityManager_.hasComponent<CreepComponent>(entity2)) {
        team2 = entityManager_.getComponent<CreepComponent>(entity2).teamId;
    } else if (entityManager_.hasComponent<ObjectComponent>(entity2)) {
        team2 = entityManager_.getComponent<ObjectComponent>(entity2).teamId;
    }
    
    return (team1 > 0 && team2 > 0 && team1 != team2);
}

f32 CreepSystem::calculateDamage(f32 baseDamage, f32 targetArmor) const {
    // Dota 2-style armor formula: damage reduction = (armor * 0.06) / (1 + armor * 0.06)
    // Actual damage = baseDamage * (1 - damageReduction)
    if (targetArmor >= 0.0f) {
        f32 damageReduction = (targetArmor * 0.06f) / (1.0f + targetArmor * 0.06f);
        return baseDamage * (1.0f - damageReduction);
    } else {
        // Negative armor increases damage
        f32 damageIncrease = (std::abs(targetArmor) * 0.06f) / (1.0f + std::abs(targetArmor) * 0.06f);
        return baseDamage * (1.0f + damageIncrease);
    }
}

i32 CreepSystem::countActiveCreeps(i32 teamId) const {
    i32 count = 0;
    auto& reg = entityManager_.getRegistry();
    auto creepView = reg.view<CreepComponent>();
    
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        if (creep.teamId == teamId && creep.state != CreepState::Dead) {
            count++;
        }
    }
    
    return count;
}

i32 CreepSystem::countCreepsFromSpawn(Entity spawnPoint) const {
    i32 count = 0;
    auto& reg = entityManager_.getRegistry();
    auto creepView = reg.view<CreepComponent>();
    
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        if (creep.spawnPoint == spawnPoint && creep.state != CreepState::Dead) {
            count++;
        }
    }
    
    return count;
}

void CreepSystem::cleanupDeadCreeps(f32 deltaTime) {
    auto& reg = entityManager_.getRegistry();
    auto creepView = reg.view<CreepComponent>();
    
    Vector<Entity> toRemove;
    
    for (auto creepEntity : creepView) {
        auto& creep = creepView.get<CreepComponent>(creepEntity);
        
        // Remove creeps that have been dead long enough
        if (creep.state == CreepState::Dead && creep.deathTime >= creep.deathDelay) {
            toRemove.push_back(creepEntity);
        }
    }
    
    // Remove dead creeps
    for (Entity entity : toRemove) {
        entityManager_.destroyEntity(entity);
    }
    
    if (!toRemove.empty()) {
        spdlog::debug("Cleaned up {} dead creeps", toRemove.size());
    }
}

Vector<Vec3> CreepSystem::buildPathForLane(i32 teamId, CreepLane lane) const {
    Vector<Vec3> path;
    auto& reg = entityManager_.getRegistry();
    
    // Find all waypoints for this team and lane
    struct WaypointInfo {
        Entity entity;
        Vec3 position;
        i32 order;
    };
    Vector<WaypointInfo> waypoints;
    
    auto waypointView = reg.view<ObjectComponent, TransformComponent>();
    for (auto entity : waypointView) {
        const auto& objComp = waypointView.get<ObjectComponent>(entity);
        const auto& transform = waypointView.get<TransformComponent>(entity);
        
        if (objComp.type == ObjectType::Waypoint) {
            // Check if waypoint matches team and lane
            bool matchesTeam = (objComp.teamId == teamId || objComp.teamId == 0);
            bool matchesLane = (objComp.waypointLane == static_cast<i32>(lane) || objComp.waypointLane == -1);
            
            if (matchesTeam && matchesLane) {
                waypoints.push_back({entity, transform.position, objComp.waypointOrder});
            }
        }
    }
    
    // Sort by order
    std::sort(waypoints.begin(), waypoints.end(), 
              [](const WaypointInfo& a, const WaypointInfo& b) {
                  return a.order < b.order;
              });
    
    // Build path from waypoints
    for (const auto& wp : waypoints) {
        path.push_back(wp.position);
    }
    
    // If no waypoints found, try to find enemy base as final destination
    Entity enemyBase = findBaseForTeam(teamId == 1 ? 2 : 1);
    if (enemyBase != INVALID_ENTITY && entityManager_.hasComponent<TransformComponent>(enemyBase)) {
        const auto& baseTransform = entityManager_.getComponent<TransformComponent>(enemyBase);
        path.push_back(baseTransform.position);
    }
    
    return path;
}

Entity CreepSystem::findBaseForTeam(i32 teamId) const {
    auto& reg = entityManager_.getRegistry();
    auto baseView = reg.view<ObjectComponent, TransformComponent>();
    
    for (auto entity : baseView) {
        const auto& objComp = baseView.get<ObjectComponent>(entity);
        if (objComp.type == ObjectType::Base && objComp.teamId == teamId) {
            return entity;
        }
    }
    
    return INVALID_ENTITY;
}

Vec3 CreepSystem::getNextWaypoint(const CreepComponent& creep, const TransformComponent& transform) const {
    // If path is empty, fallback to lane direction
    if (creep.path.empty()) {
        return transform.position + creep.laneDirection * 50.0f;
    }
    
    // Check if we've reached current waypoint
    if (creep.currentWaypointIndex < static_cast<i32>(creep.path.size())) {
        Vec3 currentWaypoint = creep.path[creep.currentWaypointIndex];
        Vec3 toWaypoint = currentWaypoint - transform.position;
        toWaypoint.y = 0.0f;
        float dist = glm::length(toWaypoint);
        
        // If close enough to waypoint, move to next one
        if (dist < 5.0f && creep.currentWaypointIndex + 1 < static_cast<i32>(creep.path.size())) {
            return creep.path[creep.currentWaypointIndex + 1];
        }
        
        return currentWaypoint;
    }
    
    // Reached end of path, return last waypoint
    return creep.path.back();
}

void CreepSystem::setupCreepStats(CreepComponent& creep, CreepType type) {
    // Base stats for different creep types (similar to Dota 2)
    switch (type) {
        case CreepType::Melee:
            creep.maxHealth = 550.0f;
            creep.currentHealth = 550.0f;
            creep.damage = 19.0f;
            creep.attackRange = 5.0f;
            creep.armor = 0.0f;
            break;
        case CreepType::Ranged:
            creep.maxHealth = 300.0f;
            creep.currentHealth = 300.0f;
            creep.damage = 21.0f;
            creep.attackRange = 10.0f;
            creep.armor = 0.0f;
            break;
        case CreepType::Siege:
            creep.maxHealth = 800.0f;
            creep.currentHealth = 800.0f;
            creep.damage = 40.0f;
            creep.attackRange = 35.0f;
            creep.armor = 0.0f;
            break;
        case CreepType::LargeMelee:
            creep.maxHealth = 1100.0f;
            creep.currentHealth = 1100.0f;
            creep.damage = 38.0f;
            creep.attackRange = 5.0f;
            creep.armor = 2.0f;
            break;
        case CreepType::LargeRanged:
            creep.maxHealth = 600.0f;
            creep.currentHealth = 600.0f;
            creep.damage = 42.0f;
            creep.attackRange = 10.0f;
            creep.armor = 2.0f;
            break;
        case CreepType::LargeSiege:
            creep.maxHealth = 1600.0f;
            creep.currentHealth = 1600.0f;
            creep.damage = 80.0f;
            creep.attackRange = 35.0f;
            creep.armor = 2.0f;
            break;
        case CreepType::MegaMelee:
            creep.maxHealth = 2200.0f;
            creep.currentHealth = 2200.0f;
            creep.damage = 76.0f;
            creep.attackRange = 5.0f;
            creep.armor = 5.0f;
            break;
        case CreepType::MegaRanged:
            creep.maxHealth = 1200.0f;
            creep.currentHealth = 1200.0f;
            creep.damage = 84.0f;
            creep.attackRange = 10.0f;
            creep.armor = 5.0f;
            break;
        case CreepType::MegaSiege:
            creep.maxHealth = 3200.0f;
            creep.currentHealth = 3200.0f;
            creep.damage = 160.0f;
            creep.attackRange = 35.0f;
            creep.armor = 5.0f;
            break;
    }
    
    // All creeps have same move speed and attack speed (can be adjusted per type if needed)
    creep.moveSpeed = 5.0f;
    creep.attackSpeed = 1.0f;
    creep.damage *= damageMultiplier_;
}

bool CreepSystem::isPathClear(const Vec3& from, const Vec3& to, f32 radius) const {
    if (!world_) {
        return true; // Assume clear if no world reference
    }
    
    auto* collisionSystem = static_cast<CollisionSystem*>(world_->getSystem("CollisionSystem"));
    if (!collisionSystem) {
        return true;
    }
    
    // Sample points along the path to check for obstacles
    Vec3 direction = to - from;
    direction.y = 0.0f;
    f32 distance = glm::length(direction);
    
    if (distance < 0.1f) {
        return true;
    }
    
    direction = glm::normalize(direction);
    
    // Check multiple points along the path.
    // Perf: clamp number of checks so we don't explode cost on long segments / many creeps.
    const f32 safeRadius = std::max(0.25f, radius);
    const i32 unclampedChecks = static_cast<i32>(distance / (safeRadius * 4.0f)) + 1;
    const i32 numChecks = std::clamp(unclampedChecks, 1, 8);
    const f32 stepSize = distance / static_cast<f32>(numChecks);
    
    for (i32 i = 1; i <= numChecks; ++i) {
        Vec3 checkPos = from + direction * (stepSize * static_cast<f32>(i));
        checkPos.y = from.y; // Keep same height
        
        // Fast early-out: do not allocate a vector for every sample.
        if (collisionSystem->hasBlockingCollisionAt(checkPos, radius, INVALID_ENTITY)) {
            return false;
        }
    }
    
    return true;
}

Vec3 CreepSystem::findPathAroundObstacle(const Vec3& from, const Vec3& to, f32 radius) const {
    if (!world_) {
        return to; // Fallback to target if no world reference
    }
    
    auto* collisionSystem = static_cast<CollisionSystem*>(world_->getSystem("CollisionSystem"));
    if (!collisionSystem) {
        return to;
    }
    
    Vec3 direction = to - from;
    direction.y = 0.0f;
    f32 distance = glm::length(direction);
    
    if (distance < 0.1f) {
        return to;
    }
    
    direction = glm::normalize(direction);
    
    // Try perpendicular directions (left and right)
    Vec3 perpendicular = glm::cross(direction, Vec3(0.0f, 1.0f, 0.0f));
    perpendicular.y = 0.0f;
    if (glm::length(perpendicular) > 0.001f) {
        perpendicular = glm::normalize(perpendicular);
    } else {
        // If direction is vertical, use arbitrary perpendicular
        perpendicular = Vec3(1.0f, 0.0f, 0.0f);
    }
    
    // Try going around obstacle at different distances
    const f32 avoidanceDistances[] = {radius * 3.0f, radius * 5.0f, radius * 7.0f};
    
    for (f32 avoidDist : avoidanceDistances) {
        for (i32 side = -1; side <= 1; side += 2) { // -1 (left) and 1 (right)
            Vec3 sideOffset = perpendicular * (avoidDist * static_cast<f32>(side));
            
            // Try intermediate point
            Vec3 intermediate = from + direction * (distance * 0.5f) + sideOffset;
            intermediate.y = from.y;
            
            // Check if intermediate point is clear
            if (isPathClear(from, intermediate, radius) && isPathClear(intermediate, to, radius)) {
                // This path is clear, use intermediate point as waypoint
                return intermediate;
            }
        }
    }
    
    // If all attempts failed, try to go perpendicular to obstacle
    // Find nearest obstacle
    Vector<Entity> nearbyObstacles = collisionSystem->getCollidingEntities(from, radius * 5.0f);
    for (Entity obstacle : nearbyObstacles) {
        if (entityManager_.hasComponent<CollisionComponent>(obstacle) &&
            entityManager_.hasComponent<TransformComponent>(obstacle)) {
            const auto& col = entityManager_.getComponent<CollisionComponent>(obstacle);
            if (col.blocksMovement && !col.isTrigger) {
                const auto& trans = entityManager_.getComponent<TransformComponent>(obstacle);
                Vec3 obstaclePos = trans.position;
                Vec3 toObstacle = obstaclePos - from;
                toObstacle.y = 0.0f;
                
                if (glm::length(toObstacle) < radius * 10.0f) {
                    // Go perpendicular to obstacle
                    Vec3 avoidDir = glm::normalize(toObstacle);
                    Vec3 avoidPerp = glm::cross(avoidDir, Vec3(0.0f, 1.0f, 0.0f));
                    avoidPerp.y = 0.0f;
                    if (glm::length(avoidPerp) > 0.001f) {
                        avoidPerp = glm::normalize(avoidPerp);
                        // Choose direction that's closer to target
                        if (glm::dot(avoidPerp, direction) < 0.0f) {
                            avoidPerp = -avoidPerp;
                        }
                        Vec3 avoidPos = from + avoidPerp * (radius * 4.0f);
                        avoidPos.y = from.y;
                        return avoidPos;
                    }
                }
            }
        }
    }
    
    // Fallback: return position slightly offset from current
    return from + direction * (radius * 2.0f);
}

} // namespace WorldEditor
