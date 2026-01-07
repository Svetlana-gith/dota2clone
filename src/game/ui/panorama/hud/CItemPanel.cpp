#include "CItemPanel.h"
#include "../widgets/CPanelWidgets.h"
#include "../widgets/CImage.h"
#include "../widgets/CLabel.h"
#include "../../../GameData.h"

namespace Panorama {

CItemPanel::CItemPanel() {
    // Create child panels for item components (single item version)
    m_icon = CPanelWidgets::CreateImage("", 0, 0, 32, 32);
    m_chargesLabel = CPanelWidgets::CreateLabel("", 20, 20);
    m_cooldownOverlay = CPanelWidgets::CreateImage("", 0, 0, 32, 32);
    
    AddChild(m_icon);
    AddChild(m_chargesLabel);
    AddChild(m_cooldownOverlay);
    
    m_cooldownOverlay->SetVisible(false);
    m_chargesLabel->SetVisible(false);
    
    // Create multiple item slots (6 inventory slots)
    m_itemIcons.resize(6);
    m_itemCooldowns.resize(6);
    m_itemCharges.resize(6);
    
    for (i32 i = 0; i < 6; ++i) {
        f32 x = static_cast<f32>((i % 3) * 40);
        f32 y = static_cast<f32>((i / 3) * 40);
        
        m_itemIcons[i] = CPanelWidgets::CreateImage("", x, y, 32, 32);
        m_itemCooldowns[i] = CPanelWidgets::CreateImage("", x, y, 32, 32);
        m_itemCharges[i] = CPanelWidgets::CreateLabel("", x + 20, y + 20);
        
        AddChild(m_itemIcons[i]);
        AddChild(m_itemCooldowns[i]);
        AddChild(m_itemCharges[i]);
        
        m_itemCooldowns[i]->SetVisible(false);
        m_itemCharges[i]->SetVisible(false);
    }
}

void CItemPanel::SetItemID(i32 itemID) {
    m_itemID = itemID;
}

void CItemPanel::SetItems(const std::vector<Game::ItemData>& items) {
    // Update item icons and data
    for (size_t i = 0; i < items.size() && i < m_itemIcons.size(); ++i) {
        const auto& item = items[i];
        
        if (item.isEmpty) {
            m_itemIcons[i]->SetVisible(false);
            m_itemCharges[i]->SetVisible(false);
        } else {
            m_itemIcons[i]->SetVisible(true);
            // TODO: Set item icon when CImage path setting is implemented
            
            if (item.charges > 0) {
                m_itemCharges[i]->SetVisible(true);
                // TODO: Set charges text when CLabel text setting is implemented
            } else {
                m_itemCharges[i]->SetVisible(false);
            }
        }
    }
}

void CItemPanel::UpdateCharges(i32 charges) {
    if (charges > 0) {
        m_chargesLabel->SetVisible(true);
        // TODO: Update charges label text
    } else {
        m_chargesLabel->SetVisible(false);
    }
}

void CItemPanel::UpdateCooldown(f32 cooldown) {
    if (cooldown > 0) {
        m_cooldownOverlay->SetVisible(true);
        // TODO: Update cooldown overlay
    } else {
        m_cooldownOverlay->SetVisible(false);
    }
}

void CItemPanel::UpdateCooldown(i32 itemIndex, f32 cooldown, f32 maxCooldown) {
    if (itemIndex >= 0 && itemIndex < static_cast<i32>(m_itemCooldowns.size())) {
        if (cooldown > 0) {
            m_itemCooldowns[itemIndex]->SetVisible(true);
            // TODO: Update cooldown overlay with progress
        } else {
            m_itemCooldowns[itemIndex]->SetVisible(false);
        }
    }
}

void CItemPanel::UpdateCooldowns(const std::vector<f32>& cooldowns) {
    for (size_t i = 0; i < cooldowns.size() && i < m_itemCooldowns.size(); ++i) {
        UpdateCooldown(static_cast<i32>(i), cooldowns[i], cooldowns[i]);
    }
}

void CItemPanel::SetEmpty(bool empty) {
    m_icon->SetVisible(!empty);
}

bool CItemPanel::OnKeyDown(i32 key) {
    // Handle item hotkeys (1-6, Z, X, C, V, B, N)
    // TODO: Implement item activation
    return false;
}

bool CItemPanel::OnKeyUp(i32 key) {
    // Handle item key releases
    return false;
}

} // namespace Panorama