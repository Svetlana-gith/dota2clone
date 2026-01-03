#include "GameState.h"
#include <algorithm>

namespace WorldEditor {

void GameState::update(f32 deltaTime) {
    switch (currentScreen_) {
        case GameScreen::Loading:
            // Simulate loading progress
            loadingTimer_ += deltaTime;
            loadingProgress_ = std::min(1.0f, loadingTimer_ / 2.0f); // 2 second load
            
            if (loadingProgress_ >= 1.0f) {
                setScreen(GameScreen::InGame);
            }
            break;
            
        case GameScreen::InGame:
            // Game logic handled elsewhere
            break;
            
        default:
            break;
    }
}

void GameState::setScreen(GameScreen screen) {
    currentScreen_ = screen;
    
    switch (screen) {
        case GameScreen::MainMenu:
            paused_ = false;
            resetStats();
            break;
            
        case GameScreen::HeroSelect:
            break;
            
        case GameScreen::Loading:
            loadingProgress_ = 0.0f;
            loadingTimer_ = 0.0f;
            break;
            
        case GameScreen::InGame:
            paused_ = false;
            break;
            
        case GameScreen::PostGame:
            paused_ = false;
            break;
    }
}

void GameState::resetStats() {
    kills_ = 0;
    deaths_ = 0;
    goldEarned_ = 0;
    loadingProgress_ = 0.0f;
    loadingTimer_ = 0.0f;
}

} // namespace WorldEditor
