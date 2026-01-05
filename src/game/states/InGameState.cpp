/**
 * InGameState - Active gameplay state
 * 
 * Uses shared NetworkClient from GameStateManager for persistent connection.
 * Connection established in HeroPickState persists here.
 */

#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/CUIEngine.h"
#include "../ui/panorama/CPanel2D.h"
#include "../ui/panorama/GameEvents.h"
#include "../../client/ClientWorld.h"
#include "../../server/ServerWorld.h"
#include "../../world/World.h"
#include "../../world/Components.h"
#include "../../network/NetworkClient.h"
#include "../../common/GameInput.h"
#include "../../auth/AuthClient.h"
#include "../../renderer/DirectXRenderer.h"
#include "../../ui/EditorCamera.h"
#include <glm/gtc/matrix_transform.hpp>

// External renderer from GameMain.cpp
extern DirectXRenderer* g_renderer;

namespace Game {

// ============ Game HUD Structure ============

struct InGameState::GameHUD {
    std::shared_ptr<Panorama::CPanel2D> root;
    
    // Top bar
    std::shared_ptr<Panorama::CPanel2D> topBar;
    std::shared_ptr<Panorama::CButton> menuButton;  // Menu button (top-left, Dota 2 style)
    std::shared_ptr<Panorama::CLabel> gameTimeLabel;
    std::shared_ptr<Panorama::CLabel> debugLabel;
    
    // Hero HUD (bottom left)
    std::shared_ptr<Panorama::CPanel2D> heroHUD;
    std::shared_ptr<Panorama::CProgressBar> healthBar;
    std::shared_ptr<Panorama::CProgressBar> manaBar;
    std::shared_ptr<Panorama::CLabel> healthLabel;
    std::shared_ptr<Panorama::CLabel> manaLabel;
    
    // Ability bar (bottom center)
    std::shared_ptr<Panorama::CPanel2D> abilityBar;
    
    // Minimap (bottom right)
    std::shared_ptr<Panorama::CPanel2D> minimap;
    
