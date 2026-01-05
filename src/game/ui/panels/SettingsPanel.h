#pragma once

#include "../panorama/CPanel2D.h"
#include "../../SettingsManager.h"
#include <memory>
#include <vector>
#include <functional>

namespace Game {

enum class SettingsTab : u8 {
    Video = 0,
    Audio,
    Controls,
    Game,
    Count
};

class SettingsPanel {
public:
    SettingsPanel();
    
    void Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight);
    void Destroy();
    
    void Show();
    void Hide();
    bool IsVisible() const;
    
    void SetOnClose(std::function<void()> callback) { m_onClose = callback; }
    
private:
    void CreateHeader(Panorama::CPanel2D* window, f32 windowWidth);
    void CreateTabs(Panorama::CPanel2D* content);
    void CreateVideoTab(Panorama::CPanel2D* container);
    void CreateAudioTab(Panorama::CPanel2D* container);
    void CreateControlsTab(Panorama::CPanel2D* container);
    void CreateGameTab(Panorama::CPanel2D* container);
    void CreateFooter(Panorama::CPanel2D* content);
    
    void SwitchTab(SettingsTab tab);
    void ApplySettings();
    void ResetDefaults();
    void RefreshUI();
    
    std::shared_ptr<Panorama::CLabel> CreateSettingRow(
        Panorama::CPanel2D* parent, 
        const std::string& label, 
        f32 yOffset
    );
    
    std::shared_ptr<Panorama::CSlider> CreateSlider(
        Panorama::CPanel2D* parent,
        const std::string& id,
        f32 x, f32 y, f32 width,
        f32 min, f32 max, f32 value,
        std::function<void(f32)> onChange
    );
    
    std::shared_ptr<Panorama::CDropDown> CreateDropdown(
        Panorama::CPanel2D* parent,
        const std::string& id,
        f32 x, f32 y, f32 width
    );
    
    std::shared_ptr<Panorama::CButton> CreateToggle(
        Panorama::CPanel2D* parent,
        const std::string& id,
        f32 x, f32 y,
        bool initialValue,
        std::function<void(bool)> onChange
    );
    
    std::shared_ptr<Panorama::CPanel2D> m_overlay;
    std::shared_ptr<Panorama::CPanel2D> m_window;
    
    std::vector<std::shared_ptr<Panorama::CButton>> m_tabButtons;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> m_tabPanels;
    
    SettingsTab m_currentTab = SettingsTab::Video;
    std::function<void()> m_onClose;
    
    // UI element references for refreshing
    std::shared_ptr<Panorama::CDropDown> m_resolutionDropdown;
    std::shared_ptr<Panorama::CDropDown> m_windowModeDropdown;
    std::shared_ptr<Panorama::CButton> m_vsyncToggle;
    std::shared_ptr<Panorama::CSlider> m_masterVolumeSlider;
    std::shared_ptr<Panorama::CSlider> m_musicVolumeSlider;
    std::shared_ptr<Panorama::CSlider> m_sfxVolumeSlider;
    std::shared_ptr<Panorama::CLabel> m_masterVolumeLabel;
    std::shared_ptr<Panorama::CLabel> m_musicVolumeLabel;
    std::shared_ptr<Panorama::CLabel> m_sfxVolumeLabel;
};

} // namespace Game
