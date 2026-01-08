#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/core/CUIEngine.h"
#include "../ui/panorama/core/CPanel2D.h"
#include "../ui/panorama/widgets/CLabel.h"
#include "../ui/panorama/widgets/CProgressBar.h"
#include "../ui/panorama/core/GameEvents.h"
#include "../../client/ClientWorld.h"
#include "../../server/ServerWorld.h"
#include "../../world/World.h"
#include "../../world/WorldLegacy.h"
#include "../../world/Components.h"
#include "../../world/CreepSystem.h"
#include "../../world/CreepSpawnSystem.h"
#include "../../world/HeroSystem.h"
#include "../../world/TowerSystem.h"
#include "../../world/ProjectileSystem.h"
#include "../../world/CollisionSystem.h"
#include "../../world/TerrainMesh.h"
#include "../../world/TerrainTools.h"
#include "../../serialization/MapIO.h"
#include "../../renderer/DirectXRenderer.h"

// External renderer from GameMain.cpp
extern DirectXRenderer* g_renderer;

namespace Game {

static void CreateFallbackTerrain(::WorldEditor::World& world) {
    // Create a simple flat terrain so InGame has something visible even when no map JSON exists.
    // Wait for GPU to finish before destroying mesh resources
    if (g_renderer) {
        g_renderer->WaitForGpuIdle();
    }
    world.clearEntities();

    Entity terrainE = world.createEntity("Terrain");
    auto& terrainTransform = world.addComponent<WorldEditor::TransformComponent>(terrainE);
    terrainTransform.position = Vec3(0.0f, 0.0f, 0.0f);
    terrainTransform.rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    terrainTransform.scale = Vec3(1.0f);

    auto& terrain = world.addComponent<WorldEditor::TerrainComponent>(terrainE);
    // Map scale: 256x256 tiles @ 64 units = 16384x16384 units
    // Navigation grid uses 64 unit cells.
    WorldEditor::TerrainTools::initTileTerrain(terrain, 256, 256, 64.0f, 128.0f);
    WorldEditor::TerrainTools::generateHeights(terrain);

    auto& terrainMesh = world.addComponent<WorldEditor::MeshComponent>(terrainE);
    terrainMesh.name = "Terrain";
    WorldEditor::TerrainMesh::buildMesh(terrain, terrainMesh);
    terrainMesh.visible = true;
    terrainMesh.gpuBuffersCreated = false;
    terrainMesh.gpuUploadNeeded = true;
    terrainMesh.gpuConstantBuffersCreated = false;

    // Simple green material.
    Entity matE = world.createEntity("TerrainMaterial");
    auto& mat = world.addComponent<WorldEditor::MaterialComponent>(matE);
    mat.name = "TerrainMaterial";
    mat.baseColor = Vec3(0.20f, 0.55f, 0.20f);
    mat.metallic = 0.0f;
    mat.roughness = 1.0f;
    mat.gpuBufferCreated = false;
    terrainMesh.materialEntity = matE;

    LOG_INFO("LoadingState: Created fallback terrain (entities={})", world.getEntityCount());
    ConsoleLog("Fallback map: generated terrain (no maps/*.json found)");
}

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
    // Note: m_isReconnect is set externally before OnEnter, don't reset it here
    CreateUI();
    LOG_INFO("LoadingState UI created, isReconnect={}", m_isReconnect);
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
        engine.ClearInputStateForSubtree(m_ui->root.get());
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
        m_serverWorld = std::make_unique<::WorldEditor::ServerWorld>();
        
        // Add all game systems
        auto& entityManager = m_serverWorld->getEntityManager();
        
        // Core MOBA systems
        m_serverWorld->addSystem(std::make_unique<::WorldEditor::HeroSystem>(entityManager));
        m_serverWorld->addSystem(std::make_unique<::WorldEditor::CreepSystem>(entityManager));
        m_serverWorld->addSystem(std::make_unique<::WorldEditor::CreepSpawnSystem>(entityManager));
        m_serverWorld->addSystem(std::make_unique<::WorldEditor::TowerSystem>(entityManager));
        m_serverWorld->addSystem(std::make_unique<::WorldEditor::ProjectileSystem>(entityManager));
        m_serverWorld->addSystem(std::make_unique<::WorldEditor::CollisionSystem>(entityManager));
        
        LOG_INFO("LoadingState: All game systems added");
        ConsoleLog("Game systems initialized: Hero, Creep, Tower, Projectile, Collision");
        
