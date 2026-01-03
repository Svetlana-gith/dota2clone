#pragma once

#include "System.h"
#include "Components.h"
#include "EntityManager.h"
#include "core/Types.h"

namespace WorldEditor {

class World;

// Hero attributes (Dota-like)
enum class HeroAttribute : u8 {
    Strength = 0,   // HP, HP regen, status resistance
    Agility = 1,    // Armor, attack speed, move speed
    Intelligence = 2 // Mana, mana regen, spell amp
};

// Buff/Debuff types
enum class BuffType : u8 {
    // Positive buffs
    Haste = 0,          // Move speed bonus
    Strength_Bonus,     // +Strength
    Agility_Bonus,      // +Agility
    Intelligence_Bonus, // +Intelligence
    DamageBonus,        // +Damage
    ArmorBonus,         // +Armor
    AttackSpeedBonus,   // +Attack speed
    Regeneration,       // HP regen
    ManaRegen,          // Mana regen
    Invisibility,       // Invisible
    Invulnerable,       // Can't be damaged
    
    // Negative debuffs
    Slow = 50,          // Move speed reduction
    Stun,               // Can't act
    Silence,            // Can't cast spells
    Disarm,             // Can't attack
    Root,               // Can't move
    Break,              // Disable passives
    Hex,                // Transformed, can't act
    DamageOverTime,     // Taking damage over time
    ArmorReduction,     // -Armor
    AttackSpeedSlow     // -Attack speed
};

// Buff instance
struct Buff {
    BuffType type = BuffType::Haste;
    String name = "Buff";
    f32 value = 0.0f;           // Effect magnitude
    f32 duration = 0.0f;        // Total duration
    f32 remainingTime = 0.0f;   // Time left
    Entity source = INVALID_ENTITY; // Who applied this buff
    bool isPurgeable = true;    // Can be dispelled
    bool isHidden = false;      // Don't show in UI
    
    // For DoT effects
    f32 tickInterval = 1.0f;
    f32 tickTimer = 0.0f;
};

// Item slot types
enum class ItemSlot : u8 {
    Inventory1 = 0,
    Inventory2,
    Inventory3,
    Inventory4,
    Inventory5,
    Inventory6,
    Backpack1,
    Backpack2,
    Backpack3,
    TpSlot,
    NeutralSlot,
    COUNT
};

// Item data
struct ItemData {
    String name = "Item";
    String description = "";
    i32 goldCost = 0;
    
    // Stats bonuses
    f32 bonusStrength = 0.0f;
    f32 bonusAgility = 0.0f;
    f32 bonusIntelligence = 0.0f;
    f32 bonusDamage = 0.0f;
    f32 bonusArmor = 0.0f;
    f32 bonusAttackSpeed = 0.0f;
    f32 bonusMoveSpeed = 0.0f;
    f32 bonusHealth = 0.0f;
    f32 bonusMana = 0.0f;
    f32 bonusHealthRegen = 0.0f;
    f32 bonusManaRegen = 0.0f;
    
    // Active ability (if any)
    bool hasActive = false;
    f32 activeCooldown = 0.0f;
    f32 activeManaCost = 0.0f;
    
    // Flags
    bool isConsumable = false;
    bool isStackable = false;
    i32 maxStack = 1;
};

// Item instance
struct Item {
    ItemData data;
    i32 charges = 1;
    f32 currentCooldown = 0.0f;
    bool isActive = true;  // Items in backpack are inactive
};

// Hero state machine
enum class HeroState : u8 {
    Idle = 0,
    Moving = 1,
    Attacking = 2,
    CastingAbility = 3,
    Stunned = 4,
    Dead = 5
};

// Ability targeting type
enum class AbilityTargetType : u8 {
    NoTarget = 0,       // Toggle or instant (e.g., Blink)
    PointTarget = 1,    // Target ground position
    UnitTarget = 2,     // Target specific unit
    VectorTarget = 3,   // Direction (e.g., Mirana arrow)
    Passive = 4         // Always active
};

// Ability data (data-driven)
struct AbilityData {
    String name = "Ability";
    String description = "";
    AbilityTargetType targetType = AbilityTargetType::NoTarget;
    
    f32 manaCost = 100.0f;
    f32 cooldown = 10.0f;
    f32 castRange = 600.0f;
    f32 castPoint = 0.3f;      // Time before ability fires
    f32 castBackswing = 0.5f;  // Time after ability fires
    
    // Damage/effect values (ability-specific)
    f32 damage = 100.0f;
    f32 duration = 0.0f;
    f32 radius = 0.0f;
    
