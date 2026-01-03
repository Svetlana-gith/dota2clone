#include "HeroSystem.h"
#include "World.h"
#include "MeshGenerators.h"
#include "ParticleSystem.h"
#include <algorithm>
#include <cmath>

namespace WorldEditor {

HeroSystem::HeroSystem(EntityManager& entityManager) 
    : entityManager_(entityManager) {
}

void HeroSystem::update(f32 deltaTime) {
    auto& registry = entityManager_.getRegistry();
    
    auto view = registry.view<HeroComponent, TransformComponent>();
    for (auto entity : view) {
        auto& hero = view.get<HeroComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        
        // Update ability cooldowns
        updateHeroAbilities(entity, hero, deltaTime);
        
        // Update item cooldowns
        updateItemCooldowns(hero, deltaTime);
        
        // Update buffs/debuffs
        updateBuffs(entity, hero, deltaTime);
        
        // Handle respawn
        if (hero.state == HeroState::Dead) {
            handleRespawn(entity, hero, deltaTime);
            continue;
        }
        
        // Regeneration (affected by buffs)
        f32 hpRegen = hero.healthRegen;
        f32 mpRegen = hero.manaRegen;
        
        for (const auto& buff : hero.buffs) {
            if (buff.type == BuffType::Regeneration) {
                hpRegen += buff.value;
            } else if (buff.type == BuffType::ManaRegen) {
                mpRegen += buff.value;
            }
        }
        
        hero.currentHealth = std::min(hero.maxHealth, hero.currentHealth + hpRegen * deltaTime);
        hero.currentMana = std::min(hero.maxMana, hero.currentMana + mpRegen * deltaTime);
        
        // Update attack cooldown
        hero.attackCooldown = std::max(0.0f, hero.attackCooldown - deltaTime);
        
        // Check if stunned
        if (hero.isStunned()) {
            hero.state = HeroState::Stunned;
            continue;
        } else if (hero.state == HeroState::Stunned) {
            hero.state = HeroState::Idle;
        }
        
        // Update AI/behavior
        updateHeroAI(entity, hero, transform, deltaTime);
    }
}

void HeroSystem::updateHeroAI(Entity entity, HeroComponent& hero, TransformComponent& transform, f32 deltaTime) {
    // Enhanced AI for non-player heroes
    if (!hero.isPlayerControlled) {
        updateEnemyHeroAI(entity, hero, transform, deltaTime);
        return;
    }
    
    switch (hero.state) {
        case HeroState::Idle:
            break;
            
        case HeroState::Moving:
            updateHeroMovement(entity, hero, transform, deltaTime);
            break;
            
        case HeroState::Attacking:
            updateHeroCombat(entity, hero, transform, deltaTime);
            break;
            
        case HeroState::CastingAbility:
            // Handle ability casting
            if (hero.currentCastingAbility >= 0 && hero.currentCastingAbility < 6) {
                auto& ability = hero.abilities[hero.currentCastingAbility];
                hero.castTimer -= deltaTime;
                
                if (hero.castTimer <= 0.0f) {
                    // Ability fires - execute effect based on ability index
                    executeAbilityEffect(entity, hero, hero.currentCastingAbility, transform);
                    
                    ability.currentCooldown = ability.data.cooldown;
                    hero.currentCastingAbility = -1;
                    hero.state = HeroState::Idle;
                }
            } else {
                hero.state = HeroState::Idle;
            }
            break;
            
        case HeroState::Stunned:
            // Can't do anything while stunned
            break;
            
        case HeroState::Dead:
            // Handled in update()
            break;
    }
}

void HeroSystem::updateEnemyHeroAI(Entity entity, HeroComponent& hero, TransformComponent& transform, f32 deltaTime) {
    // AI decision making for enemy heroes
    
    // If stunned, do nothing
    if (hero.isStunned()) {
        hero.state = HeroState::Stunned;
        return;
    }
    
    // If dead, do nothing
    if (hero.state == HeroState::Dead) return;
    
    // Health check - retreat if low HP
    f32 hpPercent = hero.currentHealth / hero.maxHealth;
    bool lowHealth = hpPercent < 0.3f;
    
    // Constants for AI behavior
    const f32 AGGRO_RADIUS = 12.0f;      // Distance to aggro on player hero
    const f32 ATTACK_SEARCH_RADIUS = 20.0f; // Distance to search for targets
    const f32 LANE_MOVE_SPEED = 5.0f;
    
    // Find player hero
    Entity playerHero = playerHero_;
    Vec3 playerPos = Vec3(0);
    bool playerAlive = false;
    f32 distToPlayer = 9999.0f;
    
    if (playerHero != INVALID_ENTITY && entityManager_.hasComponent<HeroComponent>(playerHero)) {
        auto& player = entityManager_.getComponent<HeroComponent>(playerHero);
        if (player.state != HeroState::Dead) {
            playerAlive = true;
            playerPos = entityManager_.getComponent<TransformComponent>(playerHero).position;
            distToPlayer = glm::length(playerPos - transform.position);
        }
    }
    
    // Find best target based on priority:
    // 1. Player hero if in aggro range
    // 2. Enemy creeps
    // 3. Enemy towers
    // 4. Move along lane
    
    Entity bestTarget = INVALID_ENTITY;
    Vec3 bestTargetPos = Vec3(0);
    f32 bestTargetDist = 9999.0f;
    bool targetIsHero = false;
    bool targetIsCreep = false;
    bool targetIsTower = false;
    
    // Priority 1: Check if player is in aggro range
    if (playerAlive && distToPlayer <= AGGRO_RADIUS && !lowHealth) {
        bestTarget = playerHero;
        bestTargetPos = playerPos;
        bestTargetDist = distToPlayer;
        targetIsHero = true;
    }
    
    // Priority 2: Find enemy creeps (if player not in aggro range)
    if (bestTarget == INVALID_ENTITY) {
        auto& registry = entityManager_.getRegistry();
        auto creepView = registry.view<CreepComponent, TransformComponent>();
        
        for (auto creepEntity : creepView) {
            auto& creep = creepView.get<CreepComponent>(creepEntity);
            auto& creepTransform = creepView.get<TransformComponent>(creepEntity);
            
            // Skip same team or dead creeps
            if (creep.teamId == hero.teamId || creep.state == CreepState::Dead) continue;
            
            f32 dist = glm::length(creepTransform.position - transform.position);
            if (dist < ATTACK_SEARCH_RADIUS && dist < bestTargetDist) {
                bestTarget = creepEntity;
                bestTargetPos = creepTransform.position;
                bestTargetDist = dist;
                targetIsCreep = true;
            }
        }
    }
    
    // Priority 3: Find enemy towers (if no creeps)
    if (bestTarget == INVALID_ENTITY) {
        auto& registry = entityManager_.getRegistry();
        auto towerView = registry.view<ObjectComponent, TransformComponent>();
        
        for (auto towerEntity : towerView) {
            auto& obj = towerView.get<ObjectComponent>(towerEntity);
            auto& towerTransform = towerView.get<TransformComponent>(towerEntity);
            
            // Skip non-towers or same team
            if (obj.type != ObjectType::Tower || obj.teamId == hero.teamId || obj.teamId == 0) continue;
            
            // Check if tower is alive
            if (entityManager_.hasComponent<HealthComponent>(towerEntity)) {
                auto& health = entityManager_.getComponent<HealthComponent>(towerEntity);
                if (health.isDead) continue;
            }
            
            f32 dist = glm::length(towerTransform.position - transform.position);
            if (dist < ATTACK_SEARCH_RADIUS * 2 && dist < bestTargetDist) {
                bestTarget = towerEntity;
                bestTargetPos = towerTransform.position;
                bestTargetDist = dist;
                targetIsTower = true;
            }
        }
    }
    
    // Re-check player aggro even if we have another target (player gets priority if very close)
    if (playerAlive && distToPlayer <= AGGRO_RADIUS * 0.5f && !lowHealth && !targetIsHero) {
        bestTarget = playerHero;
        bestTargetPos = playerPos;
        bestTargetDist = distToPlayer;
        targetIsHero = true;
        targetIsCreep = false;
        targetIsTower = false;
    }
    
    // AI State machine
    switch (hero.state) {
        case HeroState::Idle:
        case HeroState::Moving:
            if (lowHealth && playerAlive && distToPlayer < 20.0f) {
                // Retreat - move away from player
                Vec3 retreatDir = glm::normalize(transform.position - playerPos);
                hero.targetPosition = transform.position + retreatDir * 15.0f;
                hero.state = HeroState::Moving;
            } else if (bestTarget != INVALID_ENTITY) {
                // We have a target
                if (bestTargetDist <= hero.attackRange) {
                    // In attack range - attack!
                    hero.targetEntity = bestTarget;
                    hero.state = HeroState::Attacking;
                } else {
                    // Move towards target
                    hero.targetPosition = bestTargetPos;
                    hero.state = HeroState::Moving;
                }
                
                // Try to use abilities on target
                if (targetIsHero || targetIsCreep) {
                    tryUseAbilityAI(entity, hero, transform, bestTarget, bestTargetPos);
                }
            } else {
                // No target - move along lane towards enemy base
                // Default lane direction based on team
                Vec3 laneDir = (hero.teamId == 2) ? Vec3(-1, 0, -1) : Vec3(1, 0, 1);
                laneDir = glm::normalize(laneDir);
                hero.targetPosition = transform.position + laneDir * 10.0f;
                hero.state = HeroState::Moving;
            }
            
            // Update movement if moving
            if (hero.state == HeroState::Moving) {
                updateHeroMovement(entity, hero, transform, deltaTime);
            }
            break;
            
        case HeroState::Attacking:
            // Check if target still valid
            if (hero.targetEntity == INVALID_ENTITY || !entityManager_.isValid(hero.targetEntity)) {
                hero.state = HeroState::Idle;
                break;
            }
            
            // Check if target is dead
            if (entityManager_.hasComponent<CreepComponent>(hero.targetEntity)) {
                auto& targetCreep = entityManager_.getComponent<CreepComponent>(hero.targetEntity);
                if (targetCreep.state == CreepState::Dead) {
                    hero.targetEntity = INVALID_ENTITY;
                    hero.state = HeroState::Idle;
                    break;
                }
            }
            if (entityManager_.hasComponent<HealthComponent>(hero.targetEntity)) {
                auto& targetHealth = entityManager_.getComponent<HealthComponent>(hero.targetEntity);
                if (targetHealth.isDead) {
                    hero.targetEntity = INVALID_ENTITY;
                    hero.state = HeroState::Idle;
                    break;
                }
            }
            
            // Low health - retreat
            if (lowHealth && playerAlive && distToPlayer < 15.0f) {
                hero.state = HeroState::Idle;
                break;
            }
            
            // Check if player entered aggro range (switch target)
            if (playerAlive && distToPlayer <= AGGRO_RADIUS && hero.targetEntity != playerHero && !lowHealth) {
                hero.targetEntity = playerHero;
            }
            
            updateHeroCombat(entity, hero, transform, deltaTime);
            
            // Try abilities during combat
            if (hero.targetEntity != INVALID_ENTITY && entityManager_.hasComponent<TransformComponent>(hero.targetEntity)) {
                Vec3 targetPos = entityManager_.getComponent<TransformComponent>(hero.targetEntity).position;
                tryUseAbilityAI(entity, hero, transform, hero.targetEntity, targetPos);
            }
            break;
            
        case HeroState::CastingAbility:
            if (hero.currentCastingAbility >= 0 && hero.currentCastingAbility < 6) {
                auto& ability = hero.abilities[hero.currentCastingAbility];
                hero.castTimer -= deltaTime;
                
                if (hero.castTimer <= 0.0f) {
                    executeAbilityEffect(entity, hero, hero.currentCastingAbility, transform);
                    ability.currentCooldown = ability.data.cooldown;
                    hero.currentCastingAbility = -1;
                    hero.state = HeroState::Idle;
                }
            } else {
                hero.state = HeroState::Idle;
            }
            break;
            
        default:
            break;
    }
}

void HeroSystem::tryUseAbilityAI(Entity entity, HeroComponent& hero, TransformComponent& transform, 
                                  Entity targetEntity, const Vec3& targetPos) {
    // AI tries to use abilities intelligently
    f32 distToTarget = glm::length(targetPos - transform.position);
    
    for (int i = 0; i < 4; i++) {
        auto& ability = hero.abilities[i];
        
        // Skip if not learned or on cooldown
        if (ability.level <= 0 || ability.currentCooldown > 0) continue;
        
        // Skip if not enough mana
        if (hero.currentMana < ability.data.manaCost) continue;
        
        // Check range
        if (distToTarget > ability.data.castRange) continue;
        
        // Use the ability!
        hero.currentMana -= ability.data.manaCost;
        hero.currentCastingAbility = i;
        hero.castTimer = ability.data.castPoint;
        hero.targetPosition = targetPos;
        hero.targetEntity = targetEntity;
        hero.state = HeroState::CastingAbility;
        break;
    }
}

void HeroSystem::updateHeroMovement(Entity entity, HeroComponent& hero, TransformComponent& transform, f32 deltaTime) {
    // Can't move if rooted
    if (hero.isRooted()) {
        return;
    }
    
    if (hero.movePath.empty() || hero.currentPathIndex >= static_cast<i32>(hero.movePath.size())) {
        hero.state = HeroState::Idle;
        return;
    }
    
    Vec3 targetPos = hero.movePath[hero.currentPathIndex];
    Vec3 direction = targetPos - transform.position;
    direction.y = 0.0f; // Keep on ground plane
    f32 distance = glm::length(direction);
    
    if (distance < 1.0f) {
        // Reached waypoint
        hero.currentPathIndex++;
        if (hero.currentPathIndex >= static_cast<i32>(hero.movePath.size())) {
            hero.state = HeroState::Idle;
            hero.movePath.clear();
        }
        return;
    }
    
    // Move towards target with calculated move speed
    direction = glm::normalize(direction);
    f32 actualMoveSpeed = calculateMoveSpeed(hero);
    f32 moveDistance = actualMoveSpeed * deltaTime * 0.1f; // Faster movement
    
    if (moveDistance > distance) {
        transform.position = targetPos;
    } else {
        transform.position += direction * moveDistance;
    }
    
    // Face movement direction
    if (glm::length(direction) > 0.001f) {
        f32 yaw = std::atan2(direction.x, direction.z);
        transform.rotation = glm::angleAxis(yaw, Vec3(0, 1, 0));
    }
}

void HeroSystem::updateHeroCombat(Entity entity, HeroComponent& hero, TransformComponent& transform, f32 deltaTime) {
    // Can't attack if disarmed
    if (hero.isDisarmed()) {
        hero.state = HeroState::Idle;
        return;
    }
    
    // Check if target is still valid
    if (hero.targetEntity == INVALID_ENTITY || !entityManager_.isValid(hero.targetEntity)) {
        hero.targetEntity = INVALID_ENTITY;
        hero.state = HeroState::Idle;
        return;
    }
    
    // Check if target is still alive
    if (entityManager_.hasComponent<CreepComponent>(hero.targetEntity)) {
        auto& targetCreep = entityManager_.getComponent<CreepComponent>(hero.targetEntity);
        if (targetCreep.state == CreepState::Dead) {
            hero.targetEntity = INVALID_ENTITY;
            hero.state = HeroState::Idle;
            return;
        }
    }
    if (entityManager_.hasComponent<HeroComponent>(hero.targetEntity)) {
        auto& targetHero = entityManager_.getComponent<HeroComponent>(hero.targetEntity);
        if (targetHero.state == HeroState::Dead) {
            hero.targetEntity = INVALID_ENTITY;
            hero.state = HeroState::Idle;
            return;
        }
    }
    
    // Get target position
    if (!entityManager_.hasComponent<TransformComponent>(hero.targetEntity)) {
        hero.targetEntity = INVALID_ENTITY;
        hero.state = HeroState::Idle;
        return;
    }
    
    auto& targetTransform = entityManager_.getComponent<TransformComponent>(hero.targetEntity);
    Vec3 toTarget = targetTransform.position - transform.position;
    toTarget.y = 0.0f;
    f32 distance = glm::length(toTarget);
    
    // Check if in attack range
    if (distance <= hero.attackRange * 0.1f) { // Scale for world units
        // Face target
        if (distance > 0.001f) {
            Vec3 dir = glm::normalize(toTarget);
            f32 yaw = std::atan2(dir.x, dir.z);
            transform.rotation = glm::angleAxis(yaw, Vec3(0, 1, 0));
        }
        
        // Attack if cooldown ready
        if (hero.attackCooldown <= 0.0f) {
            f32 damage = calculateDamage(hero);
            dealDamage(entity, hero.targetEntity, damage, false);
            
            // Calculate attack cooldown based on attack speed
            f32 baseAttackTime = 1.7f; // Base attack time
            f32 attacksPerSecond = (100.0f + calculateAttackSpeed(hero)) / 100.0f / baseAttackTime;
            hero.attackCooldown = 1.0f / attacksPerSecond;
        }
    } else if (!hero.isRooted()) {
        // Move towards target
        Vec3 direction = glm::normalize(toTarget);
        f32 moveDistance = calculateMoveSpeed(hero) * deltaTime * 0.1f;
        transform.position += direction * moveDistance;
        
        // Face movement direction
        f32 yaw = std::atan2(direction.x, direction.z);
        transform.rotation = glm::angleAxis(yaw, Vec3(0, 1, 0));
    }
}

void HeroSystem::updateHeroAbilities(Entity entity, HeroComponent& hero, f32 deltaTime) {
    for (int i = 0; i < 6; ++i) {
        if (hero.abilities[i].level > 0) {
            hero.abilities[i].currentCooldown = std::max(0.0f, hero.abilities[i].currentCooldown - deltaTime);
        }
    }
}

void HeroSystem::recalculateStats(HeroComponent& hero) {
    // Calculate current stats based on level
    f32 levelBonus = static_cast<f32>(hero.level - 1);
    hero.strength = hero.baseStrength + hero.strengthGain * levelBonus;
    hero.agility = hero.baseAgility + hero.agilityGain * levelBonus;
    hero.intelligence = hero.baseIntelligence + hero.intelligenceGain * levelBonus;
    
    // Add item bonuses
    f32 bonusStr = 0.0f, bonusAgi = 0.0f, bonusInt = 0.0f;
    f32 bonusDmg = 0.0f, bonusArmor = 0.0f, bonusAS = 0.0f;
    f32 bonusHP = 0.0f, bonusMana = 0.0f;
    f32 bonusHPRegen = 0.0f, bonusManaRegen = 0.0f;
    
    for (i32 i = 0; i < 6; ++i) { // Only main inventory slots
        const auto& item = hero.inventory[i];
        if (item.data.name.empty()) continue;
        
        bonusStr += item.data.bonusStrength;
        bonusAgi += item.data.bonusAgility;
        bonusInt += item.data.bonusIntelligence;
        bonusDmg += item.data.bonusDamage;
        bonusArmor += item.data.bonusArmor;
        bonusAS += item.data.bonusAttackSpeed;
        bonusHP += item.data.bonusHealth;
        bonusMana += item.data.bonusMana;
        bonusHPRegen += item.data.bonusHealthRegen;
        bonusManaRegen += item.data.bonusManaRegen;
    }
    
    // Add buff bonuses
    for (const auto& buff : hero.buffs) {
        if (buff.remainingTime <= 0.0f) continue;
        
        switch (buff.type) {
            case BuffType::Strength_Bonus: bonusStr += buff.value; break;
            case BuffType::Agility_Bonus: bonusAgi += buff.value; break;
            case BuffType::Intelligence_Bonus: bonusInt += buff.value; break;
            case BuffType::DamageBonus: bonusDmg += buff.value; break;
            case BuffType::ArmorBonus: bonusArmor += buff.value; break;
            case BuffType::ArmorReduction: bonusArmor -= buff.value; break;
            case BuffType::AttackSpeedBonus: bonusAS += buff.value; break;
            case BuffType::AttackSpeedSlow: bonusAS -= buff.value; break;
            default: break;
        }
    }
    
    hero.strength += bonusStr;
    hero.agility += bonusAgi;
    hero.intelligence += bonusInt;
    
    // Derived stats
    // HP: 200 base + 22 per strength (Dota 2 formula)
    f32 oldMaxHealth = hero.maxHealth;
    hero.maxHealth = 200.0f + hero.strength * 22.0f + bonusHP;
    hero.healthRegen = 0.1f + hero.strength * 0.1f + bonusHPRegen;
    
    // Adjust current health proportionally
    if (oldMaxHealth > 0.0f) {
        hero.currentHealth = hero.currentHealth * (hero.maxHealth / oldMaxHealth);
    }
    
    // Mana: 75 base + 12 per intelligence
    f32 oldMaxMana = hero.maxMana;
    hero.maxMana = 75.0f + hero.intelligence * 12.0f + bonusMana;
    hero.manaRegen = 0.01f + hero.intelligence * 0.05f + bonusManaRegen;
    
    // Adjust current mana proportionally
    if (oldMaxMana > 0.0f) {
        hero.currentMana = hero.currentMana * (hero.maxMana / oldMaxMana);
    }
    
    // Armor: 0 base + 0.167 per agility
    hero.armor = hero.agility * 0.167f + bonusArmor;
    
    // Attack speed bonus from agility
    hero.attackSpeed = hero.agility + bonusAS; // 1 agility = 1 attack speed
    
    // Damage from primary attribute
    switch (hero.primaryAttribute) {
        case HeroAttribute::Strength:
            hero.damage = 50.0f + hero.strength + bonusDmg;
            break;
        case HeroAttribute::Agility:
            hero.damage = 50.0f + hero.agility + bonusDmg;
            break;
        case HeroAttribute::Intelligence:
            hero.damage = 50.0f + hero.intelligence + bonusDmg;
            break;
    }
}

f32 HeroSystem::calculateDamage(const HeroComponent& hero) const {
    f32 damage = hero.damage;
    
    // Add damage buffs
    for (const auto& buff : hero.buffs) {
        if (buff.type == BuffType::DamageBonus && buff.remainingTime > 0.0f) {
            damage += buff.value;
        }
    }
    
    return damage;
}

f32 HeroSystem::calculateArmor(const HeroComponent& hero) const {
    f32 armor = hero.armor;
    
    for (const auto& buff : hero.buffs) {
        if (buff.remainingTime <= 0.0f) continue;
        if (buff.type == BuffType::ArmorBonus) armor += buff.value;
        if (buff.type == BuffType::ArmorReduction) armor -= buff.value;
    }
    
    return armor;
}

f32 HeroSystem::calculateAttackSpeed(const HeroComponent& hero) const {
    f32 attackSpeed = hero.attackSpeed;
    
    for (const auto& buff : hero.buffs) {
        if (buff.remainingTime <= 0.0f) continue;
        if (buff.type == BuffType::AttackSpeedBonus) attackSpeed += buff.value;
        if (buff.type == BuffType::AttackSpeedSlow) attackSpeed -= buff.value;
    }
    
    // Clamp attack speed (20 min, 700 max like Dota)
    return std::clamp(attackSpeed, 20.0f, 700.0f);
}

f32 HeroSystem::calculateMoveSpeed(const HeroComponent& hero) const {
    f32 moveSpeed = hero.moveSpeed;
    
    // Add item move speed bonuses
    for (i32 i = 0; i < 6; ++i) {
        const auto& item = hero.inventory[i];
        if (!item.data.name.empty()) {
            moveSpeed += item.data.bonusMoveSpeed;
        }
    }
    
    // Apply buff modifiers
    f32 slowPercent = 0.0f;
    f32 hasteBonus = 0.0f;
    
    for (const auto& buff : hero.buffs) {
        if (buff.remainingTime <= 0.0f) continue;
        
        if (buff.type == BuffType::Haste) {
            hasteBonus = std::max(hasteBonus, buff.value);
        } else if (buff.type == BuffType::Slow) {
            slowPercent = std::max(slowPercent, buff.value);
        }
    }
    
    // Apply haste (flat bonus)
    moveSpeed += hasteBonus;
    
    // Apply slow (percentage reduction)
    moveSpeed *= (1.0f - slowPercent / 100.0f);
    
    // Clamp move speed (100 min, 550 max like Dota)
    return std::clamp(moveSpeed, 100.0f, 550.0f);
}

Entity HeroSystem::createHero(const String& heroName, i32 teamId, const Vec3& position) {
    Entity hero = entityManager_.createEntity(heroName);
    
    // Add components
    auto& heroComp = entityManager_.addComponent<HeroComponent>(hero, heroName, teamId);
    auto& transform = entityManager_.addComponent<TransformComponent>(hero);
    transform.position = position;
    transform.scale = Vec3(5.0f, 2.0f, 5.0f); // Large hero scale
    
    // Set respawn position
    heroComp.respawnPosition = position;
    heroComp.isPlayerControlled = (teamId == 1); // Team 1 hero is player controlled
    heroComp.moveSpeed = 350.0f; // Fast movement speed (7-8 units per frame at 60fps)
    
    // Initialize stats
    recalculateStats(heroComp);
    heroComp.currentHealth = heroComp.maxHealth;
    heroComp.currentMana = heroComp.maxMana;
    
    // Setup default abilities (placeholder) - hotkeys: 1, 2, 3, F
    heroComp.abilities[0].data.name = "Fireball";
    heroComp.abilities[0].data.hotkey = '1';
    heroComp.abilities[0].data.manaCost = 100.0f;
    heroComp.abilities[0].data.cooldown = 10.0f;
    heroComp.abilities[0].data.damage = 150.0f;
    heroComp.abilities[0].data.castRange = 15.0f;  // Range for targeting
    heroComp.abilities[0].data.targetType = AbilityTargetType::UnitTarget;
    heroComp.abilities[0].level = 1;  // Start with ability learned
    
    heroComp.abilities[1].data.name = "Ice Storm";
    heroComp.abilities[1].data.hotkey = '2';
    heroComp.abilities[1].data.manaCost = 80.0f;
    heroComp.abilities[1].data.cooldown = 12.0f;
    heroComp.abilities[1].data.damage = 100.0f;
    heroComp.abilities[1].data.radius = 8.0f;      // AoE radius
    heroComp.abilities[1].data.castRange = 20.0f;  // Range for targeting
    heroComp.abilities[1].data.targetType = AbilityTargetType::PointTarget;
    heroComp.abilities[1].level = 1;  // Start with ability learned
    
    heroComp.abilities[2].data.name = "Passive Aura";
    heroComp.abilities[2].data.hotkey = '3';
    heroComp.abilities[2].data.targetType = AbilityTargetType::Passive;
    heroComp.abilities[2].level = 1;  // Start with ability learned
    
    heroComp.abilities[3].data.name = "Lightning Ultimate";
    heroComp.abilities[3].data.hotkey = 'F';
    heroComp.abilities[3].data.manaCost = 200.0f;
    heroComp.abilities[3].data.cooldown = 60.0f;
    heroComp.abilities[3].data.damage = 500.0f;
    heroComp.abilities[3].data.castRange = 25.0f;  // Range for targeting
    heroComp.abilities[3].data.targetType = AbilityTargetType::UnitTarget;
    heroComp.abilities[3].level = 1;  // Start with ability learned
    
    // Create hero mesh (humanoid shape)
    auto& mesh = entityManager_.addComponent<MeshComponent>(hero, "HeroMesh");
    
    // Create a simple humanoid mesh (body + head)
    // Body (cylinder)
    MeshGenerators::GenerateCylinder(mesh, 0.5f, 2.0f, 16);
    
    // Add collision
    auto& collision = entityManager_.addComponent<CollisionComponent>(hero, CollisionShape::Capsule);
    collision.capsuleRadius = 0.5f;
    collision.capsuleHeight = 2.0f;
    collision.blocksMovement = true;
    
    // Create material with team color
    Entity materialEntity = entityManager_.createEntity("HeroMaterial");
    auto& material = entityManager_.addComponent<MaterialComponent>(materialEntity, "HeroMaterial");
    
    if (teamId == 1) {
        material.baseColor = Vec3(0.2f, 0.5f, 1.0f); // Blue for player team
    } else {
        material.baseColor = Vec3(1.0f, 0.3f, 0.3f); // Red for enemy team
    }
    material.metallic = 0.3f;
    material.roughness = 0.6f;
    
    mesh.materialEntity = materialEntity;
    mesh.gpuUploadNeeded = true;
    
    return hero;
}

void HeroSystem::issueCommand(Entity hero, const HeroCommand& command) {
    if (!entityManager_.isValid(hero) || !entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    // Block commands if dead
    if (heroComp.state == HeroState::Dead) {
        return;
    }
    
    switch (command.type) {
        case HeroCommand::Type::MoveTo:
            moveToPosition(hero, command.targetPosition);
            break;
            
        case HeroCommand::Type::AttackMove:
            // Move to position, attack enemies on the way
            heroComp.movePath.clear();
            heroComp.movePath.push_back(command.targetPosition);
            heroComp.currentPathIndex = 0;
            heroComp.state = HeroState::Moving;
            // TODO: Implement attack-move logic
            break;
            
        case HeroCommand::Type::AttackTarget:
            attackTarget(hero, command.targetEntity);
            break;
            
        case HeroCommand::Type::CastAbility:
            castAbility(hero, command.abilityIndex, command.targetPosition, command.targetEntity);
            break;
            
        case HeroCommand::Type::Stop:
            stopHero(hero);
            break;
            
        case HeroCommand::Type::Hold:
            stopHero(hero);
            // TODO: Implement hold position (don't auto-attack)
            break;
            
        default:
            break;
    }
}

void HeroSystem::moveToPosition(Entity hero, const Vec3& position) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    // Block if dead
    if (heroComp.state == HeroState::Dead) {
        return;
    }
    
    // Simple direct path for now
    // TODO: Implement proper pathfinding
    heroComp.movePath.clear();
    heroComp.movePath.push_back(position);
    heroComp.currentPathIndex = 0;
    heroComp.targetEntity = INVALID_ENTITY;
    heroComp.state = HeroState::Moving;
}

void HeroSystem::attackTarget(Entity hero, Entity target) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    // Block if dead
    if (heroComp.state == HeroState::Dead) {
        return;
    }
    
