#pragma once

#include "../panorama/core/CPanel2D.h"
#include <memory>
#include <functional>

namespace Game {

class MainMenuTopBar {
public:
    MainMenuTopBar();
    
    void Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight, 
                f32 contentWidth, f32 contentOffsetX);
    void Destroy();
    
    void SetReturnToGameVisible(bool visible);
    void SetUsername(const std::string& username);
    
    void SetOnSettingsClicked(std::function<void()> cb) { m_onSettings = cb; }
    void SetOnReturnToGameClicked(std::function<void()> cb) { m_onReturnToGame = cb; }
    void SetOnNavClicked(std::function<void(int)> cb) { m_onNavClicked = cb; }
    
    std::shared_ptr<Panorama::CPanel2D> GetTopBar() { return m_topBar; }
    
private:
    std::shared_ptr<Panorama::CPanel2D> m_topBar;
    std::shared_ptr<Panorama::CButton> m_settingsButton;
    std::shared_ptr<Panorama::CButton> m_returnToGameButton;
    std::shared_ptr<Panorama::CLabel> m_usernameLabel;
    std::vector<std::shared_ptr<Panorama::CButton>> m_navButtons;
    
    std::function<void()> m_onSettings;
    std::function<void()> m_onReturnToGame;
    std::function<void(int)> m_onNavClicked;
};

} // namespace Game
