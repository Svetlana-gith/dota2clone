#pragma once

#include "../panorama/core/CPanel2D.h"
#include <memory>
#include <vector>

namespace Game {

class MainMenuContent {
public:
    MainMenuContent();
    
    void Create(Panorama::CPanel2D* parent, f32 contentWidth, f32 contentHeight, f32 offsetX, f32 offsetY);
    void Destroy();
    
    // Update methods
    void UpdateLastMatch(const std::string& heroName, const std::string& result, 
                         int kills, int deaths, int assists, const std::string& duration);
    void AddChatMessage(const std::string& username, const std::string& message);
    void SetUsername(const std::string& username);
    void SetLevel(int level);
    void AddFriend(const std::string& name, bool online, const std::string& status = "");
    
private:
    void CreateProfileColumn(Panorama::CPanel2D* main, f32 contentWidth, f32 contentHeight);
    void CreateCenterColumn(Panorama::CPanel2D* main, f32 contentWidth, f32 contentHeight);
    void CreateRightColumn(Panorama::CPanel2D* main, f32 contentWidth, f32 contentHeight);
    void CreateChatPanel(Panorama::CPanel2D* container);
    
    std::shared_ptr<Panorama::CPanel2D> m_mainContainer;
    
    // Profile column (left)
    std::shared_ptr<Panorama::CPanel2D> m_profilePanel;
    std::shared_ptr<Panorama::CPanel2D> m_avatarPanel;
    std::shared_ptr<Panorama::CLabel> m_usernameLabel;
    std::shared_ptr<Panorama::CLabel> m_levelLabel;
    std::shared_ptr<Panorama::CPanel2D> m_friendsPanel;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> m_friendEntries;
    
    // Center column elements
    std::shared_ptr<Panorama::CPanel2D> m_newsBlock1;
    std::shared_ptr<Panorama::CPanel2D> m_newsBlock2;
    std::shared_ptr<Panorama::CPanel2D> m_featuredBlock1;
    std::shared_ptr<Panorama::CPanel2D> m_featuredBlock2;
    std::shared_ptr<Panorama::CPanel2D> m_chatPanel;
    
    // Right column elements  
    std::shared_ptr<Panorama::CPanel2D> m_lastMatchPanel;
    std::shared_ptr<Panorama::CPanel2D> m_activityPanel;
    
    // Dynamic labels for updates
    std::shared_ptr<Panorama::CLabel> m_heroNameLabel;
    std::shared_ptr<Panorama::CLabel> m_matchResultLabel;
    std::shared_ptr<Panorama::CLabel> m_kdaLabel;
    std::shared_ptr<Panorama::CLabel> m_durationLabel;
};

} // namespace Game