    // Pause menu overlay
    std::shared_ptr<Panorama::CPanel2D> pauseOverlay;
    std::shared_ptr<Panorama::CButton> resumeButton;
    std::shared_ptr<Panorama::CButton> disconnectButton;
};

// ============ InGameState Implementation ============

InGameState::InGameState() 
    : m_hud(std::make_unique<GameHUD>()) {}

InGameState::~InGameState() = default;

// Helper to get shared NetworkClient from GameStateManager
WorldEditor::Network::NetworkClient* InGameState::GetNetworkClient() {
    return m_manager ? m_manager->GetNetworkClient() : nullptr;
}

void InGameState::OnEnter() {
    m_isPaused = false;
    CreateHUD();
    
    // Log world state
    LOG_INFO("InGameState::OnEnter() - gameWorld={}, clientWorld={}, serverWorld={}", 
             m_gameWorld ? "valid" : "null",
             m_clientWorld ? "valid" : "null", 
             m_serverWorld ? "valid" : "null");
    if (m_gameWorld) {
        LOG_INFO("InGameState: gameWorld has {} entities", m_gameWorld->getEntityCount());
    }
    
    // Check if we need to connect to game server (reconnect case)
    if (m_manager && !m_manager->IsConnectedToGameServer()) {
        // Get server target from GameStateManager
        std::string serverIp;
        u16 serverPort;
        m_manager->GetGameServerTarget(serverIp, serverPort);
        
        if (!serverIp.empty() && serverPort != 0) {
            // Get username from AuthClient
            std::string username = "Player";
            if (auto* authClient = m_manager->GetAuthClient()) {
                if (authClient->IsAuthenticated()) {
                    username = authClient->GetUsername();
                }
            }
            
            LOG_INFO("InGameState: Connecting to game server {}:{} (reconnect)", serverIp, serverPort);
            ConsoleLog("Reconnecting to game server...");
            
            if (m_manager->ConnectToGameServer(serverIp, serverPort, username)) {
                LOG_INFO("InGameState: Connection initiated");
            } else {
                LOG_ERROR("InGameState: Failed to connect to game server");
                ConsoleLog("ERROR: Failed to connect to game server");
            }
        }
    }
    
    // Setup callbacks if connected
    if (m_manager && m_manager->IsConnectedToGameServer()) {
        LOG_INFO("InGameState: Using connection to game server");
        ConsoleLog("Connected to game server");
        SetupNetworkCallbacks();
    } else {
        LOG_WARN("InGameState: No active connection to game server!");
        ConsoleLog("WARNING: Not connected to game server");
    }
    
    // Subscribe to game events
    Panorama::GameEvents_Subscribe("Player_HealthChanged", [this](const Panorama::CGameEventData& data) {
        if (m_hud->healthBar) {
            f32 health = data.GetFloat("current", 100);
            f32 maxHealth = data.GetFloat("max", 100);
            m_hud->healthBar->SetValue(health / maxHealth);
            m_hud->healthLabel->SetText(std::to_string((int)health) + "/" + std::to_string((int)maxHealth));
        }
    });
    
    Panorama::GameEvents_Subscribe("Player_ManaChanged", [this](const Panorama::CGameEventData& data) {
        if (m_hud->manaBar) {
            f32 mana = data.GetFloat("current", 100);
            f32 maxMana = data.GetFloat("max", 100);
            m_hud->manaBar->SetValue(mana / maxMana);
            m_hud->manaLabel->SetText(std::to_string((int)mana) + "/" + std::to_string((int)maxMana));
        }
    });
}

void InGameState::SetupNetworkCallbacks() {
    auto* client = GetNetworkClient();
    if (!client) return;
    
    // Setup game-specific callbacks
    // These will override HeroPickState callbacks
    
    // TODO: Add InGame-specific network callbacks
    // - Game state updates
    // - Entity snapshots
    // - Player actions
}

void InGameState::OnExit() {
    // Disconnect from game server when leaving InGame
    if (m_manager) {
        m_manager->DisconnectFromGameServer();
    }
    DestroyHUD();
}

void InGameState::OnPause() {
    m_isPaused = true;
    if (m_hud->pauseOverlay) {
        m_hud->pauseOverlay->SetVisible(true);
    }
}

void InGameState::OnResume() {
    m_isPaused = false;
    if (m_hud->pauseOverlay) {
        m_hud->pauseOverlay->SetVisible(false);
    }
}


void InGameState::CreateHUD() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    f32 screenW = engine.GetScreenWidth();
    f32 screenH = engine.GetScreenHeight();
    
    // HUD Root
    m_hud->root = std::make_shared<Panorama::CPanel2D>("HUDRoot");
    m_hud->root->AddClass("HUDRoot");
    m_hud->root->GetStyle().width = Panorama::Length::Fill();
    m_hud->root->GetStyle().height = Panorama::Length::Fill();
    m_hud->root->SetAttribute("hittest", "false");
    uiRoot->AddChild(m_hud->root);
    
    // Top Bar (full width, semi-transparent)
    m_hud->topBar = std::make_shared<Panorama::CPanel2D>("TopBar");
    m_hud->topBar->AddClass("HUDTopBar");
    m_hud->topBar->GetStyle().width = Panorama::Length::Fill();
    m_hud->topBar->GetStyle().height = Panorama::Length::Px(50);
    m_hud->topBar->GetStyle().backgroundColor = Panorama::Color(0.0f, 0.0f, 0.0f, 0.6f);
    m_hud->root->AddChild(m_hud->topBar);
    
