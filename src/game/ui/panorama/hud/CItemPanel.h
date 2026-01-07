#pragma once

#include "../core/CPanel2D.h"
#include <memory>
#include <vector>

// Forward declarations
namespace Game {
    struct ItemData;
}

namespace Panorama {

class CItemPanel : public CPanel2D {
public:
    CItemPanel();
    virtual ~CItemPanel() = default;

    void SetItemID(i32 itemID);
    void SetItems(const std::vector<Game::ItemData>& items);
    
    void UpdateCharges(i32 charges);
    void UpdateCooldown(f32 cooldown);
    void UpdateCooldown(i32 itemIndex, f32 cooldown, f32 maxCooldown);
    void UpdateCooldowns(const std::vector<f32>& cooldowns);
    void SetEmpty(bool empty);
    
    // Input handling
    bool OnKeyDown(i32 key) override;
    bool OnKeyUp(i32 key) override;

private:
    i32 m_itemID = -1;
    std::shared_ptr<CPanel2D> m_icon;
    std::shared_ptr<CPanel2D> m_chargesLabel;
    std::shared_ptr<CPanel2D> m_cooldownOverlay;
    
    // Multiple item slots (6 inventory slots)
    std::vector<std::shared_ptr<CPanel2D>> m_itemIcons;
    std::vector<std::shared_ptr<CPanel2D>> m_itemCooldowns;
    std::vector<std::shared_ptr<CPanel2D>> m_itemCharges;
};

} // namespace Panorama