    heroComp.targetEntity = target;
    heroComp.movePath.clear();
    heroComp.state = HeroState::Attacking;
}

void HeroSystem::castAbility(Entity hero, i32 abilityIndex, const Vec3& targetPos, Entity targetEntity) {
    if (!canCastAbility(hero, abilityIndex)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    auto& ability = heroComp.abilities[abilityIndex];
    
    // Deduct mana
    heroComp.currentMana -= ability.data.manaCost;
    
    // Start casting
    heroComp.currentCastingAbility = abilityIndex;
    heroComp.castTimer = ability.data.castPoint;
    heroComp.targetPosition = targetPos;
    heroComp.targetEntity = targetEntity;
    heroComp.state = HeroState::CastingAbility;
}

void HeroSystem::stopHero(Entity hero) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    heroComp.movePath.clear();
    heroComp.targetEntity = INVALID_ENTITY;
    heroComp.currentCastingAbility = -1;
    heroComp.state = HeroState::Idle;
}

void HeroSystem::learnAbility(Entity hero, i32 abilityIndex) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    if (heroComp.abilityPoints <= 0 || abilityIndex < 0 || abilityIndex >= 6) {
        return;
    }
    
    auto& ability = heroComp.abilities[abilityIndex];
    
    // Check if can level up this ability
    if (ability.level >= ability.data.maxLevel) {
        return;
    }
    
    // Ultimate (index 3) requires level 6/12/18
    if (abilityIndex == 3) {
        i32 requiredLevel = 6 + (ability.level * 6);
        if (heroComp.level < requiredLevel) {
            return;
        }
    }
    
    ability.level++;
    heroComp.abilityPoints--;
}