    // Menu Button (top-left corner, Dota 2 style)
    m_hud->menuButton = std::make_shared<Panorama::CButton>("MENU", "MenuButton");
    m_hud->menuButton->AddClass("MenuButton");
    m_hud->menuButton->GetStyle().width = Panorama::Length::Px(80);
    m_hud->menuButton->GetStyle().height = Panorama::Length::Px(36);
    m_hud->menuButton->GetStyle().marginLeft = Panorama::Length::Px(10);
    m_hud->menuButton->GetStyle().marginTop = Panorama::Length::Px(7);
    m_hud->menuButton->GetStyle().backgroundColor = Panorama::Color(0.15f, 0.15f, 0.18f, 0.9f);
    m_hud->menuButton->GetStyle().borderRadius = 4.0f;
    m_hud->menuButton->GetStyle().fontSize = 14.0f;
    m_hud->menuButton->SetOnActivate([this]() {
        // Go to main menu but keep game running (push state)
        if (m_manager) {
            m_manager->SetGameInProgress(true);  // Mark that we have an active game
            m_manager->PushState(EGameState::MainMenu);
        }
    });
    m_hud->topBar->AddChild(m_hud->menuButton);
    
    // Game Time Label - centered at top of screen (Dota 2 style)
    m_hud->gameTimeLabel = std::make_shared<Panorama::CLabel>("00:00", "GameTime");
    m_hud->gameTimeLabel->AddClass("GameTimeLabel");
    m_hud->gameTimeLabel->GetStyle().fontSize = 32.0f;
    m_hud->gameTimeLabel->GetStyle().color = Panorama::Color(1.0f, 0.85f, 0.4f, 1.0f); // Gold color
    m_hud->gameTimeLabel->GetStyle().marginLeft = Panorama::Length::Px((screenW - 80) / 2); // Center horizontally
    m_hud->gameTimeLabel->GetStyle().marginTop = Panorama::Length::Px(8);
    m_hud->topBar->AddChild(m_hud->gameTimeLabel);
    
    // Debug label (right side of top bar)
    auto debugLabel = std::make_shared<Panorama::CLabel>("DEBUG", "DebugInfo");
    debugLabel->GetStyle().fontSize = 14.0f;
    debugLabel->GetStyle().color = Panorama::Color(0.6f, 0.6f, 0.6f, 1.0f);
    debugLabel->GetStyle().marginLeft = Panorama::Length::Px(screenW - 200);
    debugLabel->GetStyle().marginTop = Panorama::Length::Px(16);
    m_hud->topBar->AddChild(debugLabel);
    m_hud->debugLabel = debugLabel;
    
    // Hero HUD (Bottom Left)
    m_hud->heroHUD = std::make_shared<Panorama::CPanel2D>("HeroHUD");
    m_hud->heroHUD->AddClass("HeroHUD");
    m_hud->heroHUD->GetStyle().width = Panorama::Length::Px(300);
    m_hud->heroHUD->GetStyle().height = Panorama::Length::Px(100);
    m_hud->heroHUD->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Left;
    m_hud->heroHUD->GetStyle().verticalAlign = Panorama::VerticalAlign::Bottom;
    m_hud->heroHUD->GetStyle().marginLeft = Panorama::Length::Px(20);
    m_hud->heroHUD->GetStyle().marginBottom = Panorama::Length::Px(20);
    m_hud->heroHUD->GetStyle().backgroundColor = Panorama::Color(0.1f, 0.1f, 0.12f, 0.85f);
    m_hud->heroHUD->GetStyle().borderRadius = 8.0f;
    m_hud->heroHUD->GetStyle().paddingLeft = Panorama::Length::Px(15);
    m_hud->heroHUD->GetStyle().paddingRight = Panorama::Length::Px(15);
    m_hud->heroHUD->GetStyle().paddingTop = Panorama::Length::Px(10);
    m_hud->heroHUD->GetStyle().flowChildren = Panorama::FlowDirection::Down;
    m_hud->root->AddChild(m_hud->heroHUD);
    
