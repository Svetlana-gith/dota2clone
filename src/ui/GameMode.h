#pragma once

#include "core/Types.h"
#include "world/Components.h"
#include <imgui.h>

// Forward declarations
namespace WorldEditor {
    class World;
    class EditorCamera;
}

namespace WorldEditor {

// Game mode for testing gameplay (creep battles, towers, etc.)
class GameMode {
public:
    GameMode() = default;
    
    // Draw game mode UI panel
    void draw(World& world);
    
    // Update game simulation (called from main loop)
    void update(World& world, f32 deltaTime);
    
    // Draw HP/MP bars above units (called after world rendering, before ImGui::Render)
    void drawUnitHealthBars(World& world, const Mat4& viewProj, const Vec2& viewportSize, const ImVec2& viewportRectMin);
    
    // Game state
    bool isPaused() const { return paused_; }
    f32 getTimeScale() const { return timeScale_; }
    bool isGameModeActive() const { return gameModeActive_; }
    void setGameModeActive(bool active) { gameModeActive_ = active; }
    
    // Statistics
    struct GameStats {
        i32 radiantCreeps = 0;
        i32 direCreeps = 0;
        i32 radiantTowers = 0;
        i32 direTowers = 0;
        i32 radiantBuildings = 0;
        i32 direBuildings = 0;
        f32 gameTime = 0.0f;
        i32 totalCreepsSpawned = 0;
        i32 totalCreepsKilled = 0;
    };
    
    const GameStats& getStats() const { return stats_; }

private:
    bool gameModeActive_ = false;
    bool paused_ = false;
    f32 timeScale_ = 1.0f; // 1.0 = normal, 2.0 = 2x speed, 0.5 = 0.5x speed
    
    GameStats stats_;
    
    // UI state
    bool showStatsPanel_ = true;
    bool showCreepInfo_ = true;
    bool showTowerInfo_ = true;
    bool followCreep_ = false;
    Entity followedCreep_ = INVALID_ENTITY;
    
    // Helper methods
    void stopAndReset(World& world);
    void updateStats(World& world);
    void drawStatsPanel(World& world);
    void drawCreepInfo(World& world);
    void drawTowerInfo(World& world);
    void drawTimeControls();
    Entity findNearestCreep(World& world, const Vec3& position);
    void updateCameraFollow(World& world, EditorCamera& camera);
};

} // namespace WorldEditor
