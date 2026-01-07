#pragma once

#include "../panorama/core/CPanel2D.h"
#include <memory>
#include <functional>

namespace Game {

class MainMenuBottomBar {
public:
    MainMenuBottomBar();
    
    void Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight, 
                f32 contentWidth, f32 contentOffsetX);
    void Destroy();
    
    std::shared_ptr<Panorama::CPanel2D> GetBottomBar() { return m_bottomBar; }
    std::shared_ptr<Panorama::CButton> GetPlayButton() { return m_playButton; }
    
    void SetGameMode(const std::string& mode);
    void SetPartyMembers(int count);
    void SetPlayButtonVisible(bool visible);
    void SetPlayButtonText(const std::string& text);
    
    void SetOnPlayClicked(std::function<void()> cb) { m_onPlayClicked = cb; }
    void SetOnGameModeClicked(std::function<void()> cb) { m_onGameModeClicked = cb; }
    void SetOnAddPartyClicked(std::function<void()> cb) { m_onAddPartyClicked = cb; }
    
private:
    std::shared_ptr<Panorama::CPanel2D> m_bottomBarBg;
    std::shared_ptr<Panorama::CPanel2D> m_bottomBar;
    std::shared_ptr<Panorama::CPanel2D> m_gameModeIcon;
    std::shared_ptr<Panorama::CLabel> m_gameModeLabel;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> m_partySlots;
    std::shared_ptr<Panorama::CPanel2D> m_addPartyButton;
    std::shared_ptr<Panorama::CButton> m_playButton;
    
    std::function<void()> m_onPlayClicked;
    std::function<void()> m_onGameModeClicked;
    std::function<void()> m_onAddPartyClicked;
};

} // namespace Game
