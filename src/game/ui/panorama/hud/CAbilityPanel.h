#pragma once

#include "../core/CPanel2D.h"
#include <memory>
#include <vector>

// Forward declarations
namespace Game {
    struct AbilityData;
}

namespace Panorama {

class CAbilityPanel : public CPanel2D {
public:
    CAbilityPanel();
    virtual ~CAbilityPanel() = default;

    void SetAbilityID(i32 abilityID);
    void SetAbilities(const std::vector<Game::AbilityData>& abilities);
    
    void UpdateCooldown(f32 cooldown);
    void UpdateCooldown(i32 abilityIndex, f32 cooldown, f32 maxCooldown);
    void UpdateCooldowns(const std::vector<f32>& cooldowns);
    void UpdateLevel(i32 abilityIndex, i32 level);
    void UpdateManaCost(i32 manaCost);
    void SetEnabled(bool enabled);
    
    // Input handling
    bool OnKeyDown(i32 key) override;
    bool OnKeyUp(i32 key) override;

private:
    i32 m_abilityID = -1;
    std::shared_ptr<CPanel2D> m_icon;
    std::shared_ptr<CPanel2D> m_cooldownOverlay;
    std::shared_ptr<CPanel2D> m_hotkey;
    
    // Multiple abilities (Q, W, E, R, D, F)
    std::vector<std::shared_ptr<CPanel2D>> m_abilityIcons;
    std::vector<std::shared_ptr<CPanel2D>> m_abilityCooldowns;
    std::vector<std::shared_ptr<CPanel2D>> m_abilityHotkeys;
    std::vector<std::shared_ptr<CPanel2D>> m_abilityLevels;
};

} // namespace Panorama