    // Health Bar
    auto healthContainer = std::make_shared<Panorama::CPanel2D>("HealthContainer");
    healthContainer->GetStyle().width = Panorama::Length::Fill();
    healthContainer->GetStyle().height = Panorama::Length::Px(30);
    healthContainer->GetStyle().marginBottom = Panorama::Length::Px(8);
    m_hud->heroHUD->AddChild(healthContainer);
    
    m_hud->healthBar = std::make_shared<Panorama::CProgressBar>("HealthBar");
    m_hud->healthBar->AddClass("HealthBar");
    m_hud->healthBar->GetStyle().width = Panorama::Length::Fill();
    m_hud->healthBar->GetStyle().height = Panorama::Length::Px(24);
    m_hud->healthBar->GetStyle().backgroundColor = Panorama::Color(0.3f, 0.1f, 0.1f, 0.9f);
    m_hud->healthBar->GetStyle().borderRadius = 4.0f;
    m_hud->healthBar->SetValue(0.8f);
    healthContainer->AddChild(m_hud->healthBar);
    
    m_hud->healthLabel = std::make_shared<Panorama::CLabel>("800/1000", "HealthLabel");
    m_hud->healthLabel->GetStyle().fontSize = 14.0f;
    m_hud->healthLabel->GetStyle().color = Panorama::Color::White();
    m_hud->healthLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_hud->healthLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    healthContainer->AddChild(m_hud->healthLabel);
    
    // Mana Bar
    auto manaContainer = std::make_shared<Panorama::CPanel2D>("ManaContainer");
    manaContainer->GetStyle().width = Panorama::Length::Fill();
    manaContainer->GetStyle().height = Panorama::Length::Px(24);
    m_hud->heroHUD->AddChild(manaContainer);
    
    m_hud->manaBar = std::make_shared<Panorama::CProgressBar>("ManaBar");
    m_hud->manaBar->AddClass("ManaBar");
    m_hud->manaBar->GetStyle().width = Panorama::Length::Fill();
    m_hud->manaBar->GetStyle().height = Panorama::Length::Px(20);
    m_hud->manaBar->GetStyle().backgroundColor = Panorama::Color(0.1f, 0.1f, 0.3f, 0.9f);
    m_hud->manaBar->GetStyle().borderRadius = 4.0f;
    m_hud->manaBar->SetValue(0.6f);
    manaContainer->AddChild(m_hud->manaBar);
    
    m_hud->manaLabel = std::make_shared<Panorama::CLabel>("300/500", "ManaLabel");
    m_hud->manaLabel->GetStyle().fontSize = 12.0f;
    m_hud->manaLabel->GetStyle().color = Panorama::Color::White();
    m_hud->manaLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_hud->manaLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    manaContainer->AddChild(m_hud->manaLabel);
    
    // Ability Bar (Bottom Center)
    m_hud->abilityBar = std::make_shared<Panorama::CPanel2D>("AbilityBar");
    m_hud->abilityBar->AddClass("AbilityBar");
    m_hud->abilityBar->GetStyle().width = Panorama::Length::Px(320);
    m_hud->abilityBar->GetStyle().height = Panorama::Length::Px(80);
    m_hud->abilityBar->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_hud->abilityBar->GetStyle().verticalAlign = Panorama::VerticalAlign::Bottom;
    m_hud->abilityBar->GetStyle().marginBottom = Panorama::Length::Px(20);
    m_hud->abilityBar->GetStyle().backgroundColor = Panorama::Color(0.1f, 0.1f, 0.12f, 0.85f);
    m_hud->abilityBar->GetStyle().borderRadius = 8.0f;
    m_hud->abilityBar->GetStyle().flowChildren = Panorama::FlowDirection::Right;
    m_hud->abilityBar->GetStyle().paddingLeft = Panorama::Length::Px(10);
    m_hud->abilityBar->GetStyle().paddingTop = Panorama::Length::Px(10);
    m_hud->root->AddChild(m_hud->abilityBar);
    