bool HeroSystem::canCastAbility(Entity hero, i32 abilityIndex) const {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return false;
    }
    
    const auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    if (abilityIndex < 0 || abilityIndex >= 6) {
        return false;
    }
    
    const auto& ability = heroComp.abilities[abilityIndex];
    
    // Check if learned
    if (ability.level <= 0) {
        return false;
    }
    
    // Check cooldown
    if (ability.currentCooldown > 0.0f) {
        return false;
    }
    
    // Check mana
    if (heroComp.currentMana < ability.data.manaCost) {
        return false;
    }
    
    // Check if already casting
    if (heroComp.state == HeroState::CastingAbility) {
        return false;
    }
    
    // Check if dead or stunned
    if (heroComp.state == HeroState::Dead || heroComp.state == HeroState::Stunned) {
        return false;
    }
    
    return true;
}

void HeroSystem::giveExperience(Entity hero, f32 amount) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    if (heroComp.level >= 30) { // Max level
        return;
    }
    
    heroComp.experience += amount;
    
    while (heroComp.experience >= heroComp.experienceToNextLevel && heroComp.level < 30) {
        levelUp(hero);
    }
}

void HeroSystem::levelUp(Entity hero) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    heroComp.experience -= heroComp.experienceToNextLevel;
    heroComp.level++;
    heroComp.abilityPoints++;
    
    // Experience required increases each level
    heroComp.experienceToNextLevel = 200.0f + (heroComp.level - 1) * 100.0f;
    
    // Recalculate stats
    recalculateStats(heroComp);
    
    // Heal to full on level up (optional, Dota doesn't do this)
    // heroComp.currentHealth = heroComp.maxHealth;
    // heroComp.currentMana = heroComp.maxMana;
}

