#pragma once
/**
 * CHUDManager - Central HUD Manager for Dota 2-style Game Interface
 * Manages all HUD components and integrates with game state
 */

#include "../core/PanoramaTypes.h"
#include "../core/CUIEngine.h"
#include "../core/GameEvents.h"
#include <memory>
#include <functional>

// Forward declarations
namespace Game {
    struct GameState;
    struct HeroData;
}

namespace Panorama {

// Forward declarations of HUD components
class CHeroUnitFrame;
class CAbilityPanel;
class CItemPanel;
class CMinimap;
class CScoreboard;
class CTooltip;
class CNotificationManager;

// ============ HUD Manager ============

class CHUDManager {
public:
    static CHUDManager& Instance();
    
    // ============ Lifecycle ============
    bool Initialize(CUIEngine* uiEngine);
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }
    
    // ============ Component Access ============
    CHeroUnitFrame* GetHeroFrame() { return m_heroFrame.get(); }
    CAbilityPanel* GetAbilityPanel() { return m_abilityPanel.get(); }
    CItemPanel* GetItemPanel() { return m_itemPanel.get(); }
    CMinimap* GetMinimap() { return m_minimap.get(); }
    CScoreboard* GetScoreboard() { return m_scoreboard.get(); }
    CTooltip* GetTooltip() { return m_tooltip.get(); }
    CNotificationManager* GetNotifications() { return m_notifications.get(); }
    
    // ============ Game State Integration ============
    void UpdateFromGameState(const Game::GameState& state);
    void SetHeroData(const Game::HeroData& hero);
    
    // ============ Event Handling ============
    void OnGameEvent(const std::string& eventName, const CGameEventData& data);
    void SetupGameEventHandlers();
    
    // ============ Visibility Control ============
    void SetHUDVisible(bool visible);
    bool IsHUDVisible() const { return m_hudVisible; }
    void ToggleScoreboard();
    
    // ============ Update & Render ============
    void Update(f32 deltaTime);
    void Render();
    
    // ============ Input Handling ============
    bool OnKeyDown(i32 key);
    bool OnKeyUp(i32 key);
    bool OnMouseMove(f32 x, f32 y);
    bool OnMouseDown(f32 x, f32 y, i32 button);
    bool OnMouseUp(f32 x, f32 y, i32 button);
    
private:
    CHUDManager() = default;
    ~CHUDManager() = default;
    
    // Initialization helpers
    void CreateHUDComponents();
    void SetupComponentLayout();
    void RegisterEventHandlers();
    
    // Event handlers
    void OnHeroHealthChanged(const CGameEventData& data);
    void OnHeroManaChanged(const CGameEventData& data);
    void OnHeroLevelUp(const CGameEventData& data);
    void OnAbilityCooldownStarted(const CGameEventData& data);
    void OnItemUsed(const CGameEventData& data);
    void OnHeroPositionUpdate(const CGameEventData& data);
    void OnPlayerKilled(const CGameEventData& data);
    void OnScoreboardUpdate(const CGameEventData& data);
    
    // State
    bool m_initialized = false;
    bool m_hudVisible = true;
    CUIEngine* m_uiEngine = nullptr;
    
    // HUD Components
    std::shared_ptr<CHeroUnitFrame> m_heroFrame;
    std::shared_ptr<CAbilityPanel> m_abilityPanel;
    std::shared_ptr<CItemPanel> m_itemPanel;
    std::shared_ptr<CMinimap> m_minimap;
    std::shared_ptr<CScoreboard> m_scoreboard;
    std::shared_ptr<CTooltip> m_tooltip;
    std::shared_ptr<CNotificationManager> m_notifications;
    
    // Event subscription IDs for cleanup
    std::vector<i32> m_eventSubscriptions;
};

// ============ Convenience Functions ============

// Get HUD Manager instance
inline CHUDManager& GetHUDManager() {
    return CHUDManager::Instance();
}

// Quick access to HUD components
inline CHeroUnitFrame* GetHeroFrame() {
    return CHUDManager::Instance().GetHeroFrame();
}

inline CAbilityPanel* GetAbilityPanel() {
    return CHUDManager::Instance().GetAbilityPanel();
}

inline CItemPanel* GetItemPanel() {
    return CHUDManager::Instance().GetItemPanel();
}

inline CMinimap* GetMinimap() {
    return CHUDManager::Instance().GetMinimap();
}

inline CScoreboard* GetScoreboard() {
    return CHUDManager::Instance().GetScoreboard();
}

} // namespace Panorama