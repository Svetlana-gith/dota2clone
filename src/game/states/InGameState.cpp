#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/CUIEngine.h"
#include "../ui/panorama/CPanel2D.h"
#include "../ui/panorama/GameEvents.h"
#include "../../client/ClientWorld.h"
#include "../../server/ServerWorld.h"
#include "../../network/NetworkClient.h"
#include "../../common/GameInput.h"

namespace Game {

// ============ Network Client Wrapper ============

class InGameState::NetworkClientWrapper {
public:
    WorldEditor::Network::NetworkClient client;
    bool isMultiplayer = false;
};

// ============ Game HUD Structure ============

struct InGameState::GameHUD {
    std::shared_ptr<Panorama::CPanel2D> root;
    
    // Top bar
    std::shared_ptr<Panorama::CPanel2D> topBar;
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
    : m_hud(std::make_unique<GameHUD>())
    , m_networkClient(std::make_unique<NetworkClientWrapper>()) {}

InGameState::~InGameState() = default;

void InGameState::OnEnter() {
    m_isPaused = false;
    CreateHUD();
    
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

void InGameState::OnExit() {
    DisconnectFromServer();
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
    
    // ============ HUD Root ============
    m_hud->root = std::make_shared<Panorama::CPanel2D>("HUDRoot");
    m_hud->root->AddClass("HUDRoot");
    m_hud->root->GetStyle().width = Panorama::Length::Fill();
    m_hud->root->GetStyle().height = Panorama::Length::Fill();
    m_hud->root->SetAttribute("hittest", "false");  // Allow clicks through to game
    uiRoot->AddChild(m_hud->root);
    
    // ============ Top Bar ============
    m_hud->topBar = std::make_shared<Panorama::CPanel2D>("TopBar");
    m_hud->topBar->AddClass("HUDTopBar");
    m_hud->topBar->GetStyle().width = Panorama::Length::Fill();
    m_hud->topBar->GetStyle().height = Panorama::Length::Px(50);
    m_hud->topBar->GetStyle().backgroundColor = Panorama::Color(0.0f, 0.0f, 0.0f, 0.6f);
    m_hud->topBar->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_hud->root->AddChild(m_hud->topBar);
    
    // Game Time
    m_hud->gameTimeLabel = std::make_shared<Panorama::CLabel>("00:00", "GameTime");
    m_hud->gameTimeLabel->AddClass("GameTimeLabel");
    m_hud->gameTimeLabel->GetStyle().fontSize = 28.0f;
    m_hud->gameTimeLabel->GetStyle().color = Panorama::Color::White();
    m_hud->gameTimeLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_hud->gameTimeLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    m_hud->topBar->AddChild(m_hud->gameTimeLabel);
    
    // Debug info (right side of top bar)
    auto debugLabel = std::make_shared<Panorama::CLabel>("DEBUG", "DebugInfo");
    debugLabel->GetStyle().fontSize = 16.0f;
    debugLabel->GetStyle().color = Panorama::Color(0.7f, 0.7f, 0.7f, 1.0f);
    debugLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Right;
    debugLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    debugLabel->GetStyle().marginRight = Panorama::Length::Px(20);
    m_hud->topBar->AddChild(debugLabel);
    m_hud->debugLabel = debugLabel;
    
    // ============ Hero HUD (Bottom Left) ============
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
    
    // ============ Ability Bar (Bottom Center) ============
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
    
    // Create 4 ability slots (Q, W, E, R)
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
    
    // ============ Minimap (Bottom Right) ============
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
    
    // ============ Pause Overlay ============
    m_hud->pauseOverlay = std::make_shared<Panorama::CPanel2D>("PauseOverlay");
    m_hud->pauseOverlay->AddClass("PauseOverlay");
    m_hud->pauseOverlay->GetStyle().width = Panorama::Length::Fill();
    m_hud->pauseOverlay->GetStyle().height = Panorama::Length::Fill();
    m_hud->pauseOverlay->GetStyle().backgroundColor = Panorama::Color(0.0f, 0.0f, 0.0f, 0.7f);
    m_hud->pauseOverlay->SetVisible(false);
    m_hud->root->AddChild(m_hud->pauseOverlay);
    
    // Pause menu container
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
    
    // Reset all HUD elements
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
        // Only update UI when paused
        auto& engine = Panorama::CUIEngine::Instance();
        engine.Update(deltaTime);
        return;
    }
    
    // Update network (send/receive packets)
    UpdateNetwork(deltaTime);
    
    // Update server world (game logic) - only if not multiplayer
    if (m_serverWorld && !m_networkClient->isMultiplayer) {
        m_serverWorld->update(deltaTime);
    }
    
    // Update client world (prediction/interpolation)
    if (m_clientWorld) {
        m_clientWorld->update(deltaTime);
    }
    
    // Update HUD from game state
    UpdateHUDFromGameState();
    
    // Update game time display
    f32 gameTime = m_serverWorld ? m_serverWorld->getGameTime() : 0.0f;
    int minutes = (int)(gameTime / 60);
    int seconds = (int)gameTime % 60;
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", minutes, seconds);
    if (m_hud->gameTimeLabel) {
        m_hud->gameTimeLabel->SetText(timeStr);
    }
    
    // Update UI
    auto& engine = Panorama::CUIEngine::Instance();
    engine.Update(deltaTime);
}

void InGameState::SetWorlds(std::unique_ptr<WorldEditor::ClientWorld> client,
                            std::unique_ptr<WorldEditor::ServerWorld> server) {
    m_clientWorld = std::move(client);
    m_serverWorld = std::move(server);
}

void InGameState::UpdateHUDFromGameState() {
    // Update debug info
    if (m_hud->debugLabel && m_clientWorld) {
        size_t entityCount = m_clientWorld->getEntityCount();
        bool connected = IsConnectedToServer();
        std::string debugText = "Entities: " + std::to_string(entityCount);
        if (connected) {
            debugText += " | Connected";
        } else if (m_networkClient->isMultiplayer) {
            debugText += " | Connecting...";
        }
        m_hud->debugLabel->SetText(debugText);
    }
    
    // Update game time
    if (m_hud->gameTimeLabel && m_clientWorld) {
        f32 gameTime = m_clientWorld->getGameTime();
        int minutes = (int)gameTime / 60;
        int seconds = (int)gameTime % 60;
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", minutes, seconds);
        m_hud->gameTimeLabel->SetText(timeStr);
    }
    
    // TODO: Get player entity from client world and update HUD
    // For now, just use placeholder values
    
    // Example: Update health/mana from game state
    // if (m_clientWorld) {
    //     auto* player = m_clientWorld->getLocalPlayer();
    //     if (player) {
    //         // Update health
    //         Panorama::CGameEventData healthData;
    //         healthData.SetFloat("current", player->getHealth());
    //         healthData.SetFloat("max", player->getMaxHealth());
    //         Panorama::GameEvents_Fire("Player_HealthChanged", healthData);
    //     }
    // }
}

void InGameState::Render() {
    // Render 3D world first
    RenderWorld();
    
    // Render HUD on top
    RenderHUD();
}

void InGameState::RenderWorld() {
    if (!m_clientWorld) return;
    
    // For now, render game world using Panorama 2D panels
    // This is a temporary solution until we implement proper 3D rendering
    
    // The world is rendered as a top-down 2D view
    // Entities are shown as colored rectangles
    
    // TODO: Create Panorama panels for each entity and position them
    // For now, we just have the HUD which shows entity count
}

void InGameState::RenderHUD() {
    auto& engine = Panorama::CUIEngine::Instance();
    engine.Render();
}

bool InGameState::OnKeyDown(i32 key) {
    // ESC to toggle pause
    if (key == 27) {  // VK_ESCAPE
        OnEscapePressed();
        return true;
    }
    
    if (m_isPaused) return false;
    
    // Ability hotkeys
    // Q, W, E, R would trigger abilities
    
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

void InGameState::ConnectToServer(const char* serverIP, u16 port) {
    if (!m_networkClient) return;
    
    LOG_INFO("Connecting to server {}:{}...", serverIP, port);
    ConsoleLog(std::string("Connecting to ") + serverIP + ":" + std::to_string(port) + "...");
    
    if (m_networkClient->client.connect(serverIP, port)) {
        m_networkClient->isMultiplayer = true;
        m_wasConnected = false;  // Track if we've shown connection message
        LOG_INFO("Connection initiated");
        ConsoleLog("Connection initiated - waiting for server response...");
    } else {
        LOG_ERROR("Failed to initiate connection");
        ConsoleLog("ERROR: Failed to initiate connection!");
    }
}

void InGameState::DisconnectFromServer() {
    if (!m_networkClient) return;
    
    if (m_networkClient->isMultiplayer) {
        LOG_INFO("Disconnecting from server...");
        m_networkClient->client.disconnect();
        m_networkClient->isMultiplayer = false;
    }
}

bool InGameState::IsConnectedToServer() const {
    return m_networkClient && m_networkClient->isMultiplayer && 
           m_networkClient->client.isConnected();
}

void InGameState::UpdateNetwork(f32 deltaTime) {
    if (!m_networkClient || !m_networkClient->isMultiplayer) return;
    
    // Update network client (receive packets)
    m_networkClient->client.update(deltaTime);
    
    // Check if we just connected
    if (!m_wasConnected && IsConnectedToServer()) {
        m_wasConnected = true;
        LOG_INFO("Successfully connected to server!");
        ConsoleLog("✓ Connected to server successfully!");
    }
    
    // Send input to server (30 Hz)
    m_lastInputSendTime += deltaTime;
    const f32 inputSendInterval = 1.0f / 30.0f;  // 30 Hz
    
    if (m_lastInputSendTime >= inputSendInterval) {
        SendInputToServer();
        m_lastInputSendTime = 0.0f;
    }
    
    // Process snapshots from server
    if (m_networkClient->client.hasNewSnapshot()) {
        ProcessServerSnapshot();
        m_networkClient->client.clearNewSnapshotFlag();
    }
}

void InGameState::SendInputToServer() {
    if (!IsConnectedToServer()) return;
    
    // Create input from current game state
    WorldEditor::PlayerInput input;
    input.sequenceNumber = m_inputSequence++;
    input.commandType = WorldEditor::InputCommandType::None;
    
    // TODO: Collect actual input from mouse/keyboard
    // For now, just send empty input to keep connection alive
    
    m_networkClient->client.sendInput(input);
}

void InGameState::ProcessServerSnapshot() {
    if (!IsConnectedToServer()) return;
    
    const auto& snapshot = m_networkClient->client.getLatestSnapshot();
    
    // Update client world with server snapshot
    if (m_clientWorld) {
        // TODO: Apply snapshot to client world
        // m_clientWorld->applySnapshot(snapshot);
    }
    
    // Log for debugging
    LOG_DEBUG("Received snapshot: tick={}, entities={}", 
              snapshot.tick, snapshot.entities.size());
}

} // namespace Game