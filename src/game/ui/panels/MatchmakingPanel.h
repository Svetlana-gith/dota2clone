#pragma once

#include "../panorama/CPanel2D.h"
#include "network/MatchmakingClient.h"
#include <memory>
#include <functional>

namespace Game {

class MatchmakingPanel {
public:
    MatchmakingPanel();
    
    void Create(Panorama::CPanel2D* parent, Panorama::CPanel2D* bottomBar, 
                f32 screenWidth, f32 screenHeight, f32 contentWidth, f32 playButtonX);
    void Destroy();
    
    void Update(f32 dt);
    
    // UI visibility
    void ShowFindingUI();
    void HideFindingUI();
    void ShowAcceptOverlay(const WorldEditor::Matchmaking::LobbyInfo& lobby);
    void HideAcceptOverlay();
    
    bool IsSearching() const;
    
    // Accept UI updates
    void UpdateAcceptCountdown(f32 remainingSeconds);
    void UpdateAcceptStatus(u16 requiredPlayers, const std::vector<u64>& playerIds, 
                           const std::vector<bool>& accepted, u64 selfId);
    void OnLocalPlayerAccepted(u64 selfId, const std::vector<u64>& playerIds);
    
    // Callbacks
    void SetOnCancelClicked(std::function<void()> cb) { m_onCancelClicked = cb; }
    void SetOnAcceptClicked(std::function<void()> cb) { m_onAcceptClicked = cb; }
    void SetOnDeclineClicked(std::function<void()> cb) { m_onDeclineClicked = cb; }
    
private:
    // Finding match panel
    std::shared_ptr<Panorama::CPanel2D> m_findingPanel;
    std::shared_ptr<Panorama::CLabel> m_findingLabel;
    std::shared_ptr<Panorama::CLabel> m_findingTimeLabel;
    std::shared_ptr<Panorama::CButton> m_findingCancelButton;
    
    // Searching overlay
    std::shared_ptr<Panorama::CPanel2D> m_searchingOverlay;
    std::shared_ptr<Panorama::CLabel> m_searchingLabel;
    std::shared_ptr<Panorama::CLabel> m_searchTimeLabel;
    std::shared_ptr<Panorama::CButton> m_cancelSearchButton;
    
    // Accept overlay
    std::shared_ptr<Panorama::CPanel2D> m_acceptOverlay;
    std::shared_ptr<Panorama::CLabel> m_acceptLabel;
    std::shared_ptr<Panorama::CLabel> m_acceptCountdownLabel;
    std::shared_ptr<Panorama::CButton> m_acceptButton;
    std::shared_ptr<Panorama::CButton> m_declineButton;
    std::shared_ptr<Panorama::CLabel> m_acceptStatusLabel;
    std::shared_ptr<Panorama::CPanel2D> m_acceptStatusPanel;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> m_acceptCubes;
    
    std::function<void()> m_onCancelClicked;
    std::function<void()> m_onAcceptClicked;
    std::function<void()> m_onDeclineClicked;
    
    f32 m_screenWidth = 0;
    f32 m_screenHeight = 0;
};

} // namespace Game
