#pragma once
/**
 * HUD Components - Dota 2-style game interface elements
 * Hero unit frame, minimap, ability panel, scoreboard, etc.
 */

#include "../core/CPanel2D.h"
#include "../core/PanoramaTypes.h"
#include <array>

namespace Panorama {

// Forward declarations
struct HeroData;
struct AbilityData;
struct ItemData;

// ============ Hero Unit Frame ============

class CHeroUnitFrame : public CPanel2D {
public:
    CHeroUnitFrame();
    
    void SetHeroData(const HeroData& hero);
    void UpdateHealth(f32 current, f32 max);
    void UpdateMana(f32 current, f32 max);
    void UpdateLevel(i32 level);
    void UpdateExperience(f32 current, f32 toNext);
    
    void Render(CUIRenderer* renderer) override;
    
private:
    std::shared_ptr<CImage> m_heroPortrait;
    std::shared_ptr<CProgressBar> m_healthBar;
    std::shared_ptr<CProgressBar> m_manaBar;
    std::shared_ptr<CProgressBar> m_expBar;
    std::shared_ptr<CLabel> m_levelLabel;
    std::shared_ptr<CLabel> m_healthText;
    std::shared_ptr<CLabel> m_manaText;
    
    f32 m_currentHealth = 100.0f;
    f32 m_maxHealth = 100.0f;
    f32 m_currentMana = 100.0f;
    f32 m_maxMana = 100.0f;
    i32 m_level = 1;
};

// ============ Ability Panel ============

class CAbilitySlot : public CPanel2D {
public:
    CAbilitySlot(i32 slotIndex);
    
    void SetAbility(const AbilityData& ability);
    void SetCooldown(f32 remaining, f32 total);
    void SetManaCost(i32 cost);
    void SetLevel(i32 level, i32 maxLevel);
    void SetHotkey(const std::string& key);
    
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    void Render(CUIRenderer* renderer) override;
    
private:
    i32 m_slotIndex;
    std::shared_ptr<CImage> m_abilityIcon;
    std::shared_ptr<CLabel> m_hotkeyLabel;
    std::shared_ptr<CLabel> m_levelLabel;
    std::shared_ptr<CPanel2D> m_cooldownOverlay;
    
    f32 m_cooldownRemaining = 0.0f;
    f32 m_cooldownTotal = 0.0f;
    i32 m_manaCost = 0;
    i32 m_abilityLevel = 0;
    i32 m_maxLevel = 4;
    std::string m_hotkey;
};

class CAbilityPanel : public CPanel2D {
public:
    CAbilityPanel();
    
    void SetAbilities(const std::array<AbilityData, 6>& abilities);
    void UpdateCooldowns(const std::array<f32, 6>& cooldowns);
    void UpdateManaCosts(const std::array<i32, 6>& costs);
    
private:
    std::array<std::shared_ptr<CAbilitySlot>, 6> m_abilitySlots;
    static constexpr std::array<const char*, 6> DEFAULT_HOTKEYS = {"Q", "W", "E", "D", "F", "R"};
};

// ============ Item Panel ============

class CItemSlot : public CPanel2D {
public:
    CItemSlot(i32 slotIndex);
    
    void SetItem(const ItemData& item);
    void SetCooldown(f32 remaining, f32 total);
    void SetCharges(i32 charges);
    void SetHotkey(const std::string& key);
    void ClearItem();
    
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    void Render(CUIRenderer* renderer) override;
    
    // Drag & Drop support
    bool OnDragStart(f32 x, f32 y) override;
    void OnDragEnd(f32 x, f32 y) override;
    bool OnDrop(CPanel2D* draggedPanel) override;
    
private:
    i32 m_slotIndex;
    std::shared_ptr<CImage> m_itemIcon;
    std::shared_ptr<CLabel> m_hotkeyLabel;
    std::shared_ptr<CLabel> m_chargesLabel;
    std::shared_ptr<CPanel2D> m_cooldownOverlay;
    
    bool m_hasItem = false;
    f32 m_cooldownRemaining = 0.0f;
    f32 m_cooldownTotal = 0.0f;
    i32 m_charges = 0;
    std::string m_hotkey;
};

class CItemPanel : public CPanel2D {
public:
    CItemPanel();
    
    void SetItems(const std::array<ItemData, 6>& items);
    void UpdateCooldowns(const std::array<f32, 6>& cooldowns);
    void SwapItems(i32 fromSlot, i32 toSlot);
    
private:
    std::array<std::shared_ptr<CItemSlot>, 6> m_itemSlots;
    static constexpr std::array<const char*, 6> DEFAULT_HOTKEYS = {"Z", "X", "C", "V", "B", "N"};
};

// ============ Minimap ============

class CMinimap : public CPanel2D {
public:
    CMinimap();
    