    // Hotkey (1, 2, 3, F for abilities)
    char hotkey = '1';
    i32 maxLevel = 4;
};

// Hero ability instance
struct HeroAbility {
    AbilityData data;
    i32 level = 0;           // 0 = not learned
    f32 currentCooldown = 0.0f;
    bool isActive = false;   // For toggle abilities
};

// Hero component
struct HeroComponent {
    // Identity
    String heroName = "Hero";
    HeroAttribute primaryAttribute = HeroAttribute::Strength;
    i32 teamId = 1;
    
    // Level & Experience
    i32 level = 1;
    f32 experience = 0.0f;
    f32 experienceToNextLevel = 200.0f;
    i32 abilityPoints = 1;  // Points to spend on abilities
    
    // Base stats (level 1)
    f32 baseStrength = 20.0f;
    f32 baseAgility = 20.0f;
    f32 baseIntelligence = 20.0f;
    
    // Stat gain per level
    f32 strengthGain = 2.5f;
    f32 agilityGain = 2.0f;
    f32 intelligenceGain = 1.5f;
    
    // Current stats (base + level gains + items)
    f32 strength = 20.0f;
    f32 agility = 20.0f;
    f32 intelligence = 20.0f;
    
    // Derived stats
    f32 maxHealth = 200.0f;
    f32 currentHealth = 200.0f;
    f32 healthRegen = 0.0f;
    
    f32 maxMana = 75.0f;
    f32 currentMana = 75.0f;
    f32 manaRegen = 0.0f;
    
    f32 damage = 50.0f;
    f32 attackRange = 5.0f;  // Melee default (close range)
    f32 attackSpeed = 100.0f;  // Base attack time modifier
    f32 armor = 0.0f;
    f32 moveSpeed = 300.0f;
    
    // Combat state
    HeroState state = HeroState::Idle;
    Entity targetEntity = INVALID_ENTITY;
    Vec3 targetPosition = Vec3(0.0f);
    f32 attackCooldown = 0.0f;
    
    // Abilities (Q, W, E, R + 2 extra)
    HeroAbility abilities[6];
    i32 currentCastingAbility = -1;
    f32 castTimer = 0.0f;
    
    // Movement
    Vector<Vec3> movePath;
    i32 currentPathIndex = 0;
    
    // Respawn
    f32 respawnTimer = 0.0f;
    Vec3 respawnPosition = Vec3(0.0f);
    
    // Player control
    bool isPlayerControlled = false;
    
    // Buffs/Debuffs
    Vector<Buff> buffs;
    
    // Inventory
    Item inventory[static_cast<i32>(ItemSlot::COUNT)];
    i32 gold = 600;  // Starting gold
    
    // Kill/Death stats
    i32 kills = 0;
    i32 deaths = 0;
    i32 assists = 0;
    i32 lastHits = 0;
    i32 denies = 0;
    
    HeroComponent() = default;
    HeroComponent(const String& name, i32 team) : heroName(name), teamId(team) {}
    
    // Helper methods
    bool hasBuffType(BuffType type) const {
        for (const auto& buff : buffs) {
            if (buff.type == type && buff.remainingTime > 0.0f) {
                return true;
            }
        }
        return false;
    }
    
    bool isStunned() const { return hasBuffType(BuffType::Stun) || hasBuffType(BuffType::Hex); }
    bool isSilenced() const { return hasBuffType(BuffType::Silence) || hasBuffType(BuffType::Hex); }
    bool isDisarmed() const { return hasBuffType(BuffType::Disarm) || hasBuffType(BuffType::Hex); }
    bool isRooted() const { return hasBuffType(BuffType::Root); }
    bool isInvisible() const { return hasBuffType(BuffType::Invisibility); }
    bool isInvulnerable() const { return hasBuffType(BuffType::Invulnerable); }
};

// Input command for hero
struct HeroCommand {
    enum class Type : u8 {
        None = 0,
        MoveTo,
        AttackMove,
        AttackTarget,
        CastAbility,
        Stop,
        Hold
    };
    
    Type type = Type::None;
    Vec3 targetPosition = Vec3(0.0f);
    Entity targetEntity = INVALID_ENTITY;
    i32 abilityIndex = -1;
};

class HeroSystem : public System {
public:
    HeroSystem(EntityManager& entityManager);
    ~HeroSystem() override = default;

    void update(f32 deltaTime) override;
    String getName() const override { return "HeroSystem"; }
    
    void setWorld(World* world) { world_ = world; }
    
    // Hero creation
    Entity createHero(const String& heroName, i32 teamId, const Vec3& position);
    Entity createHeroByType(const String& heroType, i32 teamId, const Vec3& position);
    