Entity HeroSystem::findAttackTarget(const Vec3& position, i32 teamId, f32 range) {
    Entity nearest = INVALID_ENTITY;
    f32 nearestDist = range;
    
    auto& registry = entityManager_.getRegistry();
    
    // Check enemy heroes
    auto heroView = registry.view<HeroComponent, TransformComponent>();
    for (auto entity : heroView) {
        const auto& hero = heroView.get<HeroComponent>(entity);
        const auto& transform = heroView.get<TransformComponent>(entity);
        
        if (hero.teamId == teamId || hero.state == HeroState::Dead) {
            continue;
        }
        
        f32 dist = glm::length(transform.position - position);
        if (dist < nearestDist) {
            nearest = entity;
            nearestDist = dist;
        }
    }
    
    // Check enemy creeps
    auto creepView = registry.view<CreepComponent, TransformComponent>();
    for (auto entity : creepView) {
        const auto& creep = creepView.get<CreepComponent>(entity);
        const auto& transform = creepView.get<TransformComponent>(entity);
        
        if (creep.teamId == teamId || creep.state == CreepState::Dead) {
            continue;
        }
        
        f32 dist = glm::length(transform.position - position);
        if (dist < nearestDist) {
            nearest = entity;
            nearestDist = dist;
        }
    }
    
    return nearest;
}