        m_progress = 0.2f;
        LOG_INFO("LoadingState: ServerWorld created");
        ConsoleLog("ServerWorld created [20%]");
    }
    
    // === PHASE 2: Initialize Renderer (20-40%) ===
    else if (m_progress >= 0.2f && m_progress < 0.4f && !m_clientWorld) {
        LOG_INFO("LoadingState: Creating ClientWorld...");
        ConsoleLog("Creating ClientWorld...");
        m_statusText = "Initializing renderer...";
        m_clientWorld = std::make_unique<::WorldEditor::ClientWorld>();
        m_progress = 0.4f;
        LOG_INFO("LoadingState: ClientWorld created");
        ConsoleLog("ClientWorld created [40%]");
    }
    
    // === PHASE 3: Load Map from scene.json (40-70%) ===
    else if (m_progress >= 0.4f && m_progress < 0.7f && !m_worldsLoaded) {
        m_statusText = "Loading map: " + (m_mapName.empty() ? "scene" : m_mapName);
        
        // Load map from scene.json into a World object for rendering
        if (g_renderer && g_renderer->GetDevice()) {
            m_gameWorld = std::make_unique<::WorldEditor::World>(g_renderer->GetDevice());
            
            // Connect lighting system to render system (like editor does)
            auto* renderSystem = static_cast<::WorldEditor::RenderSystem*>(m_gameWorld->getSystem("RenderSystem"));
            if (renderSystem) {
                if (g_renderer->GetLightingSystem()) {
                    renderSystem->setLightingSystem(g_renderer->GetLightingSystem());
                    LOG_INFO("LoadingState: LightingSystem connected to RenderSystem");
                }
                if (g_renderer->GetWireframeGrid()) {
                    renderSystem->setWireframeGrid(g_renderer->GetWireframeGrid());
                }
            }
            
            // Try to load the map file (check multiple paths)
            std::string mapName = m_mapName.empty() ? "scene" : m_mapName;
            std::vector<std::string> searchPaths = {
                "maps/" + mapName + ".json",                    // Relative to exe
                "build/bin/Debug/maps/" + mapName + ".json",    // From workspace root
                "../maps/" + mapName + ".json"                  // One level up
            };
            
            // Wait for GPU before MapIO::load clears entities
            if (g_renderer) {
                g_renderer->WaitForGpuIdle();
            }
            
            bool loaded = false;
            for (const auto& mapPath : searchPaths) {
                String loadError;
                if (::WorldEditor::MapIO::load(*m_gameWorld, mapPath, &loadError)) {
                    LOG_INFO("LoadingState: Map loaded from {}", mapPath);
                    ConsoleLog("Map loaded: " + mapPath + " (" + std::to_string(m_gameWorld->getEntityCount()) + " entities)");
                    loaded = true;
                    break;
                }
            }
            
            if (!loaded) {
                LOG_WARN("LoadingState: Failed to load map from any path");
                ConsoleLog("WARNING: Map not found, generating fallback map");
                if (m_gameWorld) {
                    CreateFallbackTerrain(*m_gameWorld);
                }
            }
        } else {
            LOG_WARN("LoadingState: No renderer available for World creation");
        }
        
        // Also create game entities (heroes, dynamic objects)
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
    
    // === Transition when complete ===
    if (IsLoadingComplete() && m_worldsLoaded) {
        LOG_INFO("LoadingState: Loading complete");
        ConsoleLog("Loading complete [100%]");
        
        Panorama::CGameEventData data;
        Panorama::GameEvents_Fire("Loading_Complete", data);
        
        if (m_isReconnect) {
            // Reconnecting to existing game - go directly to InGame
            LOG_INFO("LoadingState: Reconnect mode - transitioning to InGame");
            ConsoleLog("Reconnecting to game...");
            
            auto* inGameState = m_manager->GetInGameState();
            if (inGameState) {
                inGameState->SetWorlds(std::move(m_clientWorld), std::move(m_serverWorld), std::move(m_gameWorld));
            }
            
            m_manager->ChangeState(EGameState::InGame);
        } else {
            // Normal flow - go to HeroPick
            LOG_INFO("LoadingState: Normal mode - transitioning to HeroPick");
            ConsoleLog("Entering hero pick phase...");
            
            auto* heroPickState = m_manager->GetHeroPickState();
            if (heroPickState) {
                heroPickState->SetWorlds(std::move(m_clientWorld), std::move(m_serverWorld));
            }
            
            // Store gameWorld for later transfer to InGameState
            // HeroPickState will need to pass it along
            auto* inGameState = m_manager->GetInGameState();
            if (inGameState && m_gameWorld) {
                inGameState->SetWorlds(nullptr, nullptr, std::move(m_gameWorld));
            }
            
            m_manager->ChangeState(EGameState::HeroPick);
        }
    }
}

