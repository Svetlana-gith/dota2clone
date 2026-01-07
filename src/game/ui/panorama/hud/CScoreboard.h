#pragma once

#include "../core/CPanel2D.h"
#include <memory>
#include <vector>

// Forward declarations
namespace Game {
    struct PlayerStats;
}

namespace Panorama {

class CScoreboard : public CPanel2D {
public:
    CScoreboard();
    virtual ~CScoreboard() = default;

    void UpdatePlayerScore(i32 playerID, i32 kills, i32 deaths, i32 assists);
    void UpdateAllPlayers(const std::vector<Game::PlayerStats>& playerStats);
    void ToggleVisibility();
    void SetVisible(bool visible);

private:
    // Scoreboard UI elements would be created here
    std::vector<std::shared_ptr<CPanel2D>> m_playerRows;
};

} // namespace Panorama