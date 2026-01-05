/**
 * InGameState - Active gameplay state
 * 
 * Uses shared NetworkClient from GameStateManager for persistent connection.
 * Connection established in HeroPickState persists here.
 * 
 * Uses GameplayController for unified gameplay logic (shared with World Editor).
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
#include "../../world/HeroSystem.h"
#include "../../network/NetworkClient.h"
#include "../../common/GameInput.h"
#include "../../auth/AuthClient.h"
#include "../../renderer/DirectXRenderer.h"
#include "../../gameplay/GameplayController.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

// External renderer from GameMain.cpp
extern DirectXRenderer* g_renderer;
// External window handle
extern HWND g_hWnd;

namespace Game {

// ============ Game HUD Structure ============

struct InGameState::GameHUD {
    std::shared_ptr<Panorama::CPanel2D> root;
    
    // Top bar
    std::shared_ptr<Panorama::CPanel2D> topBar;
    std::shared_ptr<Panorama::CButton> menuButton;
    std::shared_ptr<Panorama::CLabel> gameTimeLabel;
    std::shared_ptr<Panorama::CLabel> debugLabel;
    
    // Hero HUD (bottom left)
    std::shared_ptr<Panorama::CPanel2D> heroHUD;
    std::shared_ptr<Panorama::CProgressBar> healthBar;
    std::shared_ptr<Panorama::CProgressBar> manaBar;
    std::shared_ptr<Panorama::CLabel> healthLabel;
    std::shared_ptr<Panorama::CLabel> manaLabel;
    
    // Selected unit info
    std::shared_ptr<Panorama::CPanel2D> selectedUnitPanel;
    std::shared_ptr<Panorama::CLabel> selectedUnitName;
    std::shared_ptr<Panorama::CProgressBar> selectedUnitHealth;
    
    // Ability bar (bottom center)
    std::shared_ptr<Panorama::CPanel2D> abilityBar;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> abilitySlots;
    
    // Minimap (bottom right)
    std::shared_ptr<Panorama::CPanel2D> minimap;
    
    // Pause menu overlay
    std::shared_ptr<Panorama::CPanel2D> pauseOverlay;
    std::shared_ptr<Panorama::CButton> resumeButton;
    std::shared_ptr<Panorama::CButton> disconnectButton;
};

// ============ InGameState Implementation ============

InGameState::InGameState() 
    : m_hud(std::make_unique<GameHUD>())
    , m_gameplayController(std::make_unique<WorldEditor::GameplayController>()) 
{
    // Configure for RTS-style camera (Dota-like)
    m_gameplayController->setCameraMode(WorldEditor::CameraMode::RTS);
    m_gameplayController->setEdgePanEnabled(true);
    m_gameplayController->setEdgePanSpeed(800.0f);
    m_gameplayController->setEdgePanMargin(20.0f);
}

InGameState::~InGameState() = default;

WorldEditor::Network::NetworkClient* InGameState::GetNetworkClient() {
    return m_manager ? m_manager->GetNetworkClient() : nullptr;
}

void InGameState::OnEnter() {
    m_isPaused = false;
    
    // Initialize GameplayController with world
    if (m_gameWorld) {
        m_gameplayController->setWorld(m_gameWorld.get());
        m_gameplayController->setWindowHandle(g_hWnd);
        m_gameplayController->startGame();
        
        // Try to find or create player hero
        if (auto* heroSystem = dynamic_cast<WorldEditor::HeroSystem*>(m_gameWorld->getSystem("HeroSystem"))) {
            Entity playerHero = heroSystem->getPlayerHero();
            if (playerHero == INVALID_ENTITY) {
                // Determine team from GameStateManager
                bool isRadiant = m_manager ? m_manager->IsPlayerRadiant() : true;
                i32 teamId = isRadiant ? 1 : 2;
                
                // Find spawn position
                Vec3 spawnPos(100.0f, 0.0f, 100.0f);
                auto& reg = m_gameWorld->getEntityManager().getRegistry();
                auto viewBases = reg.view<WorldEditor::ObjectComponent, WorldEditor::TransformComponent>();
                for (auto e : viewBases) {
                    const auto& obj = viewBases.get<WorldEditor::ObjectComponent>(e);
                    if (obj.type == WorldEditor::ObjectType::Base && obj.teamId == teamId) {
                        spawnPos = viewBases.get<WorldEditor::TransformComponent>(e).position;
                        spawnPos += Vec3(20.0f, 0.0f, 20.0f); // Offset from base
                        break;
                    }
                }
                
                playerHero = heroSystem->createHeroByType("Warrior", teamId, spawnPos);
                heroSystem->setPlayerHero(playerHero);
            }
            m_gameplayController->setPlayerHero(playerHero);
            m_gameplayController->focusOnEntity(playerHero);
        }
    }
    
    CreateHUD();
    
    LOG_INFO("InGameState::OnEnter() - gameWorld={}, clientWorld={}, serverWorld={}", 
             m_gameWorld ? "valid" : "null",
             m_clientWorld ? "valid" : "null", 
             m_serverWorld ? "valid" : "null");
    
    // Check if we need to connect to game server (reconnect case)
    if (m_manager && !m_manager->IsConnectedToGameServer()) {
        std::string serverIp;
        u16 serverPort;
        m_manager->GetGameServerTarget(serverIp, serverPort);
        
        if (!serverIp.empty() && serverPort != 0) {
            std::string username = "Player";
            if (auto* authClient = m_manager->GetAuthClient()) {
                if (authClient->IsAuthenticated()) {
                    username = authClient->GetUsername();
                }
            }
            
            LOG_INFO("InGameState: Connecting to game server {}:{}", serverIp, serverPort);
            ConsoleLog("Reconnecting to game server...");
            
            if (m_manager->ConnectToGameServer(serverIp, serverPort, username)) {
                LOG_INFO("InGameState: Connection initiated");
            } else {
                LOG_ERROR("InGameState: Failed to connect to game server");
            }
        }
    }
    
    if (m_manager && m_manager->IsConnectedToGameServer()) {
        LOG_INFO("InGameState: Using connection to game server");
        SetupNetworkCallbacks();
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
    
    // Initialize input state
    m_currentInput = WorldEditor::GameplayInput{};
    if (g_renderer) {
        m_currentInput.screenWidth = (f32)g_renderer->GetWidth();
        m_currentInput.screenHeight = (f32)g_renderer->GetHeight();
        m_currentInput.viewportMax = Vec2(m_currentInput.screenWidth, m_currentInput.screenHeight);
    }
}

void InGameState::SetupNetworkCallbacks() {
    auto* client = GetNetworkClient();
    if (!client) return;
    
    // TODO: Add InGame-specific network callbacks
}

void InGameState::OnExit() {
    if (m_gameplayController) {
        m_gameplayController->stopGame();
    }
    
    if (m_manager) {
        m_manager->DisconnectFromGameServer();
    }
    DestroyHUD();
}

void InGameState::OnPause() {
    m_isPaused = true;
    if (m_gameplayController) {
        m_gameplayController->pauseGame();
    }
    if (m_hud->pauseOverlay) {
        m_hud->pauseOverlay->SetVisible(true);
    }
}

void InGameState::OnResume() {
    m_isPaused = false;
    if (m_gameplayController) {
        m_gameplayController->resumeGame();
    }
    if (m_hud->pauseOverlay) {
        m_hud->pauseOverlay->SetVisible(false);
    }
}

void InGameState::UpdateInputState() {
    // Update keyboard state
    for (int i = 0; i < 256; i++) {
        m_currentInput.keys[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
    }
    m_currentInput.shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    m_currentInput.ctrlHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    m_currentInput.altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    
    // Mouse position is updated via OnMouseMove
    m_currentInput.mouseInViewport = true; // Full screen game
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
    
    // Top Bar
    m_hud->topBar = std::make_shared<Panorama::CPanel2D>("TopBar");
    m_hud->topBar->AddClass("HUDTopBar");
    m_hud->topBar->GetStyle().width = Panorama::Length::Fill();
    m_hud->topBar->GetStyle().height = Panorama::Length::Px(50);
    m_hud->topBar->GetStyle().backgroundColor = Panorama::Color(0.0f, 0.0f, 0.0f, 0.6f);
    m_hud->root->AddChild(m_hud->topBar);
    
    // Menu Button
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
        if (m_manager) {
            m_manager->SetGameInProgress(true);
            m_manager->PushState(EGameState::MainMenu);
        }
    });
    m_hud->topBar->AddChild(m_hud->menuButton);
    
    // Game Time Label
    m_hud->gameTimeLabel = std::make_shared<Panorama::CLabel>("00:00", "GameTime");
    m_hud->gameTimeLabel->AddClass("GameTimeLabel");
    m_hud->gameTimeLabel->GetStyle().fontSize = 32.0f;
    m_hud->gameTimeLabel->GetStyle().color = Panorama::Color(1.0f, 0.85f, 0.4f, 1.0f);
    m_hud->gameTimeLabel->GetStyle().marginLeft = Panorama::Length::Px((screenW - 80) / 2);
    m_hud->gameTimeLabel->GetStyle().marginTop = Panorama::Length::Px(8);
    m_hud->topBar->AddChild(m_hud->gameTimeLabel);
    
    // Debug label
    m_hud->debugLabel = std::make_shared<Panorama::CLabel>("DEBUG", "DebugInfo");
    m_hud->debugLabel->GetStyle().fontSize = 14.0f;
    m_hud->debugLabel->GetStyle().color = Panorama::Color(0.6f, 0.6f, 0.6f, 1.0f);
    m_hud->debugLabel->GetStyle().marginLeft = Panorama::Length::Px(screenW - 200);
    m_hud->debugLabel->GetStyle().marginTop = Panorama::Length::Px(16);
    m_hud->topBar->AddChild(m_hud->debugLabel);
    
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
    m_hud->healthBar->SetValue(1.0f);
    healthContainer->AddChild(m_hud->healthBar);
    
    m_hud->healthLabel = std::make_shared<Panorama::CLabel>("100/100", "HealthLabel");
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
    m_hud->manaBar->SetValue(1.0f);
    manaContainer->AddChild(m_hud->manaBar);
    
    m_hud->manaLabel = std::make_shared<Panorama::CLabel>("100/100", "ManaLabel");
    m_hud->manaLabel->GetStyle().fontSize = 12.0f;
    m_hud->manaLabel->GetStyle().color = Panorama::Color::White();
    m_hud->manaLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_hud->manaLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    manaContainer->AddChild(m_hud->manaLabel);
    
    // Ability Bar (Bottom Center)
    m_hud->abilityBar = std::make_shared<Panorama::CPanel2D>("AbilityBar");
    m_hud->abilityBar->AddClass("AbilityBar");
    m_hud->abilityBar->GetStyle().width = Panorama::Length::Px(400);
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
    
    const char* hotkeys[] = {"Q", "W", "E", "R", "D", "F"};
    for (int i = 0; i < 6; i++) {
        auto slot = std::make_shared<Panorama::CPanel2D>("AbilitySlot" + std::to_string(i));
        slot->AddClass("AbilitySlot");
        slot->GetStyle().width = Panorama::Length::Px(60);
        slot->GetStyle().height = Panorama::Length::Px(60);
        slot->GetStyle().marginRight = Panorama::Length::Px(5);
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
        m_hud->abilitySlots.push_back(slot);
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
        if (m_gameplayController) m_gameplayController->resumeGame();
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
    m_hud->abilitySlots.clear();
    m_hud->minimap.reset();
    m_hud->pauseOverlay.reset();
    m_hud->resumeButton.reset();
    m_hud->disconnectButton.reset();
}

void InGameState::Update(f32 deltaTime) {
    // Update input state from Windows
    UpdateInputState();
    
    // Update GameplayController (handles camera, input, commands)
    f32 scaledDeltaTime = deltaTime;
    if (m_gameplayController) {
        scaledDeltaTime = m_gameplayController->update(deltaTime, m_currentInput);
    }
    
    // Update network
    UpdateNetwork(deltaTime);
    
    // Update world with scaled time
    if (m_gameWorld && !m_isPaused) {
        m_gameWorld->update(scaledDeltaTime, true);
    }
    
    // Update client world (prediction/interpolation)
    if (m_clientWorld) {
        m_clientWorld->update(scaledDeltaTime);
    }
    
    // Update HUD from game state
    UpdateHUDFromGameState();
    
    // Update UI engine
    auto& engine = Panorama::CUIEngine::Instance();
    engine.Update(deltaTime);
}

void InGameState::UpdateHUDFromGameState() {
    if (!m_gameplayController) return;
    
    // Update game time display
    const auto& stats = m_gameplayController->getStats();
    int minutes = (int)(stats.gameTime / 60);
    int seconds = (int)stats.gameTime % 60;
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", minutes, seconds);
    if (m_hud->gameTimeLabel) {
        m_hud->gameTimeLabel->SetText(timeStr);
    }
    
    // Update debug label
    if (m_hud->debugLabel) {
        size_t entityCount = m_gameWorld ? m_gameWorld->getEntityCount() : 0;
        bool connected = m_manager && m_manager->IsConnectedToGameServer();
        
        const auto& selectedInfo = m_gameplayController->getSelectedUnitInfo();
        std::string debugText = "Entities: " + std::to_string(entityCount);
        debugText += connected ? " | Online" : " | Local";
        if (selectedInfo.entity != INVALID_ENTITY) {
            debugText += " | Sel: " + selectedInfo.name;
        }
        m_hud->debugLabel->SetText(debugText);
    }
    
    // Update player hero health/mana
    Entity playerHero = m_gameplayController->getPlayerHero();
    if (playerHero != INVALID_ENTITY && m_gameWorld) {
        auto& reg = m_gameWorld->getEntityManager().getRegistry();
        if (reg.valid(playerHero) && reg.all_of<WorldEditor::HeroComponent>(playerHero)) {
            const auto& hero = reg.get<WorldEditor::HeroComponent>(playerHero);
            
            if (m_hud->healthBar) {
                f32 hpPct = hero.maxHealth > 0 ? hero.currentHealth / hero.maxHealth : 0;
                m_hud->healthBar->SetValue(hpPct);
                m_hud->healthLabel->SetText(std::to_string((int)hero.currentHealth) + "/" + std::to_string((int)hero.maxHealth));
            }
            if (m_hud->manaBar) {
                f32 mpPct = hero.maxMana > 0 ? hero.currentMana / hero.maxMana : 0;
                m_hud->manaBar->SetValue(mpPct);
                m_hud->manaLabel->SetText(std::to_string((int)hero.currentMana) + "/" + std::to_string((int)hero.maxMana));
            }
        }
    }
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

void InGameState::Render() {
    RenderWorld();
    RenderHealthBars();
    RenderTopBar();
    RenderHUD();
}

void InGameState::RenderWorld() {
    if (!m_gameWorld || !g_renderer || !m_gameplayController) {
        return;
    }
    
    auto* commandList = g_renderer->GetCommandList();
    if (!commandList) return;
    
    static float totalTime = 0.0f;
    totalTime += 0.016f;
    
    float screenW = (float)g_renderer->GetWidth();
    float screenH = (float)g_renderer->GetHeight();
    float aspect = screenW / screenH;
    
    // Get camera from GameplayController
    const auto& camera = m_gameplayController->getCamera();
    
    // Update lighting
    g_renderer->UpdateLighting(camera.position, totalTime);
    
    // Get view-projection matrix
    Mat4 viewProjMatrix = m_gameplayController->getViewProjectionMatrix(aspect);
    
    // Render the world
    m_gameWorld->render(commandList, viewProjMatrix, camera.position, false);
}

void InGameState::RenderHealthBars() {
    if (!m_gameWorld || !m_gameplayController || !g_renderer) return;
    
    auto& engine = Panorama::CUIEngine::Instance();
    auto* renderer = engine.GetRenderer();
    if (!renderer) return;
    
    f32 screenW = (f32)g_renderer->GetWidth();
    f32 screenH = (f32)g_renderer->GetHeight();
    f32 aspect = screenW / screenH;
    
    Mat4 viewProj = m_gameplayController->getViewProjectionMatrix(aspect);
    auto& reg = m_gameWorld->getEntityManager().getRegistry();
    
    auto project = [&](const Vec3& worldPos, Vec2& outScreen) -> bool {
        const Vec4 clip = viewProj * Vec4(worldPos, 1.0f);
        if (clip.w <= 0.0001f || !std::isfinite(clip.w)) return false;
        const Vec3 ndc = Vec3(clip) / clip.w;
        if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || !std::isfinite(ndc.z)) return false;
        if (ndc.z < 0.0f || ndc.z > 1.0f) return false;
        outScreen.x = (ndc.x + 1.0f) * 0.5f * screenW;
        outScreen.y = (1.0f - ndc.y) * 0.5f * screenH;
        return true;
    };
    
    renderer->PushClipRect(Panorama::Rect2D{0, 0, screenW, screenH});
    
    auto heroView = reg.view<WorldEditor::HeroComponent, WorldEditor::TransformComponent>();
    for (auto entity : heroView) {
        const auto& hero = heroView.get<WorldEditor::HeroComponent>(entity);
        const auto& transform = heroView.get<WorldEditor::TransformComponent>(entity);
        
        if (hero.state == WorldEditor::HeroState::Dead) continue;
        
        Vec3 barPos = transform.position + Vec3(0.0f, 4.0f, 0.0f);
        Vec2 screenPos;
        if (!project(barPos, screenPos)) continue;
        
        const f32 barWidth = 80.0f;
        const f32 barHeight = 10.0f;
        
        f32 hpPct = hero.maxHealth > 0 ? std::clamp(hero.currentHealth / hero.maxHealth, 0.0f, 1.0f) : 0;
        
        Panorama::Rect2D hpBgRect{screenPos.x - barWidth * 0.5f, screenPos.y - 25.0f, barWidth, barHeight};
        renderer->DrawRect(hpBgRect, Panorama::Color(0.0f, 0.0f, 0.0f, 0.78f));
        
        Panorama::Rect2D hpFillRect{screenPos.x - barWidth * 0.5f, screenPos.y - 25.0f, barWidth * hpPct, barHeight};
        renderer->DrawRect(hpFillRect, Panorama::Color(1.0f - hpPct, hpPct, 0.0f, 1.0f));
        
        renderer->DrawRectOutline(hpBgRect, Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f), 1.0f);
        
        f32 mpPct = hero.maxMana > 0 ? std::clamp(hero.currentMana / hero.maxMana, 0.0f, 1.0f) : 0;
        Panorama::Rect2D mpBgRect{screenPos.x - barWidth * 0.5f, screenPos.y - 12.0f, barWidth, 6.0f};
        renderer->DrawRect(mpBgRect, Panorama::Color(0.0f, 0.0f, 0.0f, 0.78f));
        
        Panorama::Rect2D mpFillRect{screenPos.x - barWidth * 0.5f, screenPos.y - 12.0f, barWidth * mpPct, 6.0f};
        renderer->DrawRect(mpFillRect, Panorama::Color(0.2f, 0.4f, 0.78f, 1.0f));
        
        char lvlText[16];
        snprintf(lvlText, sizeof(lvlText), "Lv%d", hero.level);
        Panorama::Rect2D lvlRect{screenPos.x - barWidth * 0.5f - 30.0f, screenPos.y - 25.0f, 28.0f, 14.0f};
        Panorama::FontInfo font;
        font.size = 12.0f;
        renderer->DrawText(lvlText, lvlRect, Panorama::Color(1.0f, 0.84f, 0.0f, 1.0f), font);
        
        Panorama::Rect2D nameRect{screenPos.x - 50.0f, screenPos.y - 42.0f, 100.0f, 16.0f};
        renderer->DrawText(hero.heroName, nameRect, Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f), font, 
                          Panorama::HorizontalAlign::Center, Panorama::VerticalAlign::Top);
    }
    
    auto creepView = reg.view<WorldEditor::CreepComponent, WorldEditor::TransformComponent>();
    for (auto entity : creepView) {
        const auto& creep = creepView.get<WorldEditor::CreepComponent>(entity);
        const auto& transform = creepView.get<WorldEditor::TransformComponent>(entity);
        
        if (creep.state == WorldEditor::CreepState::Dead) continue;
        
        Vec3 barPos = transform.position + Vec3(0.0f, 3.0f, 0.0f);
        Vec2 screenPos;
        if (!project(barPos, screenPos)) continue;
        
        const f32 barWidth = 60.0f;
        const f32 barHeight = 8.0f;
        
        f32 hpPct = std::clamp(creep.currentHealth / creep.maxHealth, 0.0f, 1.0f);
        
        Panorama::Rect2D hpBgRect{screenPos.x - barWidth * 0.5f, screenPos.y - barHeight - 2.0f, barWidth, barHeight};
        renderer->DrawRect(hpBgRect, Panorama::Color(0.0f, 0.0f, 0.0f, 0.78f));
        
        Panorama::Rect2D hpFillRect{screenPos.x - barWidth * 0.5f, screenPos.y - barHeight - 2.0f, barWidth * hpPct, barHeight};
        renderer->DrawRect(hpFillRect, Panorama::Color(1.0f - hpPct, hpPct, 0.0f, 1.0f));
        
        renderer->DrawRectOutline(hpBgRect, Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f), 1.0f);
    }
    
    auto objectView = reg.view<WorldEditor::ObjectComponent, WorldEditor::TransformComponent>();
    for (auto entity : objectView) {
        const auto& objComp = objectView.get<WorldEditor::ObjectComponent>(entity);
        const auto& transform = objectView.get<WorldEditor::TransformComponent>(entity);
        
        if (objComp.type != WorldEditor::ObjectType::Tower && 
            objComp.type != WorldEditor::ObjectType::Building && 
            objComp.type != WorldEditor::ObjectType::Base)
            continue;
        
        if (!reg.all_of<WorldEditor::HealthComponent>(entity)) continue;
        const auto& health = reg.get<WorldEditor::HealthComponent>(entity);
        if (health.isDead) continue;
        
        Vec3 barPos = transform.position + Vec3(0.0f, 8.0f, 0.0f);
        Vec2 screenPos;
        if (!project(barPos, screenPos)) continue;
        
        const f32 barWidth = 80.0f;
        const f32 barHeight = 10.0f;
        
        f32 hpPct = std::clamp(health.currentHealth / health.maxHealth, 0.0f, 1.0f);
        
        Panorama::Rect2D hpBgRect{screenPos.x - barWidth * 0.5f, screenPos.y - barHeight - 2.0f, barWidth, barHeight};
        renderer->DrawRect(hpBgRect, Panorama::Color(0.0f, 0.0f, 0.0f, 0.78f));
        
        Panorama::Rect2D hpFillRect{screenPos.x - barWidth * 0.5f, screenPos.y - barHeight - 2.0f, barWidth * hpPct, barHeight};
        renderer->DrawRect(hpFillRect, Panorama::Color(1.0f - hpPct, hpPct, 0.0f, 1.0f));
        
        renderer->DrawRectOutline(hpBgRect, Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f), 1.0f);
        
        char hpText[32];
        snprintf(hpText, sizeof(hpText), "%.0f/%.0f", health.currentHealth, health.maxHealth);
        Panorama::Rect2D hpTextRect{screenPos.x - 50.0f, screenPos.y - barHeight - 18.0f, 100.0f, 14.0f};
        Panorama::FontInfo font;
        font.size = 12.0f;
        renderer->DrawText(hpText, hpTextRect, Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f), font, 
                          Panorama::HorizontalAlign::Center, Panorama::VerticalAlign::Top);
    }
    
    renderer->PopClipRect();
}

void InGameState::RenderTopBar() {
    if (!m_gameWorld || !m_gameplayController) return;
    
    auto& engine = Panorama::CUIEngine::Instance();
    auto* renderer = engine.GetRenderer();
    if (!renderer) return;
    
    f32 screenW = renderer->GetScreenWidth();
    f32 screenH = renderer->GetScreenHeight();
    
    const f32 barHeight = 45.0f;
    const f32 portraitSize = 38.0f;
    const f32 portraitSpacing = 5.0f;
    const f32 timeBoxWidth = 80.0f;
    const i32 slotsPerTeam = 5;
    
    const f32 centerX = screenW * 0.5f;
    const f32 topY = 5.0f;
    
    Panorama::Rect2D barRect{0.0f, topY, screenW, barHeight};
    renderer->DrawRect(barRect, Panorama::Color(0.08f, 0.08f, 0.1f, 0.86f));
    renderer->DrawLine(0.0f, topY + barHeight, screenW, topY + barHeight, Panorama::Color(0.24f, 0.24f, 0.27f, 1.0f), 2.0f);
    
    const auto& stats = m_gameplayController->getStats();
    i32 minutes = static_cast<i32>(stats.gameTime) / 60;
    i32 seconds = static_cast<i32>(stats.gameTime) % 60;
    char timeText[16];
    snprintf(timeText, sizeof(timeText), "%02d:%02d", minutes, seconds);
    
    Panorama::Rect2D timeBoxRect{centerX - timeBoxWidth * 0.5f, topY + 3.0f, timeBoxWidth, barHeight - 6.0f};
    renderer->DrawRect(timeBoxRect, Panorama::Color(0.16f, 0.16f, 0.2f, 1.0f));
    renderer->DrawRectOutline(timeBoxRect, Panorama::Color(0.31f, 0.31f, 0.39f, 1.0f), 2.0f);
    
    Panorama::FontInfo timeFont;
    timeFont.size = 18.0f;
    renderer->DrawText(timeText, timeBoxRect, Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f), timeFont,
                      Panorama::HorizontalAlign::Center, Panorama::VerticalAlign::Center);
    
    auto& reg = m_gameWorld->getEntityManager().getRegistry();
    auto heroView = reg.view<WorldEditor::HeroComponent, WorldEditor::TransformComponent>();
    
    std::vector<entt::entity> radiantHeroes;
    std::vector<entt::entity> direHeroes;
    
    for (auto entity : heroView) {
        const auto& hero = heroView.get<WorldEditor::HeroComponent>(entity);
        if (hero.teamId == 1) {
            radiantHeroes.push_back(entity);
        } else if (hero.teamId == 2) {
            direHeroes.push_back(entity);
        }
    }
    
    f32 radiantStartX = centerX - timeBoxWidth * 0.5f - 20.0f - (slotsPerTeam * (portraitSize + portraitSpacing));
    
    Panorama::FontInfo heroFont;
    heroFont.size = 16.0f;
    
    for (i32 i = 0; i < slotsPerTeam; i++) {
        f32 slotX = radiantStartX + i * (portraitSize + portraitSpacing);
        f32 slotY = topY + (barHeight - portraitSize) * 0.5f;
        
        Panorama::Rect2D slotRect{slotX, slotY, portraitSize, portraitSize};
        
        if (i < static_cast<i32>(radiantHeroes.size())) {
            entt::entity heroEntity = radiantHeroes[i];
            const auto& hero = reg.get<WorldEditor::HeroComponent>(heroEntity);
            bool isDead = (hero.state == WorldEditor::HeroState::Dead);
            
            Panorama::Color bgColor = isDead ? Panorama::Color(0.16f, 0.16f, 0.16f, 1.0f) 
                                              : Panorama::Color(0.12f, 0.31f, 0.12f, 1.0f);
            renderer->DrawRect(slotRect, bgColor);
            
            f32 hpPct = hero.maxHealth > 0 ? std::clamp(hero.currentHealth / hero.maxHealth, 0.0f, 1.0f) : 0;
            Panorama::Rect2D hpRect{slotX, slotY + portraitSize - 4.0f, portraitSize, 4.0f};
            renderer->DrawRect(hpRect, Panorama::Color(0.0f, 0.0f, 0.0f, 0.78f));
            Panorama::Rect2D hpFillRect{slotX, slotY + portraitSize - 4.0f, portraitSize * hpPct, 4.0f};
            renderer->DrawRect(hpFillRect, Panorama::Color(0.2f, 0.78f, 0.2f, 1.0f));
            
            char heroInitial[4];
            snprintf(heroInitial, sizeof(heroInitial), "%c", hero.heroName.empty() ? '?' : hero.heroName[0]);
            Panorama::Rect2D initialRect{slotX, slotY, portraitSize, portraitSize - 4.0f};
            Panorama::Color textColor = isDead ? Panorama::Color(0.39f, 0.39f, 0.39f, 1.0f) 
                                                : Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f);
            renderer->DrawText(heroInitial, initialRect, textColor, heroFont,
                              Panorama::HorizontalAlign::Center, Panorama::VerticalAlign::Center);
            
            if (isDead && hero.respawnTimer > 0) {
                renderer->DrawRect(slotRect, Panorama::Color(0.0f, 0.0f, 0.0f, 0.7f));
                char timerText[8];
                snprintf(timerText, sizeof(timerText), "%.0f", hero.respawnTimer);
                renderer->DrawText(timerText, slotRect, Panorama::Color(1.0f, 0.31f, 0.31f, 1.0f), heroFont,
                                  Panorama::HorizontalAlign::Center, Panorama::VerticalAlign::Center);
            }
            
            renderer->DrawRectOutline(slotRect, Panorama::Color(0.2f, 0.59f, 0.2f, 1.0f), 2.0f);
        } else {
            renderer->DrawRect(slotRect, Panorama::Color(0.12f, 0.14f, 0.12f, 0.78f));
            renderer->DrawRectOutline(slotRect, Panorama::Color(0.2f, 0.24f, 0.2f, 0.59f), 1.0f);
        }
    }
    
    f32 direStartX = centerX + timeBoxWidth * 0.5f + 20.0f;
    
    for (i32 i = 0; i < slotsPerTeam; i++) {
        f32 slotX = direStartX + i * (portraitSize + portraitSpacing);
        f32 slotY = topY + (barHeight - portraitSize) * 0.5f;
        
        Panorama::Rect2D slotRect{slotX, slotY, portraitSize, portraitSize};
        
        if (i < static_cast<i32>(direHeroes.size())) {
            entt::entity heroEntity = direHeroes[i];
            const auto& hero = reg.get<WorldEditor::HeroComponent>(heroEntity);
            bool isDead = (hero.state == WorldEditor::HeroState::Dead);
            
            Panorama::Color bgColor = isDead ? Panorama::Color(0.16f, 0.16f, 0.16f, 1.0f) 
                                              : Panorama::Color(0.31f, 0.12f, 0.12f, 1.0f);
            renderer->DrawRect(slotRect, bgColor);
            
            f32 hpPct = hero.maxHealth > 0 ? std::clamp(hero.currentHealth / hero.maxHealth, 0.0f, 1.0f) : 0;
            Panorama::Rect2D hpRect{slotX, slotY + portraitSize - 4.0f, portraitSize, 4.0f};
            renderer->DrawRect(hpRect, Panorama::Color(0.0f, 0.0f, 0.0f, 0.78f));
            Panorama::Rect2D hpFillRect{slotX, slotY + portraitSize - 4.0f, portraitSize * hpPct, 4.0f};
            renderer->DrawRect(hpFillRect, Panorama::Color(0.78f, 0.2f, 0.2f, 1.0f));
            
            char heroInitial[4];
            snprintf(heroInitial, sizeof(heroInitial), "%c", hero.heroName.empty() ? '?' : hero.heroName[0]);
            Panorama::Rect2D initialRect{slotX, slotY, portraitSize, portraitSize - 4.0f};
            Panorama::Color textColor = isDead ? Panorama::Color(0.39f, 0.39f, 0.39f, 1.0f) 
                                                : Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f);
            renderer->DrawText(heroInitial, initialRect, textColor, heroFont,
                              Panorama::HorizontalAlign::Center, Panorama::VerticalAlign::Center);
            
            if (isDead && hero.respawnTimer > 0) {
                renderer->DrawRect(slotRect, Panorama::Color(0.0f, 0.0f, 0.0f, 0.7f));
                char timerText[8];
                snprintf(timerText, sizeof(timerText), "%.0f", hero.respawnTimer);
                renderer->DrawText(timerText, slotRect, Panorama::Color(1.0f, 0.31f, 0.31f, 1.0f), heroFont,
                                  Panorama::HorizontalAlign::Center, Panorama::VerticalAlign::Center);
            }
            
            renderer->DrawRectOutline(slotRect, Panorama::Color(0.59f, 0.2f, 0.2f, 1.0f), 2.0f);
        } else {
            renderer->DrawRect(slotRect, Panorama::Color(0.14f, 0.12f, 0.12f, 0.78f));
            renderer->DrawRectOutline(slotRect, Panorama::Color(0.24f, 0.2f, 0.2f, 0.59f), 1.0f);
        }
    }
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
    
    // Update input state for GameplayController
    if (key >= 0 && key < 256) {
        m_currentInput.keys[key] = true;
    }
    
    if (m_isPaused) return false;
    return false;
}

bool InGameState::OnKeyUp(i32 key) {
    if (key >= 0 && key < 256) {
        m_currentInput.keys[key] = false;
    }
    return false;
}

bool InGameState::OnMouseMove(f32 x, f32 y) {
    // Update input state
    m_currentInput.mouseDelta = Vec2(x - m_currentInput.mousePos.x, y - m_currentInput.mousePos.y);
    m_currentInput.mousePos = Vec2(x, y);
    
    auto& engine = Panorama::CUIEngine::Instance();
    engine.OnMouseMove(x, y);
    return true;
}

bool InGameState::OnMouseDown(f32 x, f32 y, i32 button) {
    m_currentInput.mousePos = Vec2(x, y);
    
    if (button == 0) {
        m_currentInput.leftClick = true;
        m_currentInput.leftHeld = true;
    } else if (button == 1) {
        m_currentInput.rightClick = true;
        m_currentInput.rightHeld = true;
    }
    
    auto& engine = Panorama::CUIEngine::Instance();
    engine.OnMouseDown(x, y, button);
    return true;
}

bool InGameState::OnMouseUp(f32 x, f32 y, i32 button) {
    m_currentInput.mousePos = Vec2(x, y);
    
    if (button == 0) {
        m_currentInput.leftClick = false;
        m_currentInput.leftHeld = false;
    } else if (button == 1) {
        m_currentInput.rightClick = false;
        m_currentInput.rightHeld = false;
    }
    
    auto& engine = Panorama::CUIEngine::Instance();
    engine.OnMouseUp(x, y, button);
    return true;
}

bool InGameState::OnMouseWheel(f32 delta) {
    m_currentInput.scrollDelta = delta;
    return true;
}

void InGameState::OnEscapePressed() {
    m_isPaused = !m_isPaused;
    if (m_gameplayController) {
        if (m_isPaused) {
            m_gameplayController->pauseGame();
        } else {
            m_gameplayController->resumeGame();
        }
    }
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
    
    client->update(deltaTime);
    
    m_lastInputSendTime += deltaTime;
    const f32 inputSendInterval = 1.0f / 30.0f;
    
    if (m_lastInputSendTime >= inputSendInterval) {
        SendInputToServer();
        m_lastInputSendTime = 0.0f;
    }
    
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
    input.timestamp = static_cast<f32>(m_inputSequence) / 30.0f;
    
    if (!m_gameplayController || !m_gameWorld) {
        client->sendInput(input);
        return;
    }
    
    Entity playerHero = m_gameplayController->getPlayerHero();
    if (playerHero == INVALID_ENTITY) {
        client->sendInput(input);
        return;
    }
    
    auto& reg = m_gameWorld->getEntityManager().getRegistry();
    if (!reg.valid(playerHero) || !reg.all_of<WorldEditor::HeroComponent>(playerHero)) {
        client->sendInput(input);
        return;
    }
    
    const auto& hero = reg.get<WorldEditor::HeroComponent>(playerHero);
    const auto& transform = reg.get<WorldEditor::TransformComponent>(playerHero);
    
    input.isShiftQueued = m_currentInput.shiftHeld;
    
    switch (hero.state) {
        case WorldEditor::HeroState::Moving:
            input.commandType = WorldEditor::InputCommandType::Move;
            input.targetPosition = hero.targetPosition;
            input.moveDirection = glm::normalize(hero.targetPosition - transform.position);
            break;
            
        case WorldEditor::HeroState::Attacking:
            if (hero.targetEntity != INVALID_ENTITY) {
                input.commandType = WorldEditor::InputCommandType::AttackTarget;
                input.targetEntityId = m_clientWorld ? 
                    m_clientWorld->getNetworkId(hero.targetEntity) : WorldEditor::INVALID_NETWORK_ID;
            } else {
                input.commandType = WorldEditor::InputCommandType::AttackMove;
                input.targetPosition = hero.targetPosition;
                input.isAttackMove = true;
            }
            break;
            
        case WorldEditor::HeroState::CastingAbility:
            if (hero.currentCastingAbility >= 0) {
                input.commandType = WorldEditor::InputCommandType::CastAbility;
                input.abilityIndex = hero.currentCastingAbility;
                input.abilityTargetPosition = hero.targetPosition;
                if (hero.targetEntity != INVALID_ENTITY) {
                    input.abilityTargetType = WorldEditor::TargetType::Unit;
                    input.abilityTargetEntityId = m_clientWorld ?
                        m_clientWorld->getNetworkId(hero.targetEntity) : WorldEditor::INVALID_NETWORK_ID;
                } else {
                    input.abilityTargetType = WorldEditor::TargetType::Position;
                }
            }
            break;
            
        case WorldEditor::HeroState::Idle:
        case WorldEditor::HeroState::Stunned:
        case WorldEditor::HeroState::Dead:
        default:
            input.commandType = WorldEditor::InputCommandType::None;
            break;
    }
    
    client->sendInput(input);
}

void InGameState::ProcessServerSnapshot() {
    auto* client = GetNetworkClient();
    if (!client || !client->isConnected()) return;
    
    const auto& snapshot = client->getLatestSnapshot();
    
    if (m_clientWorld) {
        m_clientWorld->applySnapshot(snapshot);
        
        Entity localPlayer = m_clientWorld->getLocalPlayer();
        if (localPlayer != INVALID_ENTITY) {
            m_clientWorld->reconcile(snapshot);
        }
    }
    
    if (m_gameWorld) {
        auto& reg = m_gameWorld->getEntityManager().getRegistry();
        
        for (const auto& entitySnapshot : snapshot.entities) {
            Entity entity = INVALID_ENTITY;
            
            if (m_clientWorld) {
                entity = m_clientWorld->getEntityByNetworkId(entitySnapshot.networkId);
            }
            
            if (entity == INVALID_ENTITY) continue;
            if (!reg.valid(entity)) continue;
            
            if (reg.all_of<WorldEditor::TransformComponent>(entity)) {
                auto& transform = reg.get<WorldEditor::TransformComponent>(entity);
                transform.position = entitySnapshot.position;
                transform.rotation = entitySnapshot.rotation;
            }
            
            if (reg.all_of<WorldEditor::HeroComponent>(entity)) {
                auto& hero = reg.get<WorldEditor::HeroComponent>(entity);
                hero.currentHealth = entitySnapshot.health;
                hero.maxHealth = entitySnapshot.maxHealth;
                hero.currentMana = entitySnapshot.mana;
                hero.maxMana = entitySnapshot.maxMana;
                hero.teamId = entitySnapshot.teamId;
            } else if (reg.all_of<WorldEditor::CreepComponent>(entity)) {
                auto& creep = reg.get<WorldEditor::CreepComponent>(entity);
                creep.currentHealth = entitySnapshot.health;
                creep.maxHealth = entitySnapshot.maxHealth;
                creep.teamId = entitySnapshot.teamId;
            } else if (reg.all_of<WorldEditor::HealthComponent>(entity)) {
                auto& health = reg.get<WorldEditor::HealthComponent>(entity);
                health.currentHealth = entitySnapshot.health;
                health.maxHealth = entitySnapshot.maxHealth;
            }
        }
    }
    
    LOG_DEBUG("Applied snapshot: tick={}, entities={}", 
              snapshot.tick, snapshot.entities.size());
}

} // namespace Game