void HeroSystem::dealDamage(Entity attacker, Entity target, f32 damage, bool isMagical) {
    // Check invulnerability
    if (entityManager_.hasComponent<HeroComponent>(target)) {
        auto& targetHero = entityManager_.getComponent<HeroComponent>(target);
        if (targetHero.isInvulnerable()) {
            return;
        }
    }
    
    // Deal damage to hero
    if (entityManager_.hasComponent<HeroComponent>(target)) {
        auto& targetHero = entityManager_.getComponent<HeroComponent>(target);
        
        f32 actualDamage = damage;
        
        if (!isMagical) {
            // Physical damage - apply armor reduction
            f32 armor = calculateArmor(targetHero);
            f32 armorReduction = 1.0f - (0.06f * armor) / (1.0f + 0.06f * std::abs(armor));
            actualDamage = damage * armorReduction;
        } else {
            // Magical damage - 25% base magic resistance
            actualDamage = damage * 0.75f;
        }
        
        targetHero.currentHealth -= actualDamage;
        
        if (targetHero.currentHealth <= 0.0f) {
            targetHero.currentHealth = 0.0f;
            targetHero.state = HeroState::Dead;
            targetHero.deaths++;
            targetHero.respawnTimer = calculateRespawnTime(targetHero.level);
            
            // Clear movement and targeting
            targetHero.movePath.clear();
            targetHero.targetEntity = INVALID_ENTITY;
            targetHero.currentCastingAbility = -1;
            
            // Hide mesh when dead
            if (entityManager_.hasComponent<MeshComponent>(target)) {
                auto& mesh = entityManager_.getComponent<MeshComponent>(target);
                mesh.visible = false;
            }
            
            // Give kill credit
            if (entityManager_.hasComponent<HeroComponent>(attacker)) {
                auto& attackerHero = entityManager_.getComponent<HeroComponent>(attacker);
                attackerHero.kills++;
                giveExperience(attacker, 100.0f + targetHero.level * 20.0f);
                giveGold(attacker, 200 + targetHero.level * 10);
            }
        }
    }
    
    // Deal damage to creep
    if (entityManager_.hasComponent<CreepComponent>(target)) {
        auto& targetCreep = entityManager_.getComponent<CreepComponent>(target);
        targetCreep.currentHealth -= damage;
        
        if (targetCreep.currentHealth <= 0.0f) {
            targetCreep.currentHealth = 0.0f;
            targetCreep.state = CreepState::Dead;
            targetCreep.deathTime = 0.0f;
            
            // Give experience and gold to killer
            if (entityManager_.hasComponent<HeroComponent>(attacker)) {
                auto& attackerHero = entityManager_.getComponent<HeroComponent>(attacker);
                attackerHero.lastHits++;
                giveExperience(attacker, 40.0f);
                giveGold(attacker, 40);
            }
        }
    }
}

void HeroSystem::dealAreaDamage(Entity attacker, const Vec3& center, f32 radius, f32 damage, i32 teamId, bool isMagical) {
    auto& registry = entityManager_.getRegistry();
    
    // Damage enemy creeps in area
    auto creepView = registry.view<CreepComponent, TransformComponent>();
    for (auto entity : creepView) {
        auto& creep = creepView.get<CreepComponent>(entity);
        auto& transform = creepView.get<TransformComponent>(entity);
        
        if (creep.teamId == teamId || creep.state == CreepState::Dead) continue;
        
        f32 dist = glm::length(transform.position - center);
        if (dist <= radius) {
            dealDamage(attacker, entity, damage, isMagical);
        }
    }
    
    // Damage enemy heroes in area
    auto heroView = registry.view<HeroComponent, TransformComponent>();
    for (auto entity : heroView) {
        auto& hero = heroView.get<HeroComponent>(entity);
        auto& transform = heroView.get<TransformComponent>(entity);
        
        if (hero.teamId == teamId || hero.state == HeroState::Dead) continue;
        
        f32 dist = glm::length(transform.position - center);
        if (dist <= radius) {
            dealDamage(attacker, entity, damage, isMagical);
        }
    }
}