    const char* hotkeys[] = {"Q", "W", "E", "R"};
    for (int i = 0; i < 4; i++) {
        auto slot = std::make_shared<Panorama::CPanel2D>("AbilitySlot" + std::to_string(i));
        slot->AddClass("AbilitySlot");
        slot->GetStyle().width = Panorama::Length::Px(60);
        slot->GetStyle().height = Panorama::Length::Px(60);
        slot->GetStyle().marginRight = Panorama::Length::Px(10);
        slot->GetStyle().backgroundColor = Panorama::Color(0.2f, 0.2f, 0.25f, 0.9f);
        slot->GetStyle().borderRadius = 6.0f;
        slot->GetStyle().borderWidth = 2.0f;
        slot->GetStyle().borderColor = Panorama::Color(0.4f, 0.4f, 0.45f, 0.8f);
        
        auto hotkeyLabel = std::make_shared<Panorama::CLabel>(hotkeys[i]);
        hotkeyLabel->GetStyle().fontSize = 12.0f;
        hotkeyLabel->GetStyle().color = Panorama::Color(0.7f, 0.7f, 0.7f, 1.0f);
        hotkeyLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Right;
        hotkeyLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Bottom;
        hotkeyLabel->GetStyle().marginRight = Panorama::Length::Px(4);
        hotkeyLabel->GetStyle().marginBottom = Panorama::Length::Px(2);
        slot->AddChild(hotkeyLabel);
        
        m_hud->abilityBar->AddChild(slot);
    }
    
    // Minimap (Bottom Right)
    m_hud->minimap = std::make_shared<Panorama::CPanel2D>("Minimap");
    m_hud->minimap->AddClass("Minimap");
    m_hud->minimap->GetStyle().width = Panorama::Length::Px(220);
    m_hud->minimap->GetStyle().height = Panorama::Length::Px(220);
    m_hud->minimap->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Right;
    m_hud->minimap->GetStyle().verticalAlign = Panorama::VerticalAlign::Bottom;
    m_hud->minimap->GetStyle().marginRight = Panorama::Length::Px(20);
    m_hud->minimap->GetStyle().marginBottom = Panorama::Length::Px(20);
    m_hud->minimap->GetStyle().backgroundColor = Panorama::Color(0.05f, 0.08f, 0.05f, 0.9f);
    m_hud->minimap->GetStyle().borderRadius = 4.0f;
    m_hud->minimap->GetStyle().borderWidth = 2.0f;
    m_hud->minimap->GetStyle().borderColor = Panorama::Color(0.3f, 0.35f, 0.3f, 0.8f);
    m_hud->root->AddChild(m_hud->minimap);
    
    // Pause Overlay
    m_hud->pauseOverlay = std::make_shared<Panorama::CPanel2D>("PauseOverlay");
    m_hud->pauseOverlay->AddClass("PauseOverlay");
    m_hud->pauseOverlay->GetStyle().width = Panorama::Length::Fill();
    m_hud->pauseOverlay->GetStyle().height = Panorama::Length::Fill();
    m_hud->pauseOverlay->GetStyle().backgroundColor = Panorama::Color(0.0f, 0.0f, 0.0f, 0.7f);
    m_hud->pauseOverlay->SetVisible(false);
    m_hud->root->AddChild(m_hud->pauseOverlay);
    
    auto pauseMenu = std::make_shared<Panorama::CPanel2D>("PauseMenu");
    pauseMenu->GetStyle().width = Panorama::Length::Px(300);
    pauseMenu->GetStyle().height = Panorama::Length::FitChildren();
    pauseMenu->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    pauseMenu->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    pauseMenu->GetStyle().backgroundColor = Panorama::Color(0.1f, 0.1f, 0.12f, 0.95f);
    pauseMenu->GetStyle().borderRadius = 12.0f;
    pauseMenu->GetStyle().paddingTop = Panorama::Length::Px(30);
    pauseMenu->GetStyle().paddingBottom = Panorama::Length::Px(30);
    pauseMenu->GetStyle().paddingLeft = Panorama::Length::Px(30);
    pauseMenu->GetStyle().paddingRight = Panorama::Length::Px(30);
    pauseMenu->GetStyle().flowChildren = Panorama::FlowDirection::Down;
    m_hud->pauseOverlay->AddChild(pauseMenu);
    
