#pragma once
/**
 * Game Data Structures - Runtime game state data for HUD and UI
 */

#include "../core/Types.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Game {

// ============ Hero Data ============

struct HeroData {
    std::string heroId;
    std::string heroName;
    std::string portraitPath;
    
    // Base stats
    f32 baseHealth = 100.0f;
    f32 baseMana = 100.0f;
    f32 baseArmor = 0.0f;
    f32 baseDamage = 50.0f;
    
    // Growth per level
    f32 healthPerLevel = 20.0f;
    f32 manaPerLevel = 15.0f;
    f32 armorPerLevel = 0.5f;
    f32 damagePerLevel = 3.0f;
};

// ============ Ability Data ============

struct AbilityData {
    i32 abilityId = 0;
    std::string name;
    std::string iconPath;
    std::string hotkey;
    i32 level = 0;
    i32 maxLevel = 4;
    f32 cooldown = 0.0f;
    f32 maxCooldown = 10.0f;
    i32 manaCost = 50;
    bool isPassive = false;
    bool isUltimate = false;
};

// ============ Item Data ============

struct ItemData {
    i32 itemId = 0;
    std::string name;
    std::string iconPath;
    i32 charges = 0;
    f32 cooldown = 0.0f;
    f32 maxCooldown = 0.0f;
    bool isEmpty = true;
};

// ============ Player Stats ============

struct PlayerStats {
    u64 playerId = 0;
    std::string playerName;
    std::string heroName;
    i32 kills = 0;
    i32 deaths = 0;
    i32 assists = 0;
    i32 lastHits = 0;
    i32 denies = 0;
    i32 gold = 0;
    i32 level = 1;
    bool isAlive = true;
    u8 teamSlot = 0; // 0-4 Radiant, 5-9 Dire
};

// ============ Tower Data ============

struct TowerData {
    i32 towerId = 0;
    Vec3 position;
    i32 team = 0; // 0 = Radiant, 1 = Dire
    f32 health = 100.0f;
    f32 maxHealth = 100.0f;
    bool isAlive = true;
};

// ============ Game State Data ============

struct GameState {
    // Hero state
    f32 currentHealth = 100.0f;
    f32 maxHealth = 100.0f;
    f32 currentMana = 100.0f;
    f32 maxMana = 100.0f;
    i32 level = 1;
    f32 experience = 0.0f;
    f32 experienceToNext = 100.0f;
    
    // Abilities (Q, W, E, R, D, F)
    std::vector<AbilityData> abilities;
    std::vector<f32> abilityCooldowns;
    std::vector<i32> abilityLevels;
    
    // Items (6 inventory slots)
    std::vector<ItemData> items;
    std::vector<f32> itemCooldowns;
    
    // World state
    Vec3 heroPosition;
    Vec3 cameraPosition;
    std::vector<PlayerStats> allHeroes;
    std::vector<TowerData> towers;
    std::vector<PlayerStats> playerStats;
    
    // Game info
    f32 gameTime = 0.0f;
    i32 radiantKills = 0;
    i32 direKills = 0;
    
    GameState() {
        // Initialize with default data
        abilities.resize(6);
        abilityCooldowns.resize(6, 0.0f);
        abilityLevels.resize(6, 0);
        
        items.resize(6);
        itemCooldowns.resize(6, 0.0f);
    }
};

} // namespace Game