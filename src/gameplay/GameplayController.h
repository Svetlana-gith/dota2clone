#pragma once

/**
 * GameplayController - Unified gameplay logic for both Editor and Game Client
 * 
 * This class provides a shared interface for:
 * - Camera control (WASD, edge panning, zoom)
 * - Unit selection and commands
 * - Hero control (move, attack, abilities)
 * - Game state management (pause, time scale)
 * 
 * The UI layer (ImGui for Editor, Panorama for Game) uses this controller
 * to interact with the game world without duplicating logic.
 */

#include "core/Types.h"
#include "ui/EditorCamera.h"
#include "world/HeroSystem.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace WorldEditor {

class World;
class HeroSystem;

// Camera mode for different gameplay styles
enum class CameraMode : u8 {
    Free = 0,       // Free camera (editor default)
    RTS = 1,        // RTS-style (WASD pan, edge scroll, fixed angle)
    FollowHero = 2  // Follow selected hero
};

// Input state from UI layer
struct GameplayInput {
    // Mouse state
    Vec2 mousePos = Vec2(0.0f);
    Vec2 mouseDelta = Vec2(0.0f);
    bool leftClick = false;
    bool rightClick = false;
    bool leftHeld = false;
    bool rightHeld = false;
    f32 scrollDelta = 0.0f;
    
    // Keyboard state
    bool keys[256] = {};
    bool shiftHeld = false;
    bool ctrlHeld = false;
    bool altHeld = false;
    
    // Screen dimensions (for edge panning)
    f32 screenWidth = 1920.0f;
    f32 screenHeight = 1080.0f;
    
    // Viewport info (for editor with docked windows)
    Vec2 viewportMin = Vec2(0.0f);
    Vec2 viewportMax = Vec2(1920.0f, 1080.0f);
    bool mouseInViewport = true;
};

// Game statistics
struct GameplayStats {
    f32 gameTime = 0.0f;
    i32 radiantCreeps = 0;
    i32 direCreeps = 0;
    i32 radiantTowers = 0;
    i32 direTowers = 0;
    i32 radiantBuildings = 0;
    i32 direBuildings = 0;
    i32 totalCreepsSpawned = 0;
    i32 totalCreepsKilled = 0;
};

// Selected unit info for UI
struct SelectedUnitInfo {
    Entity entity = INVALID_ENTITY;
    String name;
    i32 teamId = 0;
    f32 currentHealth = 0.0f;
    f32 maxHealth = 0.0f;
    f32 currentMana = 0.0f;
    f32 maxMana = 0.0f;
    bool isHero = false;
    bool isCreep = false;
    bool isTower = false;
    bool isBuilding = false;
};

class GameplayController {
public:
    GameplayController();
    ~GameplayController();
    
    // Initialize with a world
    void setWorld(World* world);
    World* getWorld() const { return world_; }
    
    // Window handle for input
    void setWindowHandle(HWND hwnd) { hwnd_ = hwnd; }
    
    // ==================== Game State ====================
    
    void startGame();
    void stopGame();
    void pauseGame();
    void resumeGame();
    void resetGame();
    
    bool isGameActive() const { return gameActive_; }
    void setGameActive(bool active) { gameActive_ = active; }
    bool isPaused() const { return paused_; }
    
    f32 getTimeScale() const { return timeScale_; }
    void setTimeScale(f32 scale) { timeScale_ = std::clamp(scale, 0.1f, 10.0f); }
    
    const GameplayStats& getStats() const { return stats_; }
    
    // ==================== Update ====================
    
    // Main update - call once per frame
    // Returns scaled delta time
    f32 update(f32 deltaTime, const GameplayInput& input);
    
    // Update only camera (for editor when game is not active)
    void updateCameraOnly(f32 deltaTime, const GameplayInput& input);
    
    // ==================== Camera ====================
    
    EditorCamera& getCamera() { return camera_; }
    const EditorCamera& getCamera() const { return camera_; }
    