    auto pauseTitle = std::make_shared<Panorama::CLabel>("PAUSED", "PauseTitle");
    pauseTitle->GetStyle().fontSize = 32.0f;
    pauseTitle->GetStyle().color = Panorama::Color::White();
    pauseTitle->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    pauseTitle->GetStyle().marginBottom = Panorama::Length::Px(30);
    pauseMenu->AddChild(pauseTitle);
    
    m_hud->resumeButton = std::make_shared<Panorama::CButton>("RESUME", "ResumeButton");
    m_hud->resumeButton->GetStyle().width = Panorama::Length::Fill();
    m_hud->resumeButton->GetStyle().height = Panorama::Length::Px(50);
    m_hud->resumeButton->GetStyle().marginBottom = Panorama::Length::Px(15);
    m_hud->resumeButton->GetStyle().backgroundColor = Panorama::Color(0.15f, 0.4f, 0.15f, 0.9f);
    m_hud->resumeButton->GetStyle().borderRadius = 6.0f;
    m_hud->resumeButton->SetOnActivate([this]() {
        m_isPaused = false;
        m_hud->pauseOverlay->SetVisible(false);
    });
    pauseMenu->AddChild(m_hud->resumeButton);
    
    m_hud->disconnectButton = std::make_shared<Panorama::CButton>("DISCONNECT", "DisconnectButton");
    m_hud->disconnectButton->GetStyle().width = Panorama::Length::Fill();
    m_hud->disconnectButton->GetStyle().height = Panorama::Length::Px(50);
    m_hud->disconnectButton->GetStyle().backgroundColor = Panorama::Color(0.4f, 0.15f, 0.15f, 0.9f);
    m_hud->disconnectButton->GetStyle().borderRadius = 6.0f;
    m_hud->disconnectButton->SetOnActivate([this]() { OnDisconnect(); });
    pauseMenu->AddChild(m_hud->disconnectButton);
}

void InGameState::DestroyHUD() {
    if (m_hud->root) {
        auto& engine = Panorama::CUIEngine::Instance();
        if (auto* uiRoot = engine.GetRoot()) {
            uiRoot->RemoveChild(m_hud->root.get());
        }
    }
    
    m_hud->root.reset();
    m_hud->topBar.reset();
    m_hud->gameTimeLabel.reset();
    m_hud->heroHUD.reset();
    m_hud->healthBar.reset();
    m_hud->manaBar.reset();
    m_hud->healthLabel.reset();
    m_hud->manaLabel.reset();
    m_hud->abilityBar.reset();
    m_hud->minimap.reset();
    m_hud->pauseOverlay.reset();
    m_hud->resumeButton.reset();
    m_hud->disconnectButton.reset();
}


void InGameState::Update(f32 deltaTime) {
    if (m_isPaused) {
        auto& engine = Panorama::CUIEngine::Instance();
        engine.Update(deltaTime);
        return;
    }
    
    // Update shared network client
    UpdateNetwork(deltaTime);
    
    // Update client world (prediction/interpolation)
    if (m_clientWorld) {
        m_clientWorld->update(deltaTime);
    }
    
    UpdateHUDFromGameState();
    
    // Update game time display - always from server
    f32 gameTime = 0.0f;
    if (auto* client = GetNetworkClient()) {
        gameTime = client->getServerGameTime();
    }
    
    int minutes = (int)(gameTime / 60);
    int seconds = (int)gameTime % 60;
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", minutes, seconds);
    if (m_hud->gameTimeLabel) {
        m_hud->gameTimeLabel->SetText(timeStr);
    }
    
    auto& engine = Panorama::CUIEngine::Instance();
    engine.Update(deltaTime);
}

