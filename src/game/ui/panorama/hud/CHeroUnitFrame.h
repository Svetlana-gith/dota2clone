#pragma once

#include "../core/CPanel2D.h"
#include <memory>
#include <string>

// Forward declarations
namespace Game {
    struct HeroData;
}

namespace Panorama {

// Forward declarations
class CImage;
class CLabel;
class CProgressBar;

/**
 * CHeroUnitFrame - Hero Unit Frame Component
 * 
 * Displays hero portrait, health/mana bars, level, and experience.
 * Implements Requirements 1.1, 1.6, 1.7 from the DOTA2-style HUD spec.
 */
class CHeroUnitFrame : public CPanel2D {
public:
    CHeroUnitFrame();
    virtual ~CHeroUnitFrame() = default;

    // ============ Hero Data Management ============
    void SetHeroID(i32 heroID);
    void SetHeroData(const Game::HeroData& hero);
    
    // ============ Health and Mana Updates ============
    void UpdateHealth(f32 health, f32 maxHealth);
    void UpdateHealthBar(f32 health, f32 maxHealth); // Alias for compatibility
    void UpdateMana(f32 mana, f32 maxMana);
    void UpdateManaBar(f32 mana, f32 maxMana); // Alias for compatibility
    
    // ============ Level and Experience ============
    void UpdateLevel(i32 level);
    void UpdateExperience(f32 experience, f32 experienceToNext);
    
    // ============ Portrait ============
    void UpdatePortrait(const std::string& portraitPath);
    
    // ============ Animation Support ============
    void AnimateHealthChange(f32 oldValue, f32 newValue);
    void AnimateManaChange(f32 oldValue, f32 newValue);
    
    // ============ Overrides ============
    void Update(f32 deltaTime) override;

private:
    // ============ Data ============
    i32 m_heroID = -1;
    f32 m_currentHealth = 100.0f;
    f32 m_maxHealth = 100.0f;
    f32 m_currentMana = 100.0f;
    f32 m_maxMana = 100.0f;
    i32 m_level = 1;
    f32 m_experience = 0.0f;
    f32 m_experienceToNext = 100.0f;
    
    // ============ UI Components ============
    std::shared_ptr<CImage> m_heroPortrait;
    std::shared_ptr<CProgressBar> m_healthBar;
    std::shared_ptr<CProgressBar> m_manaBar;
    std::shared_ptr<CProgressBar> m_experienceBar;
    std::shared_ptr<CLabel> m_levelLabel;
    std::shared_ptr<CLabel> m_healthLabel;
    std::shared_ptr<CLabel> m_manaLabel;
    
    // ============ Animation State ============
    struct BarAnimation {
        bool active = false;
        f32 startValue = 0.0f;
        f32 targetValue = 0.0f;
        f32 currentValue = 0.0f;
        f32 duration = 0.3f;
        f32 elapsed = 0.0f;
    };
    
    BarAnimation m_healthAnimation;
    BarAnimation m_manaAnimation;
    
    // ============ Helper Methods ============
    void CreateComponents();
    void LayoutComponents();
    void UpdateHealthDisplay();
    void UpdateManaDisplay();
    void UpdateLevelDisplay();
    void UpdateExperienceDisplay();
    void StartAnimation(BarAnimation& anim, f32 startValue, f32 targetValue);
    void UpdateAnimation(BarAnimation& anim, f32 deltaTime);
};

} // namespace Panorama