    void SetMapTexture(const std::string& texturePath);
    void UpdateHeroPositions(const std::vector<Vector2D>& positions, const std::vector<i32>& teams);
    void UpdateTowerStates(const std::vector<bool>& towerStates);
    void SetCameraPosition(const Vector2D& worldPos);
    
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    void Render(CUIRenderer* renderer) override;
    
private:
    std::shared_ptr<CImage> m_mapBackground;
    std::vector<Vector2D> m_heroPositions;
    std::vector<i32> m_heroTeams;
    std::vector<bool> m_towerStates;
    Vector2D m_cameraPosition;
    
    Vector2D WorldToMinimap(const Vector2D& worldPos) const;
    Vector2D MinimapToWorld(const Vector2D& minimapPos) const;
    
    static constexpr f32 MAP_SIZE = 8192.0f; // Dota 2 map size
};

// ============ Scoreboard ============

struct PlayerScoreData {
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
    f32 respawnTime = 0.0f;
};

class CScoreboardRow : public CPanel2D {
public:
    CScoreboardRow(i32 playerIndex, bool isRadiant);
    
    void UpdatePlayerData(const PlayerScoreData& data);
    
private:
    i32 m_playerIndex;
    bool m_isRadiant;
    
    std::shared_ptr<CImage> m_heroIcon;
    std::shared_ptr<CLabel> m_playerName;
    std::shared_ptr<CLabel> m_kdaLabel;
    std::shared_ptr<CLabel> m_lastHitsLabel;
    std::shared_ptr<CLabel> m_goldLabel;
    std::shared_ptr<CLabel> m_levelLabel;
    std::shared_ptr<CPanel2D> m_respawnOverlay;
};

class CScoreboard : public CPanel2D {
public:
    CScoreboard();
    
    void UpdateAllPlayers(const std::array<PlayerScoreData, 10>& playerData);
    void SetVisible(bool visible) override;
    void ToggleVisibility();
    
private:
    std::array<std::shared_ptr<CScoreboardRow>, 10> m_playerRows;
    std::shared_ptr<CPanel2D> m_radiantTeam;
    std::shared_ptr<CPanel2D> m_direTeam;
    std::shared_ptr<CLabel> m_gameTimeLabel;
    
    void CreateTeamHeaders();
};

// ============ Tooltip System ============

class CTooltip : public CPanel2D {
public:
    CTooltip();
    
    void ShowAbilityTooltip(const AbilityData& ability, const Vector2D& position);
    void ShowItemTooltip(const ItemData& item, const Vector2D& position);
    void ShowHeroTooltip(const HeroData& hero, const Vector2D& position);
    void Hide();
    
    void Update(f32 deltaTime) override;
    void Render(CUIRenderer* renderer) override;
    
private:
    std::shared_ptr<CPanel2D> m_background;
    std::shared_ptr<CLabel> m_titleLabel;
    std::shared_ptr<CLabel> m_descriptionLabel;
    std::shared_ptr<CLabel> m_statsLabel;
    
    f32 m_showDelay = 0.5f;
    f32 m_currentDelay = 0.0f;
    bool m_shouldShow = false;
    
    void PositionTooltip(const Vector2D& targetPos);
};

// ============ Notification System ============

enum class NotificationType {
    Info,
    Warning,
    Error,
    Achievement,
    KillFeed
};

class CNotification : public CPanel2D {
public:
    CNotification(NotificationType type, const std::string& message, f32 duration = 3.0f);
    
    void Update(f32 deltaTime) override;
    bool IsExpired() const { return m_timeRemaining <= 0.0f; }
    
private:
    NotificationType m_type;
    f32 m_duration;
    f32 m_timeRemaining;
    std::shared_ptr<CLabel> m_messageLabel;
    std::shared_ptr<CImage> m_iconImage;
};

class CNotificationManager : public CPanel2D {
public:
    CNotificationManager();
    
    void ShowNotification(NotificationType type, const std::string& message, f32 duration = 3.0f);
    void ShowKillFeed(const std::string& killer, const std::string& victim, const std::string& ability = "");
    
    void Update(f32 deltaTime) override;
    
private:
    std::vector<std::shared_ptr<CNotification>> m_notifications;
    static constexpr i32 MAX_NOTIFICATIONS = 5;
    
    void RemoveExpiredNotifications();
    void RepositionNotifications();
};

// ============ Data Structures ============

struct HeroData {
    std::string name;
    std::string displayName;
    std::string portraitPath;
    f32 baseHealth = 100.0f;
    f32 baseMana = 100.0f;
    i32 baseArmor = 0;
    f32 baseDamage = 50.0f;
    f32 moveSpeed = 300.0f;
};

struct AbilityData {
    std::string name;
    std::string displayName;
    std::string description;
    std::string iconPath;
    i32 manaCost = 0;
    f32 cooldown = 0.0f;
    i32 maxLevel = 4;
    std::vector<std::string> levelDescriptions;
};

struct ItemData {
    std::string name;
    std::string displayName;
    std::string description;
    std::string iconPath;
    i32 cost = 0;
    f32 cooldown = 0.0f;
    bool isActive = false;
    std::vector<std::string> components;
};

} // namespace Panorama