void InGameState::SetWorlds(std::unique_ptr<WorldEditor::ClientWorld> client,
                            std::unique_ptr<WorldEditor::ServerWorld> server) {
    m_clientWorld = std::move(client);
    m_serverWorld = std::move(server);
}

void InGameState::SetWorlds(std::unique_ptr<WorldEditor::ClientWorld> client,
                            std::unique_ptr<WorldEditor::ServerWorld> server,
                            std::unique_ptr<WorldEditor::World> gameWorld) {
    m_clientWorld = std::move(client);
    m_serverWorld = std::move(server);
    m_gameWorld = std::move(gameWorld);
}

void InGameState::UpdateHUDFromGameState() {
    if (m_hud->debugLabel) {
        size_t entityCount = m_clientWorld ? m_clientWorld->getEntityCount() : 0;
        bool connected = m_manager && m_manager->IsConnectedToGameServer();
        std::string debugText = "Entities: " + std::to_string(entityCount);
        debugText += connected ? " | Online" : " | Local";
        m_hud->debugLabel->SetText(debugText);
    }
    
    // Game time is updated in Update() method
}

void InGameState::Render() {
    RenderWorld();
    RenderHUD();
}

void InGameState::RenderWorld() {
    if (!m_gameWorld || !g_renderer) {
        return;
    }
    
    auto* commandList = g_renderer->GetCommandList();
    if (!commandList) return;
    
    // Update lighting (like editor does)
    static float totalTime = 0.0f;
    totalTime += 0.016f; // ~60fps approximation
    
    // Use EditorCamera directly (same as editor game mode)
    static WorldEditor::EditorCamera camera;
    static bool cameraInitialized = false;
    
    if (!cameraInitialized) {
        // Setup Dota-style camera (same as editor game mode)
        camera.yawDeg = -45.0f;
        camera.pitchDeg = -45.0f;
        camera.fovDeg = 60.0f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 50000.0f;
        camera.orthographic = false;
        camera.lockTopDown = false;
        
        // Find focus point (same logic as editor game mode)
        Vec3 focusPoint(150.0f, 0.0f, 150.0f);  // Default fallback
        float cameraHeight = 50.0f;
        
        // Determine player's team from GameStateManager
        bool isRadiant = true;  // Default to Radiant
        if (m_manager) {
            isRadiant = m_manager->IsPlayerRadiant();
            LOG_INFO("InGameState: Player is on {} team (slot {})", 
                     isRadiant ? "Radiant" : "Dire", m_manager->GetPlayerTeamSlot());
        }
        
        // Team ID: Radiant = 1, Dire = 2
        int playerTeamId = isRadiant ? 1 : 2;
        
        // Try to center on terrain first
        auto& reg = m_gameWorld->getEntityManager().getRegistry();
        auto viewT = reg.view<WorldEditor::TerrainComponent>();
        if (auto it = viewT.begin(); it != viewT.end()) {
            const auto& t = viewT.get<WorldEditor::TerrainComponent>(*it);
            focusPoint = Vec3(t.size * 0.5f, 0.0f, t.size * 0.5f);
        }
        
        // Find player's team base
        auto viewBases = reg.view<WorldEditor::ObjectComponent, WorldEditor::TransformComponent>();
        for (auto e : viewBases) {
            const auto& obj = viewBases.get<WorldEditor::ObjectComponent>(e);
            if (obj.type == WorldEditor::ObjectType::Base && obj.teamId == playerTeamId) {
                const auto& tr = viewBases.get<WorldEditor::TransformComponent>(e);
                focusPoint = tr.position;
                LOG_INFO("InGameState: Camera focused on {} base at ({}, {}, {})", 
                         isRadiant ? "Radiant" : "Dire", focusPoint.x, focusPoint.y, focusPoint.z);
                break;
            }
        }
        
        // Calculate camera position from focus point (same as editor)
        Vec3 forward = camera.getForwardLH();
        float fy = forward.y;
        float distance = 95.0f;
        if (std::abs(fy) > 0.0001f) {
            distance = (focusPoint.y - cameraHeight) / fy;
            distance = std::clamp(distance, 5.0f, 10000.0f);
        }
        camera.position = focusPoint - forward * distance;
        
        cameraInitialized = true;
    }
    
    float screenW = (float)g_renderer->GetWidth();
    float screenH = (float)g_renderer->GetHeight();
    float aspect = screenW / screenH;
    
    // Update lighting with camera position
    g_renderer->UpdateLighting(camera.position, totalTime);
    
    // Get view-projection matrix from EditorCamera (same method as editor uses)
    Mat4 viewProjMatrix = camera.getViewProjLH_ZO(aspect);
    
    // Render the world (terrain, towers, buildings from scene.json)
    m_gameWorld->render(commandList, viewProjMatrix, camera.position, false);
}

