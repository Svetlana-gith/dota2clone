#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/CUIEngine.h"
#include "../ui/panorama/CPanel2D.h"
#include "../ui/panorama/CStyleSheet.h"
#include "../ui/panorama/GameEvents.h"
#include "network/MatchmakingClient.h"
#include "auth/AuthClient.h"
#include <cstdio>

namespace Game {

struct MainMenuState::MenuUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    std::shared_ptr<Panorama::CPanel2D> bottomBar;
    std::shared_ptr<Panorama::CButton> playButton;
    std::shared_ptr<Panorama::CLabel> serverIPLabel;
    std::shared_ptr<Panorama::CLabel> usernameLabel;  // Profile username
    std::string serverIP = "127.0.0.1";  // Default to localhost
    
    // Top-left buttons (Dota 2 style)
    std::shared_ptr<Panorama::CButton> returnToGameButton;  // Shows when game in progress
    std::shared_ptr<Panorama::CButton> settingsButton;      // Settings gear icon
    std::shared_ptr<Panorama::CPanel2D> settingsPanel;      // Settings overlay
    
    // Matchmaking UI
    std::shared_ptr<Panorama::CPanel2D> searchingOverlay;
    std::shared_ptr<Panorama::CLabel> searchingLabel;
    std::shared_ptr<Panorama::CLabel> searchTimeLabel;
    std::shared_ptr<Panorama::CButton> cancelSearchButton;

    // Dota-like "Finding Match" button swap (bottom bar)
    std::shared_ptr<Panorama::CPanel2D> findingPanel;
    std::shared_ptr<Panorama::CLabel> findingTimeLabel;
    std::shared_ptr<Panorama::CLabel> findingLabel;
    std::shared_ptr<Panorama::CButton> findingCancelButton;

    std::shared_ptr<Panorama::CPanel2D> acceptOverlay;
    std::shared_ptr<Panorama::CLabel> acceptLabel;
    std::shared_ptr<Panorama::CLabel> acceptCountdownLabel;
    std::shared_ptr<Panorama::CButton> acceptButton;
    std::shared_ptr<Panorama::CButton> declineButton;
    std::shared_ptr<Panorama::CLabel> acceptStatusLabel;
    std::shared_ptr<Panorama::CPanel2D> acceptStatusPanel;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> acceptCubes;
    
    // Reconnect UI (Dota 2 style)
    std::shared_ptr<Panorama::CPanel2D> reconnectOverlay;
    std::shared_ptr<Panorama::CLabel> reconnectLabel;
    std::shared_ptr<Panorama::CLabel> reconnectInfoLabel;
    std::shared_ptr<Panorama::CButton> reconnectButton;
    std::shared_ptr<Panorama::CButton> abandonButton;
};

MainMenuState::MainMenuState() : m_ui(std::make_unique<MenuUI>()) {}
MainMenuState::~MainMenuState() = default;

void MainMenuState::OnEnter() { 
    LOG_INFO("MainMenuState::OnEnter()");
    // Load layered menu stylesheet from disk (no recompilation needed for tweaks).
    // main_menu.css should @import base.css to keep shared defaults.
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/main_menu.css");
    CreateUI(); 
    LOG_INFO("MainMenuState UI created");
    ConsoleLog("Main Menu loaded");
    
    // Check for active game to reconnect
    CheckForActiveGame();
}
void MainMenuState::OnExit() {
    // Restore base stylesheet for other screens unless they load their own.
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/base.css");
    DestroyUI();
}

// Scaled helpers
static float S(float v) { return v * 1.35f; } // Scale factor (35% increase) for layout

static std::shared_ptr<Panorama::CPanel2D> P(const std::string& id, float w, float h, Panorama::Color bg) {
    auto p = std::make_shared<Panorama::CPanel2D>(id);
    if (w > 0) p->GetStyle().width = Panorama::Length::Px(S(w));
    else p->GetStyle().width = Panorama::Length::Fill();
    if (h > 0) p->GetStyle().height = Panorama::Length::Px(S(h));
    else p->GetStyle().height = Panorama::Length::Fill();
    p->GetStyle().backgroundColor = bg;
    return p;
}

// Label helper using CSS classes from base.css
static std::shared_ptr<Panorama::CLabel> L(const std::string& text, const std::string& cssClass, Panorama::Color col) {
    auto l = std::make_shared<Panorama::CLabel>(text, text);
    l->AddClass(cssClass);  // Use CSS class for font size
    l->GetStyle().color = col;
    return l;
}

