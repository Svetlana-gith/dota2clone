#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/CUIEngine.h"
#include "../ui/panorama/CPanel2D.h"
#include "../ui/panorama/GameEvents.h"
#include "../../client/ClientWorld.h"
#include "../../server/ServerWorld.h"

namespace Game {

// ============ Loading UI Structure ============

struct LoadingState::LoadingUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    std::shared_ptr<Panorama::CPanel2D> background;
    std::shared_ptr<Panorama::CLabel> mapNameLabel;
    std::shared_ptr<Panorama::CLabel> statusLabel;
    std::shared_ptr<Panorama::CProgressBar> progressBar;
    std::shared_ptr<Panorama::CLabel> tipLabel;
    std::shared_ptr<Panorama::CPanel2D> spinnerPanel;
};

// ============ LoadingState Implementation ============

LoadingState::LoadingState() : m_ui(std::make_unique<LoadingUI>()) {}

LoadingState::~LoadingState() = default;

void LoadingState::OnEnter() {
    LOG_INFO("LoadingState::OnEnter()");
    ConsoleLog("=== LOADING GAME ===");
    m_progress = 0.0f;
    m_statusText = "Initializing...";
    m_worldsLoaded = false;
    CreateUI();
    LOG_INFO("LoadingState UI created");
    ConsoleLog("Loading UI created");
}

void LoadingState::OnExit() {
    DestroyUI();
}

void LoadingState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    // ============ Root Loading Panel ============
    m_ui->root = std::make_shared<Panorama::CPanel2D>("LoadingRoot");
    m_ui->root->AddClass("LoadingRoot");
    m_ui->root->GetStyle().width = Panorama::Length::Fill();
    m_ui->root->GetStyle().height = Panorama::Length::Fill();
    uiRoot->AddChild(m_ui->root);
    
    // ============ Background ============
    m_ui->background = std::make_shared<Panorama::CPanel2D>("LoadingBackground");
    m_ui->background->AddClass("LoadingBackground");
    m_ui->background->GetStyle().width = Panorama::Length::Fill();
    m_ui->background->GetStyle().height = Panorama::Length::Fill();
    m_ui->background->GetStyle().backgroundColor = Panorama::Color(0.02f, 0.02f, 0.04f, 1.0f);
    m_ui->root->AddChild(m_ui->background);
    
    // ============ Map Name ============
    m_ui->mapNameLabel = std::make_shared<Panorama::CLabel>(m_mapName, "MapNameLabel");
    m_ui->mapNameLabel->AddClass("LoadingMapName");
    m_ui->mapNameLabel->GetStyle().fontSize = 48.0f;
    m_ui->mapNameLabel->GetStyle().color = Panorama::Color::White();
    m_ui->mapNameLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_ui->mapNameLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    m_ui->mapNameLabel->GetStyle().marginBottom = Panorama::Length::Px(100);
    m_ui->root->AddChild(m_ui->mapNameLabel);
    
    // ============ Progress Bar Container ============
    auto progressContainer = std::make_shared<Panorama::CPanel2D>("ProgressContainer");
    progressContainer->GetStyle().width = Panorama::Length::Px(600);
    progressContainer->GetStyle().height = Panorama::Length::Px(40);
    progressContainer->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    progressContainer->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    progressContainer->GetStyle().marginTop = Panorama::Length::Px(50);
    m_ui->root->AddChild(progressContainer);
    
    // Progress Bar
    m_ui->progressBar = std::make_shared<Panorama::CProgressBar>("LoadingProgressBar");
    m_ui->progressBar->AddClass("LoadingProgressBar");
    m_ui->progressBar->GetStyle().width = Panorama::Length::Fill();
    m_ui->progressBar->GetStyle().height = Panorama::Length::Px(20);
    m_ui->progressBar->GetStyle().backgroundColor = Panorama::Color(0.15f, 0.15f, 0.2f, 0.9f);
    m_ui->progressBar->GetStyle().borderRadius = 10.0f;
    m_ui->progressBar->GetStyle().borderWidth = 1.0f;
    m_ui->progressBar->GetStyle().borderColor = Panorama::Color(0.3f, 0.3f, 0.35f, 0.8f);
    m_ui->progressBar->SetValue(0.0f);
    progressContainer->AddChild(m_ui->progressBar);
    
    // ============ Status Label ============
    m_ui->statusLabel = std::make_shared<Panorama::CLabel>(m_statusText, "StatusLabel");
    m_ui->statusLabel->AddClass("LoadingStatus");
    m_ui->statusLabel->GetStyle().fontSize = 18.0f;
    m_ui->statusLabel->GetStyle().color = Panorama::Color(0.7f, 0.7f, 0.7f, 1.0f);
    m_ui->statusLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_ui->statusLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    m_ui->statusLabel->GetStyle().marginTop = Panorama::Length::Px(120);
    m_ui->root->AddChild(m_ui->statusLabel);
    
    // ============ Loading Tip ============
    m_ui->tipLabel = std::make_shared<Panorama::CLabel>("TIP: Press ESC to open the menu during gameplay", "TipLabel");
    m_ui->tipLabel->AddClass("LoadingTip");
    m_ui->tipLabel->GetStyle().fontSize = 16.0f;
    m_ui->tipLabel->GetStyle().color = Panorama::Color(0.5f, 0.5f, 0.55f, 0.8f);
    m_ui->tipLabel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_ui->tipLabel->GetStyle().verticalAlign = Panorama::VerticalAlign::Bottom;
    m_ui->tipLabel->GetStyle().marginBottom = Panorama::Length::Px(50);
    m_ui->root->AddChild(m_ui->tipLabel);
    
    // ============ Animated Spinner ============
    m_ui->spinnerPanel = std::make_shared<Panorama::CPanel2D>("Spinner");
    m_ui->spinnerPanel->AddClass("LoadingSpinner");
    m_ui->spinnerPanel->GetStyle().width = Panorama::Length::Px(40);
    m_ui->spinnerPanel->GetStyle().height = Panorama::Length::Px(40);
    m_ui->spinnerPanel->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Center;
    m_ui->spinnerPanel->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    m_ui->spinnerPanel->GetStyle().marginTop = Panorama::Length::Px(180);
    m_ui->spinnerPanel->GetStyle().borderWidth = 3.0f;
    m_ui->spinnerPanel->GetStyle().borderColor = Panorama::Color::Gold();
    m_ui->spinnerPanel->GetStyle().borderRadius = 20.0f;
    // Animation would be applied via CSS animation
    m_ui->spinnerPanel->StartAnimation("spin");
    m_ui->root->AddChild(m_ui->spinnerPanel);
}

