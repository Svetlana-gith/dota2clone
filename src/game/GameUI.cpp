#include "GameUI.h"
#include "../world/World.h"
#include "../world/HeroSystem.h"
#include <cstdio>

namespace WorldEditor {

GameUI::GameUI() {
    applyPanoramaStyle();
}

void GameUI::drawMainMenu(GameState& state) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    
    // Full screen background
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("MainMenu", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    drawBackground();
    drawLogo();
    
    // Menu buttons
    ImVec2 buttonSize(300, 60);
    f32 startY = center.y;
    
    ImGui::SetCursorPos(ImVec2(center.x - buttonSize.x * 0.5f, startY));
    if (drawMenuButton("PLAY", buttonSize)) {
        state.setScreen(GameScreen::HeroSelect);
    }
    
    ImGui::SetCursorPos(ImVec2(center.x - buttonSize.x * 0.5f, startY + 80));
    if (drawMenuButton("SETTINGS", buttonSize)) {
        // Settings handled by Panorama UI in MainMenuState
    }
    
    ImGui::SetCursorPos(ImVec2(center.x - buttonSize.x * 0.5f, startY + 160));
    if (drawMenuButton("QUIT", buttonSize)) {
        state.requestQuit();
    }
    
    ImGui::End();
}

void GameUI::drawHeroSelect(GameState& state) {
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("HeroSelect", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    
    drawBackground();
    
    // Title
    ImGui::SetCursorPos(ImVec2(io.DisplaySize.x * 0.5f - 100, 50));
    ImGui::PushFont(nullptr);
    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.13f, 1.0f), "SELECT YOUR HERO");
    ImGui::PopFont();
    
    // Hero cards
    f32 cardWidth = 250;
    f32 cardHeight = 350;
    f32 spacing = 30;
    f32 totalWidth = cardWidth * 3 + spacing * 2;
    f32 startX = (io.DisplaySize.x - totalWidth) * 0.5f;
    f32 startY = 150;
    
    HeroType selectedHero = state.getSelectedHero();
    
    ImGui::SetCursorPos(ImVec2(startX, startY));
    if (drawHeroCard("WARRIOR", "Melee fighter with high armor\nand powerful strikes", 
                     HeroType::Warrior, selectedHero == HeroType::Warrior)) {
        state.selectHero(HeroType::Warrior);
    }
    
    ImGui::SetCursorPos(ImVec2(startX + cardWidth + spacing, startY));
    if (drawHeroCard("MAGE", "Ranged spellcaster with\ndevastating magic abilities",
                     HeroType::Mage, selectedHero == HeroType::Mage)) {
        state.selectHero(HeroType::Mage);
    }
    
    ImGui::SetCursorPos(ImVec2(startX + (cardWidth + spacing) * 2, startY));
    if (drawHeroCard("RANGER", "Swift archer with high\nmobility and precision",
                     HeroType::Ranger, selectedHero == HeroType::Ranger)) {
        state.selectHero(HeroType::Ranger);
    }
    
    // Start button
    ImGui::SetCursorPos(ImVec2(io.DisplaySize.x * 0.5f - 150, io.DisplaySize.y - 120));
    if (drawMenuButton("START GAME", ImVec2(300, 60))) {
        state.setScreen(GameScreen::Loading);
    }
    
    // Back button
    ImGui::SetCursorPos(ImVec2(30, io.DisplaySize.y - 70));
    if (drawMenuButton("BACK", ImVec2(120, 40))) {
        state.setScreen(GameScreen::MainMenu);
    }
    
    ImGui::End();
}

void GameUI::drawLoadingScreen(GameState& state, World& world) {
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Loading", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    
    drawBackground();
    
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    
    // Loading text
    ImGui::SetCursorPos(ImVec2(center.x - 80, center.y - 50));
    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.13f, 1.0f), "LOADING...");
    
    // Progress bar
    ImGui::SetCursorPos(ImVec2(center.x - 200, center.y));
    drawProgressBar(state.getLoadingProgress(), ImVec2(400, 20));
    
    // Tip text
    ImGui::SetCursorPos(ImVec2(center.x - 150, center.y + 50));
    ImGui::TextDisabled("Tip: Use abilities wisely to defeat enemies");
    
    ImGui::End();
}