void HeroSystem::executeAbilityEffect(Entity heroEntity, HeroComponent& hero, i32 abilityIndex, TransformComponent& transform) {
    const auto& ability = hero.abilities[abilityIndex];
    f32 damage = ability.data.damage * (1.0f + ability.level * 0.25f); // Scale with level
    f32 radius = ability.data.radius;
    
    // Get particle system for visual effects
    ParticleSystem* particleSys = nullptr;
    if (world_) {
        particleSys = dynamic_cast<ParticleSystem*>(world_->getSystem("ParticleSystem"));
    }
    
    // Determine effect color based on ability damage type
    Vec4 effectColor = Vec4(0.4f, 0.6f, 1.0f, 1.0f);  // Default blue magic
    if (ability.data.name.find("Fire") != String::npos || ability.data.name.find("Flame") != String::npos) {
        effectColor = Vec4(1.0f, 0.5f, 0.1f, 1.0f);  // Orange fire
    } else if (ability.data.name.find("Ice") != String::npos || ability.data.name.find("Frost") != String::npos) {
        effectColor = Vec4(0.6f, 0.9f, 1.0f, 1.0f);  // Ice blue
    } else if (ability.data.name.find("Poison") != String::npos || ability.data.name.find("Venom") != String::npos) {
        effectColor = Vec4(0.3f, 0.8f, 0.2f, 1.0f);  // Toxic green
    } else if (ability.data.name.find("Lightning") != String::npos || ability.data.name.find("Storm") != String::npos) {
        effectColor = Vec4(0.7f, 0.8f, 1.0f, 1.0f);  // Electric blue
    } else if (ability.data.name.find("Shadow") != String::npos || ability.data.name.find("Dark") != String::npos) {
        effectColor = Vec4(0.4f, 0.2f, 0.6f, 1.0f);  // Purple dark
    } else if (ability.data.name.find("Holy") != String::npos || ability.data.name.find("Light") != String::npos) {
        effectColor = Vec4(1.0f, 0.95f, 0.7f, 1.0f);  // Golden light
    }
    
    // Use player-selected target if available, otherwise find nearest enemy
    Entity selectedTarget = hero.targetEntity;
    
    // If no target selected, find nearest enemy for targeted abilities
    if (selectedTarget == INVALID_ENTITY || !entityManager_.isValid(selectedTarget)) {
        f32 nearestDist = ability.data.castRange;
        
        auto& registry = entityManager_.getRegistry();
        
        // Search for enemy creeps
        auto creepView = registry.view<CreepComponent, TransformComponent>();
        for (auto entity : creepView) {
            auto& creep = creepView.get<CreepComponent>(entity);
            auto& creepTransform = creepView.get<TransformComponent>(entity);
            
            if (creep.teamId == hero.teamId || creep.state == CreepState::Dead) continue;
            
            f32 dist = glm::length(creepTransform.position - transform.position);
            if (dist < nearestDist) {
                selectedTarget = entity;
                nearestDist = dist;
            }
        }
        
        // Search for enemy heroes
        auto heroView = registry.view<HeroComponent, TransformComponent>();
        for (auto entity : heroView) {
            auto& targetHero = heroView.get<HeroComponent>(entity);
            auto& heroTransform = heroView.get<TransformComponent>(entity);
            
            if (targetHero.teamId == hero.teamId || targetHero.state == HeroState::Dead) continue;
            if (entity == heroEntity) continue;
            
            f32 dist = glm::length(heroTransform.position - transform.position);
            if (dist < nearestDist) {
                selectedTarget = entity;
                nearestDist = dist;
            }
        }
    }
    
    // Spawn cast effect at hero position with ability-specific color
    if (particleSys) {
        particleSys->spawnCastEffect(transform.position + Vec3(0, 2, 0), effectColor);
    }
    
    // Execute ability based on type
    switch (ability.data.targetType) {
        case AbilityTargetType::UnitTarget:
            // Damage selected/nearest enemy
            if (selectedTarget != INVALID_ENTITY && entityManager_.hasComponent<TransformComponent>(selectedTarget)) {
                Vec3 targetPos = entityManager_.getComponent<TransformComponent>(selectedTarget).position;
                
                // Spawn projectile trail from hero to target
                if (particleSys) {
                    particleSys->spawnProjectileTrail(
                        transform.position + Vec3(0, 1.5f, 0),
                        targetPos + Vec3(0, 1.5f, 0),
                        effectColor
                    );
                }
                
                dealDamage(heroEntity, selectedTarget, damage, true);
                
                // Spawn hit effect on target based on damage type
                if (particleSys && entityManager_.hasComponent<TransformComponent>(selectedTarget)) {
                    if (ability.data.name.find("Lightning") != String::npos) {
                        particleSys->spawnLightningEffect(
                            transform.position + Vec3(0, 5, 0),
                            targetPos + Vec3(0, 1, 0)
                        );
                    } else if (ability.data.name.find("Ice") != String::npos) {
                        particleSys->spawnIceEffect(targetPos + Vec3(0, 1, 0));
                    } else if (ability.data.name.find("Fire") != String::npos) {
                        particleSys->spawnFireEffect(targetPos + Vec3(0, 0.5f, 0));
                    } else if (ability.data.name.find("Poison") != String::npos) {
                        particleSys->spawnPoisonEffect(targetPos + Vec3(0, 1, 0));
                    } else {
                        particleSys->spawnAttackEffect(targetPos + Vec3(0, 1.5f, 0), Vec3(0, 1, 0));
                    }
                }
                
                // Apply stun if ability has duration (like Shield Bash)
                if (ability.data.duration > 0.0f && entityManager_.hasComponent<HeroComponent>(selectedTarget)) {
                    auto& targetHero = entityManager_.getComponent<HeroComponent>(selectedTarget);
                    Buff stunBuff;
                    stunBuff.type = BuffType::Stun;
                    stunBuff.name = ability.data.name;
                    stunBuff.duration = ability.data.duration;
                    stunBuff.source = heroEntity;
                    applyBuff(selectedTarget, stunBuff);
                    
                    // Stun effect
                    if (particleSys) {
                        particleSys->spawnStunEffect(selectedTarget);
                    }
                }
            }
            break;
            
        case AbilityTargetType::PointTarget:
            // AoE damage at target position or around hero
            if (radius > 0.0f) {
                Vec3 targetPos = hero.targetPosition;
                if (glm::length(targetPos) < 0.1f) {
                    targetPos = transform.position; // Default to hero position
                }
                
                // Spawn AoE indicator on ground
                if (particleSys) {
                    particleSys->spawnAoEIndicator(targetPos, radius, effectColor);
                }
                
                dealAreaDamage(heroEntity, targetPos, radius, damage, hero.teamId, true);
                
                // Spawn explosion effect with type-specific visuals
                if (particleSys) {
                    if (ability.data.name.find("Fire") != String::npos || ability.data.name.find("Flame") != String::npos) {
                        particleSys->spawnFireEffect(targetPos + Vec3(0, 0.5f, 0));
                        particleSys->spawnExplosion(targetPos + Vec3(0, 0.5f, 0), radius);
                    } else if (ability.data.name.find("Ice") != String::npos) {
                        particleSys->spawnIceEffect(targetPos + Vec3(0, 0.5f, 0));
                    } else if (ability.data.name.find("Lightning") != String::npos) {
                        // Multiple lightning strikes in area
                        for (i32 i = 0; i < 3; i++) {
                            Vec3 strikePos = targetPos + Vec3(
                                (rand() % 100 - 50) / 50.0f * radius,
                                0,
                                (rand() % 100 - 50) / 50.0f * radius
                            );
                            particleSys->spawnLightningEffect(strikePos + Vec3(0, 8, 0), strikePos);
                        }
                    } else {
                        particleSys->spawnExplosion(targetPos + Vec3(0, 0.5f, 0), radius);
                    }
                }
            }
            break;
            
        case AbilityTargetType::NoTarget:
            // Self-buff or instant effect
            if (ability.data.duration > 0.0f) {
                // Apply buff to self (like Berserker Rage)
                Buff selfBuff;
                selfBuff.type = BuffType::DamageBonus;
                selfBuff.name = ability.data.name;
                selfBuff.value = damage * 0.5f; // Bonus damage
                selfBuff.duration = ability.data.duration;
                selfBuff.source = heroEntity;
                applyBuff(heroEntity, selfBuff);
                
                // Buff effect on self - shield or aura based on ability
                if (particleSys) {
                    if (ability.data.name.find("Shield") != String::npos || ability.data.name.find("Barrier") != String::npos) {
                        particleSys->spawnShieldEffect(heroEntity);
                    } else {
                        particleSys->spawnAuraEffect(heroEntity, effectColor);
                        particleSys->createEffect(ParticleEffectType::Buff, transform.position + Vec3(0, 1, 0), ability.data.duration);
                    }
                }
                
                // Also add attack speed buff
                Buff asBuff;
                asBuff.type = BuffType::AttackSpeedBonus;
                asBuff.name = ability.data.name;
                asBuff.value = 100.0f; // +100 attack speed
                asBuff.duration = ability.data.duration;
                asBuff.source = heroEntity;
                applyBuff(heroEntity, asBuff);
            } else {
                // Instant AoE around hero
                if (particleSys) {
                    particleSys->spawnAoEIndicator(transform.position, ability.data.castRange, effectColor);
                }
                dealAreaDamage(heroEntity, transform.position, ability.data.castRange, damage, hero.teamId, true);
                
                // Explosion around hero
                if (particleSys) {
                    particleSys->spawnExplosion(transform.position + Vec3(0, 1, 0), ability.data.castRange);
                }
            }
            break;
            
        case AbilityTargetType::Passive:
            // Passive abilities don't have active effects but can show aura
            if (particleSys && ability.level > 0) {
                particleSys->spawnAuraEffect(heroEntity, Vec4(0.8f, 0.8f, 0.8f, 0.4f));
            }
            break;
            
        default:
            break;
    }
}

