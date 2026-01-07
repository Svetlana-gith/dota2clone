#include "CHUDManager.h"
#include "CHeroUnitFrame.h"
#include "CAbilityPanel.h"
#include "CItemPanel.h"
#include "CMinimap.h"
#include "CScoreboard.h"
#include "CTooltip.h"
#include "CNotificationManager.h"
#include "../layout/CStyleSheet.h"
#include "../../../GameData.h"
#include <spdlog/spdlog.h>

namespace Panorama {

// ============ CHUDManager Implementation ============

CHUDManager& CHUDManager::Instance() {
    static CHUDManager instance;
    return instance;
}

bool CHUDManager::Initialize(CUIEngine* uiEngine) {
    if (m_initialized) {
        LOG_WARN("CHUDManager already initialized");
        return true;
    }
    
    if (!uiEngine) {
        LOG_ERROR("CHUDManager::Initialize - uiEngine is null");
        return false;
    }
    
    LOG_INFO("CHUDManager::Initialize - Starting HUD initialization");
    
    m_uiEngine = uiEngine;
    
    // Create HUD components
    CreateHUDComponents();
    
    // Setup component layout
    SetupComponentLayout();
    
    // Register event handlers
    RegisterEventHandlers();
    
    m_initialized = true;
    LOG_INFO("CHUDManager initialized successfully");
    
    return true;
}

void CHUDManager::Shutdown() {
    if (!m_initialized) {
        return;
    }
    
    LOG_INFO("CHUDManager::Shutdown - Cleaning up HUD");
    
    // Unsubscribe from all events
    for (i32 subscriptionId : m_eventSubscriptions) {
        CGameEvents::Instance().Unsubscribe(subscriptionId);
    }
    m_eventSubscriptions.clear();
    
    // Destroy components in reverse order
    m_notifications.reset();
    m_tooltip.reset();
    m_scoreboard.reset();
    m_minimap.reset();
    m_itemPanel.reset();
    m_abilityPanel.reset();
    m_heroFrame.reset();
    
    m_uiEngine = nullptr;
    m_initialized = false;
    
    LOG_INFO("CHUDManager shutdown complete");
}

void CHUDManager::CreateHUDComponents() {
    LOG_INFO("CHUDManager::CreateHUDComponents - Creating HUD components");
    
    // Create components
    m_heroFrame = std::make_shared<CHeroUnitFrame>();
    m_abilityPanel = std::make_shared<CAbilityPanel>();
    m_itemPanel = std::make_shared<CItemPanel>();
    m_minimap = std::make_shared<CMinimap>();
    m_scoreboard = std::make_shared<CScoreboard>();
    m_tooltip = std::make_shared<CTooltip>();
    m_notifications = std::make_shared<CNotificationManager>();
    
    // Add components to UI root
    auto root = m_uiEngine->GetRoot();
    if (root) {
        root->AddChild(m_heroFrame);
        root->AddChild(m_abilityPanel);
        root->AddChild(m_itemPanel);
        root->AddChild(m_minimap);
        root->AddChild(m_scoreboard);
        root->AddChild(m_tooltip);
        root->AddChild(m_notifications);
    }
    
    LOG_INFO("HUD components created successfully");
}

void CHUDManager::SetupComponentLayout() {
    LOG_INFO("CHUDManager::SetupComponentLayout - Loading CSS styles for HUD layout");
    
    // Load HUD stylesheet
    auto& styleManager = CStyleManager::Instance();
    styleManager.LoadGlobalStyles("resources/styles/hud.css");
    
    // Set component IDs for CSS targeting
    if (m_heroFrame) {
        m_heroFrame->SetID("HeroUnitFrame");
    }
    
    if (m_abilityPanel) {
        m_abilityPanel->SetID("AbilityPanel");
    }
    
    if (m_itemPanel) {
        m_itemPanel->SetID("ItemPanel");
    }
    
    if (m_minimap) {
        m_minimap->SetID("Minimap");
    }
    
    if (m_scoreboard) {
        m_scoreboard->SetID("Scoreboard");
        m_scoreboard->SetVisible(false);
    }
    
    if (m_tooltip) {
        m_tooltip->SetID("Tooltip");
        m_tooltip->SetVisible(false);
    }
    
    if (m_notifications) {
        m_notifications->SetID("NotificationManager");
    }
    
    LOG_INFO("HUD layout setup complete - styles loaded from CSS");
}

void CHUDManager::RegisterEventHandlers() {
    LOG_INFO("CHUDManager::RegisterEventHandlers - Setting up event handlers");
    
    auto& events = CGameEvents::Instance();
    
    // Hero state events
    m_eventSubscriptions.push_back(
        events.Subscribe("hud_hero_health_changed", [this](const CGameEventData& data) {
            OnHeroHealthChanged(data);
        })
    );
    
    m_eventSubscriptions.push_back(
        events.Subscribe("hud_hero_mana_changed", [this](const CGameEventData& data) {
            OnHeroManaChanged(data);
        })
    );
    
    m_eventSubscriptions.push_back(
        events.Subscribe("hud_hero_level_up", [this](const CGameEventData& data) {
            OnHeroLevelUp(data);
        })
    );
    
    // Ability events
    m_eventSubscriptions.push_back(
        events.Subscribe("hud_ability_cooldown_started", [this](const CGameEventData& data) {
            OnAbilityCooldownStarted(data);
        })
    );
    
    // Item events
    m_eventSubscriptions.push_back(
        events.Subscribe("hud_item_used", [this](const CGameEventData& data) {
            OnItemUsed(data);
        })
    );
    
    // Position events
    m_eventSubscriptions.push_back(
        events.Subscribe("hud_hero_position_update", [this](const CGameEventData& data) {
            OnHeroPositionUpdate(data);
        })
    );
    
    // Combat events
    m_eventSubscriptions.push_back(
        events.Subscribe("hud_player_killed", [this](const CGameEventData& data) {
            OnPlayerKilled(data);
        })
    );
    
    // Scoreboard events
    m_eventSubscriptions.push_back(
        events.Subscribe("hud_scoreboard_update", [this](const CGameEventData& data) {
            OnScoreboardUpdate(data);
        })
    );
    
    LOG_INFO("Event handlers registered successfully");
}

void CHUDManager::UpdateFromGameState(const Game::GameState& state) {
    if (!m_initialized) return;
    
    // Update hero frame
    if (m_heroFrame) {
        m_heroFrame->UpdateHealth(state.currentHealth, state.maxHealth);
        m_heroFrame->UpdateMana(state.currentMana, state.maxMana);
        m_heroFrame->UpdateLevel(state.level);
        m_heroFrame->UpdateExperience(state.experience, state.experienceToNext);
    }
    
    // Update ability panel
    if (m_abilityPanel) {
        m_abilityPanel->SetAbilities(state.abilities);
        m_abilityPanel->UpdateCooldowns(state.abilityCooldowns);
        for (i32 i = 0; i < 6; ++i) {
            m_abilityPanel->UpdateLevel(i, state.abilityLevels[i]);
        }
    }
    
    // Update item panel
    if (m_itemPanel) {
        m_itemPanel->SetItems(state.items);
        m_itemPanel->UpdateCooldowns(state.itemCooldowns);
    }
    
    // Update minimap
    if (m_minimap) {
        m_minimap->UpdateHeroPositions(state.allHeroes);
        m_minimap->UpdateTowerStates(state.towers);
        m_minimap->UpdateCameraPosition(state.cameraPosition);
    }
    
    // Update scoreboard
    if (m_scoreboard) {
        m_scoreboard->UpdateAllPlayers(state.playerStats);
    }
}

void CHUDManager::SetHeroData(const Game::HeroData& hero) {
    if (m_heroFrame) {
        m_heroFrame->SetHeroData(hero);
    }
}

void CHUDManager::Update(f32 deltaTime) {
    if (!m_initialized || !m_hudVisible) return;
    
    // Update all components
    if (m_heroFrame) m_heroFrame->Update(deltaTime);
    if (m_abilityPanel) m_abilityPanel->Update(deltaTime);
    if (m_itemPanel) m_itemPanel->Update(deltaTime);
    if (m_minimap) m_minimap->Update(deltaTime);
    if (m_scoreboard) m_scoreboard->Update(deltaTime);
    if (m_tooltip) m_tooltip->Update(deltaTime);
    if (m_notifications) m_notifications->Update(deltaTime);
}

void CHUDManager::Render() {
    if (!m_initialized || !m_hudVisible) return;
    
    // Components are rendered automatically by the UI engine
    // This method can be used for custom rendering if needed
}

void CHUDManager::SetHUDVisible(bool visible) {
    m_hudVisible = visible;
    
    // Update visibility of all components
    if (m_heroFrame) m_heroFrame->SetVisible(visible);
    if (m_abilityPanel) m_abilityPanel->SetVisible(visible);
    if (m_itemPanel) m_itemPanel->SetVisible(visible);
    if (m_minimap) m_minimap->SetVisible(visible);
    if (m_notifications) m_notifications->SetVisible(visible);
    
    // Scoreboard visibility is controlled separately
}

void CHUDManager::ToggleScoreboard() {
    if (m_scoreboard) {
        m_scoreboard->ToggleVisibility();
    }
}

bool CHUDManager::OnKeyDown(i32 key) {
    if (!m_initialized) return false;
    
    // Tab key toggles scoreboard
    if (key == 9) { // Tab key
        ToggleScoreboard();
        return true;
    }
    
    // Forward to components
    bool handled = false;
    if (m_abilityPanel) handled |= m_abilityPanel->OnKeyDown(key);
    if (m_itemPanel) handled |= m_itemPanel->OnKeyDown(key);
    
    return handled;
}

bool CHUDManager::OnKeyUp(i32 key) {
    if (!m_initialized) return false;
    
    // Forward to components
    bool handled = false;
    if (m_abilityPanel) handled |= m_abilityPanel->OnKeyUp(key);
    if (m_itemPanel) handled |= m_itemPanel->OnKeyUp(key);
    
    return handled;
}

bool CHUDManager::OnMouseMove(f32 x, f32 y) {
    if (!m_initialized) return false;
    
    // Forward to components
    bool handled = false;
    if (m_heroFrame) handled |= m_heroFrame->OnMouseMove(x, y);
    if (m_abilityPanel) handled |= m_abilityPanel->OnMouseMove(x, y);
    if (m_itemPanel) handled |= m_itemPanel->OnMouseMove(x, y);
    if (m_minimap) handled |= m_minimap->OnMouseMove(x, y);
    if (m_scoreboard && m_scoreboard->IsVisible()) handled |= m_scoreboard->OnMouseMove(x, y);
    
    return handled;
}

bool CHUDManager::OnMouseDown(f32 x, f32 y, i32 button) {
    if (!m_initialized) return false;
    
    // Forward to components
    bool handled = false;
    if (m_scoreboard && m_scoreboard->IsVisible()) handled |= m_scoreboard->OnMouseDown(x, y, button);
    if (!handled && m_minimap) handled |= m_minimap->OnMouseDown(x, y, button);
    if (!handled && m_itemPanel) handled |= m_itemPanel->OnMouseDown(x, y, button);
    if (!handled && m_abilityPanel) handled |= m_abilityPanel->OnMouseDown(x, y, button);
    if (!handled && m_heroFrame) handled |= m_heroFrame->OnMouseDown(x, y, button);
    
    return handled;
}

bool CHUDManager::OnMouseUp(f32 x, f32 y, i32 button) {
    if (!m_initialized) return false;
    
    // Forward to components
    bool handled = false;
    if (m_scoreboard && m_scoreboard->IsVisible()) handled |= m_scoreboard->OnMouseUp(x, y, button);
    if (!handled && m_minimap) handled |= m_minimap->OnMouseUp(x, y, button);
    if (!handled && m_itemPanel) handled |= m_itemPanel->OnMouseUp(x, y, button);
    if (!handled && m_abilityPanel) handled |= m_abilityPanel->OnMouseUp(x, y, button);
    if (!handled && m_heroFrame) handled |= m_heroFrame->OnMouseUp(x, y, button);
    
    return handled;
}

// ============ Event Handlers ============

void CHUDManager::OnHeroHealthChanged(const CGameEventData& data) {
    if (m_heroFrame) {
        f32 current = data.GetFloat("current", 0.0f);
        f32 max = data.GetFloat("max", 100.0f);
        m_heroFrame->UpdateHealth(current, max);
    }
}

void CHUDManager::OnHeroManaChanged(const CGameEventData& data) {
    if (m_heroFrame) {
        f32 current = data.GetFloat("current", 0.0f);
        f32 max = data.GetFloat("max", 100.0f);
        m_heroFrame->UpdateMana(current, max);
    }
}

void CHUDManager::OnHeroLevelUp(const CGameEventData& data) {
    if (m_heroFrame) {
        i32 level = data.GetInt("level", 1);
        m_heroFrame->UpdateLevel(level);
    }
}

void CHUDManager::OnAbilityCooldownStarted(const CGameEventData& data) {
    if (m_abilityPanel) {
        i32 abilityIndex = data.GetInt("ability_index", 0);
        f32 cooldown = data.GetFloat("cooldown", 0.0f);
        m_abilityPanel->UpdateCooldown(abilityIndex, cooldown, cooldown);
    }
}

void CHUDManager::OnItemUsed(const CGameEventData& data) {
    if (m_itemPanel) {
        i32 itemIndex = data.GetInt("item_index", 0);
        f32 cooldown = data.GetFloat("cooldown", 0.0f);
        m_itemPanel->UpdateCooldown(itemIndex, cooldown, cooldown);
    }
}

void CHUDManager::OnHeroPositionUpdate(const CGameEventData& data) {
    // This would typically update minimap with new hero positions
    // Implementation depends on the data format
}

void CHUDManager::OnPlayerKilled(const CGameEventData& data) {
    if (m_notifications) {
        std::string killer = data.GetString("killer", "Unknown");
        std::string victim = data.GetString("victim", "Unknown");
        std::string ability = data.GetString("ability", "");
        m_notifications->ShowKillFeed(killer, victim, ability);
    }
}

void CHUDManager::OnScoreboardUpdate(const CGameEventData& data) {
    // This would update scoreboard with new player stats
    // Implementation depends on the data format
}

} // namespace Panorama