void InGameState::RenderHUD() {
    auto& engine = Panorama::CUIEngine::Instance();
    engine.Render();
}

bool InGameState::OnKeyDown(i32 key) {
    if (key == 27) {  // VK_ESCAPE
        OnEscapePressed();
        return true;
    }
    if (m_isPaused) return false;
    return false;
}

bool InGameState::OnMouseMove(f32 x, f32 y) {
    auto& engine = Panorama::CUIEngine::Instance();
    engine.OnMouseMove(x, y);
    return true;
}

bool InGameState::OnMouseDown(f32 x, f32 y, i32 button) {
    auto& engine = Panorama::CUIEngine::Instance();
    engine.OnMouseDown(x, y, button);
    return true;
}

bool InGameState::OnMouseUp(f32 x, f32 y, i32 button) {
    auto& engine = Panorama::CUIEngine::Instance();
    engine.OnMouseUp(x, y, button);
    return true;
}

void InGameState::OnEscapePressed() {
    m_isPaused = !m_isPaused;
    if (m_hud->pauseOverlay) {
        m_hud->pauseOverlay->SetVisible(m_isPaused);
    }
}

void InGameState::OnDisconnect() {
    Panorama::CGameEventData data;
    Panorama::GameEvents_Fire("Game_Disconnect", data);
    m_manager->ChangeState(EGameState::MainMenu);
}

// ============ Network Methods ============

void InGameState::UpdateNetwork(f32 deltaTime) {
    auto* client = GetNetworkClient();
    if (!client) return;
    
    // Update network client (receive packets)
    client->update(deltaTime);
    
    // Send input to server (30 Hz)
    m_lastInputSendTime += deltaTime;
    const f32 inputSendInterval = 1.0f / 30.0f;
    
    if (m_lastInputSendTime >= inputSendInterval) {
        SendInputToServer();
        m_lastInputSendTime = 0.0f;
    }
    
    // Process snapshots from server
    if (client->hasNewSnapshot()) {
        ProcessServerSnapshot();
        client->clearNewSnapshotFlag();
    }
}

void InGameState::SendInputToServer() {
    auto* client = GetNetworkClient();
    if (!client || !client->isConnected()) return;
    
    WorldEditor::PlayerInput input;
    input.sequenceNumber = m_inputSequence++;
    input.commandType = WorldEditor::InputCommandType::None;
    
    // TODO: Collect actual input from mouse/keyboard
    client->sendInput(input);
}

void InGameState::ProcessServerSnapshot() {
    auto* client = GetNetworkClient();
    if (!client || !client->isConnected()) return;
    
    const auto& snapshot = client->getLatestSnapshot();
    
    if (m_clientWorld) {
        // TODO: Apply snapshot to client world
    }
    
    LOG_DEBUG("Received snapshot: tick={}, entities={}", 
              snapshot.tick, snapshot.entities.size());
}

} // namespace Game