void HeroSystem::handleRespawn(Entity entity, HeroComponent& hero, f32 deltaTime) {
    hero.respawnTimer -= deltaTime;
    
    if (hero.respawnTimer <= 0.0f) {
        // Respawn
        hero.state = HeroState::Idle;
        hero.currentHealth = hero.maxHealth;
        hero.currentMana = hero.maxMana;
        hero.targetEntity = INVALID_ENTITY;
        hero.movePath.clear();
        
        // Move to respawn position
        if (entityManager_.hasComponent<TransformComponent>(entity)) {
            auto& transform = entityManager_.getComponent<TransformComponent>(entity);
            transform.position = hero.respawnPosition;
        }
        
        // Make visible again
        if (entityManager_.hasComponent<MeshComponent>(entity)) {
            auto& mesh = entityManager_.getComponent<MeshComponent>(entity);
            mesh.visible = true;
        }
    }
}

f32 HeroSystem::calculateRespawnTime(i32 level) const {
    // Dota 2 formula: level * 2.5 seconds (simplified)
    return static_cast<f32>(level) * 2.5f;
}

// ============ Buff System ============

void HeroSystem::updateBuffs(Entity entity, HeroComponent& hero, f32 deltaTime) {
    bool needsRecalc = false;
    
    for (auto it = hero.buffs.begin(); it != hero.buffs.end(); ) {
        it->remainingTime -= deltaTime;
        
        // Handle DoT effects
        if (it->type == BuffType::DamageOverTime && it->remainingTime > 0.0f) {
            it->tickTimer -= deltaTime;
            if (it->tickTimer <= 0.0f) {
                hero.currentHealth -= it->value;
                it->tickTimer = it->tickInterval;
                
                if (hero.currentHealth <= 0.0f) {
                    hero.currentHealth = 0.0f;
                    hero.state = HeroState::Dead;
                    hero.respawnTimer = calculateRespawnTime(hero.level);
                    hero.movePath.clear();
                    hero.targetEntity = INVALID_ENTITY;
                    hero.currentCastingAbility = -1;
                    
                    // Hide mesh
                    if (entityManager_.hasComponent<MeshComponent>(entity)) {
                        auto& mesh = entityManager_.getComponent<MeshComponent>(entity);
                        mesh.visible = false;
                    }
                }
            }
        }
        
        // Remove expired buffs
        if (it->remainingTime <= 0.0f) {
            it = hero.buffs.erase(it);
            needsRecalc = true;
        } else {
            ++it;
        }
    }
    
    if (needsRecalc) {
        recalculateStats(hero);
    }
}

void HeroSystem::applyBuff(Entity target, const Buff& buff) {
    if (!entityManager_.hasComponent<HeroComponent>(target)) {
        return;
    }
    
    auto& hero = entityManager_.getComponent<HeroComponent>(target);
    
    // Check for existing buff of same type - refresh or stack
    for (auto& existingBuff : hero.buffs) {
        if (existingBuff.type == buff.type && existingBuff.source == buff.source) {
            // Refresh duration
            existingBuff.remainingTime = buff.duration;
            existingBuff.value = std::max(existingBuff.value, buff.value);
            return;
        }
    }
    
    // Add new buff
    Buff newBuff = buff;
    newBuff.remainingTime = buff.duration;
    newBuff.tickTimer = buff.tickInterval;
    hero.buffs.push_back(newBuff);
    
    recalculateStats(hero);
}

void HeroSystem::removeBuff(Entity target, BuffType type) {
    if (!entityManager_.hasComponent<HeroComponent>(target)) {
        return;
    }
    
    auto& hero = entityManager_.getComponent<HeroComponent>(target);
    
    hero.buffs.erase(
        std::remove_if(hero.buffs.begin(), hero.buffs.end(),
            [type](const Buff& b) { return b.type == type; }),
        hero.buffs.end()
    );
    
    recalculateStats(hero);
}

void HeroSystem::purgeBuffs(Entity target, bool purgePositive, bool purgeNegative) {
    if (!entityManager_.hasComponent<HeroComponent>(target)) {
        return;
    }
    
    auto& hero = entityManager_.getComponent<HeroComponent>(target);
    
    hero.buffs.erase(
        std::remove_if(hero.buffs.begin(), hero.buffs.end(),
            [purgePositive, purgeNegative](const Buff& b) {
                if (!b.isPurgeable) return false;
                bool isNegative = static_cast<u8>(b.type) >= 50;
                return (isNegative && purgeNegative) || (!isNegative && purgePositive);
            }),
        hero.buffs.end()
    );
    
    recalculateStats(hero);
}

// ============ Item System ============

void HeroSystem::updateItemCooldowns(HeroComponent& hero, f32 deltaTime) {
    for (i32 i = 0; i < static_cast<i32>(ItemSlot::COUNT); ++i) {
        if (hero.inventory[i].currentCooldown > 0.0f) {
            hero.inventory[i].currentCooldown -= deltaTime;
        }
    }
}

void HeroSystem::giveGold(Entity hero, i32 amount) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    heroComp.gold += amount;
    if (heroComp.gold < 0) heroComp.gold = 0;
}

bool HeroSystem::giveItem(Entity hero, const ItemData& itemData) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return false;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    // Find empty slot
    for (i32 i = 0; i < 6; ++i) {
        if (heroComp.inventory[i].data.name.empty()) {
            heroComp.inventory[i].data = itemData;
            heroComp.inventory[i].charges = 1;
            heroComp.inventory[i].isActive = true;
            recalculateStats(heroComp);
            return true;
        }
    }
    
    // Try backpack
    for (i32 i = 6; i < 9; ++i) {
        if (heroComp.inventory[i].data.name.empty()) {
            heroComp.inventory[i].data = itemData;
            heroComp.inventory[i].charges = 1;
            heroComp.inventory[i].isActive = false;
            return true;
        }
    }
    
    return false; // Inventory full
}

void HeroSystem::dropItem(Entity hero, ItemSlot slot) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    i32 slotIndex = static_cast<i32>(slot);
    
    heroComp.inventory[slotIndex] = Item();
    recalculateStats(heroComp);
}

void HeroSystem::swapItems(Entity hero, ItemSlot slot1, ItemSlot slot2) {
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    i32 idx1 = static_cast<i32>(slot1);
    i32 idx2 = static_cast<i32>(slot2);
    
    std::swap(heroComp.inventory[idx1], heroComp.inventory[idx2]);
    
    // Update active status
    heroComp.inventory[idx1].isActive = (idx1 < 6);
    heroComp.inventory[idx2].isActive = (idx2 < 6);
    
    recalculateStats(heroComp);
}

// ============ Predefined Items ============

ItemData HeroSystem::createItem_IronBranch() {
    ItemData item;
    item.name = "Iron Branch";
    item.description = "+1 to all attributes";
    item.goldCost = 50;
    item.bonusStrength = 1.0f;
    item.bonusAgility = 1.0f;
    item.bonusIntelligence = 1.0f;
    return item;
}

ItemData HeroSystem::createItem_Tango() {
    ItemData item;
    item.name = "Tango";
    item.description = "Consume to restore HP";
    item.goldCost = 90;
    item.isConsumable = true;
    item.isStackable = true;
    item.maxStack = 3;
    item.hasActive = true;
    item.activeCooldown = 0.0f;
    return item;
}

ItemData HeroSystem::createItem_HealingSalve() {
    ItemData item;
    item.name = "Healing Salve";
    item.description = "Restore 400 HP over 8 seconds";
    item.goldCost = 110;
    item.isConsumable = true;
    item.hasActive = true;
    item.activeCooldown = 0.0f;
    return item;
}

ItemData HeroSystem::createItem_ClarityPotion() {
    ItemData item;
    item.name = "Clarity";
    item.description = "Restore 150 mana over 25 seconds";
    item.goldCost = 50;
    item.isConsumable = true;
    item.hasActive = true;
    item.activeCooldown = 0.0f;
    return item;
}

