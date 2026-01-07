#pragma once

#include "../panorama/core/CPanel2D.h"
#include "../panorama/widgets/CLabel.h"
#include "../panorama/widgets/CButton.h"
#include "network/MatchmakingClient.h"
#include <memory>
#include <functional>

namespace Game {

class ReconnectPanel {
public:
    ReconnectPanel();
    
    void Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight);
    void Destroy();
    
    void Show(const WorldEditor::Matchmaking::ActiveGameInfo& gameInfo);
    void Hide();
    bool IsVisible() const;
    
    const WorldEditor::Matchmaking::ActiveGameInfo& GetActiveGameInfo() const { return m_activeGameInfo; }
    
    void SetOnReconnect(std::function<void()> cb) { m_onReconnect = cb; }
    void SetOnAbandon(std::function<void()> cb) { m_onAbandon = cb; }
    
private:
    std::shared_ptr<Panorama::CPanel2D> m_overlay;
    std::shared_ptr<Panorama::CLabel> m_titleLabel;
    std::shared_ptr<Panorama::CLabel> m_infoLabel;
    std::shared_ptr<Panorama::CButton> m_reconnectButton;
    std::shared_ptr<Panorama::CButton> m_abandonButton;
    
    WorldEditor::Matchmaking::ActiveGameInfo m_activeGameInfo;
    
    std::function<void()> m_onReconnect;
    std::function<void()> m_onAbandon;
    
    f32 m_screenWidth = 0;
    f32 m_screenHeight = 0;
};

} // namespace Game
