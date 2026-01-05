#pragma once

#include "core/Types.h"
#include "world/Components.h"
#include "gameplay/GameplayController.h"
#include <imgui.h>

// Forward declarations
namespace WorldEditor {
    class World;
    class EditorCamera;
}

namespace WorldEditor {

// Game mode for testing gameplay (creep battles, towers, etc.)
// This is a thin UI layer over GameplayController
class GameMode {
public:
    GameMode() = default;
    
    // Set the gameplay controller to delegate logic to
    void setController(GameplayController* controller) { controller_ = controller; }
    GameplayController* getController() const { return controller_; }
    
    // Draw game mode UI panel
    void draw(World& world);
    
    // Update game simulation (called from main loop)
    void update(World& world, f32 deltaTime);
    
    // Draw HP/MP bars above units (called after world rendering, before ImGui::Render)
    void drawUnitHealthBars(World& world, const Mat4& viewProj, const Vec2& viewportSize, const ImVec2& viewportRectMin);
    
    // Draw top bar with game time and hero portraits (Dota-style)
    void drawTopBar(World& world, const Vec2& viewportSize, const ImVec2& viewportRectMin);
    
    // Game state - delegate to controller when available
    bool isPaused() const { return controller_ ? controller_->isPaused() : paused_; }
    f32 getTimeScale() const { return controller_ ? controller_->getTimeScale() : timeScale_; }
    bool isGameModeActive() const { return gameModeActive_; }
    void setGameModeActive(bool active) { gameModeActive_ = active; }
    
    // Statistics - delegate to controller when available
    const GameplayStats& getStats() const { 
        if (controller_) return controller_->getStats();
        return fallbackStats_; 
    }

private:
    GameplayController* controller_ = nullptr;
    
    // Game mode active state (UI-specific)
    bool gameModeActive_ = false;
    
    // Fallback state when no controller is set
    bool paused_ = false;
    f32 timeScale_ = 1.0f;
    GameplayStats fallbackStats_;
    
    // UI state (kept locally - not in controller)
    bool showStatsPanel_ = true;
    bool showCreepInfo_ = true;
    bool showTowerInfo_ = true;
    bool showTowerRange_ = true;
    bool showAbilityIndicators_ = true;
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