void LoadingState::DestroyUI() {
    if (m_ui->root) {
        auto& engine = Panorama::CUIEngine::Instance();
        if (auto* uiRoot = engine.GetRoot()) {
            uiRoot->RemoveChild(m_ui->root.get());
        }
    }
    
    m_ui->root.reset();
    m_ui->background.reset();
    m_ui->mapNameLabel.reset();
    m_ui->statusLabel.reset();
    m_ui->progressBar.reset();
    m_ui->tipLabel.reset();
    m_ui->spinnerPanel.reset();
}

void LoadingState::Update(f32 deltaTime) {
    auto& engine = Panorama::CUIEngine::Instance();
    engine.Update(deltaTime);
    
    // Update progress bar
    if (m_ui->progressBar) {
        m_ui->progressBar->SetValue(m_progress);
    }
    
    // Update status text
    if (m_ui->statusLabel) {
        m_ui->statusLabel->SetText(m_statusText);
    }
    
    // === PHASE 1: Initialize Game Logic (0-20%) ===
    if (m_progress < 0.2f && !m_serverWorld) {
        LOG_INFO("LoadingState: Creating ServerWorld...");
        ConsoleLog("Creating ServerWorld...");
        m_statusText = "Initializing game logic...";
        m_serverWorld = std::make_unique<WorldEditor::ServerWorld>();
        m_progress = 0.2f;
        LOG_INFO("LoadingState: ServerWorld created");
        ConsoleLog("ServerWorld created [20%]");
    }
    
    // === PHASE 2: Initialize Renderer (20-40%) ===
    else if (m_progress >= 0.2f && m_progress < 0.4f && !m_clientWorld) {
        LOG_INFO("LoadingState: Creating ClientWorld...");
        ConsoleLog("Creating ClientWorld...");
        m_statusText = "Initializing renderer...";
        m_clientWorld = std::make_unique<WorldEditor::ClientWorld>();
        m_progress = 0.4f;
        LOG_INFO("LoadingState: ClientWorld created");
        ConsoleLog("ClientWorld created [40%]");
    }
    
    // === PHASE 3: Load Map (40-70%) ===
    else if (m_progress >= 0.4f && m_progress < 0.7f && !m_worldsLoaded) {
        m_statusText = "Loading map: " + (m_mapName.empty() ? "Default Map" : m_mapName);
        LoadGameWorld();
        m_worldsLoaded = true;
        m_progress = 0.7f;
    }
    
    // === PHASE 4: Finalize (70-100%) ===
    else if (m_progress >= 0.7f && m_progress < 1.0f) {
        m_statusText = "Starting game...";
        m_progress += deltaTime * 0.5f; // Fast finish
        if (m_progress > 1.0f) m_progress = 1.0f;
    }
    
    // === Transition to InGame when complete ===
    if (IsLoadingComplete() && m_worldsLoaded) {
        LOG_INFO("LoadingState: Loading complete, transitioning to InGame");
        ConsoleLog("Loading complete [100%]");
        
        Panorama::CGameEventData data;
        Panorama::GameEvents_Fire("Loading_Complete", data);
        
        // Transfer worlds to InGameState BEFORE changing state
        auto* inGameState = m_manager->GetInGameState();
        if (inGameState) {
            LOG_INFO("LoadingState: Transferring worlds to InGameState");
            ConsoleLog("Transferring worlds to InGameState...");
            inGameState->SetWorlds(std::move(m_clientWorld), std::move(m_serverWorld));
            
            // Connect to server (provided by matchmaking)
            LOG_INFO("LoadingState: Connecting to server {}:{}", m_serverIp, m_serverPort);
            ConsoleLog("Connecting to server " + m_serverIp + ":" + std::to_string(m_serverPort) + "...");
            inGameState->ConnectToServer(m_serverIp.c_str(), m_serverPort);
        }
        
        // Now change state
        m_manager->ChangeState(EGameState::InGame);
    }
}