void GameUI::drawGameHUD(World& world, GameState& state) {
    ImGuiIO& io = ImGui::GetIO();
    
    // Get hero data through system
    System* heroSys = world.getSystem("HeroSystem");
    if (!heroSys) {
        // Draw minimal HUD without hero data
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowSize(ImVec2(200, 50));
        ImGui::Begin("GameInfo", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoBackground);
        ImGui::Text("Game Active");
        ImGui::End();
        return;
    }
    
    // Top-left: Health/Mana bars (placeholder values for now)
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(250, 100));
    ImGui::Begin("HeroStats", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground);
    
    // Health bar
    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "HP");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::ProgressBar(0.75f, ImVec2(180, 18), "");
    ImGui::PopStyleColor();
    
    // Mana bar
    ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f), "MP");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.4f, 0.9f, 1.0f));
    ImGui::ProgressBar(0.6f, ImVec2(180, 18), "");
    ImGui::PopStyleColor();
    
    // Level & Gold
    ImGui::Text("Lv 1");
    ImGui::SameLine(100);
    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.13f, 1.0f), "0 gold");
    
    ImGui::End();
    
    // Bottom center: Ability bar
    f32 abilityBarWidth = 280;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - abilityBarWidth * 0.5f, 
                                   io.DisplaySize.y - 90));
    ImGui::SetNextWindowSize(ImVec2(abilityBarWidth, 80));
    ImGui::Begin("AbilityBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    
    const char* hotkeys[] = { "Q", "W", "E", "R" };
    for (int i = 0; i < 4; i++) {
        if (i > 0) ImGui::SameLine();
        
        ImGui::BeginGroup();
        
        char abilityLabel[32];
        snprintf(abilityLabel, sizeof(abilityLabel), "##ability%d", i);
        ImGui::Button(abilityLabel, ImVec2(50, 50));
        
        // Hotkey label
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18);
        ImGui::TextDisabled("%s", hotkeys[i]);
        
        ImGui::EndGroup();
    }
    
    ImGui::End();
    
    // Bottom right: Minimap
    f32 minimapSize = 180;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - minimapSize - 10, 
                                   io.DisplaySize.y - minimapSize - 10));
    ImGui::SetNextWindowSize(ImVec2(minimapSize, minimapSize));
    ImGui::Begin("Minimap", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 mapPos = ImGui::GetWindowPos();
    
    // Map background
    drawList->AddRectFilled(mapPos, 
        ImVec2(mapPos.x + minimapSize, mapPos.y + minimapSize),
        IM_COL32(20, 30, 20, 200));
    
    // Hero position (center for now)
    f32 heroX = mapPos.x + minimapSize * 0.5f;
    f32 heroY = mapPos.y + minimapSize * 0.5f;
    drawList->AddCircleFilled(ImVec2(heroX, heroY), 5, IM_COL32(50, 200, 50, 255));
    
    ImGui::End();
}

void GameUI::drawPauseMenu(GameState& state) {
    if (!showPauseMenu_) return;
    
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    
    // Dim background
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("PauseBG", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 150));
    ImGui::End();
    
    // Pause menu panel
    ImVec2 menuSize(350, 300);
    ImGui::SetNextWindowPos(ImVec2(center.x - menuSize.x * 0.5f, center.y - menuSize.y * 0.5f));
    ImGui::SetNextWindowSize(menuSize);
    ImGui::Begin("PauseMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    
    ImGui::SetCursorPos(ImVec2(menuSize.x * 0.5f - 50, 20));
    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.13f, 1.0f), "PAUSED");
    
    ImVec2 buttonSize(250, 50);
    f32 buttonX = (menuSize.x - buttonSize.x) * 0.5f;
    
    ImGui::SetCursorPos(ImVec2(buttonX, 70));
    if (drawMenuButton("RESUME", buttonSize)) {
        showPauseMenu_ = false;
    }
    
    ImGui::SetCursorPos(ImVec2(buttonX, 130));
    if (drawMenuButton("SETTINGS", buttonSize)) {
        // Settings handled by Panorama UI
    }
    
    ImGui::SetCursorPos(ImVec2(buttonX, 190));
    if (drawMenuButton("QUIT TO MENU", buttonSize)) {
        showPauseMenu_ = false;
        state.setScreen(GameScreen::MainMenu);
    }
    
    ImGui::End();
}

