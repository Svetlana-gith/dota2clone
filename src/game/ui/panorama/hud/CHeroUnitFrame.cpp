#include "CHeroUnitFrame.h"
#include "../widgets/CPanelWidgets.h"
#include "../widgets/CImage.h"
#include "../widgets/CProgressBar.h"
#include "../widgets/CLabel.h"
#include "../../../GameData.h"

namespace Panorama {

CHeroUnitFrame::CHeroUnitFrame() {
    // Create child panels for hero unit frame components
    m_heroPortrait = CPanelWidgets::CreateImage("", 0, 0, 64, 64);
    m_healthBar = CPanelWidgets::CreateProgressBar(70, 10, 100, 20);
    m_manaBar = CPanelWidgets::CreateProgressBar(70, 35, 100, 15);
    m_experienceBar = CPanelWidgets::CreateProgressBar(70, 55, 100, 8);
    m_levelLabel = CPanelWidgets::CreateLabel("1", 5, 5);
    
    AddChild(m_heroPortrait);
    AddChild(m_healthBar);
    AddChild(m_manaBar);
    AddChild(m_experienceBar);
    AddChild(m_levelLabel);
}

void CHeroUnitFrame::SetHeroID(i32 heroID) {
    m_heroID = heroID;
}

void CHeroUnitFrame::SetHeroData(const Game::HeroData& hero) {
    m_heroID = 0; // Could use hero.heroId if it was an int
    UpdatePortrait(hero.portraitPath);
    // Could set other hero-specific data here
}

void CHeroUnitFrame::UpdateHealth(f32 health, f32 maxHealth) {
    UpdateHealthBar(health, maxHealth);
}

void CHeroUnitFrame::UpdateHealthBar(f32 health, f32 maxHealth) {
    // Update health bar progress
    if (maxHealth > 0 && m_healthBar) {
        f32 healthPercent = health / maxHealth;
        // TODO: Update progress bar value when CProgressBar is implemented
        // For now, just store the values
    }
}

void CHeroUnitFrame::UpdateMana(f32 mana, f32 maxMana) {
    UpdateManaBar(mana, maxMana);
}

void CHeroUnitFrame::UpdateManaBar(f32 mana, f32 maxMana) {
    // Update mana bar progress
    if (maxMana > 0 && m_manaBar) {
        f32 manaPercent = mana / maxMana;
        // TODO: Update progress bar value when CProgressBar is implemented
    }
}

void CHeroUnitFrame::UpdateLevel(i32 level) {
    if (m_levelLabel) {
        // TODO: Update level label text when CLabel text setting is implemented
    }
}

void CHeroUnitFrame::UpdateExperience(f32 experience, f32 experienceToNext) {
    if (experienceToNext > 0 && m_experienceBar) {
        f32 expPercent = experience / experienceToNext;
        // TODO: Update experience bar value when CProgressBar is implemented
    }
}

void CHeroUnitFrame::UpdatePortrait(const std::string& portraitPath) {
    if (m_heroPortrait) {
        // TODO: Update portrait image when CImage path setting is implemented
    }
}

void CHeroUnitFrame::Update(f32 deltaTime) {
    CPanel2D::Update(deltaTime);
    
    // Update health bar animation
    if (m_healthAnimation.active) {
        UpdateAnimation(m_healthAnimation, deltaTime);
        if (m_healthBar) {
            m_healthBar->SetValue(m_healthAnimation.currentValue);
        }
    }
    
    // Update mana bar animation
    if (m_manaAnimation.active) {
        UpdateAnimation(m_manaAnimation, deltaTime);
        if (m_manaBar) {
            m_manaBar->SetValue(m_manaAnimation.currentValue);
        }
    }
}

void CHeroUnitFrame::StartAnimation(BarAnimation& anim, f32 startValue, f32 targetValue) {
    anim.active = true;
    anim.startValue = startValue;
    anim.targetValue = targetValue;
    anim.currentValue = startValue;
    anim.elapsed = 0.0f;
}

void CHeroUnitFrame::UpdateAnimation(BarAnimation& anim, f32 deltaTime) {
    if (!anim.active) return;
    
    anim.elapsed += deltaTime;
    f32 t = anim.elapsed / anim.duration;
    
    if (t >= 1.0f) {
        anim.currentValue = anim.targetValue;
        anim.active = false;
    } else {
        // Smooth interpolation
        t = t * t * (3.0f - 2.0f * t); // Smoothstep
        anim.currentValue = anim.startValue + (anim.targetValue - anim.startValue) * t;
    }
}

void CHeroUnitFrame::AnimateHealthChange(f32 oldValue, f32 newValue) {
    StartAnimation(m_healthAnimation, oldValue, newValue);
}

void CHeroUnitFrame::AnimateManaChange(f32 oldValue, f32 newValue) {
    StartAnimation(m_manaAnimation, oldValue, newValue);
}

} // namespace Panorama