void LoadingState::LoadGameWorld() {
    if (!m_serverWorld || !m_clientWorld) return;
    
    // Initialize game world
    // ServerWorld handles game logic (heroes, creeps, towers, etc.)
    // ClientWorld handles rendering and client-side prediction
    
    // Create a simple test terrain
    // TODO: Load from file when map editor is ready
    LOG_INFO("Creating test terrain...");
    
    // Create terrain entity in server world
    auto terrainEntity = m_serverWorld->createEntity("Terrain");
    // TODO: Add TerrainComponent and MeshComponent
    
    // Start the game (creates heroes, spawns creeps, etc.)
    m_serverWorld->startGame();
    
    LOG_INFO("Game world loaded with {} entities", m_serverWorld->getEntityCount());
    
    // TODO: Load map terrain and objects from file
    // TODO: Initialize client-side rendering resources
}

void LoadingState::Render() {
    auto& engine = Panorama::CUIEngine::Instance();
    engine.Render();
}

void LoadingState::SetLoadingTarget(const std::string& mapName) {
    m_mapName = mapName;
    if (m_ui->mapNameLabel) {
        m_ui->mapNameLabel->SetText(mapName);
    }
}

void LoadingState::SetServerTarget(const std::string& serverIp, u16 serverPort) {
    if (!serverIp.empty()) m_serverIp = serverIp;
    if (serverPort != 0) m_serverPort = serverPort;
}

void LoadingState::SetProgress(f32 progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    m_progress = progress;
}

void LoadingState::SetStatusText(const std::string& text) {
    m_statusText = text;
}

} // namespace Game