    CameraMode getCameraMode() const { return cameraMode_; }
    void setCameraMode(CameraMode mode) { cameraMode_ = mode; }
    
    // Edge panning settings
    f32 getEdgePanSpeed() const { return edgePanSpeed_; }
    void setEdgePanSpeed(f32 speed) { edgePanSpeed_ = speed; }
    f32 getEdgePanMargin() const { return edgePanMargin_; }
    void setEdgePanMargin(f32 margin) { edgePanMargin_ = margin; }
    bool isEdgePanEnabled() const { return edgePanEnabled_; }
    void setEdgePanEnabled(bool enabled) { edgePanEnabled_ = enabled; }
    
    // Camera positioning
    void focusOnPosition(const Vec3& position);
    void focusOnEntity(Entity entity);
    
    // Get view-projection matrix
    Mat4 getViewProjectionMatrix(f32 aspectRatio) const;
    
    // ==================== Selection ====================
    
    Entity getSelectedEntity() const { return selectedEntity_; }
    void selectEntity(Entity entity);
    void clearSelection();
    
    const SelectedUnitInfo& getSelectedUnitInfo() const { return selectedUnitInfo_; }
    
    // Get player's controlled hero
    Entity getPlayerHero() const { return playerHero_; }
    void setPlayerHero(Entity hero) { playerHero_ = hero; }
    
    // ==================== Commands ====================
    
    // Issue move command to selected unit (or player hero)
    void commandMoveTo(const Vec3& position);
    
    // Issue attack command
    void commandAttackMove(const Vec3& position);
    void commandAttackTarget(Entity target);
    
    // Issue ability command
    void commandCastAbility(i32 abilityIndex, const Vec3& targetPos, Entity targetEntity);
    
    // Stop command
    void commandStop();
    
    // ==================== World Queries ====================
    
    // Raycast from screen position to world
    bool screenToWorld(const Vec2& screenPos, Vec3& worldPos) const;
    
    // Pick entity at screen position
    Entity pickEntityAt(const Vec2& screenPos) const;
    
    // Find nearest entity of type
    Entity findNearestCreep(const Vec3& position, i32 teamId = -1) const;
    Entity findNearestHero(const Vec3& position, i32 teamId = -1) const;
    Entity findNearestEnemy(const Vec3& position, i32 myTeamId) const;
    
    // ==================== Visual Options ====================
    
    bool showTowerRange = true;
    bool showAbilityIndicators = true;
    bool showHealthBars = true;
    bool showMinimap = true;
    
private:
    World* world_ = nullptr;
    HWND hwnd_ = nullptr;
    
    // Game state
    bool gameActive_ = false;
    bool paused_ = false;
    f32 timeScale_ = 1.0f;
    GameplayStats stats_;
    
    // Camera
    EditorCamera camera_;
    CameraMode cameraMode_ = CameraMode::RTS;
    f32 edgePanSpeed_ = 800.0f;
    f32 edgePanMargin_ = 20.0f;
    bool edgePanEnabled_ = true;
    
    // Selection
    Entity selectedEntity_ = INVALID_ENTITY;
    Entity playerHero_ = INVALID_ENTITY;
    SelectedUnitInfo selectedUnitInfo_;
    
    // Input state
    GameplayInput lastInput_;
    
    // Internal methods
    void updateCamera(f32 deltaTime, const GameplayInput& input);
    void updateEdgePanning(f32 deltaTime, const GameplayInput& input);
    void updateKeyboardCamera(f32 deltaTime, const GameplayInput& input);
    void updateMouseCamera(f32 deltaTime, const GameplayInput& input);
    void updateCameraFollow(f32 deltaTime);
    
    void updateInput(const GameplayInput& input);
    void handleLeftClick(const GameplayInput& input);
    void handleRightClick(const GameplayInput& input);
    void handleAbilityKeys(const GameplayInput& input);
    
    void updateStats();
    void updateSelectedUnitInfo();
    
    // Helper to get HeroSystem
    HeroSystem* getHeroSystem() const;
};

} // namespace WorldEditor