void LoadingState::LoadGameWorld() {
    if (!m_serverWorld || !m_clientWorld) return;
    
    LOG_INFO("Creating game world...");
    ConsoleLog("Creating game world...");
    
    auto& entityManager = m_serverWorld->getEntityManager();
    
    // === Create Player Hero ===
    auto heroEntity = m_serverWorld->createEntity("PlayerHero");
    
    WorldEditor::TransformComponent heroTransform;
    heroTransform.position = Vec3(1600.0f, 50.0f, 1600.0f);
    heroTransform.rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    heroTransform.scale = Vec3(1.0f);
    entityManager.addComponent<WorldEditor::TransformComponent>(heroEntity, heroTransform);
    
    WorldEditor::HeroComponent heroComp;
    heroComp.heroName = "TestHero";
    heroComp.level = 1;
    heroComp.experience = 0.0f;
    heroComp.currentHealth = 600.0f;
    heroComp.maxHealth = 600.0f;
    heroComp.currentMana = 300.0f;
    heroComp.maxMana = 300.0f;
    heroComp.damage = 50.0f;
    heroComp.attackSpeed = 100.0f;
    heroComp.moveSpeed = 300.0f;
    heroComp.teamId = 1;  // Radiant
    heroComp.isPlayerControlled = true;
    entityManager.addComponent<WorldEditor::HeroComponent>(heroEntity, heroComp);
    
    LOG_INFO("Created player hero entity {}", static_cast<u64>(heroEntity));
    
    // === Create Towers ===
    // Radiant towers
    CreateTower(Vec3(2200.0f, 0.0f, 1800.0f), 1, "RadiantTower1");
    CreateTower(Vec3(1800.0f, 0.0f, 2200.0f), 1, "RadiantTower2");
    
    // Dire towers
    CreateTower(Vec3(13800.0f, 0.0f, 14200.0f), 2, "DireTower1");
    CreateTower(Vec3(14200.0f, 0.0f, 13800.0f), 2, "DireTower2");
    
    // === Create Test Creeps ===
    CreateCreep(Vec3(3000.0f, 0.0f, 3000.0f), 2, "DireCreep1");
    CreateCreep(Vec3(3200.0f, 0.0f, 3000.0f), 2, "DireCreep2");
    CreateCreep(Vec3(3400.0f, 0.0f, 3000.0f), 2, "DireCreep3");
    
    // Start the game (activates systems, starts creep spawning)
    m_serverWorld->startGame();
    
    LOG_INFO("Game world loaded with {} entities", m_serverWorld->getEntityCount());
    ConsoleLog("World created: " + std::to_string(m_serverWorld->getEntityCount()) + " entities");
}

void LoadingState::CreateTower(const Vec3& pos, int team, const std::string& name) {
    auto& entityManager = m_serverWorld->getEntityManager();
    auto entity = m_serverWorld->createEntity(name);
    
    WorldEditor::TransformComponent transform;
    transform.position = pos;
    transform.rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    transform.scale = Vec3(1.0f);
    entityManager.addComponent<WorldEditor::TransformComponent>(entity, transform);
    
    // Use ObjectComponent with Tower type (as used in the editor)
    WorldEditor::ObjectComponent towerObj;
    towerObj.type = WorldEditor::ObjectType::Tower;
    towerObj.teamId = team;
    towerObj.attackRange = 20.0f;
    towerObj.attackDamage = 150.0f;
    towerObj.attackSpeed = 1.0f;
    entityManager.addComponent<WorldEditor::ObjectComponent>(entity, towerObj);
    
    WorldEditor::HealthComponent health;
    health.maxHealth = 2000.0f;
    health.currentHealth = 2000.0f;
    health.armor = 10.0f;
    entityManager.addComponent<WorldEditor::HealthComponent>(entity, health);
    
    LOG_INFO("Created tower '{}' at ({}, {}, {})", name, pos.x, pos.y, pos.z);
}

void LoadingState::CreateCreep(const Vec3& pos, int team, const std::string& name) {
    auto& entityManager = m_serverWorld->getEntityManager();
    auto entity = m_serverWorld->createEntity(name);
    
    WorldEditor::TransformComponent transform;
    transform.position = pos;
    transform.rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    transform.scale = Vec3(1.0f);
    entityManager.addComponent<WorldEditor::TransformComponent>(entity, transform);
    
    WorldEditor::CreepComponent creep;
    creep.teamId = team;
    creep.maxHealth = 550.0f;
    creep.currentHealth = 550.0f;
    creep.damage = 20.0f;
    creep.attackRange = 5.0f;
    creep.attackSpeed = 1.0f;
    creep.moveSpeed = 5.0f;
    creep.lane = WorldEditor::CreepLane::Middle;
    creep.type = WorldEditor::CreepType::Melee;
    entityManager.addComponent<WorldEditor::CreepComponent>(entity, creep);
    
    LOG_INFO("Created creep '{}' at ({}, {}, {})", name, pos.x, pos.y, pos.z);
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
    
    // Also save to GameStateManager for shared access across states
    if (m_manager) {
        m_manager->SetGameServerTarget(m_serverIp, m_serverPort);
        LOG_INFO("LoadingState: Set game server target to {}:{}", m_serverIp, m_serverPort);
    }
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