void MainMenuState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    float sw = engine.GetScreenWidth();
    float sh = engine.GetScreenHeight();
    
    // Colors - Dark blue-black gradient background like Dota 2
    Panorama::Color bg(0.02f, 0.04f, 0.08f, 1.0f);  // Very dark blue-black
    Panorama::Color panel(0.08f, 0.09f, 0.11f, 0.92f);  // Slightly transparent dark panels
    Panorama::Color header(0.01f, 0.02f, 0.04f, 0.95f);  // Almost black with blue tint
    Panorama::Color gray(0.45f, 0.45f, 0.45f, 1.0f);
    Panorama::Color light(0.65f, 0.65f, 0.65f, 1.0f);
    Panorama::Color green(0.35f, 0.65f, 0.35f, 1.0f);
    Panorama::Color greenBtn(0.18f, 0.45f, 0.18f, 1.0f);
    Panorama::Color none(0,0,0,0);

    // Calculate content dimensions (80% of screen width, centered)
    float contentWidth = sw * 0.8f;
    float contentHeight = (sh - 70) * 0.9f;
    float contentOffsetX = (sw - contentWidth) / 2.0f;
    float contentOffsetY = ((sh - 70) - contentHeight) / 2.0f;

    // ROOT
    m_ui->root = P("Root", 0, 0, bg);
    uiRoot->AddChild(m_ui->root);

    // ===== TOP BAR =====
    // Full width background
    auto top = P("Top", 0, 55, header);
    top->GetStyle().marginTop = Panorama::Length::Px(0);
    m_ui->root->AddChild(top);
    
    // === TOP-LEFT BUTTONS (Settings, Return to Game) ===
    // Settings button (gear icon) - always visible
    m_ui->settingsButton = std::make_shared<Panorama::CButton>("⚙", "SettingsBtn");
    m_ui->settingsButton->GetStyle().width = Panorama::Length::Px(40);
    m_ui->settingsButton->GetStyle().height = Panorama::Length::Px(40);
    m_ui->settingsButton->GetStyle().marginLeft = Panorama::Length::Px(10);
    m_ui->settingsButton->GetStyle().marginTop = Panorama::Length::Px(7);
    m_ui->settingsButton->GetStyle().backgroundColor = Panorama::Color(0.12f, 0.12f, 0.15f, 0.9f);
    m_ui->settingsButton->GetStyle().borderRadius = 4.0f;
    m_ui->settingsButton->GetStyle().fontSize = 20.0f;
    m_ui->settingsButton->SetOnActivate([this]() { OnSettingsClicked(); });
    top->AddChild(m_ui->settingsButton);
    
    // Return to Game button - only visible when game in progress
    m_ui->returnToGameButton = std::make_shared<Panorama::CButton>("←", "ReturnBtn");
    m_ui->returnToGameButton->GetStyle().width = Panorama::Length::Px(160);
    m_ui->returnToGameButton->GetStyle().height = Panorama::Length::Px(40);
    m_ui->returnToGameButton->GetStyle().marginLeft = Panorama::Length::Px(60);
    m_ui->returnToGameButton->GetStyle().marginTop = Panorama::Length::Px(7);
    m_ui->returnToGameButton->GetStyle().backgroundColor = Panorama::Color(0.18f, 0.45f, 0.18f, 1.0f);
    m_ui->returnToGameButton->GetStyle().borderRadius = 4.0f;
    m_ui->returnToGameButton->GetStyle().fontSize = 14.0f;
    m_ui->returnToGameButton->GetStyle().color = Panorama::Color::White();
    m_ui->returnToGameButton->SetOnActivate([this]() {
        // Return to the game (pop this state)
        if (m_manager) {
            m_manager->PopState();
        }
    });
    m_ui->returnToGameButton->SetVisible(m_manager && m_manager->IsGameInProgress());
    top->AddChild(m_ui->returnToGameButton);
    
    // ===== SETTINGS POPUP OVERLAY =====
    m_ui->settingsPanel = std::make_shared<Panorama::CPanel2D>("SettingsOverlay");
    m_ui->settingsPanel->GetStyle().width = Panorama::Length::Fill();
    m_ui->settingsPanel->GetStyle().height = Panorama::Length::Fill();
    m_ui->settingsPanel->GetStyle().backgroundColor = Panorama::Color(0.0f, 0.0f, 0.0f, 0.75f);
    m_ui->settingsPanel->SetVisible(false);
    m_ui->root->AddChild(m_ui->settingsPanel);
    
    // Settings popup window (centered)
    auto settingsWindow = std::make_shared<Panorama::CPanel2D>("SettingsWindow");
    settingsWindow->GetStyle().width = Panorama::Length::Px(600);
    settingsWindow->GetStyle().height = Panorama::Length::Px(450);
    settingsWindow->GetStyle().backgroundColor = Panorama::Color(0.08f, 0.09f, 0.12f, 0.98f);
    settingsWindow->GetStyle().borderRadius = 8.0f;
    settingsWindow->GetStyle().marginLeft = Panorama::Length::Px((sw - 600) / 2);
    settingsWindow->GetStyle().marginTop = Panorama::Length::Px((sh - 450) / 2);
    m_ui->settingsPanel->AddChild(settingsWindow);
    
    // Header bar
    auto settingsHeader = std::make_shared<Panorama::CPanel2D>("SettingsHeader");
    settingsHeader->GetStyle().width = Panorama::Length::Fill();
    settingsHeader->GetStyle().height = Panorama::Length::Px(50);
    settingsHeader->GetStyle().backgroundColor = Panorama::Color(0.05f, 0.06f, 0.08f, 1.0f);
    settingsWindow->AddChild(settingsHeader);
    
    auto settingsTitle = std::make_shared<Panorama::CLabel>("SETTINGS", "SettingsTitle");
    settingsTitle->GetStyle().fontSize = 22.0f;
    settingsTitle->GetStyle().color = Panorama::Color(0.9f, 0.9f, 0.9f, 1.0f);
    settingsTitle->GetStyle().marginLeft = Panorama::Length::Px(20);
    settingsTitle->GetStyle().marginTop = Panorama::Length::Px(12);
    settingsHeader->AddChild(settingsTitle);
    
    // Close button (X)
    auto closeBtn = std::make_shared<Panorama::CButton>("✕", "SettingsClose");
    closeBtn->GetStyle().width = Panorama::Length::Px(40);
    closeBtn->GetStyle().height = Panorama::Length::Px(40);
    closeBtn->GetStyle().backgroundColor = Panorama::Color(0.5f, 0.15f, 0.15f, 0.9f);
    closeBtn->GetStyle().borderRadius = 4.0f;
    closeBtn->GetStyle().fontSize = 18.0f;
    closeBtn->GetStyle().color = Panorama::Color::White();
    closeBtn->GetStyle().marginLeft = Panorama::Length::Px(550);
    closeBtn->GetStyle().marginTop = Panorama::Length::Px(5);
    closeBtn->SetOnActivate([this]() {
        if (m_ui && m_ui->settingsPanel) m_ui->settingsPanel->SetVisible(false);
    });
    settingsHeader->AddChild(closeBtn);
    
    // Settings content area
    auto settingsContent = std::make_shared<Panorama::CPanel2D>("SettingsContent");
    settingsContent->GetStyle().width = Panorama::Length::Fill();
    settingsContent->GetStyle().height = Panorama::Length::Px(400);
    settingsContent->GetStyle().marginTop = Panorama::Length::Px(50);
    settingsContent->GetStyle().paddingLeft = Panorama::Length::Px(20);
    settingsContent->GetStyle().paddingTop = Panorama::Length::Px(20);
    settingsWindow->AddChild(settingsContent);
    
    // Tab buttons row
    const char* tabs[] = {"VIDEO", "AUDIO", "CONTROLS", "GAME"};
    for (int i = 0; i < 4; i++) {
        auto tabBtn = std::make_shared<Panorama::CButton>(tabs[i], std::string("SettingsTab") + std::to_string(i));
        tabBtn->GetStyle().width = Panorama::Length::Px(120);
        tabBtn->GetStyle().height = Panorama::Length::Px(36);
        tabBtn->GetStyle().backgroundColor = (i == 0) 
            ? Panorama::Color(0.2f, 0.22f, 0.28f, 1.0f)  // Active tab
            : Panorama::Color(0.12f, 0.13f, 0.16f, 1.0f);
        tabBtn->GetStyle().borderRadius = 4.0f;
        tabBtn->GetStyle().fontSize = 14.0f;
        tabBtn->GetStyle().color = Panorama::Color(0.8f, 0.8f, 0.8f, 1.0f);
        tabBtn->GetStyle().marginLeft = Panorama::Length::Px(i * 130);
        tabBtn->GetStyle().marginTop = Panorama::Length::Px(0);
        settingsContent->AddChild(tabBtn);
    }
    
    // Video settings (placeholder content)
    auto videoSection = std::make_shared<Panorama::CPanel2D>("VideoSettings");
    videoSection->GetStyle().width = Panorama::Length::Px(540);
    videoSection->GetStyle().height = Panorama::Length::Px(280);
    videoSection->GetStyle().backgroundColor = Panorama::Color(0.06f, 0.07f, 0.09f, 0.8f);
    videoSection->GetStyle().borderRadius = 4.0f;
    videoSection->GetStyle().marginTop = Panorama::Length::Px(50);
    settingsContent->AddChild(videoSection);
    
    // Resolution label
    auto resLabel = std::make_shared<Panorama::CLabel>("Resolution", "ResLabel");
    resLabel->GetStyle().fontSize = 14.0f;
    resLabel->GetStyle().color = Panorama::Color(0.7f, 0.7f, 0.7f, 1.0f);
    resLabel->GetStyle().marginLeft = Panorama::Length::Px(20);
    resLabel->GetStyle().marginTop = Panorama::Length::Px(20);
    videoSection->AddChild(resLabel);
    
    auto resValue = std::make_shared<Panorama::CLabel>("1920 x 1080", "ResValue");
    resValue->GetStyle().fontSize = 14.0f;
    resValue->GetStyle().color = Panorama::Color(0.9f, 0.9f, 0.9f, 1.0f);
    resValue->GetStyle().marginLeft = Panorama::Length::Px(200);
    resValue->GetStyle().marginTop = Panorama::Length::Px(20);
    videoSection->AddChild(resValue);
    
    // Display Mode
    auto modeLabel = std::make_shared<Panorama::CLabel>("Display Mode", "ModeLabel");
    modeLabel->GetStyle().fontSize = 14.0f;
    modeLabel->GetStyle().color = Panorama::Color(0.7f, 0.7f, 0.7f, 1.0f);
    modeLabel->GetStyle().marginLeft = Panorama::Length::Px(20);
    modeLabel->GetStyle().marginTop = Panorama::Length::Px(55);
    videoSection->AddChild(modeLabel);
    
    auto modeValue = std::make_shared<Panorama::CLabel>("Fullscreen", "ModeValue");
    modeValue->GetStyle().fontSize = 14.0f;
    modeValue->GetStyle().color = Panorama::Color(0.9f, 0.9f, 0.9f, 1.0f);
    modeValue->GetStyle().marginLeft = Panorama::Length::Px(200);
    modeValue->GetStyle().marginTop = Panorama::Length::Px(55);
    videoSection->AddChild(modeValue);
    
    // VSync
    auto vsyncLabel = std::make_shared<Panorama::CLabel>("VSync", "VsyncLabel");
    vsyncLabel->GetStyle().fontSize = 14.0f;
    vsyncLabel->GetStyle().color = Panorama::Color(0.7f, 0.7f, 0.7f, 1.0f);
    vsyncLabel->GetStyle().marginLeft = Panorama::Length::Px(20);
    vsyncLabel->GetStyle().marginTop = Panorama::Length::Px(90);
    videoSection->AddChild(vsyncLabel);
    
    auto vsyncValue = std::make_shared<Panorama::CLabel>("On", "VsyncValue");
    vsyncValue->GetStyle().fontSize = 14.0f;
    vsyncValue->GetStyle().color = Panorama::Color(0.5f, 0.8f, 0.5f, 1.0f);
    vsyncValue->GetStyle().marginLeft = Panorama::Length::Px(200);
    vsyncValue->GetStyle().marginTop = Panorama::Length::Px(90);
    videoSection->AddChild(vsyncValue);
    
    // Render Quality
    auto qualLabel = std::make_shared<Panorama::CLabel>("Render Quality", "QualLabel");
    qualLabel->GetStyle().fontSize = 14.0f;
    qualLabel->GetStyle().color = Panorama::Color(0.7f, 0.7f, 0.7f, 1.0f);
    qualLabel->GetStyle().marginLeft = Panorama::Length::Px(20);
    qualLabel->GetStyle().marginTop = Panorama::Length::Px(125);
    videoSection->AddChild(qualLabel);
    
    auto qualValue = std::make_shared<Panorama::CLabel>("High", "QualValue");
    qualValue->GetStyle().fontSize = 14.0f;
    qualValue->GetStyle().color = Panorama::Color(0.9f, 0.9f, 0.9f, 1.0f);
    qualValue->GetStyle().marginLeft = Panorama::Length::Px(200);
    qualValue->GetStyle().marginTop = Panorama::Length::Px(125);
    videoSection->AddChild(qualValue);
    
    // Apply button
    auto applyBtn = std::make_shared<Panorama::CButton>("APPLY", "SettingsApply");
    applyBtn->GetStyle().width = Panorama::Length::Px(120);
    applyBtn->GetStyle().height = Panorama::Length::Px(40);
    applyBtn->GetStyle().backgroundColor = Panorama::Color(0.18f, 0.45f, 0.18f, 1.0f);
    applyBtn->GetStyle().borderRadius = 4.0f;
    applyBtn->GetStyle().fontSize = 14.0f;
    applyBtn->GetStyle().color = Panorama::Color::White();
    applyBtn->GetStyle().marginLeft = Panorama::Length::Px(290);
    applyBtn->GetStyle().marginTop = Panorama::Length::Px(340);
    applyBtn->SetOnActivate([this]() {
        ConsoleLog("Settings applied");
        if (m_ui && m_ui->settingsPanel) m_ui->settingsPanel->SetVisible(false);
    });
    settingsContent->AddChild(applyBtn);
    
    // Cancel button
    auto cancelBtn = std::make_shared<Panorama::CButton>("CANCEL", "SettingsCancel");
    cancelBtn->GetStyle().width = Panorama::Length::Px(120);
    cancelBtn->GetStyle().height = Panorama::Length::Px(40);
    cancelBtn->GetStyle().backgroundColor = Panorama::Color(0.25f, 0.25f, 0.3f, 1.0f);
    cancelBtn->GetStyle().borderRadius = 4.0f;
    cancelBtn->GetStyle().fontSize = 14.0f;
    cancelBtn->GetStyle().color = Panorama::Color::White();
    cancelBtn->GetStyle().marginLeft = Panorama::Length::Px(420);
    cancelBtn->GetStyle().marginTop = Panorama::Length::Px(340);
    cancelBtn->SetOnActivate([this]() {
        if (m_ui && m_ui->settingsPanel) m_ui->settingsPanel->SetVisible(false);
    });
    settingsContent->AddChild(cancelBtn);
    
    // Content container (centered, 80% width)
    auto topContent = P("TopContent", contentWidth, 55, none);
    topContent->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    top->AddChild(topContent);

    // Calculate vertical center offset for elements
    float topBarHeight = 55.0f;
    float logoHeight = S(22);
    float logoOffsetY = (topBarHeight - logoHeight) / 2.0f;
    float textOffsetY = (topBarHeight - S(11)) / 2.0f; // 11 is font size

    auto logo = P("Logo", 32, 22, Panorama::Color(0.75f, 0.12f, 0.12f, 1.0f));
    logo->GetStyle().borderRadius = S(2);
    logo->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    logo->GetStyle().marginTop = Panorama::Length::Px(logoOffsetY);
    topContent->AddChild(logo);

    const char* nav[] = {"HEROES", "STORE", "WATCH", "LEARN", "ARCADE"};
    for (int i = 0; i < 5; i++) {
        auto nb = std::make_shared<Panorama::CButton>(nav[i], std::string("Nav") + std::to_string(i));
        nb->AddClass("MainMenuNavButton");

        // Let CSS control the "look" (background/border/text). Keep only geometry here.
        // CButton constructor sets some inline defaults; clear them so stylesheet can override.
        nb->GetStyle().backgroundColor.reset();
        nb->GetStyle().borderWidth.reset();
        nb->GetStyle().borderRadius.reset();
        nb->GetStyle().borderColor.reset();

        nb->GetStyle().width = Panorama::Length::Px(S(55));
        nb->GetStyle().height = Panorama::Length::Px(topBarHeight); // Full height
        nb->GetStyle().marginLeft = Panorama::Length::Px(S(48 + i * 61));
        nb->GetStyle().marginTop = Panorama::Length::Px(0); // No top margin
        
        if (i == 0) { // HEROES button
            nb->SetOnActivate([this]() { 
                m_manager->ChangeState(EGameState::Heroes); 
            });
        }
        
        topContent->AddChild(nb);
    }
    
    auto pc = L("824,156 PLAYING", "caption", gray);
    pc->GetStyle().marginLeft = Panorama::Length::Px(S(contentWidth - 180));
    pc->GetStyle().marginTop = Panorama::Length::Px((topBarHeight - S(9)) / 2.0f);
    topContent->AddChild(pc);
    
    auto tm = L("7:48 PM", "caption", gray);
    tm->GetStyle().marginLeft = Panorama::Length::Px(S(contentWidth - 60));
    tm->GetStyle().marginTop = Panorama::Length::Px((topBarHeight - S(9)) / 2.0f);
    topContent->AddChild(tm);


    // ===== MAIN CONTENT =====
    // Create wrapper for centering (without flow layout)
    auto mainWrapper = P("MainWrapper", 0, sh - 70, none);
    mainWrapper->GetStyle().marginTop = Panorama::Length::Px(55);
    m_ui->root->AddChild(mainWrapper);
    
    auto main = P("Main", contentWidth, contentHeight, none);
    // NO FLOW LAYOUT - pure absolute positioning
    main->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    main->GetStyle().marginTop = Panorama::Length::Px(contentOffsetY);
    mainWrapper->AddChild(main);

    // ===== LEFT COLUMN =====
    auto left = P("Left", 250, contentHeight, none);
    // NO FLOW LAYOUT - children positioned absolutely
    left->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    left->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(left);

    // Profile
    auto prof = P("Prof", 0, 130, panel);
    prof->GetStyle().borderRadius = S(3);
    prof->GetStyle().marginTop = Panorama::Length::Px(S(0));
    left->AddChild(prof);

    auto ph = L("MY PROFILE", "body", gray);
    ph->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    ph->GetStyle().marginTop = Panorama::Length::Px(S(6));
    prof->AddChild(ph);

    // Avatar
    auto av = P("Av", 40, 40, Panorama::Color(0.28f, 0.45f, 0.28f, 1.0f));
    av->GetStyle().borderRadius = S(3);
    av->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    av->GetStyle().marginTop = Panorama::Length::Px(S(24));
    prof->AddChild(av);

    // Username - get from AuthClient
    std::string username = "Player";
    if (auto* authClient = m_manager->GetAuthClient()) {
        if (authClient->IsAuthenticated()) {
            username = authClient->GetUsername();
        }
    }
    m_ui->usernameLabel = L(username, "body", green);
    m_ui->usernameLabel->GetStyle().marginLeft = Panorama::Length::Px(S(54));
    m_ui->usernameLabel->GetStyle().marginTop = Panorama::Length::Px(S(26));
    prof->AddChild(m_ui->usernameLabel);
    
    // Status
    auto us = L("Main Menu", "caption", gray);
    us->GetStyle().marginLeft = Panorama::Length::Px(S(54));
    us->GetStyle().marginTop = Panorama::Length::Px(S(42));
    prof->AddChild(us);

    // Level badge
    auto lvl = P("Lvl", 26, 26, Panorama::Color(0.55f, 0.45f, 0.18f, 1.0f));
    lvl->GetStyle().borderRadius = S(13);
    lvl->GetStyle().marginLeft = Panorama::Length::Px(S(210));
    lvl->GetStyle().marginTop = Panorama::Length::Px(S(30));
    prof->AddChild(lvl);

    // Hero trophies
    Panorama::Color hc[] = {{0.55f,0.28f,0.18f,1}, {0.28f,0.35f,0.55f,1}, {0.45f,0.28f,0.45f,1}, {0.35f,0.45f,0.28f,1}};
    for (int i = 0; i < 4; i++) {
        auto ht = P("HT" + std::to_string(i), 20, 20, hc[i]);
        ht->GetStyle().borderRadius = S(2);
        ht->GetStyle().marginLeft = Panorama::Length::Px(S(8 + i * 28));
        ht->GetStyle().marginTop = Panorama::Length::Px(S(76));
        prof->AddChild(ht);
    }

    // Stats
    auto ms = L("MATCHES\n776", "small", gray);
    ms->GetStyle().marginLeft = Panorama::Length::Px(S(30));
    ms->GetStyle().marginTop = Panorama::Length::Px(S(102));
    prof->AddChild(ms);
    
    auto cs = L("COMMENDS\n167", "small", gray);
    cs->GetStyle().marginLeft = Panorama::Length::Px(S(140));
    cs->GetStyle().marginTop = Panorama::Length::Px(S(102));
    prof->AddChild(cs);

    // Battle Pass
    auto bp = P("BP", 0, 50, Panorama::Color(0.1f, 0.12f, 0.08f, 1.0f));
    bp->GetStyle().borderRadius = S(3);
    bp->GetStyle().marginTop = Panorama::Length::Px(S(136));
    left->AddChild(bp);

    auto bpt = L("BATTLE PASS", "small", gray);
    bpt->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    bpt->GetStyle().marginTop = Panorama::Length::Px(S(5));
    bp->AddChild(bpt);
    
    auto bpx = L("The Fall 2024 Battle Pass is Here", "caption", green);
    bpx->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    bpx->GetStyle().marginTop = Panorama::Length::Px(S(18));
    bp->AddChild(bpx);

    auto lkb = std::make_shared<Panorama::CButton>("LOOK INSIDE", "LkB");
    lkb->GetStyle().width = Panorama::Length::Px(S(230));
    lkb->GetStyle().height = Panorama::Length::Px(S(18));
    lkb->GetStyle().backgroundColor = Panorama::Color(0.28f, 0.5f, 0.18f, 1.0f);
    lkb->GetStyle().borderRadius = S(2);
    lkb->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    lkb->GetStyle().marginTop = Panorama::Length::Px(S(30));
    lkb->AddClass("caption");  // CSS class for font size
    lkb->GetStyle().color = Panorama::Color::White();
    bp->AddChild(lkb);


    // Friends
    auto fr = P("Fr", 0, 200, panel);
    fr->GetStyle().borderRadius = S(3);
    fr->GetStyle().marginTop = Panorama::Length::Px(S(192));
    left->AddChild(fr);

    auto fh = L("FRIENDS", "small", gray);
    fh->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    fh->GetStyle().marginTop = Panorama::Length::Px(S(6));
    fr->AddChild(fh);
    
    auto id = L("IN DOTA (4)", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    id->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    id->GetStyle().marginTop = Panorama::Length::Px(S(22));
    fr->AddChild(id);

    const char* fn[] = {"Choice", "Ivy R.", "Mythe", "standi", "Iphone"};
    const char* fhr[] = {"Serenity", "Pudge Lvl 18", "Crystal Maiden", "INDO...", "Pudge Lvl 18"};
    
    for (int i = 0; i < 5; i++) {
        // Friend row container
        auto frw = P("FW" + std::to_string(i), 230, 28, none);
        frw->GetStyle().marginLeft = Panorama::Length::Px(S(8));
        frw->GetStyle().marginTop = Panorama::Length::Px(S(40 + i * 32));
        fr->AddChild(frw);

        // Avatar
        auto fa = P("FA" + std::to_string(i), 22, 22, Panorama::Color(0.22f + i*0.03f, 0.28f, 0.32f, 1.0f));
        fa->GetStyle().borderRadius = S(2);
        fa->GetStyle().marginTop = Panorama::Length::Px(S(3));
        frw->AddChild(fa);

        // Friend name
        auto fnl = L(fn[i], "caption", Panorama::Color(0.45f, 0.65f, 0.45f, 1.0f));
        fnl->GetStyle().marginLeft = Panorama::Length::Px(S(28));
        fnl->GetStyle().marginTop = Panorama::Length::Px(S(2));
        frw->AddChild(fnl);
        
        // Friend status
        auto fhl = L(fhr[i], "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
        fhl->GetStyle().marginLeft = Panorama::Length::Px(S(28));
        fhl->GetStyle().marginTop = Panorama::Length::Px(S(14));
        frw->AddChild(fhl);
    }

    // ===== CENTER COLUMN =====
    float cw = contentWidth - S(250) - S(270) - S(50);
    auto ctr = P("Ctr", cw / S(1), contentHeight, none);
    ctr->GetStyle().marginLeft = Panorama::Length::Px(S(270));
    ctr->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(ctr);

    float bw = cw / S(1) / 2 - S(4); // Block width for 2x2 grid

    // ===== ROW 1: NEWS BLOCKS =====
    // News 1 - Top Left
    auto n1 = P("N1", bw, 115, Panorama::Color(0.12f, 0.1f, 0.15f, 1.0f));
    n1->GetStyle().borderRadius = S(3);
    ctr->AddChild(n1);

    auto n1h = L("NEW IN DOTA 2", "small", gray);
    n1h->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1h->GetStyle().marginTop = Panorama::Length::Px(S(6));
    n1->AddChild(n1h);
    
    auto n1i = P("N1I", bw - S(20), 70, Panorama::Color(0.35f, 0.22f, 0.42f, 1.0f));
    n1i->GetStyle().borderRadius = S(3);
    n1i->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1i->GetStyle().marginTop = Panorama::Length::Px(S(20));
    n1->AddChild(n1i);
    
    auto n1t = L("FALL 2024 TREASURE II", "caption", Panorama::Color::White());
    n1t->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1t->GetStyle().marginTop = Panorama::Length::Px(S(95));
    n1->AddChild(n1t);

    // News 2 - Top Right
    auto n2 = P("N2", bw, 115, Panorama::Color(0.1f, 0.12f, 0.15f, 1.0f));
    n2->GetStyle().borderRadius = S(3);
    n2->GetStyle().marginLeft = Panorama::Length::Px(S(bw + 8));
    n2->GetStyle().marginTop = Panorama::Length::Px(S(0));
    ctr->AddChild(n2);

    auto n2h = L("PRO PLAYING LIVE", "small", gray);
    n2h->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2h->GetStyle().marginTop = Panorama::Length::Px(S(6));
    n2->AddChild(n2h);
    
    auto n2i = P("N2I", bw - S(20), 70, Panorama::Color(0.18f, 0.25f, 0.32f, 1.0f));
    n2i->GetStyle().borderRadius = S(3);
    n2i->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2i->GetStyle().marginTop = Panorama::Length::Px(S(20));
    n2->AddChild(n2i);
    
    auto n2t = L("FlipSid3.RodjER", "caption", Panorama::Color::White());
    n2t->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2t->GetStyle().marginTop = Panorama::Length::Px(S(95));
    n2->AddChild(n2t);

    // ===== ROW 2: FEATURED BLOCKS =====
    // Featured 1 - Bottom Left (Battle Pass)
    auto ft1 = P("FT1", bw, 80, Panorama::Color(0.15f, 0.12f, 0.08f, 1.0f));
    ft1->GetStyle().borderRadius = S(3);
    ft1->GetStyle().marginTop = Panorama::Length::Px(S(121));
    ctr->AddChild(ft1);

    auto bpi = P("BPI", 45, 50, Panorama::Color(0.5f, 0.4f, 0.18f, 1.0f));
    bpi->GetStyle().borderRadius = S(3);
    bpi->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    bpi->GetStyle().marginTop = Panorama::Length::Px(S(15));
    ft1->AddChild(bpi);
    
    auto bpl1 = L("FALL 2024", "small", Panorama::Color(0.75f, 0.65f, 0.35f, 1.0f));
    bpl1->GetStyle().marginLeft = Panorama::Length::Px(S(59));
    bpl1->GetStyle().marginTop = Panorama::Length::Px(S(20));
    ft1->AddChild(bpl1);
    
    auto bpl2 = L("BATTLE PASS", "small", Panorama::Color(0.75f, 0.65f, 0.35f, 1.0f));
    bpl2->GetStyle().marginLeft = Panorama::Length::Px(S(59));
    bpl2->GetStyle().marginTop = Panorama::Length::Px(S(32));
    ft1->AddChild(bpl2);

    // Featured 2 - Bottom Right (Game of the Day)
    auto ft2 = P("FT2", bw, 80, Panorama::Color(0.08f, 0.1f, 0.12f, 1.0f));
    ft2->GetStyle().borderRadius = S(3);
    ft2->GetStyle().marginLeft = Panorama::Length::Px(S(bw + 8));
    ft2->GetStyle().marginTop = Panorama::Length::Px(S(121));
    ctr->AddChild(ft2);

    auto gd = L("GAME OF THE DAY", "small", gray);
    gd->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    gd->GetStyle().marginTop = Panorama::Length::Px(S(6));
    ft2->AddChild(gd);
    
    auto hm = P("HM", bw - S(20), 40, Panorama::Color(0.12f, 0.15f, 0.18f, 1.0f));
    hm->GetStyle().borderRadius = S(3);
    hm->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    hm->GetStyle().marginTop = Panorama::Length::Px(S(26));
    ft2->AddChild(hm);
    
    auto hml = L("HORDE MODE", "body", Panorama::Color::White());
    hml->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    hml->GetStyle().marginTop = Panorama::Length::Px(S(12));
    hm->AddChild(hml);

    // ===== CHAT (Below 2x2 grid) =====
    auto ch = P("Ch", 0, 110, Panorama::Color(0.08f, 0.08f, 0.1f, 1.0f));
    ch->GetStyle().borderRadius = S(3);
    ch->GetStyle().marginTop = Panorama::Length::Px(S(207));
    ctr->AddChild(ch);

    // Chat header
    auto chh = P("CHH", 0, 22, Panorama::Color(0.06f, 0.06f, 0.08f, 1.0f));
    ch->AddChild(chh);

    auto pty = L("Party", "caption", Panorama::Color(0.55f, 0.55f, 0.55f, 1.0f));
    pty->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    pty->GetStyle().marginTop = Panorama::Length::Px(S(5));
    chh->AddChild(pty);
    
    auto chl = L("CHANNELS +", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    chl->GetStyle().marginLeft = Panorama::Length::Px(S(150));
    chl->GetStyle().marginTop = Panorama::Length::Px(S(7));
    chh->AddChild(chl);

    // Chat messages
    auto m1 = L("Serenity: noob riki", "caption", Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    m1->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    m1->GetStyle().marginTop = Panorama::Length::Px(S(28));
    ch->AddChild(m1);
    
    auto m2 = L("Choice: poor pudge", "caption", Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    m2->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    m2->GetStyle().marginTop = Panorama::Length::Px(S(44));
    ch->AddChild(m2);

    // Chat input
    auto chi = P("CHI", 240, 18, Panorama::Color(0.12f, 0.12f, 0.14f, 1.0f));
    chi->GetStyle().borderRadius = S(2);
    chi->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    chi->GetStyle().marginTop = Panorama::Length::Px(S(78));
    ch->AddChild(chi);


    // ===== RIGHT COLUMN =====
    auto rgt = P("Rgt", 270, contentHeight, none);
    // NO FLOW LAYOUT - children positioned absolutely
    rgt->GetStyle().marginLeft = Panorama::Length::Px(contentWidth - S(270) - S(10));
    rgt->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(rgt);

    // Last Match
    auto lm = P("LM", 0, 120, panel);
    lm->GetStyle().borderRadius = S(3);
    lm->GetStyle().marginTop = Panorama::Length::Px(S(0));
    rgt->AddChild(lm);

    auto lmh = L("YOUR LAST MATCH", "small", gray);
    lmh->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    lmh->GetStyle().marginTop = Panorama::Length::Px(S(6));
    lm->AddChild(lmh);
    
    auto lmt = L("10/31/2024  7:03 PM", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    lmt->GetStyle().marginLeft = Panorama::Length::Px(S(135));
    lmt->GetStyle().marginTop = Panorama::Length::Px(S(6));
    lm->AddChild(lmt);

    auto hn = L("JUGGERNAUT", "subheading", Panorama::Color::White());
    hn->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    hn->GetStyle().marginTop = Panorama::Length::Px(S(24));
    lm->AddChild(hn);

    auto rs = L("WON - ALL PICK", "caption", Panorama::Color(0.28f, 0.7f, 0.28f, 1.0f));
    rs->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    rs->GetStyle().marginTop = Panorama::Length::Px(S(44));
    lm->AddChild(rs);

    auto kdl = L("K/D/A", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    kdl->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    kdl->GetStyle().marginTop = Panorama::Length::Px(S(62));
    lm->AddChild(kdl);
    
    auto kdv = L("9 / 2 / 4", "caption", Panorama::Color::White());
    kdv->GetStyle().marginLeft = Panorama::Length::Px(S(50));
    kdv->GetStyle().marginTop = Panorama::Length::Px(S(61));
    lm->AddChild(kdv);
    
    auto drl = L("DURATION", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    drl->GetStyle().marginLeft = Panorama::Length::Px(S(135));
    drl->GetStyle().marginTop = Panorama::Length::Px(S(62));
    lm->AddChild(drl);
    
    auto drv = L("37:14", "caption", Panorama::Color::White());
    drv->GetStyle().marginLeft = Panorama::Length::Px(S(200));
    drv->GetStyle().marginTop = Panorama::Length::Px(S(61));
    lm->AddChild(drv);

    auto itl = L("ITEMS", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    itl->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    itl->GetStyle().marginTop = Panorama::Length::Px(S(80));
    lm->AddChild(itl);

    Panorama::Color ic[] = {{0.45f,0.32f,0.18f,1}, {0.28f,0.45f,0.28f,1}, {0.55f,0.28f,0.28f,1}, {0.35f,0.35f,0.45f,1}, {0.45f,0.35f,0.22f,1}, {0.32f,0.32f,0.35f,1}};
    for (int i = 0; i < 6; i++) {
        auto it = P("IT" + std::to_string(i), 24, 18, ic[i]);
        it->GetStyle().borderRadius = S(1);
        it->GetStyle().marginLeft = Panorama::Length::Px(S(8 + i * 32));
        it->GetStyle().marginTop = Panorama::Length::Px(S(95));
        lm->AddChild(it);
    }

    // Activity
    auto act = P("Act", 0, 220, panel);
    act->GetStyle().borderRadius = S(3);
    act->GetStyle().marginTop = Panorama::Length::Px(S(126));
    rgt->AddChild(act);

    auto acth = L("Say something on your feed...", "caption", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    acth->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    acth->GetStyle().marginTop = Panorama::Length::Px(S(6));
    act->AddChild(acth);

    const char* at[] = {"HHr got a RAMPAGE as Sven", "Dota2wage advanced to Semi", "Iphone posted: random games", "Choice is now playing"};
    const char* atm[] = {"Yesterday", "Saturday", "Friday", "Just now"};
    
    for (int i = 0; i < 4; i++) {
        // Activity entry container
        auto ae = P("AE" + std::to_string(i), 250, 40, Panorama::Color(0.08f, 0.08f, 0.1f, 0.5f));
        ae->GetStyle().borderRadius = S(2);
        ae->GetStyle().marginLeft = Panorama::Length::Px(S(8));
        ae->GetStyle().marginTop = Panorama::Length::Px(S(26 + i * 45));
        act->AddChild(ae);

        // Avatar
        auto aa = P("AA" + std::to_string(i), 26, 26, Panorama::Color(0.28f, 0.22f + i*0.03f, 0.18f, 1.0f));
        aa->GetStyle().borderRadius = S(2);
        aa->GetStyle().marginLeft = Panorama::Length::Px(S(5));
        aa->GetStyle().marginTop = Panorama::Length::Px(S(7));
        ae->AddChild(aa);

        // Activity text
        auto atx = L(at[i], "small", Panorama::Color(0.55f, 0.55f, 0.55f, 1.0f));
        atx->GetStyle().marginLeft = Panorama::Length::Px(S(36));
        atx->GetStyle().marginTop = Panorama::Length::Px(S(8));
        ae->AddChild(atx);
        
        // Time
        auto atmx = L(atm[i], "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
        atmx->GetStyle().marginLeft = Panorama::Length::Px(S(36));
        atmx->GetStyle().marginTop = Panorama::Length::Px(S(22));
        ae->AddChild(atmx);
    }


    // ===== BOTTOM BAR =====
    // Full width background
    auto bottomBarBg = P("BotBg", 0, 70, Panorama::Color(0.03f, 0.04f, 0.06f, 0.95f));
    bottomBarBg->GetStyle().marginTop = Panorama::Length::Px(sh - 70);
    m_ui->root->AddChild(bottomBarBg);
    
    // Content container (centered, 80% width)
    m_ui->bottomBar = P("Bot", contentWidth, 70, none);
    m_ui->bottomBar->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    bottomBarBg->AddChild(m_ui->bottomBar);

    // Game mode icon
    auto gi = P("GI", 40, 40, Panorama::Color(0.12f, 0.12f, 0.15f, 1.0f));
    gi->GetStyle().borderRadius = S(3);
    gi->GetStyle().marginLeft = Panorama::Length::Px(15);
    gi->GetStyle().marginTop = Panorama::Length::Px(15);
    m_ui->bottomBar->AddChild(gi);

    // Party members
    Panorama::Color pc2[] = {{0.45f,0.28f,0.22f,1}, {0.28f,0.32f,0.45f,1}, {0.35f,0.45f,0.32f,1}};
    for (int i = 0; i < 3; i++) {
        auto pt = P("PT" + std::to_string(i), 35, 40, pc2[i]);
        pt->GetStyle().borderRadius = S(2);
        pt->GetStyle().marginLeft = Panorama::Length::Px(65 + i * 40);
        pt->GetStyle().marginTop = Panorama::Length::Px(15);
        m_ui->bottomBar->AddChild(pt);
    }
    
    // Add party button
    auto addParty = P("AddParty", 35, 40, Panorama::Color(0.15f, 0.15f, 0.18f, 1.0f));
    addParty->GetStyle().borderRadius = S(2);
    addParty->GetStyle().marginLeft = Panorama::Length::Px(185);
    addParty->GetStyle().marginTop = Panorama::Length::Px(15);
    m_ui->bottomBar->AddChild(addParty);
    
    auto plusLabel = L("+", "title", Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    plusLabel->GetStyle().marginLeft = Panorama::Length::Px(11);
    plusLabel->GetStyle().marginTop = Panorama::Length::Px(5);
    addParty->AddChild(plusLabel);

    // Game mode label
    auto gameModeLabel = L("ALL PICK", "body", Panorama::Color(0.6f, 0.6f, 0.6f, 1.0f));
    gameModeLabel->GetStyle().marginLeft = Panorama::Length::Px(250);
    gameModeLabel->GetStyle().marginTop = Panorama::Length::Px(28);
    m_ui->bottomBar->AddChild(gameModeLabel);

    // Play button
    m_ui->playButton = std::make_shared<Panorama::CButton>("PLAY DOTA", "PB");
    m_ui->playButton->GetStyle().width = Panorama::Length::Px(180);
    m_ui->playButton->GetStyle().height = Panorama::Length::Px(45);
    m_ui->playButton->GetStyle().backgroundColor = greenBtn;
    m_ui->playButton->GetStyle().borderRadius = S(3);
    m_ui->playButton->AddClass("subheading");  // CSS class for font size
    m_ui->playButton->GetStyle().color = Panorama::Color::White();
    m_ui->playButton->GetStyle().marginLeft = Panorama::Length::Px(contentWidth - 200);
    m_ui->playButton->GetStyle().marginTop = Panorama::Length::Px(12);
    m_ui->playButton->SetOnActivate([this]() { OnPlayClicked(); });
    m_ui->bottomBar->AddChild(m_ui->playButton);

    // ===== Finding match button (swap while searching) =====
    // Same placement as play button, but with [X] cancel and a timer above.
    const float playX = contentWidth - 200;
    const float playY = 12.0f;

    m_ui->findingPanel = P("MM_FindingPanel", 180, 45, Panorama::Color(0.12f, 0.16f, 0.22f, 1.0f));
    m_ui->findingPanel->GetStyle().borderRadius = S(3);
    m_ui->findingPanel->GetStyle().marginLeft = Panorama::Length::Px(playX);
    m_ui->findingPanel->GetStyle().marginTop = Panorama::Length::Px(playY);
    m_ui->findingPanel->SetVisible(false);
    m_ui->bottomBar->AddChild(m_ui->findingPanel);

    m_ui->findingLabel = L("FINDING MATCH", "body", Panorama::Color(0.85f, 0.9f, 0.95f, 1.0f));
    m_ui->findingLabel->GetStyle().marginLeft = Panorama::Length::Px(S(14));
    m_ui->findingLabel->GetStyle().marginTop = Panorama::Length::Px(S(14));
    m_ui->findingPanel->AddChild(m_ui->findingLabel);

    m_ui->findingCancelButton = std::make_shared<Panorama::CButton>("X", "MM_FindCancel");
    m_ui->findingCancelButton->GetStyle().width = Panorama::Length::Px(S(30));
    m_ui->findingCancelButton->GetStyle().height = Panorama::Length::Px(S(30));
    m_ui->findingCancelButton->GetStyle().backgroundColor = Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
    m_ui->findingCancelButton->GetStyle().borderRadius = S(3);
    m_ui->findingCancelButton->AddClass("subheading");  // CSS class for font size
    m_ui->findingCancelButton->GetStyle().color = Panorama::Color::White();
    // Position inside 180px button (right aligned)
    m_ui->findingCancelButton->GetStyle().marginLeft = Panorama::Length::Px(S(180 - 36));
    m_ui->findingCancelButton->GetStyle().marginTop = Panorama::Length::Px(S(7));
    m_ui->findingCancelButton->SetOnActivate([this]() {
        if (m_mmClient) m_mmClient->cancelQueue();
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
        if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
        ConsoleLog("Matchmaking cancelled");
    });
    m_ui->findingPanel->AddChild(m_ui->findingCancelButton);

    // Timer label for finding match (shown above play button when searching)
    m_ui->findingTimeLabel = L("00:00", "body", Panorama::Color(0.65f, 0.75f, 0.85f, 1.0f));
    m_ui->findingTimeLabel->GetStyle().marginLeft = Panorama::Length::Px(playX + S(2));
    m_ui->findingTimeLabel->GetStyle().marginTop = Panorama::Length::Px(S(2));
    m_ui->findingTimeLabel->SetVisible(false);
    m_ui->bottomBar->AddChild(m_ui->findingTimeLabel);

    // ===== Matchmaking overlays =====
    // Searching overlay
    m_ui->searchingOverlay = P("MM_SearchingOverlay", 0, 0, Panorama::Color(0.0f, 0.0f, 0.0f, 0.55f));
    m_ui->searchingOverlay->SetVisible(false);
    m_ui->root->AddChild(m_ui->searchingOverlay);

    auto searchingBox = P("MM_SearchingBox", 420, 170, Panorama::Color(0.08f, 0.09f, 0.11f, 0.96f));
    searchingBox->GetStyle().borderRadius = S(4);
    searchingBox->GetStyle().marginLeft = Panorama::Length::Px((sw - S(420)) * 0.5f);
    searchingBox->GetStyle().marginTop = Panorama::Length::Px((sh - S(170)) * 0.5f);
    m_ui->searchingOverlay->AddChild(searchingBox);

    m_ui->searchingLabel = L("SEARCHING FOR MATCH...", "subheading", Panorama::Color(0.85f, 0.85f, 0.85f, 1.0f));
    m_ui->searchingLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->searchingLabel->GetStyle().marginTop = Panorama::Length::Px(S(22));
    searchingBox->AddChild(m_ui->searchingLabel);

    m_ui->searchTimeLabel = L("00:00", "body", Panorama::Color(0.65f, 0.65f, 0.65f, 1.0f));
    m_ui->searchTimeLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->searchTimeLabel->GetStyle().marginTop = Panorama::Length::Px(S(55));
    searchingBox->AddChild(m_ui->searchTimeLabel);

    m_ui->cancelSearchButton = std::make_shared<Panorama::CButton>("CANCEL", "MM_Cancel");
    m_ui->cancelSearchButton->GetStyle().width = Panorama::Length::Px(S(140));
    m_ui->cancelSearchButton->GetStyle().height = Panorama::Length::Px(S(40));
    m_ui->cancelSearchButton->GetStyle().backgroundColor = Panorama::Color(0.25f, 0.25f, 0.3f, 0.95f);
    m_ui->cancelSearchButton->GetStyle().borderRadius = S(3);
    m_ui->cancelSearchButton->AddClass("body");  // CSS class for font size
    m_ui->cancelSearchButton->GetStyle().color = Panorama::Color::White();
    m_ui->cancelSearchButton->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->cancelSearchButton->GetStyle().marginTop = Panorama::Length::Px(S(95));
    m_ui->cancelSearchButton->SetOnActivate([this]() {
        if (m_mmClient) m_mmClient->cancelQueue();
        if (m_ui->searchingOverlay) m_ui->searchingOverlay->SetVisible(false);
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
        if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
        if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
        ConsoleLog("Matchmaking cancelled");
    });
    searchingBox->AddChild(m_ui->cancelSearchButton);

    // Accept overlay
    m_ui->acceptOverlay = P("MM_AcceptOverlay", 0, 0, Panorama::Color(0.0f, 0.0f, 0.0f, 0.65f));
    m_ui->acceptOverlay->SetVisible(false);
    m_ui->root->AddChild(m_ui->acceptOverlay);

    auto acceptBox = P("MM_AcceptBox", 460, 210, Panorama::Color(0.08f, 0.09f, 0.11f, 0.98f));
    acceptBox->GetStyle().borderRadius = S(4);
    acceptBox->GetStyle().marginLeft = Panorama::Length::Px((sw - S(460)) * 0.5f);
    acceptBox->GetStyle().marginTop = Panorama::Length::Px((sh - S(210)) * 0.5f);
    m_ui->acceptOverlay->AddChild(acceptBox);

    m_ui->acceptLabel = L("MATCH FOUND", "heading", Panorama::Color(0.92f, 0.92f, 0.92f, 1.0f));
    m_ui->acceptLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->acceptLabel->GetStyle().marginTop = Panorama::Length::Px(S(25));
    acceptBox->AddChild(m_ui->acceptLabel);

    // Countdown (Dota-like accept timer)
    m_ui->acceptCountdownLabel = L("00:20", "body", Panorama::Color(0.65f, 0.75f, 0.85f, 1.0f));
    m_ui->acceptCountdownLabel->GetStyle().marginLeft = Panorama::Length::Px(S(360));
    m_ui->acceptCountdownLabel->GetStyle().marginTop = Panorama::Length::Px(S(28));
    acceptBox->AddChild(m_ui->acceptCountdownLabel);

    // Status row (Dota-like cubes). Hidden until local player accepts.
    m_ui->acceptStatusLabel = L("0/0 ACCEPTED", "body", Panorama::Color(0.75f, 0.75f, 0.75f, 1.0f));
    m_ui->acceptStatusLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->acceptStatusLabel->GetStyle().marginTop = Panorama::Length::Px(S(60));
    m_ui->acceptStatusLabel->SetVisible(false);
    acceptBox->AddChild(m_ui->acceptStatusLabel);

    m_ui->acceptStatusPanel = P("MM_AcceptStatusPanel", 420, 28, Panorama::Color(0, 0, 0, 0));
    m_ui->acceptStatusPanel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->acceptStatusPanel->GetStyle().marginTop = Panorama::Length::Px(S(85));
    m_ui->acceptStatusPanel->SetVisible(false);
    acceptBox->AddChild(m_ui->acceptStatusPanel);

    m_ui->acceptButton = std::make_shared<Panorama::CButton>("ACCEPT", "MM_Accept");
    m_ui->acceptButton->GetStyle().width = Panorama::Length::Px(S(160));
    m_ui->acceptButton->GetStyle().height = Panorama::Length::Px(S(46));
    m_ui->acceptButton->GetStyle().backgroundColor = Panorama::Color(0.18f, 0.45f, 0.18f, 1.0f);
    m_ui->acceptButton->GetStyle().borderRadius = S(3);
    m_ui->acceptButton->AddClass("subheading");  // CSS class for font size
    m_ui->acceptButton->GetStyle().color = Panorama::Color::White();
    m_ui->acceptButton->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->acceptButton->GetStyle().marginTop = Panorama::Length::Px(S(115));
    m_ui->acceptButton->SetOnActivate([this]() {
        if (m_mmClient) m_mmClient->acceptMatch();
        ConsoleLog("Match accepted");
        // Swap UI: buttons -> status row
        if (m_ui && m_ui->acceptButton) m_ui->acceptButton->SetVisible(false);
        if (m_ui && m_ui->declineButton) m_ui->declineButton->SetVisible(false);
        if (m_ui && m_ui->acceptStatusLabel) m_ui->acceptStatusLabel->SetVisible(true);
        if (m_ui && m_ui->acceptStatusPanel) m_ui->acceptStatusPanel->SetVisible(true);

        // Optimistic local UI: immediately show 1/N accepted for this client.
        // Coordinator will send MatchAcceptStatus shortly after; that will correct any mismatch.
        if (m_ui && m_ui->acceptStatusLabel) {
            const int total = (int)m_ui->acceptCubes.size();
            if (total > 0) {
                m_ui->acceptStatusLabel->SetText("1/" + std::to_string(total) + " ACCEPTED");
            }
        }
        if (m_ui) {
            // Color a cube green for local acceptance if we can identify our slot.
            int selfIndex = -1;
            if (m_mmClient) {
                const u64 selfId = m_mmClient->getPlayerInfo().steamId;
                const auto& ids = m_mmClient->getAcceptPlayerIds();
                for (size_t i = 0; i < ids.size(); ++i) {
                    if (ids[i] == selfId) { selfIndex = (int)i; break; }
                }
            }
            if (selfIndex < 0) selfIndex = 0; // fallback if status packet not received yet
            for (int i = 0; i < (int)m_ui->acceptCubes.size(); ++i) {
                if (!m_ui->acceptCubes[i]) continue;
                m_ui->acceptCubes[i]->GetStyle().backgroundColor = (i == selfIndex)
                    ? Panorama::Color(0.18f, 0.55f, 0.18f, 1.0f)
                    : Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
            }
        }
    });
    acceptBox->AddChild(m_ui->acceptButton);

    m_ui->declineButton = std::make_shared<Panorama::CButton>("DECLINE", "MM_Decline");
    m_ui->declineButton->GetStyle().width = Panorama::Length::Px(S(160));
    m_ui->declineButton->GetStyle().height = Panorama::Length::Px(S(46));
    m_ui->declineButton->GetStyle().backgroundColor = Panorama::Color(0.45f, 0.18f, 0.18f, 1.0f);
    m_ui->declineButton->GetStyle().borderRadius = S(3);
    m_ui->declineButton->AddClass("subheading");  // CSS class for font size
    m_ui->declineButton->GetStyle().color = Panorama::Color::White();
    m_ui->declineButton->GetStyle().marginLeft = Panorama::Length::Px(S(210));
    m_ui->declineButton->GetStyle().marginTop = Panorama::Length::Px(S(115));
    m_ui->declineButton->SetOnActivate([this]() {
        if (m_mmClient) m_mmClient->declineMatch();
        if (m_ui->acceptOverlay) m_ui->acceptOverlay->SetVisible(false);
        if (m_ui->searchingOverlay) m_ui->searchingOverlay->SetVisible(false);
        ConsoleLog("Match declined");
    });
    acceptBox->AddChild(m_ui->declineButton);
}

void MainMenuState::DestroyUI() {
    if (m_ui->root) {
        auto& engine = Panorama::CUIEngine::Instance();
        if (auto* uiRoot = engine.GetRoot()) {
            uiRoot->RemoveChild(m_ui->root.get());
        }
    }
    m_ui->root.reset();
}

void MainMenuState::Update(f32 dt) {
    if (m_mmClient) {
        m_mmClient->update(dt);

        // Update finding match timer (shown above play button).
        if (m_ui && m_ui->findingPanel && m_ui->findingPanel->IsVisible() && m_ui->findingTimeLabel) {
            const auto& qs = m_mmClient->getQueueStatus();
            const int t = (int)qs.searchTime;
            const int mm = t / 60;
            const int ss = t % 60;
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
            m_ui->findingTimeLabel->SetText(buf);
        }

        // Update accept countdown if accept overlay visible.
        if (m_ui && m_ui->acceptOverlay && m_ui->acceptOverlay->IsVisible() && m_ui->acceptCountdownLabel) {
            const float remaining = m_mmClient->getAcceptTimeRemainingSeconds();
            const int sec = std::max(0, (int)std::ceil(remaining));
            const int mm = sec / 60;
            const int ss = sec % 60;
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
            m_ui->acceptCountdownLabel->SetText(buf);

            // Disable buttons at 0 to match Dota feel; coordinator will cancel shortly after.
            const bool expired = (sec <= 0);
            if (m_ui->acceptButton) m_ui->acceptButton->SetEnabled(!expired);
            if (m_ui->declineButton) m_ui->declineButton->SetEnabled(!expired);
        }
    }

    Panorama::CUIEngine::Instance().Update(dt);
}
void MainMenuState::Render() { Panorama::CUIEngine::Instance().Render(); }

bool MainMenuState::OnKeyDown(i32 key) {
    if (key == 27) { OnExitClicked(); return true; }
    // Removed Enter key handler - use Play button instead
    return false;
}

bool MainMenuState::OnMouseMove(f32 x, f32 y) { Panorama::CUIEngine::Instance().OnMouseMove(x, y); return true; }
bool MainMenuState::OnMouseDown(f32 x, f32 y, i32 b) { Panorama::CUIEngine::Instance().OnMouseDown(x, y, b); return true; }
bool MainMenuState::OnMouseUp(f32 x, f32 y, i32 b) { Panorama::CUIEngine::Instance().OnMouseUp(x, y, b); return true; }

void MainMenuState::OnPlayClicked() {
    LOG_INFO("=== PLAY BUTTON CLICKED ===");
    ConsoleLog("=== PLAY BUTTON CLICKED ===");
    ConsoleLog("Searching for match...");

    if (!m_mmClient) {
        m_mmClient = std::make_unique<WorldEditor::Matchmaking::MatchmakingClient>();
    }
    
    // Always setup callbacks (in case client was created by CheckForActiveGame)
    SetupMatchmakingCallbacks();

    if (!m_mmClient->isConnected()) {
        // Get session token from AuthClient
        auto* authClient = m_manager->GetAuthClient();
        if (!authClient || !authClient->IsAuthenticated()) {
            ConsoleLog("Not authenticated - please login first");
            LOG_ERROR("No session token set - authentication required before queueing");
            return;
        }
        
        std::string sessionToken = authClient->GetSessionToken();
        if (sessionToken.empty()) {
            ConsoleLog("No session token - please login first");
            LOG_ERROR("No session token set - authentication required before queueing");
            return;
        }
        
        // Set session token for matchmaking
        m_mmClient->setSessionToken(sessionToken);
        
        // Connect to matchmaking coordinator on port 27017 (not auth server 27016!)
        if (!m_mmClient->connect("127.0.0.1", 27017)) {
            ConsoleLog("Failed to connect to matchmaking coordinator");
            return;
        }
        
        LOG_INFO("Connected to matchmaking coordinator");
    }

    WorldEditor::Matchmaking::MatchPreferences prefs;
    prefs.mode = WorldEditor::Matchmaking::MatchMode::AllPick;
    prefs.region = "auto";

    if (m_mmClient->queueForMatch(prefs)) {
        // Swap play button -> finding match UI
        if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(false);
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(true);
        if (m_ui && m_ui->findingTimeLabel) {
            m_ui->findingTimeLabel->SetText("00:00");
            m_ui->findingTimeLabel->SetVisible(true);
        }
    } else {
        ConsoleLog("Failed to queue for match");
    }
}

void MainMenuState::OnSettingsClicked() {
    // Toggle settings panel visibility
    if (m_ui && m_ui->settingsPanel) {
        bool visible = m_ui->settingsPanel->IsVisible();
        m_ui->settingsPanel->SetVisible(!visible);
    }
}

void MainMenuState::OnExitClicked() {
    Panorama::CGameEventData d;
    Panorama::GameEvents_Fire("Game_RequestExit", d);
}

void MainMenuState::CheckForActiveGame() {
    // Get account ID from AuthClient
    auto* authClient = m_manager->GetAuthClient();
    if (!authClient || !authClient->IsAuthenticated()) {
        return;  // Not logged in, no active game check needed
    }
    
    u64 accountId = authClient->GetAccountId();
    if (accountId == 0) return;
    
    // Create matchmaking client if needed
    if (!m_mmClient) {
        m_mmClient = std::make_unique<WorldEditor::Matchmaking::MatchmakingClient>();
        SetupReconnectCallbacks();
    }
    
    // Connect to coordinator if not connected
    if (!m_mmClient->isConnected()) {
        std::string sessionToken = authClient->GetSessionToken();
        m_mmClient->setSessionToken(sessionToken);
        
        if (!m_mmClient->connect("127.0.0.1", 27017)) {
            LOG_WARN("Failed to connect to matchmaking coordinator for active game check");
            return;
        }
    }
    
    // Check for active game
    m_mmClient->checkForActiveGame(accountId);
    LOG_INFO("Checking for active game for account {}", accountId);
}

void MainMenuState::SetupMatchmakingCallbacks() {
    if (!m_mmClient) return;
    
    m_mmClient->setOnQueueConfirmed([this]() {
        ConsoleLog("Queue confirmed");
    });
    
    m_mmClient->setOnMatchFound([this](const WorldEditor::Matchmaking::LobbyInfo& lobby) {
        LOG_INFO("=== onMatchFound callback triggered! ===");
        ConsoleLog("Match found!");
        // Hide finding UI and show accept overlay
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
        if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
        if (m_ui && m_ui->acceptOverlay) m_ui->acceptOverlay->SetVisible(true);

        // Reset accept UI to initial state (buttons visible, status hidden)
        if (m_ui && m_ui->acceptButton) m_ui->acceptButton->SetVisible(true);
        if (m_ui && m_ui->declineButton) m_ui->declineButton->SetVisible(true);
        if (m_ui && m_ui->acceptButton) m_ui->acceptButton->SetEnabled(true);
        if (m_ui && m_ui->declineButton) m_ui->declineButton->SetEnabled(true);
        if (m_ui && m_ui->acceptStatusLabel) m_ui->acceptStatusLabel->SetVisible(false);
        if (m_ui && m_ui->acceptStatusPanel) m_ui->acceptStatusPanel->SetVisible(false);

        // Prime countdown text immediately.
        if (m_ui && m_ui->acceptCountdownLabel && m_mmClient) {
            const int sec = std::max(0, (int)std::ceil(m_mmClient->getAcceptTimeRemainingSeconds()));
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "%02d:%02d", sec / 60, sec % 60);
            m_ui->acceptCountdownLabel->SetText(buf);
        }

        // Build cubes (dev: lobby.players.size() == requiredPlayers).
        if (m_ui && m_ui->acceptStatusPanel) {
            m_ui->acceptStatusPanel->RemoveAndDeleteChildren();
            m_ui->acceptCubes.clear();
            const int count = (int)lobby.players.size();
            const float cube = S(18);
            const float gap = S(8);
            for (int i = 0; i < count; ++i) {
                auto c = P("MM_Cube_" + std::to_string(i), 18, 18, Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f));
                c->GetStyle().borderRadius = S(2);
                c->GetStyle().marginLeft = Panorama::Length::Px(i * (cube + gap));
                c->GetStyle().marginTop = Panorama::Length::Px(0);
                m_ui->acceptStatusPanel->AddChild(c);
                m_ui->acceptCubes.push_back(c);
            }
        }
    });

    m_mmClient->setOnMatchAcceptStatus([this](u16 requiredPlayers, const std::vector<u64>& playerIds, const std::vector<bool>& accepted) {
        if (!m_ui) return;
        const int count = (int)accepted.size();

        int acceptedCount = 0;
        for (bool a : accepted) if (a) acceptedCount++;

        if (m_ui->acceptStatusLabel) {
            m_ui->acceptStatusLabel->SetText(std::to_string(acceptedCount) + "/" + std::to_string(requiredPlayers) + " ACCEPTED");
        }

        // Update cube colors: green = accepted, red = not accepted
        for (int i = 0; i < count && i < (int)m_ui->acceptCubes.size(); ++i) {
            if (!m_ui->acceptCubes[i]) continue;
            m_ui->acceptCubes[i]->GetStyle().backgroundColor = accepted[i]
                ? Panorama::Color(0.18f, 0.55f, 0.18f, 1.0f)
                : Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
        }

        // If local player accepted, show status row (Dota-like).
        if (m_mmClient) {
            const u64 selfId = m_mmClient->getPlayerInfo().steamId;
            bool selfAccepted = false;
            for (size_t i = 0; i < playerIds.size() && i < accepted.size(); ++i) {
                if (playerIds[i] == selfId) {
                    selfAccepted = accepted[i];
                    break;
                }
            }
            if (selfAccepted) {
                if (m_ui->acceptButton) m_ui->acceptButton->SetVisible(false);
                if (m_ui->declineButton) m_ui->declineButton->SetVisible(false);
                if (m_ui->acceptStatusLabel) m_ui->acceptStatusLabel->SetVisible(true);
                if (m_ui->acceptStatusPanel) m_ui->acceptStatusPanel->SetVisible(true);
            }
        }
    });
    
    m_mmClient->setOnMatchReady([this](const std::string& serverIP, u16 port) {
        ConsoleLog("Match ready! Connecting...");
        if (m_ui && m_ui->acceptOverlay) m_ui->acceptOverlay->SetVisible(false);
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
        if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);

        // Pass server endpoint to LoadingState (no manual selection).
        if (m_manager) {
            if (auto* loading = m_manager->GetLoadingState()) {
                loading->SetServerTarget(serverIP, port);
                loading->SetReconnect(false);  // Normal flow - go through hero pick
            }
        }

        LOG_INFO("Transitioning to Loading state (server {}:{})", serverIP, port);
        m_manager->ChangeState(EGameState::Loading);
    });
    
    m_mmClient->setOnMatchCancelled([this](const std::string& reason, bool shouldRequeue) {
        ConsoleLog(std::string("Match cancelled: ") + reason + (shouldRequeue ? " (returning to queue)" : ""));
        if (m_ui && m_ui->acceptOverlay) m_ui->acceptOverlay->SetVisible(false);
        
        if (shouldRequeue) {
            // Player accepted but someone else didn't - return to search UI
            if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(true);
            if (m_ui && m_ui->findingTimeLabel) {
                m_ui->findingTimeLabel->SetText("00:00");
                m_ui->findingTimeLabel->SetVisible(true);
            }
            if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(false);
        } else {
            // This player didn't accept - show normal Play button (no timer)
            if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
            if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
            if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
        }
    });
    
    m_mmClient->setOnError([this](const std::string& error) {
        ConsoleLog(std::string("MM error: ") + error);
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
        if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
        if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
    });
    
    m_mmClient->setOnQueueRejected([this](const std::string& reason, bool authFailed, bool isBanned) {
        ConsoleLog(std::string("Queue rejected: ") + reason);
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
        if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
        if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
    });
}

void MainMenuState::SetupReconnectCallbacks() {
    if (!m_mmClient) return;
    
    m_mmClient->setOnActiveGameFound([this](const WorldEditor::Matchmaking::ActiveGameInfo& gameInfo) {
        LOG_INFO("Active game found! Hero: {}, Server: {}:{}", 
                 gameInfo.heroName, gameInfo.serverIP, gameInfo.serverPort);
        ConsoleLog("You have an active game! Click RECONNECT to rejoin.");
        
        // Show reconnect overlay
        ShowReconnectOverlay(gameInfo);
    });
    
    m_mmClient->setOnNoActiveGame([this]() {
        LOG_INFO("No active game found");
        // Hide reconnect overlay if visible
        if (m_ui && m_ui->reconnectOverlay) {
            m_ui->reconnectOverlay->SetVisible(false);
        }
    });
    
    m_mmClient->setOnReconnectApproved([this](const std::string& serverIP, u16 port, u8 teamSlot, const std::string& heroName) {
        LOG_INFO("Reconnect approved! Connecting to {}:{}", serverIP, port);
        ConsoleLog("Reconnecting to game...");
        
        // Hide overlay
        if (m_ui && m_ui->reconnectOverlay) {
            m_ui->reconnectOverlay->SetVisible(false);
        }
        
        // Set server target and mark as reconnect
        if (m_manager) {
            if (auto* loading = m_manager->GetLoadingState()) {
                loading->SetServerTarget(serverIP, port);
                loading->SetReconnect(true);  // Skip hero pick, go directly to InGame
            }
            m_manager->ChangeState(EGameState::Loading);
        }
    });
}

void MainMenuState::ShowReconnectOverlay(const WorldEditor::Matchmaking::ActiveGameInfo& gameInfo) {
    if (!m_ui || !m_ui->root) return;
    
    auto& engine = Panorama::CUIEngine::Instance();
    float sw = engine.GetScreenWidth();
    float sh = engine.GetScreenHeight();
    
    // Create overlay if not exists
    if (!m_ui->reconnectOverlay) {
        // Semi-transparent background
        m_ui->reconnectOverlay = std::make_shared<Panorama::CPanel2D>("ReconnectOverlay");
        m_ui->reconnectOverlay->GetStyle().width = Panorama::Length::Fill();
        m_ui->reconnectOverlay->GetStyle().height = Panorama::Length::Fill();
        m_ui->reconnectOverlay->GetStyle().backgroundColor = Panorama::Color(0.0f, 0.0f, 0.0f, 0.85f);
        m_ui->root->AddChild(m_ui->reconnectOverlay);
        
        // Center panel
        auto panel = std::make_shared<Panorama::CPanel2D>("ReconnectPanel");
        panel->GetStyle().width = Panorama::Length::Px(500);
        panel->GetStyle().height = Panorama::Length::Px(250);
        panel->GetStyle().backgroundColor = Panorama::Color(0.1f, 0.12f, 0.15f, 0.98f);
        panel->GetStyle().borderRadius = 8.0f;
        panel->GetStyle().marginLeft = Panorama::Length::Px((sw - 500) / 2);
        panel->GetStyle().marginTop = Panorama::Length::Px((sh - 250) / 2);
        m_ui->reconnectOverlay->AddChild(panel);
        
        // Title
        m_ui->reconnectLabel = std::make_shared<Panorama::CLabel>("GAME IN PROGRESS", "ReconnectTitle");
        m_ui->reconnectLabel->GetStyle().fontSize = 28.0f;
        m_ui->reconnectLabel->GetStyle().color = Panorama::Color(0.95f, 0.75f, 0.25f, 1.0f);
        m_ui->reconnectLabel->GetStyle().marginLeft = Panorama::Length::Px(120);
        m_ui->reconnectLabel->GetStyle().marginTop = Panorama::Length::Px(30);
        panel->AddChild(m_ui->reconnectLabel);
        
        // Info label
        m_ui->reconnectInfoLabel = std::make_shared<Panorama::CLabel>("", "ReconnectInfo");
        m_ui->reconnectInfoLabel->GetStyle().fontSize = 16.0f;
        m_ui->reconnectInfoLabel->GetStyle().color = Panorama::Color(0.7f, 0.7f, 0.7f, 1.0f);
        m_ui->reconnectInfoLabel->GetStyle().marginLeft = Panorama::Length::Px(50);
        m_ui->reconnectInfoLabel->GetStyle().marginTop = Panorama::Length::Px(80);
        panel->AddChild(m_ui->reconnectInfoLabel);
        
        // Reconnect button
        m_ui->reconnectButton = std::make_shared<Panorama::CButton>("RECONNECT", "ReconnectBtn");
        m_ui->reconnectButton->GetStyle().width = Panorama::Length::Px(180);
        m_ui->reconnectButton->GetStyle().height = Panorama::Length::Px(50);
        m_ui->reconnectButton->GetStyle().backgroundColor = Panorama::Color(0.2f, 0.55f, 0.2f, 1.0f);
        m_ui->reconnectButton->GetStyle().borderRadius = 4.0f;
        m_ui->reconnectButton->GetStyle().color = Panorama::Color::White();
        m_ui->reconnectButton->GetStyle().marginLeft = Panorama::Length::Px(50);
        m_ui->reconnectButton->GetStyle().marginTop = Panorama::Length::Px(160);
        m_ui->reconnectButton->SetOnActivate([this]() {
            OnReconnectClicked();
        });
        panel->AddChild(m_ui->reconnectButton);
        
        // Abandon button
        m_ui->abandonButton = std::make_shared<Panorama::CButton>("ABANDON", "AbandonBtn");
        m_ui->abandonButton->GetStyle().width = Panorama::Length::Px(180);
        m_ui->abandonButton->GetStyle().height = Panorama::Length::Px(50);
        m_ui->abandonButton->GetStyle().backgroundColor = Panorama::Color(0.55f, 0.2f, 0.2f, 1.0f);
        m_ui->abandonButton->GetStyle().borderRadius = 4.0f;
        m_ui->abandonButton->GetStyle().color = Panorama::Color::White();
        m_ui->abandonButton->GetStyle().marginLeft = Panorama::Length::Px(270);
        m_ui->abandonButton->GetStyle().marginTop = Panorama::Length::Px(160);
        m_ui->abandonButton->SetOnActivate([this]() {
            OnAbandonClicked();
        });
        panel->AddChild(m_ui->abandonButton);
    }
    
    // Update info text
    if (m_ui->reconnectInfoLabel) {
        char buf[256];
        snprintf(buf, sizeof(buf), 
                 "Hero: %s\nGame Time: %.0f seconds\nDisconnected: %.0f seconds ago",
                 gameInfo.heroName.c_str(), gameInfo.gameTime, gameInfo.disconnectTime);
        m_ui->reconnectInfoLabel->SetText(buf);
    }
    
    // Store game info for reconnect
    m_activeGameInfo = gameInfo;
    
    // Show overlay
    m_ui->reconnectOverlay->SetVisible(true);
}

void MainMenuState::OnReconnectClicked() {
    if (!m_mmClient) return;
    
    LOG_INFO("Reconnect clicked, requesting reconnect to lobby {}", m_activeGameInfo.lobbyId);
    m_mmClient->requestReconnect(m_activeGameInfo.lobbyId);
}

void MainMenuState::OnAbandonClicked() {
    LOG_INFO("Abandon clicked");
    ConsoleLog("Game abandoned. You may receive a penalty.");
    
    // Hide overlay
    if (m_ui && m_ui->reconnectOverlay) {
        m_ui->reconnectOverlay->SetVisible(false);
    }
    
    // TODO: Send abandon notification to coordinator
    // This would update player stats and potentially apply penalties
}

} // namespace Game
