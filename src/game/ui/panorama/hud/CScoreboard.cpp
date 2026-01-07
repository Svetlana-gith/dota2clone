#include "CScoreboard.h"
#include "../widgets/CPanelWidgets.h"
#include "../widgets/CLabel.h"
#include "../../../GameData.h"

namespace Panorama {

CScoreboard::CScoreboard() {
    SetVisible(false); // Hidden by default
    
    // Create player rows (10 players max)
    m_playerRows.resize(10);
    for (i32 i = 0; i < 10; ++i) {
        f32 y = static_cast<f32>(i * 40 + 50); // Header space + row height
        m_playerRows[i] = CPanelWidgets::CreateLabel("", 10, y);
        AddChild(m_playerRows[i]);
    }
}

void CScoreboard::UpdatePlayerScore(i32 playerID, i32 kills, i32 deaths, i32 assists) {
    // TODO: Update specific player score row
    if (playerID >= 0 && playerID < static_cast<i32>(m_playerRows.size())) {
        // TODO: Update player row text when CLabel text setting is implemented
    }
}

void CScoreboard::UpdateAllPlayers(const std::vector<Game::PlayerStats>& playerStats) {
    // Update all player rows
    for (size_t i = 0; i < playerStats.size() && i < m_playerRows.size(); ++i) {
        const auto& player = playerStats[i];
        // TODO: Format and set player stats text when CLabel text setting is implemented
        // Format: "PlayerName (HeroName) - K/D/A: kills/deaths/assists"
    }
}

void CScoreboard::ToggleVisibility() {
    SetVisible(!IsVisible());
}

void CScoreboard::SetVisible(bool visible) {
    CPanel2D::SetVisible(visible);
}

} // namespace Panorama