void GameUI::drawPostGameScreen(GameState& state) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("PostGame", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    
    drawBackground();
    
    // Victory/Defeat text
    bool victory = state.isVictory();
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y - 100));
    if (victory) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "VICTORY!");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "DEFEAT");
    }
    
    // Stats
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y - 30));
    ImGui::Text("Kills: %d", state.getKills());
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y));
    ImGui::Text("Deaths: %d", state.getDeaths());
    ImGui::SetCursorPos(ImVec2(center.x - 100, center.y + 30));
    ImGui::Text("Gold Earned: %d", state.getGoldEarned());
    
    // Buttons
    ImGui::SetCursorPos(ImVec2(center.x - 150, center.y + 100));
    if (drawMenuButton("PLAY AGAIN", ImVec2(300, 50))) {
        state.setScreen(GameScreen::HeroSelect);
    }
    
    ImGui::SetCursorPos(ImVec2(center.x - 150, center.y + 160));
    if (drawMenuButton("MAIN MENU", ImVec2(300, 50))) {
        state.setScreen(GameScreen::MainMenu);
    }
    
    ImGui::End();
}

void GameUI::drawBackground() {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // Gradient background
    drawList->AddRectFilledMultiColor(
        ImVec2(0, 0), io.DisplaySize,
        IM_COL32(15, 15, 25, 255),
        IM_COL32(15, 15, 25, 255),
        IM_COL32(25, 20, 35, 255),
        IM_COL32(25, 20, 35, 255)
    );
}

void GameUI::drawLogo() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, 150);
    
    // Simple text logo for now
    ImGui::SetCursorPos(ImVec2(center.x - 120, 80));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.65f, 0.13f, 1.0f));
    ImGui::Text("WORLD EDITOR");
    ImGui::PopStyleColor();
    
    ImGui::SetCursorPos(ImVec2(center.x - 60, 110));
    ImGui::TextDisabled("GAME MODE");
}

bool GameUI::drawHeroCard(const char* name, const char* description, HeroType type, bool selected) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(250, 350);
    
    // Card background
    ImU32 bgColor = selected ? IM_COL32(60, 50, 30, 255) : IM_COL32(25, 25, 30, 255);
    ImU32 borderColor = selected ? IM_COL32(200, 160, 60, 255) : IM_COL32(60, 60, 70, 255);
    
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor, 8.0f);
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), borderColor, 8.0f, 0, 2.0f);
    
    // Hero icon placeholder
    ImVec2 iconPos(pos.x + 50, pos.y + 30);
    drawList->AddRectFilled(iconPos, ImVec2(iconPos.x + 150, iconPos.y + 150),
        IM_COL32(40, 40, 50, 255), 4.0f);
    
    // Hero name
    ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x * 0.5f - 40, pos.y + 200));
    ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.13f, 1.0f), "%s", name);
    
    // Description
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 20, pos.y + 230));
    ImGui::PushTextWrapPos(pos.x + size.x - 20);
    ImGui::TextDisabled("%s", description);
    ImGui::PopTextWrapPos();
    
    // Invisible button for click detection
    ImGui::SetCursorScreenPos(pos);
    char btnId[32];
    snprintf(btnId, sizeof(btnId), "##hero_%d", (int)type);
    return ImGui::InvisibleButton(btnId, size);
}

void GameUI::applyPanoramaStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Dark Panorama-like theme
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.90f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);
    
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    
    // Gold accent for buttons
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.60f, 0.50f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.55f, 0.15f, 1.00f);
    
    colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.50f, 0.40f, 0.15f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.50f, 0.20f, 1.00f);
    
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // Rounded corners
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(20, 20);
    style.FramePadding = ImVec2(12, 8);
    style.ItemSpacing = ImVec2(12, 8);
}

void GameUI::togglePauseMenu() {
    showPauseMenu_ = !showPauseMenu_;
}


bool GameUI::drawMenuButton(const char* label, const ImVec2& size) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 15));
    
    bool clicked = ImGui::Button(label, size);
    
    ImGui::PopStyleVar(2);
    return clicked;
}

void GameUI::drawProgressBar(f32 progress, const ImVec2& size) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    // Background
    drawList->AddRectFilled(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(30, 30, 35, 255),
        4.0f
    );
    
    // Progress fill (gold gradient)
    f32 fillWidth = size.x * progress;
    if (fillWidth > 0) {
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + fillWidth, pos.y + size.y),
            IM_COL32(200, 160, 60, 255),
            4.0f
        );
    }
    
    // Border
    drawList->AddRect(
        pos,
        ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(80, 80, 90, 255),
        4.0f, 0, 2.0f
    );
    
    ImGui::Dummy(size);
}

void GameUI::drawStatBox(const char* label, const char* value) {
    ImGui::BeginGroup();
    ImGui::TextDisabled("%s", label);
    ImGui::Text("%s", value);
    ImGui::EndGroup();
}

} // namespace WorldEditor
