#pragma once

#include "../core/Types.h"
#include "GameState.h"
#include <imgui.h>

namespace WorldEditor {

class World;

// Panorama-style Game UI
class GameUI {
public:
    GameUI();
    
    // Main screens
    void drawMainMenu(GameState& state);
    void drawHeroSelect(GameState& state);
    void drawLoadingScreen(GameState& state, World& world);
    void drawGameHUD(World& world, GameState& state);
    void drawPauseMenu(GameState& state);
    void drawPostGameScreen(GameState& state);
    
    // Toggle pause menu (ESC key)
    void togglePauseMenu();
    
private:
    // UI styling
    void applyPanoramaStyle();
    void drawBackground();
    void drawLogo();
    
    // Helper widgets
    bool drawMenuButton(const char* label, const ImVec2& size);
    bool drawHeroCard(const char* name, const char* description, HeroType type, bool selected);
    void drawProgressBar(f32 progress, const ImVec2& size);
    void drawStatBox(const char* label, const char* value);
    
    // Animation state
    f32 menuFadeIn_ = 0.0f;
    f32 buttonHoverAnim_[10] = {};
    i32 hoveredButton_ = -1;
    
    // Pause menu state
    bool showPauseMenu_ = false;
};

} // namespace WorldEditor
