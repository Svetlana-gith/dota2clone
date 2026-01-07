#pragma once

#include "../core/CPanel2D.h"
#include <memory>
#include <vector>

// Forward declarations
namespace Game {
    struct PlayerStats;
    struct TowerData;
}

namespace Panorama {

class CMinimap : public CPanel2D {
public:
    CMinimap();
    virtual ~CMinimap() = default;

    void UpdatePlayerPosition(i32 playerID, f32 x, f32 y);
    void UpdateCreepPosition(i32 creepID, f32 x, f32 y);
    void UpdateHeroPositions(const std::vector<Game::PlayerStats>& heroes);
    void UpdateTowerStates(const std::vector<Game::TowerData>& towers);
    void UpdateCameraPosition(const Vec3& cameraPos);
    void SetMapBounds(f32 minX, f32 minY, f32 maxX, f32 maxY);

private:
    std::shared_ptr<CPanel2D> m_mapImage;
    f32 m_mapMinX = -1000.0f;
    f32 m_mapMinY = -1000.0f;
    f32 m_mapMaxX = 1000.0f;
    f32 m_mapMaxY = 1000.0f;
};

} // namespace Panorama