ItemData HeroSystem::createItem_BootsOfSpeed() {
    ItemData item;
    item.name = "Boots of Speed";
    item.description = "+45 Movement Speed";
    item.goldCost = 500;
    item.bonusMoveSpeed = 45.0f;
    return item;
}

ItemData HeroSystem::createItem_PowerTreads() {
    ItemData item;
    item.name = "Power Treads";
    item.description = "+45 MS, +25 AS, +10 selected attribute";
    item.goldCost = 1400;
    item.bonusMoveSpeed = 45.0f;
    item.bonusAttackSpeed = 25.0f;
    item.bonusStrength = 10.0f;
    return item;
}

ItemData HeroSystem::createItem_BladeMail() {
    ItemData item;
    item.name = "Blade Mail";
    item.description = "+28 Damage, +6 Armor";
    item.goldCost = 2100;
    item.bonusDamage = 28.0f;
    item.bonusArmor = 6.0f;
    item.hasActive = true;
    item.activeCooldown = 25.0f;
    return item;
}

ItemData HeroSystem::createItem_Blink() {
    ItemData item;
    item.name = "Blink Dagger";
    item.description = "Teleport to target point";
    item.goldCost = 2250;
    item.hasActive = true;
    item.activeCooldown = 15.0f;
    return item;
}

// ============ Hero Templates ============

void HeroSystem::setupHero_Warrior(HeroComponent& hero) {
    hero.heroName = "Warrior";
    hero.primaryAttribute = HeroAttribute::Strength;
    hero.baseStrength = 25.0f;
    hero.baseAgility = 15.0f;
    hero.baseIntelligence = 14.0f;
    hero.strengthGain = 3.2f;
    hero.agilityGain = 1.5f;
    hero.intelligenceGain = 1.3f;
    hero.attackRange = 5.0f;  // Melee
    
    hero.abilities[0].data.name = "Shield Bash";
    hero.abilities[0].data.description = "Stuns target enemy";
    hero.abilities[0].data.hotkey = '1';
    hero.abilities[0].data.manaCost = 90.0f;
    hero.abilities[0].data.cooldown = 12.0f;
    hero.abilities[0].data.damage = 100.0f;
    hero.abilities[0].data.duration = 2.0f;
    hero.abilities[0].data.castRange = 150.0f;
    hero.abilities[0].data.targetType = AbilityTargetType::UnitTarget;
    
    hero.abilities[1].data.name = "Charge";
    hero.abilities[1].data.hotkey = '2';
    hero.abilities[1].data.manaCost = 75.0f;
    hero.abilities[1].data.cooldown = 14.0f;
    hero.abilities[1].data.castRange = 800.0f;
    hero.abilities[1].data.targetType = AbilityTargetType::PointTarget;
    
    hero.abilities[2].data.name = "Tough Skin";
    hero.abilities[2].data.hotkey = '3';
    hero.abilities[2].data.targetType = AbilityTargetType::Passive;
    
    hero.abilities[3].data.name = "Berserker Rage";
    hero.abilities[3].data.hotkey = 'F';
    hero.abilities[3].data.manaCost = 150.0f;
    hero.abilities[3].data.cooldown = 80.0f;
    hero.abilities[3].data.duration = 8.0f;
    hero.abilities[3].data.targetType = AbilityTargetType::NoTarget;
}

void HeroSystem::setupHero_Mage(HeroComponent& hero) {
    hero.heroName = "Mage";
    hero.primaryAttribute = HeroAttribute::Intelligence;
    hero.baseStrength = 16.0f;
    hero.baseAgility = 15.0f;
    hero.baseIntelligence = 27.0f;
    hero.strengthGain = 1.7f;
    hero.agilityGain = 1.6f;
    hero.intelligenceGain = 3.4f;
    hero.attackRange = 600.0f;
    
    hero.abilities[0].data.name = "Fireball";
    hero.abilities[0].data.hotkey = '1';
    hero.abilities[0].data.manaCost = 110.0f;
    hero.abilities[0].data.cooldown = 8.0f;
    hero.abilities[0].data.damage = 200.0f;
    hero.abilities[0].data.castRange = 700.0f;
    hero.abilities[0].data.targetType = AbilityTargetType::UnitTarget;
    
    hero.abilities[1].data.name = "Frost Nova";
    hero.abilities[1].data.hotkey = '2';
    hero.abilities[1].data.manaCost = 130.0f;
    hero.abilities[1].data.cooldown = 10.0f;
    hero.abilities[1].data.damage = 150.0f;
    hero.abilities[1].data.radius = 300.0f;
    hero.abilities[1].data.duration = 4.0f;
    hero.abilities[1].data.castRange = 600.0f;
    hero.abilities[1].data.targetType = AbilityTargetType::PointTarget;
    
    hero.abilities[2].data.name = "Blink";
    hero.abilities[2].data.hotkey = '3';
    hero.abilities[2].data.manaCost = 60.0f;
    hero.abilities[2].data.cooldown = 12.0f;
    hero.abilities[2].data.castRange = 1000.0f;
    hero.abilities[2].data.targetType = AbilityTargetType::PointTarget;
    
    hero.abilities[3].data.name = "Meteor Storm";
    hero.abilities[3].data.hotkey = 'F';
    hero.abilities[3].data.manaCost = 300.0f;
    hero.abilities[3].data.cooldown = 120.0f;
    hero.abilities[3].data.damage = 600.0f;
    hero.abilities[3].data.radius = 500.0f;
    hero.abilities[3].data.castRange = 800.0f;
    hero.abilities[3].data.targetType = AbilityTargetType::PointTarget;
}

void HeroSystem::setupHero_Assassin(HeroComponent& hero) {
    hero.heroName = "Assassin";
    hero.primaryAttribute = HeroAttribute::Agility;
    hero.baseStrength = 18.0f;
    hero.baseAgility = 26.0f;
    hero.baseIntelligence = 14.0f;
    hero.strengthGain = 2.0f;
    hero.agilityGain = 3.0f;
    hero.intelligenceGain = 1.4f;
    hero.attackRange = 5.0f;  // Melee
    hero.moveSpeed = 320.0f;
    
    hero.abilities[0].data.name = "Backstab";
    hero.abilities[0].data.hotkey = '1';
    hero.abilities[0].data.manaCost = 50.0f;
    hero.abilities[0].data.cooldown = 6.0f;
    hero.abilities[0].data.damage = 150.0f;
    hero.abilities[0].data.castRange = 150.0f;
    hero.abilities[0].data.targetType = AbilityTargetType::UnitTarget;
    
    hero.abilities[1].data.name = "Shadow Step";
    hero.abilities[1].data.hotkey = '2';
    hero.abilities[1].data.manaCost = 80.0f;
    hero.abilities[1].data.cooldown = 10.0f;
    hero.abilities[1].data.castRange = 700.0f;
    hero.abilities[1].data.targetType = AbilityTargetType::UnitTarget;
    
    hero.abilities[2].data.name = "Blur";
    hero.abilities[2].data.hotkey = '3';
    hero.abilities[2].data.targetType = AbilityTargetType::Passive;
    
    hero.abilities[3].data.name = "Shadow Dance";
    hero.abilities[3].data.hotkey = 'F';
    hero.abilities[3].data.manaCost = 100.0f;
    hero.abilities[3].data.cooldown = 60.0f;
    hero.abilities[3].data.duration = 10.0f;
    hero.abilities[3].data.targetType = AbilityTargetType::NoTarget;
}

Entity HeroSystem::createHeroByType(const String& heroType, i32 teamId, const Vec3& position) {
    Entity hero = createHero(heroType, teamId, position);
    
    if (!entityManager_.hasComponent<HeroComponent>(hero)) {
        return hero;
    }
    
    auto& heroComp = entityManager_.getComponent<HeroComponent>(hero);
    
    if (heroType == "Warrior") {
        setupHero_Warrior(heroComp);
    } else if (heroType == "Mage") {
        setupHero_Mage(heroComp);
    } else if (heroType == "Assassin") {
        setupHero_Assassin(heroComp);
    }
    
    recalculateStats(heroComp);
    heroComp.currentHealth = heroComp.maxHealth;
    heroComp.currentMana = heroComp.maxMana;
    
    return hero;
}

} // namespace WorldEditor