    // Hero commands (from player input)
    void issueCommand(Entity hero, const HeroCommand& command);
    void moveToPosition(Entity hero, const Vec3& position);
    void attackTarget(Entity hero, Entity target);
    void castAbility(Entity hero, i32 abilityIndex, const Vec3& targetPos, Entity targetEntity);
    void stopHero(Entity hero);
    
    // Ability management
    void learnAbility(Entity hero, i32 abilityIndex);
    bool canCastAbility(Entity hero, i32 abilityIndex) const;
    void executeAbility(Entity hero, i32 abilityIndex, const Vec3& targetPos, Entity targetEntity);
    
    // Buff/Debuff system
    void applyBuff(Entity target, const Buff& buff);
    void removeBuff(Entity target, BuffType type);
    void purgeBuffs(Entity target, bool purgePosive, bool purgeNegative);
    void updateBuffs(Entity entity, HeroComponent& hero, f32 deltaTime);
    
    // Item system
    bool giveItem(Entity hero, const ItemData& item);
    bool useItem(Entity hero, ItemSlot slot, const Vec3& targetPos, Entity targetEntity);
    void dropItem(Entity hero, ItemSlot slot);
    void swapItems(Entity hero, ItemSlot slot1, ItemSlot slot2);
    void giveGold(Entity hero, i32 amount);
    
    // Experience & Leveling
    void giveExperience(Entity hero, f32 amount);
    void levelUp(Entity hero);
    
    // Get player-controlled hero
    Entity getPlayerHero() const { return playerHero_; }
    void setPlayerHero(Entity hero) { playerHero_ = hero; }
    
    // Predefined items
    static ItemData createItem_IronBranch();
    static ItemData createItem_Tango();
    static ItemData createItem_HealingSalve();
    static ItemData createItem_ClarityPotion();
    static ItemData createItem_BootsOfSpeed();
    static ItemData createItem_PowerTreads();
    static ItemData createItem_BladeMail();
    static ItemData createItem_Blink();

private:
    EntityManager& entityManager_;
    World* world_ = nullptr;
    Entity playerHero_ = INVALID_ENTITY;
    
    // Hero AI/behavior
    void updateHeroAI(Entity entity, HeroComponent& hero, TransformComponent& transform, f32 deltaTime);
    void updateEnemyHeroAI(Entity entity, HeroComponent& hero, TransformComponent& transform, f32 deltaTime);
    void tryUseAbilityAI(Entity entity, HeroComponent& hero, TransformComponent& transform, Entity targetEntity, const Vec3& targetPos);
    void updateHeroMovement(Entity entity, HeroComponent& hero, TransformComponent& transform, f32 deltaTime);
    void updateHeroCombat(Entity entity, HeroComponent& hero, TransformComponent& transform, f32 deltaTime);
    void updateHeroAbilities(Entity entity, HeroComponent& hero, f32 deltaTime);
    void updateItemCooldowns(HeroComponent& hero, f32 deltaTime);
    
    // Stats calculation
    void recalculateStats(HeroComponent& hero);
    f32 calculateDamage(const HeroComponent& hero) const;
    f32 calculateArmor(const HeroComponent& hero) const;
    f32 calculateAttackSpeed(const HeroComponent& hero) const;
    f32 calculateMoveSpeed(const HeroComponent& hero) const;
    
    // Combat helpers
    Entity findAttackTarget(const Vec3& position, i32 teamId, f32 range);
    void dealDamage(Entity attacker, Entity target, f32 damage, bool isMagical = false);
    void dealAreaDamage(Entity attacker, const Vec3& center, f32 radius, f32 damage, i32 teamId, bool isMagical = false);
    
    // Ability effect execution
    void executeAbilityEffect(Entity heroEntity, HeroComponent& hero, i32 abilityIndex, TransformComponent& transform);
    
    // Ability implementations
    void executeAbility_Fireball(Entity hero, const Vec3& targetPos, Entity targetEntity);
    void executeAbility_Blink(Entity hero, const Vec3& targetPos);
    void executeAbility_Stun(Entity hero, Entity targetEntity);
    void executeAbility_AreaNuke(Entity hero, const Vec3& targetPos);
    
    // Respawn
    void handleRespawn(Entity entity, HeroComponent& hero, f32 deltaTime);
    f32 calculateRespawnTime(i32 level) const;
    
    // Hero templates
    void setupHero_Warrior(HeroComponent& hero);
    void setupHero_Mage(HeroComponent& hero);
    void setupHero_Assassin(HeroComponent& hero);
};

} // namespace WorldEditor
