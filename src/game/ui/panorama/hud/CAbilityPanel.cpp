#include "CAbilityPanel.h"
#include "../widgets/CPanelWidgets.h"
#include "../widgets/CImage.h"
#include "../widgets/CLabel.h"
#include "../../../GameData.h"

namespace Panorama {

CAbilityPanel::CAbilityPanel() {
    // Create child panels for ability components (single ability version)
    m_icon = CPanelWidgets::CreateImage("", 0, 0, 48, 48);
    m_cooldownOverlay = CPanelWidgets::CreateImage("", 0, 0, 48, 48);
    m_hotkey = CPanelWidgets::CreateLabel("Q", 2, 2);
    
    AddChild(m_icon);
    AddChild(m_cooldownOverlay);
    AddChild(m_hotkey);
    
    m_cooldownOverlay->SetVisible(false);
    
    // Create multiple ability panels (Q, W, E, R, D, F)
    m_abilityIcons.resize(6);
    m_abilityCooldowns.resize(6);
    m_abilityHotkeys.resize(6);
    m_abilityLevels.resize(6);
    
    const std::vector<std::string> hotkeys = {"Q", "W", "E", "R", "D", "F"};
    
    for (i32 i = 0; i < 6; ++i) {
        f32 x = static_cast<f32>(i * 60);
        
        m_abilityIcons[i] = CPanelWidgets::CreateImage("", x, 0, 48, 48);
        m_abilityCooldowns[i] = CPanelWidgets::CreateImage("", x, 0, 48, 48);
        m_abilityHotkeys[i] = CPanelWidgets::CreateLabel(hotkeys[i], x + 2, 2);
        m_abilityLevels[i] = CPanelWidgets::CreateLabel("0", x + 35, 35);
        
        AddChild(m_abilityIcons[i]);
        AddChild(m_abilityCooldowns[i]);
        AddChild(m_abilityHotkeys[i]);
        AddChild(m_abilityLevels[i]);
        
        m_abilityCooldowns[i]->SetVisible(false);
    }
}

void CAbilityPanel::SetAbilityID(i32 abilityID) {
    m_abilityID = abilityID;
}

void CAbilityPanel::SetAbilities(const std::vector<Game::AbilityData>& abilities) {
    // Update ability icons and data
    for (size_t i = 0; i < abilities.size() && i < m_abilityIcons.size(); ++i) {
        const auto& ability = abilities[i];
        
        // TODO: Set ability icon when CImage path setting is implemented
        // TODO: Set hotkey text when CLabel text setting is implemented
        // TODO: Set level text when CLabel text setting is implemented
    }
}

void CAbilityPanel::UpdateCooldown(f32 cooldown) {
    if (cooldown > 0) {
        m_cooldownOverlay->SetVisible(true);
        // TODO: Update cooldown overlay
    } else {
        m_cooldownOverlay->SetVisible(false);
    }
}

void CAbilityPanel::UpdateCooldown(i32 abilityIndex, f32 cooldown, f32 maxCooldown) {
    if (abilityIndex >= 0 && abilityIndex < static_cast<i32>(m_abilityCooldowns.size())) {
        if (cooldown > 0) {
            m_abilityCooldowns[abilityIndex]->SetVisible(true);
            // TODO: Update cooldown overlay with progress
        } else {
            m_abilityCooldowns[abilityIndex]->SetVisible(false);
        }
    }
}

void CAbilityPanel::UpdateCooldowns(const std::vector<f32>& cooldowns) {
    for (size_t i = 0; i < cooldowns.size() && i < m_abilityCooldowns.size(); ++i) {
        UpdateCooldown(static_cast<i32>(i), cooldowns[i], cooldowns[i]);
    }
}

void CAbilityPanel::UpdateLevel(i32 abilityIndex, i32 level) {
    if (abilityIndex >= 0 && abilityIndex < static_cast<i32>(m_abilityLevels.size())) {
        // TODO: Update level label text when CLabel text setting is implemented
    }
}

void CAbilityPanel::UpdateManaCost(i32 manaCost) {
    // TODO: Update mana cost display
}

void CAbilityPanel::SetEnabled(bool enabled) {
    SetVisible(enabled);
}

bool CAbilityPanel::OnKeyDown(i32 key) {
    // Handle ability hotkeys (Q, W, E, R, D, F)
    // TODO: Implement ability activation
    return false;
}

bool CAbilityPanel::OnKeyUp(i32 key) {
    // Handle ability key releases
    return false;
}

} // namespace Panorama