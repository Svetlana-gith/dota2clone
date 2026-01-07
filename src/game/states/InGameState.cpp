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
#include "../ui/panorama/core/CUIEngine.h"
#include "../ui/panorama/core/CPanel2D.h"
#include "../ui/panorama/widgets/CLabel.h"
#include "../ui/panorama/widgets/CButton.h"
#include "../ui/panorama/widgets/CProgressBar.h"
#include "../ui/panorama/core/GameEvents.h"
#include "../ui/panorama/hud/CHUDManager.h"
#include "../../client/ClientWorld.h"
#include "../../server/ServerWorld.h"
#include "../../world/World.h"
#include "../../world/Components.h"
#include "../../world/HeroSystem.h"
#include "../../world/MeshGenerators.h"
#include "../../world/WorldLegacy.h"
#include "../../serialization/MapIO.h"
#include "../../network/NetworkClient.h"
#include "../../common/GameInput.h"
#include "../../common/NetworkTypes.h"
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

namespace {
static Vec2 GetMousePosClient(HWND hwnd) {
    POINT p{};
    if (!hwnd) return Vec2(0.0f);
    if (!GetCursorPos(&p)) return Vec2(0.0f);
    if (!ScreenToClient(hwnd, &p)) return Vec2(0.0f);
    return Vec2((f32)p.x, (f32)p.y);
}

static bool IsMouseInClientRect(HWND hwnd, const Vec2& clientPos) {
    if (!hwnd) return false;
    RECT rc{};
    if (!GetClientRect(hwnd, &rc)) return false;
    return clientPos.x >= (f32)rc.left && clientPos.y >= (f32)rc.top &&
           clientPos.x < (f32)rc.right && clientPos.y < (f32)rc.bottom;
}
} // namespace

InGameState::InGameState() 
    : m_hud(std::make_unique<GameHUD>())
    , m_gameplayController(std::make_unique<WorldEditor::GameplayController>()) 
{
    // Configure for RTS-style camera (Dota-like, scaled for 16000x16000 map)
    m_gameplayController->setCameraMode(WorldEditor::CameraMode::RTS);
    m_gameplayController->setEdgePanEnabled(true);
    m_gameplayController->setEdgePanSpeed(4000.0f);  // Scaled for 16000 map
    m_gameplayController->setEdgePanMargin(25.0f);
}

InGameState::~InGameState() = default;

WorldEditor::Network::NetworkClient* InGameState::GetNetworkClient() {
    return m_manager ? m_manager->GetNetworkClient() : nullptr;
}

void InGameState::OnEnter() {
    m_isPaused = false;
    
    // In multiplayer mode, m_gameWorld may not be set (HeroPickState doesn't pass it)
    // Create it here if needed
    if (!m_gameWorld && g_renderer && g_renderer->GetDevice()) {
        LOG_INFO("InGameState: Creating m_gameWorld for multiplayer mode");
        m_gameWorld = std::make_unique<WorldEditor::World>(g_renderer->GetDevice());
        
        // Connect lighting system to render system
        auto* renderSystem = static_cast<WorldEditor::RenderSystem*>(m_gameWorld->getSystem("RenderSystem"));
        if (renderSystem) {
            if (g_renderer->GetLightingSystem()) {
                renderSystem->setLightingSystem(g_renderer->GetLightingSystem());
            }
            if (g_renderer->GetWireframeGrid()) {
                renderSystem->setWireframeGrid(g_renderer->GetWireframeGrid());
            }
        }
        
        // Load map
        std::vector<std::string> searchPaths = {
            "maps/scene.json",
            "build/bin/Debug/maps/scene.json",
            "../maps/scene.json"
        };
        
        g_renderer->WaitForGpuIdle();
        
        bool loaded = false;
        for (const auto& mapPath : searchPaths) {
            String loadError;
            if (WorldEditor::MapIO::load(*m_gameWorld, mapPath, &loadError)) {
                LOG_INFO("InGameState: Map loaded from {}", mapPath);
                loaded = true;
                break;
            }
        }
        
        if (!loaded) {
            LOG_WARN("InGameState: Failed to load map, creating fallback terrain");
            // Create fallback terrain with visible mesh
            auto& em = m_gameWorld->getEntityManager();
            Entity terrain = em.createEntity("FallbackTerrain");
            auto& transform = em.addComponent<WorldEditor::TransformComponent>(terrain);
            transform.position = Vec3(8000.0f, 0.0f, 8000.0f);  // Center of 16000x16000 map
            transform.scale = Vec3(1.0f);
            
            // Create terrain mesh (flat plane 16000x16000)
            auto& mesh = em.addComponent<WorldEditor::MeshComponent>(terrain, "TerrainMesh");
            const f32 halfSize = 8000.0f;
            mesh.vertices = {
                Vec3(-halfSize, 0.0f, -halfSize),
                Vec3( halfSize, 0.0f, -halfSize),
                Vec3( halfSize, 0.0f,  halfSize),
                Vec3(-halfSize, 0.0f,  halfSize)
            };
            mesh.normals = {
                Vec3(0.0f, 1.0f, 0.0f),
                Vec3(0.0f, 1.0f, 0.0f),
                Vec3(0.0f, 1.0f, 0.0f),
                Vec3(0.0f, 1.0f, 0.0f)
            };
            mesh.texCoords = {
                Vec2(0.0f, 0.0f),
                Vec2(1.0f, 0.0f),
                Vec2(1.0f, 1.0f),
                Vec2(0.0f, 1.0f)
            };
            mesh.indices = { 0, 1, 2, 0, 2, 3 };
            mesh.gpuUploadNeeded = true;
            
            // Create terrain material (dark green grass)
            Entity materialEntity = em.createEntity("TerrainMaterial");
            auto& material = em.addComponent<WorldEditor::MaterialComponent>(materialEntity, "TerrainMaterial");
            material.baseColor = Vec3(0.15f, 0.35f, 0.12f);  // Dark green
            material.metallic = 0.0f;
            material.roughness = 0.9f;
            mesh.materialEntity = materialEntity;
            
            LOG_INFO("Created fallback terrain: 16000x16000 flat plane centered at (8000, 0, 8000)");
        }
        
        // Add HeroSystem if not present
        if (!m_gameWorld->getSystem("HeroSystem")) {
            m_gameWorld->addSystem(std::make_unique<WorldEditor::HeroSystem>(m_gameWorld->getEntityManager()));
        }
    }
    
    // Also create m_clientWorld if not set (needed for network ID mapping)
    if (!m_clientWorld) {
        LOG_INFO("InGameState: Creating m_clientWorld for multiplayer mode");
        m_clientWorld = std::make_unique<WorldEditor::ClientWorld>();
    }
    
    // Setup map objects if not already present (bases, towers, barracks, spawns, waypoints)
    if (m_gameWorld) {
        // Check if map objects already exist
        auto& reg = m_gameWorld->getEntityManager().getRegistry();
        auto baseView = reg.view<WorldEditor::ObjectComponent>();
        bool hasMapObjects = false;
        for (auto e : baseView) {
            const auto& obj = baseView.get<WorldEditor::ObjectComponent>(e);
            if (obj.type == WorldEditor::ObjectType::Base) {
                hasMapObjects = true;
                break;
            }
        }
        
        if (!hasMapObjects) {
            SetupMapObjects();
        }
    }
    
    // Check if we're in multiplayer mode (connected to game server)
    bool isMultiplayer = m_manager && m_manager->IsConnectedToGameServer();
    
    // Initialize GameplayController with world
    if (m_gameWorld) {
        m_gameplayController->setWorld(m_gameWorld.get());
        m_gameplayController->setWindowHandle(g_hWnd);
        
        // In multiplayer, don't call startGame() - it creates local heroes
        // Heroes will come from server snapshots
        if (!isMultiplayer) {
            m_gameplayController->startGame();
        } else {
            // Just activate the game without creating heroes
            // Must set both GameplayController and World active to prevent
            // World::update() from calling startGame() which creates default heroes
            m_gameplayController->setGameActive(true);
            m_gameWorld->setGameActive(true);
        }
        
        // Try to find or create player hero
        if (auto* heroSystem = dynamic_cast<WorldEditor::HeroSystem*>(m_gameWorld->getSystem("HeroSystem"))) {
            Entity playerHero = heroSystem->getPlayerHero();
            
            // In multiplayer, don't create hero locally - server will send it via snapshots
            // In local/offline mode, create hero if not exists
            if (playerHero == INVALID_ENTITY && !isMultiplayer) {
                // Determine team from GameStateManager
                bool isRadiant = m_manager ? m_manager->IsPlayerRadiant() : true;
                i32 teamId = isRadiant ? 1 : 2;
                
                // Find hero spawn point for this team (use first available spawn)
                Vec3 spawnPos(1500.0f, 0.0f, 1500.0f);  // Default fallback (Radiant base area)
                if (teamId == 2) {
                    spawnPos = Vec3(14500.0f, 0.0f, 14500.0f);  // Dire base area
                }
                
                auto& reg = m_gameWorld->getEntityManager().getRegistry();
                auto viewSpawns = reg.view<WorldEditor::ObjectComponent, WorldEditor::TransformComponent>();
                for (auto e : viewSpawns) {
                    const auto& obj = viewSpawns.get<WorldEditor::ObjectComponent>(e);
                    if (obj.type == WorldEditor::ObjectType::HeroSpawn && obj.teamId == teamId) {
                        // Use first hero spawn point (index 0)
                        if (obj.spawnLane == 0) {
                            spawnPos = viewSpawns.get<WorldEditor::TransformComponent>(e).position;
                            spawnPos.y += 50.0f;  // Spawn slightly above the platform
                            break;
                        }
                    }
                }
                
                playerHero = heroSystem->createHeroByType(m_selectedHeroType, teamId, spawnPos);
                heroSystem->setPlayerHero(playerHero);
                
                // Assign network ID for multiplayer sync if server world is available
                if (m_serverWorld) {
                    m_serverWorld->assignNetworkId(playerHero);
                    LOG_INFO("Spawned player hero '{}' at ({}, {}, {}) with networkId={}", 
                             m_selectedHeroType, spawnPos.x, spawnPos.y, spawnPos.z,
                             m_serverWorld->getNetworkId(playerHero));
                } else {
                    LOG_INFO("Spawned player hero '{}' at ({}, {}, {})", 
                             m_selectedHeroType, spawnPos.x, spawnPos.y, spawnPos.z);
                }
                
                m_gameplayController->setPlayerHero(playerHero);
                m_gameplayController->focusOnEntity(playerHero);
            } else if (playerHero != INVALID_ENTITY) {
                m_gameplayController->setPlayerHero(playerHero);
                m_gameplayController->focusOnEntity(playerHero);
            } else {
                // Multiplayer mode - hero will be received from server snapshots
                LOG_INFO("Multiplayer mode: waiting for hero from server snapshots");
            }
        }
    }
    
    // Initialize HUD Manager
    auto& hudManager = Panorama::CHUDManager::Instance();
    if (!hudManager.Initialize(&Panorama::CUIEngine::Instance())) {
        LOG_ERROR("Failed to initialize HUD Manager");
    }

    CreateHUD();

    // Inject custom immediate-mode UI draws into the Panorama render pass.
    // NOTE: RenderHealthBars/RenderTopBar must NOT call CUIEngine::Render() themselves.
    Panorama::CUIEngine::Instance().SetCustomRenderCallback([this](Panorama::CUIRenderer* uiRenderer) {
        if (!uiRenderer) return;
        // NOTE: Keep the top bar as a Panorama panel (CreateHUD) to avoid layout conflicts.
        // Only inject simple immediate-mode overlays here.
        if (m_showCrosshair && !m_isPaused) {
            RenderCrosshair(uiRenderer);
        }
        RenderHealthBars();
    });
    
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

    // Sync initial mouse position to avoid "edge pan" kicking in on the first frame.
    // (GameplayInput defaults to (0,0), which is inside the edge-pan margin.)
    m_currentInput.mousePos = GetMousePosClient(g_hWnd);
    m_currentInput.mouseDelta = Vec2(0.0f);
    m_currentInput.scrollDelta = 0.0f;
    m_currentInput.mouseInViewport = IsMouseInClientRect(g_hWnd, m_currentInput.mousePos);
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
    
    // Shutdown HUD Manager
    auto& hudManager = Panorama::CHUDManager::Instance();
    hudManager.Shutdown();
    
    DestroyHUD();

    // Remove custom UI hook to avoid calling into a destroyed state.
    Panorama::CUIEngine::Instance().SetCustomRenderCallback(nullptr);
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
        engine.ClearInputStateForSubtree(m_hud->root.get());
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

    UpdatePerformanceStats(deltaTime);
    
    // Update GameplayController (handles camera, input, commands)
    f32 scaledDeltaTime = deltaTime;
    if (m_gameplayController) {
        scaledDeltaTime = m_gameplayController->update(deltaTime, m_currentInput);
    }

    // Reset per-frame deltas so they don't "stick" if no new events arrive.
    // (Mouse move and wheel are event-driven in this state.)
    m_currentInput.mouseDelta = Vec2(0.0f);
    m_currentInput.scrollDelta = 0.0f;
    
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
    
    // Update HUD Manager
    auto& hudManager = Panorama::CHUDManager::Instance();
    if (hudManager.IsInitialized()) {
        hudManager.Update(deltaTime);
    }
    
    // Update UI engine
    auto& engine = Panorama::CUIEngine::Instance();
    engine.Update(deltaTime);
}

void InGameState::UpdatePerformanceStats(f32 deltaTime) {
    // Basic FPS smoothing (EMA). Keep it robust against large spikes.
    if (deltaTime <= 0.0f) return;
    if (deltaTime > 0.25f) deltaTime = 0.25f;

    const f32 instantFps = 1.0f / deltaTime;
    if (m_fpsEma <= 0.0f) {
        m_fpsEma = instantFps;
        return;
    }
    // 0.1 ~ quick response but not too jittery.
    m_fpsEma = m_fpsEma * 0.9f + instantFps * 0.1f;
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
        std::string debugText = "FPS: " + std::to_string((int)(m_fpsEma + 0.5f));
        debugText += " | Ent: " + std::to_string(entityCount);
        debugText += connected ? " | Online" : " | Local";

        if (auto* net = GetNetworkClient(); net && net->isConnected()) {
            const int pingMs = (int)std::lround(net->getRoundTripTime() * 1000.0f);
            const u32 loss = net->getPacketLoss();
            if (pingMs > 0) {
                debugText += " | Ping: " + std::to_string(pingMs) + "ms";
            }
            if (loss > 0) {
                debugText += " | Loss: " + std::to_string(loss) + "%";
            }
        }
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
    if (client) m_clientWorld = std::move(client);
    if (server) m_serverWorld = std::move(server);
}

void InGameState::SetWorlds(std::unique_ptr<WorldEditor::ClientWorld> client,
                            std::unique_ptr<WorldEditor::ServerWorld> server,
                            std::unique_ptr<WorldEditor::World> gameWorld) {
    if (client) m_clientWorld = std::move(client);
    if (server) m_serverWorld = std::move(server);
    if (gameWorld) m_gameWorld = std::move(gameWorld);
}

void InGameState::Render() {
    RenderWorld();
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

void InGameState::RenderCrosshair(Panorama::CUIRenderer* renderer) {
    if (!renderer) return;

    const f32 w = renderer->GetScreenWidth();
    const f32 h = renderer->GetScreenHeight();
    const f32 cx = w * 0.5f;
    const f32 cy = h * 0.5f;

    // A simple tactical crosshair: small gap + 4 lines + center dot.
    const f32 gap = 3.0f;
    const f32 len = 10.0f;
    const f32 thickness = 2.0f;

    // Slight shadow for readability.
    const Panorama::Color shadow(0.0f, 0.0f, 0.0f, 0.6f);
    const Panorama::Color fg(1.0f, 1.0f, 1.0f, 0.85f);

    // Shadow (offset by 1px).
    renderer->DrawLine(cx - (gap + len) + 1, cy + 1, cx - gap + 1, cy + 1, shadow, thickness);
    renderer->DrawLine(cx + gap + 1, cy + 1, cx + (gap + len) + 1, cy + 1, shadow, thickness);
    renderer->DrawLine(cx + 1, cy - (gap + len) + 1, cx + 1, cy - gap + 1, shadow, thickness);
    renderer->DrawLine(cx + 1, cy + gap + 1, cx + 1, cy + (gap + len) + 1, shadow, thickness);

    // Foreground.
    renderer->DrawLine(cx - (gap + len), cy, cx - gap, cy, fg, thickness);
    renderer->DrawLine(cx + gap, cy, cx + (gap + len), cy, fg, thickness);
    renderer->DrawLine(cx, cy - (gap + len), cx, cy - gap, fg, thickness);
    renderer->DrawLine(cx, cy + gap, cx, cy + (gap + len), fg, thickness);

    renderer->DrawCircle(cx, cy, 1.5f, fg, true);
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
    // Forward to HUD Manager first
    auto& hudManager = Panorama::CHUDManager::Instance();
    if (hudManager.IsInitialized() && hudManager.OnKeyDown(key)) {
        return true;
    }
    
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
    // Forward to HUD Manager first
    auto& hudManager = Panorama::CHUDManager::Instance();
    if (hudManager.IsInitialized() && hudManager.OnKeyUp(key)) {
        return true;
    }
    
    if (key >= 0 && key < 256) {
        m_currentInput.keys[key] = false;
    }
    return false;
}

bool InGameState::OnMouseMove(f32 x, f32 y) {
    // Update input state
    m_currentInput.mouseDelta = Vec2(x - m_currentInput.mousePos.x, y - m_currentInput.mousePos.y);
    m_currentInput.mousePos = Vec2(x, y);
    
    // Forward to HUD Manager first
    auto& hudManager = Panorama::CHUDManager::Instance();
    if (hudManager.IsInitialized() && hudManager.OnMouseMove(x, y)) {
        return true;
    }
    
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
    
    // Forward to HUD Manager first
    auto& hudManager = Panorama::CHUDManager::Instance();
    if (hudManager.IsInitialized() && hudManager.OnMouseDown(x, y, button)) {
        return true;
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
    
    // Forward to HUD Manager first
    auto& hudManager = Panorama::CHUDManager::Instance();
    if (hudManager.IsInitialized() && hudManager.OnMouseUp(x, y, button)) {
        return true;
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
    WorldEditor::ClientId myClientId = client->getClientId();
    
    // Debug: log received snapshot periodically
    static int snapCount = 0;
    if (++snapCount % 300 == 1) {
        LOG_INFO("Client {} received snapshot: tick={}, entities={}", 
                 myClientId, snapshot.tick, snapshot.entities.size());
        for (const auto& e : snapshot.entities) {
            LOG_INFO("  Entity: netId={}, type={}, owner={}, pos=({:.0f},{:.0f},{:.0f})", 
                     e.networkId, e.entityType, e.ownerClientId,
                     e.position.x, e.position.y, e.position.z);
        }
    }
    
    if (m_clientWorld) {
        m_clientWorld->applySnapshot(snapshot);
        
        Entity localPlayer = m_clientWorld->getLocalPlayer();
        if (localPlayer != INVALID_ENTITY) {
            m_clientWorld->reconcile(snapshot);
        }
    }
    
    if (m_gameWorld) {
        auto& reg = m_gameWorld->getEntityManager().getRegistry();
        auto* heroSystem = dynamic_cast<WorldEditor::HeroSystem*>(m_gameWorld->getSystem("HeroSystem"));
        
        for (const auto& entitySnapshot : snapshot.entities) {
            Entity entity = INVALID_ENTITY;
            
            if (m_clientWorld) {
                entity = m_clientWorld->getEntityByNetworkId(entitySnapshot.networkId);
            }
            
            // If entity doesn't exist in gameWorld but exists in clientWorld, 
            // we need to create it in gameWorld for rendering
            if (entity == INVALID_ENTITY) {
                // Create new entity for heroes from snapshot
                if (entitySnapshot.entityType == 1) { // Hero
                    entity = m_gameWorld->getEntityManager().createEntity("NetworkHero");
                    
                    // Add components
                    auto& transform = m_gameWorld->getEntityManager().addComponent<WorldEditor::TransformComponent>(entity);
                    transform.position = entitySnapshot.position;
                    transform.rotation = entitySnapshot.rotation;
                    transform.scale = Vec3(1.0f);
                    
                    // Check if this is our hero using ownerClientId
                    bool isOurHero = (entitySnapshot.ownerClientId == myClientId);
                    
                    auto& hero = m_gameWorld->getEntityManager().addComponent<WorldEditor::HeroComponent>(entity, "NetworkHero", entitySnapshot.teamId);
                    hero.currentHealth = entitySnapshot.health;
                    hero.maxHealth = entitySnapshot.maxHealth;
                    hero.currentMana = entitySnapshot.mana;
                    hero.maxMana = entitySnapshot.maxMana;
                    hero.isPlayerControlled = isOurHero;
                    
                    // Create mesh for hero
                    auto& mesh = m_gameWorld->getEntityManager().addComponent<WorldEditor::MeshComponent>(entity, "HeroMesh");
                    WorldEditor::MeshGenerators::GenerateCylinder(mesh, 50.0f, 120.0f, 16);
                    
                    // Create material with team color
                    Entity materialEntity = m_gameWorld->getEntityManager().createEntity("HeroMaterial");
                    auto& material = m_gameWorld->getEntityManager().addComponent<WorldEditor::MaterialComponent>(materialEntity, "HeroMaterial");
                    if (entitySnapshot.teamId == 1) {
                        material.baseColor = Vec3(0.2f, 0.5f, 1.0f); // Blue for Radiant
                    } else {
                        material.baseColor = Vec3(1.0f, 0.3f, 0.3f); // Red for Dire
                    }
                    material.metallic = 0.3f;
                    material.roughness = 0.6f;
                    mesh.materialEntity = materialEntity;
                    mesh.gpuUploadNeeded = true;
                    
                    // Register in clientWorld for network ID mapping
                    if (m_clientWorld) {
                        m_clientWorld->assignNetworkId(entity, entitySnapshot.networkId);
                    }
                    
                    // If this is our hero, set it as player hero
                    if (isOurHero && heroSystem && heroSystem->getPlayerHero() == INVALID_ENTITY) {
                        heroSystem->setPlayerHero(entity);
                        m_gameplayController->setPlayerHero(entity);
                        m_gameplayController->focusOnEntity(entity);
                        LOG_INFO("Set player hero from snapshot: networkId={}, ownerClientId={}, myClientId={}", 
                                 entitySnapshot.networkId, entitySnapshot.ownerClientId, myClientId);
                    }
                    
                    LOG_INFO("Created hero from snapshot: networkId={}, team={}, owner={}, isOurs={}, pos=({}, {}, {})", 
                             entitySnapshot.networkId, entitySnapshot.teamId, entitySnapshot.ownerClientId, isOurHero,
                             entitySnapshot.position.x, entitySnapshot.position.y, entitySnapshot.position.z);
                }
                continue;
            }
            
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

// ============ Map Object Setup ============

void InGameState::SetupMapObjects() {
    if (!m_gameWorld) {
        LOG_ERROR("InGameState::SetupMapObjects - No game world!");
        return;
    }
    
    LOG_INFO("Setting up map objects for MOBA gameplay...");
    
    // Map size: 16000x16000 units (Dota 2 scale)
    // Radiant base: bottom-left corner (~1500, 1500)
    // Dire base: top-right corner (~14500, 14500)
    
    const f32 MAP_SIZE = 16000.0f;
    const f32 MAP_MIN = 500.0f;           // Playable area margin
    const f32 MAP_MAX = 15500.0f;         // MAP_SIZE - margin
    const f32 MAP_CENTER = 8000.0f;       // Center of map
    
    // Base positions (corners)
    const f32 RADIANT_BASE = 1500.0f;     // Radiant base center
    const f32 DIRE_BASE = 14500.0f;       // Dire base center
    
    // Lane distances (increased for larger objects)
    const f32 T3_DIST = 1200.0f;          // T3 tower distance from base
    const f32 T2_DIST = 4000.0f;          // T2 tower distance from base
    const f32 T1_DIST = 6500.0f;          // T1 tower distance (near river)
    const f32 BARRACKS_DIST = 900.0f;     // Barracks distance from base
    
    // ============ BASES ============
    CreateBase(Vec3(RADIANT_BASE, 0.0f, RADIANT_BASE), 1, "RadiantAncient");
    CreateBase(Vec3(DIRE_BASE, 0.0f, DIRE_BASE), 2, "DireAncient");
    
    // ============ RADIANT TOWERS (Team 1) ============
    // Top Lane (goes along top edge: low X, high Z)
    CreateTower(Vec3(RADIANT_BASE, 0.0f, RADIANT_BASE + T3_DIST), 1, 3, 0, "Radiant_Top_T3");
    CreateTower(Vec3(RADIANT_BASE, 0.0f, RADIANT_BASE + T2_DIST), 1, 2, 0, "Radiant_Top_T2");
    CreateTower(Vec3(MAP_MIN + 1000.0f, 0.0f, MAP_MAX - 500.0f), 1, 1, 0, "Radiant_Top_T1");
    
    // Mid Lane (diagonal from base to base)
    CreateTower(Vec3(RADIANT_BASE + T3_DIST * 0.7f, 0.0f, RADIANT_BASE + T3_DIST * 0.7f), 1, 3, 1, "Radiant_Mid_T3");
    CreateTower(Vec3(RADIANT_BASE + T2_DIST * 0.7f, 0.0f, RADIANT_BASE + T2_DIST * 0.7f), 1, 2, 1, "Radiant_Mid_T2");
    CreateTower(Vec3(MAP_CENTER - 1500.0f, 0.0f, MAP_CENTER - 1500.0f), 1, 1, 1, "Radiant_Mid_T1");
    
    // Bot Lane (goes along right edge: high X, low Z)
    CreateTower(Vec3(RADIANT_BASE + T3_DIST, 0.0f, RADIANT_BASE), 1, 3, 2, "Radiant_Bot_T3");
    CreateTower(Vec3(RADIANT_BASE + T2_DIST, 0.0f, RADIANT_BASE), 1, 2, 2, "Radiant_Bot_T2");
    CreateTower(Vec3(MAP_MAX - 500.0f, 0.0f, MAP_MIN + 1000.0f), 1, 1, 2, "Radiant_Bot_T1");
    
    // Base towers (T4) - protecting Ancient
    CreateTower(Vec3(RADIANT_BASE + 300.0f, 0.0f, RADIANT_BASE - 100.0f), 1, 4, -1, "Radiant_T4_Left");
    CreateTower(Vec3(RADIANT_BASE - 100.0f, 0.0f, RADIANT_BASE + 300.0f), 1, 4, -1, "Radiant_T4_Right");
    
    // ============ DIRE TOWERS (Team 2) ============
    // Top Lane
    CreateTower(Vec3(MAP_MIN + 500.0f, 0.0f, MAP_MAX - 1000.0f), 2, 1, 0, "Dire_Top_T1");
    CreateTower(Vec3(DIRE_BASE - T2_DIST, 0.0f, DIRE_BASE), 2, 2, 0, "Dire_Top_T2");
    CreateTower(Vec3(DIRE_BASE - T3_DIST, 0.0f, DIRE_BASE), 2, 3, 0, "Dire_Top_T3");
    
    // Mid Lane
    CreateTower(Vec3(MAP_CENTER + 1500.0f, 0.0f, MAP_CENTER + 1500.0f), 2, 1, 1, "Dire_Mid_T1");
    CreateTower(Vec3(DIRE_BASE - T2_DIST * 0.7f, 0.0f, DIRE_BASE - T2_DIST * 0.7f), 2, 2, 1, "Dire_Mid_T2");
    CreateTower(Vec3(DIRE_BASE - T3_DIST * 0.7f, 0.0f, DIRE_BASE - T3_DIST * 0.7f), 2, 3, 1, "Dire_Mid_T3");
    
    // Bot Lane
    CreateTower(Vec3(MAP_MAX - 1000.0f, 0.0f, MAP_MIN + 500.0f), 2, 1, 2, "Dire_Bot_T1");
    CreateTower(Vec3(DIRE_BASE, 0.0f, DIRE_BASE - T2_DIST), 2, 2, 2, "Dire_Bot_T2");
    CreateTower(Vec3(DIRE_BASE, 0.0f, DIRE_BASE - T3_DIST), 2, 3, 2, "Dire_Bot_T3");
    
    // Base towers (T4)
    CreateTower(Vec3(DIRE_BASE - 300.0f, 0.0f, DIRE_BASE + 100.0f), 2, 4, -1, "Dire_T4_Left");
    CreateTower(Vec3(DIRE_BASE + 100.0f, 0.0f, DIRE_BASE - 300.0f), 2, 4, -1, "Dire_T4_Right");
    
    // ============ RADIANT BARRACKS (Team 1) ============
    // Top Lane barracks (near top T3)
    CreateBarracks(Vec3(RADIANT_BASE - 150.0f, 0.0f, RADIANT_BASE + BARRACKS_DIST), 1, 0, true, "Radiant_Top_Melee");
    CreateBarracks(Vec3(RADIANT_BASE + 150.0f, 0.0f, RADIANT_BASE + BARRACKS_DIST), 1, 0, false, "Radiant_Top_Ranged");
    
    // Mid Lane barracks
    CreateBarracks(Vec3(RADIANT_BASE + BARRACKS_DIST * 0.5f, 0.0f, RADIANT_BASE + BARRACKS_DIST * 0.7f), 1, 1, true, "Radiant_Mid_Melee");
    CreateBarracks(Vec3(RADIANT_BASE + BARRACKS_DIST * 0.7f, 0.0f, RADIANT_BASE + BARRACKS_DIST * 0.5f), 1, 1, false, "Radiant_Mid_Ranged");
    
    // Bot Lane barracks (near bot T3)
    CreateBarracks(Vec3(RADIANT_BASE + BARRACKS_DIST, 0.0f, RADIANT_BASE - 150.0f), 1, 2, true, "Radiant_Bot_Melee");
    CreateBarracks(Vec3(RADIANT_BASE + BARRACKS_DIST, 0.0f, RADIANT_BASE + 150.0f), 1, 2, false, "Radiant_Bot_Ranged");
    
    // ============ DIRE BARRACKS (Team 2) ============
    // Top Lane barracks
    CreateBarracks(Vec3(DIRE_BASE - BARRACKS_DIST, 0.0f, DIRE_BASE + 150.0f), 2, 0, true, "Dire_Top_Melee");
    CreateBarracks(Vec3(DIRE_BASE - BARRACKS_DIST, 0.0f, DIRE_BASE - 150.0f), 2, 0, false, "Dire_Top_Ranged");
    
    // Mid Lane barracks
    CreateBarracks(Vec3(DIRE_BASE - BARRACKS_DIST * 0.5f, 0.0f, DIRE_BASE - BARRACKS_DIST * 0.7f), 2, 1, true, "Dire_Mid_Melee");
    CreateBarracks(Vec3(DIRE_BASE - BARRACKS_DIST * 0.7f, 0.0f, DIRE_BASE - BARRACKS_DIST * 0.5f), 2, 1, false, "Dire_Mid_Ranged");
    
    // Bot Lane barracks
    CreateBarracks(Vec3(DIRE_BASE + 150.0f, 0.0f, DIRE_BASE - BARRACKS_DIST), 2, 2, true, "Dire_Bot_Melee");
    CreateBarracks(Vec3(DIRE_BASE - 150.0f, 0.0f, DIRE_BASE - BARRACKS_DIST), 2, 2, false, "Dire_Bot_Ranged");
    
    // ============ CREEP SPAWNS ============
    // Radiant spawns (near barracks)
    CreateCreepSpawn(Vec3(RADIANT_BASE, 0.0f, RADIANT_BASE + T3_DIST + 200.0f), 1, 0, "Radiant_Top_Spawn");
    CreateCreepSpawn(Vec3(RADIANT_BASE + T3_DIST * 0.8f, 0.0f, RADIANT_BASE + T3_DIST * 0.8f), 1, 1, "Radiant_Mid_Spawn");
    CreateCreepSpawn(Vec3(RADIANT_BASE + T3_DIST + 200.0f, 0.0f, RADIANT_BASE), 1, 2, "Radiant_Bot_Spawn");
    
    // Dire spawns (near barracks)
    CreateCreepSpawn(Vec3(DIRE_BASE - T3_DIST - 200.0f, 0.0f, DIRE_BASE), 2, 0, "Dire_Top_Spawn");
    CreateCreepSpawn(Vec3(DIRE_BASE - T3_DIST * 0.8f, 0.0f, DIRE_BASE - T3_DIST * 0.8f), 2, 1, "Dire_Mid_Spawn");
    CreateCreepSpawn(Vec3(DIRE_BASE, 0.0f, DIRE_BASE - T3_DIST - 200.0f), 2, 2, "Dire_Bot_Spawn");
    
    // ============ WAYPOINTS ============
    // Top Lane waypoints (Radiant -> Dire along top edge)
    CreateWaypoint(Vec3(MAP_MIN + 800.0f, 0.0f, MAP_MAX - 800.0f), 0, 0, "WP_Top_0");
    CreateWaypoint(Vec3(MAP_MIN + 800.0f, 0.0f, MAP_CENTER + 2000.0f), 0, 1, "WP_Top_1");
    CreateWaypoint(Vec3(MAP_CENTER - 2000.0f, 0.0f, MAP_MAX - 800.0f), 0, 2, "WP_Top_2");
    CreateWaypoint(Vec3(MAP_CENTER + 2000.0f, 0.0f, MAP_MAX - 800.0f), 0, 3, "WP_Top_3");
    CreateWaypoint(Vec3(MAP_MAX - 800.0f, 0.0f, MAP_MAX - 800.0f), 0, 4, "WP_Top_4");
    
    // Mid Lane waypoints (diagonal)
    CreateWaypoint(Vec3(RADIANT_BASE + 2000.0f, 0.0f, RADIANT_BASE + 2000.0f), 1, 0, "WP_Mid_0");
    CreateWaypoint(Vec3(MAP_CENTER - 1000.0f, 0.0f, MAP_CENTER - 1000.0f), 1, 1, "WP_Mid_1");
    CreateWaypoint(Vec3(MAP_CENTER, 0.0f, MAP_CENTER), 1, 2, "WP_Mid_2");
    CreateWaypoint(Vec3(MAP_CENTER + 1000.0f, 0.0f, MAP_CENTER + 1000.0f), 1, 3, "WP_Mid_3");
    CreateWaypoint(Vec3(DIRE_BASE - 2000.0f, 0.0f, DIRE_BASE - 2000.0f), 1, 4, "WP_Mid_4");
    
    // Bot Lane waypoints (Radiant -> Dire along right edge)
    CreateWaypoint(Vec3(MAP_MAX - 800.0f, 0.0f, MAP_MIN + 800.0f), 2, 0, "WP_Bot_0");
    CreateWaypoint(Vec3(MAP_CENTER + 2000.0f, 0.0f, MAP_MIN + 800.0f), 2, 1, "WP_Bot_1");
    CreateWaypoint(Vec3(MAP_MAX - 800.0f, 0.0f, MAP_CENTER - 2000.0f), 2, 2, "WP_Bot_2");
    CreateWaypoint(Vec3(MAP_MAX - 800.0f, 0.0f, MAP_CENTER + 2000.0f), 2, 3, "WP_Bot_3");
    CreateWaypoint(Vec3(MAP_MAX - 800.0f, 0.0f, MAP_MAX - 800.0f), 2, 4, "WP_Bot_4");
    
    // ============ HERO SPAWN POINTS (Fountain area) ============
    // Radiant fountain - behind the Ancient, safe spawn area
    // 5 spawn positions for 5 heroes, arranged in a semi-circle
    const f32 FOUNTAIN_OFFSET = 800.0f;  // Distance behind base
    const f32 HERO_SPACING = 200.0f;     // Space between hero spawns
    
    // Radiant hero spawns (behind Radiant Ancient)
    CreateHeroSpawn(Vec3(RADIANT_BASE - FOUNTAIN_OFFSET, 0.0f, RADIANT_BASE - FOUNTAIN_OFFSET), 1, 0, "Radiant_Hero_Spawn_1");
    CreateHeroSpawn(Vec3(RADIANT_BASE - FOUNTAIN_OFFSET - HERO_SPACING, 0.0f, RADIANT_BASE - FOUNTAIN_OFFSET + HERO_SPACING), 1, 1, "Radiant_Hero_Spawn_2");
    CreateHeroSpawn(Vec3(RADIANT_BASE - FOUNTAIN_OFFSET + HERO_SPACING, 0.0f, RADIANT_BASE - FOUNTAIN_OFFSET - HERO_SPACING), 1, 2, "Radiant_Hero_Spawn_3");
    CreateHeroSpawn(Vec3(RADIANT_BASE - FOUNTAIN_OFFSET - HERO_SPACING * 2, 0.0f, RADIANT_BASE - FOUNTAIN_OFFSET), 1, 3, "Radiant_Hero_Spawn_4");
    CreateHeroSpawn(Vec3(RADIANT_BASE - FOUNTAIN_OFFSET, 0.0f, RADIANT_BASE - FOUNTAIN_OFFSET - HERO_SPACING * 2), 1, 4, "Radiant_Hero_Spawn_5");
    
    // Dire hero spawns (behind Dire Ancient)
    CreateHeroSpawn(Vec3(DIRE_BASE + FOUNTAIN_OFFSET, 0.0f, DIRE_BASE + FOUNTAIN_OFFSET), 2, 0, "Dire_Hero_Spawn_1");
    CreateHeroSpawn(Vec3(DIRE_BASE + FOUNTAIN_OFFSET + HERO_SPACING, 0.0f, DIRE_BASE + FOUNTAIN_OFFSET - HERO_SPACING), 2, 1, "Dire_Hero_Spawn_2");
    CreateHeroSpawn(Vec3(DIRE_BASE + FOUNTAIN_OFFSET - HERO_SPACING, 0.0f, DIRE_BASE + FOUNTAIN_OFFSET + HERO_SPACING), 2, 2, "Dire_Hero_Spawn_3");
    CreateHeroSpawn(Vec3(DIRE_BASE + FOUNTAIN_OFFSET + HERO_SPACING * 2, 0.0f, DIRE_BASE + FOUNTAIN_OFFSET), 2, 3, "Dire_Hero_Spawn_4");
    CreateHeroSpawn(Vec3(DIRE_BASE + FOUNTAIN_OFFSET, 0.0f, DIRE_BASE + FOUNTAIN_OFFSET + HERO_SPACING * 2), 2, 4, "Dire_Hero_Spawn_5");
    
    LOG_INFO("Map objects setup complete (16000x16000 map): {} entities", m_gameWorld->getEntityCount());
}

Entity InGameState::CreateBase(const Vec3& pos, i32 teamId, const std::string& name) {
    Entity entity = m_gameWorld->createEntity(name);
    
    auto& transform = m_gameWorld->addComponent<WorldEditor::TransformComponent>(entity);
    transform.position = pos;
    transform.scale = Vec3(1.0f);
    
    auto& obj = m_gameWorld->addComponent<WorldEditor::ObjectComponent>(entity);
    obj.type = WorldEditor::ObjectType::Base;
    obj.teamId = teamId;
    
    auto& health = m_gameWorld->addComponent<WorldEditor::HealthComponent>(entity);
    health.maxHealth = 4500.0f;  // Ancient HP
    health.currentHealth = 4500.0f;
    health.armor = 15.0f;
    
    // Add mesh - Ancient: 400 units radius, 500 height (large and imposing)
    auto& mesh = m_gameWorld->addComponent<WorldEditor::MeshComponent>(entity, name);
    WorldEditor::MeshGenerators::GenerateCylinder(mesh, 400.0f, 500.0f, 16);
    mesh.gpuUploadNeeded = true;
    
    // Material
    Entity matEntity = m_gameWorld->createEntity(name + "_Mat");
    auto& mat = m_gameWorld->addComponent<WorldEditor::MaterialComponent>(matEntity, name + "_Mat");
    mat.baseColor = teamId == 1 ? Vec3(0.2f, 0.7f, 0.3f) : Vec3(0.7f, 0.2f, 0.2f);
    mat.emissiveColor = teamId == 1 ? Vec3(0.1f, 0.3f, 0.1f) : Vec3(0.3f, 0.1f, 0.1f);
    mesh.materialEntity = matEntity;
    
    LOG_INFO("Created Base '{}' at ({}, {}, {})", name, pos.x, pos.y, pos.z);
    return entity;
}

Entity InGameState::CreateTower(const Vec3& pos, i32 teamId, i32 tier, i32 lane, const std::string& name) {
    Entity entity = m_gameWorld->createEntity(name);
    
    auto& transform = m_gameWorld->addComponent<WorldEditor::TransformComponent>(entity);
    transform.position = pos;
    transform.scale = Vec3(1.0f);
    
    auto& obj = m_gameWorld->addComponent<WorldEditor::ObjectComponent>(entity);
    obj.type = WorldEditor::ObjectType::Tower;
    obj.teamId = teamId;
    obj.spawnLane = lane;
    
    // Tower stats based on tier
    f32 hpMultiplier = 1.0f + (4 - tier) * 0.3f; // T4 > T3 > T2 > T1
    obj.attackRange = 700.0f;  // ~700 units attack range (Dota 2 scale)
    obj.attackDamage = 100.0f + (4 - tier) * 20.0f;
    obj.attackSpeed = 1.0f;
    
    auto& health = m_gameWorld->addComponent<WorldEditor::HealthComponent>(entity);
    health.maxHealth = 1800.0f * hpMultiplier;
    health.currentHealth = health.maxHealth;
    health.armor = 10.0f + (4 - tier) * 2.0f;
    
    // Add mesh - Tower: 100 units radius, 300-400 height (visible from distance)
    auto& mesh = m_gameWorld->addComponent<WorldEditor::MeshComponent>(entity, name);
    f32 towerHeight = 300.0f + tier * 30.0f;  // T1=330, T2=360, T3=390, T4=420
    WorldEditor::MeshGenerators::GenerateCylinder(mesh, 100.0f, towerHeight, 8);
    mesh.gpuUploadNeeded = true;
    
    // Material
    Entity matEntity = m_gameWorld->createEntity(name + "_Mat");
    auto& mat = m_gameWorld->addComponent<WorldEditor::MaterialComponent>(matEntity, name + "_Mat");
    mat.baseColor = teamId == 1 ? Vec3(0.3f, 0.6f, 0.3f) : Vec3(0.6f, 0.3f, 0.3f);
    mesh.materialEntity = matEntity;
    
    // Runtime tower state
    m_gameWorld->addComponent<WorldEditor::TowerRuntimeComponent>(entity);
    
    LOG_INFO("Created Tower '{}' T{} at ({}, {}, {})", name, tier, pos.x, pos.y, pos.z);
    return entity;
}

Entity InGameState::CreateBarracks(const Vec3& pos, i32 teamId, i32 lane, bool isMelee, const std::string& name) {
    Entity entity = m_gameWorld->createEntity(name);
    
    auto& transform = m_gameWorld->addComponent<WorldEditor::TransformComponent>(entity);
    transform.position = pos;
    transform.scale = Vec3(1.0f);
    
    auto& obj = m_gameWorld->addComponent<WorldEditor::ObjectComponent>(entity);
    obj.type = WorldEditor::ObjectType::Building;
    obj.teamId = teamId;
    obj.spawnLane = lane;
    obj.customData = isMelee ? "melee_barracks" : "ranged_barracks";
    
    auto& health = m_gameWorld->addComponent<WorldEditor::HealthComponent>(entity);
    health.maxHealth = isMelee ? 2200.0f : 1300.0f; // Melee rax has more HP
    health.currentHealth = health.maxHealth;
    health.armor = 10.0f;
    
    // Add mesh - Barracks: 200-250 units wide, 150-180 height (substantial buildings)
    auto& mesh = m_gameWorld->addComponent<WorldEditor::MeshComponent>(entity, name);
    f32 width = isMelee ? 250.0f : 200.0f;
    f32 height = isMelee ? 180.0f : 150.0f;
    WorldEditor::MeshGenerators::GenerateCube(mesh, Vec3(width, height, width));
    mesh.gpuUploadNeeded = true;
    
    // Material
    Entity matEntity = m_gameWorld->createEntity(name + "_Mat");
    auto& mat = m_gameWorld->addComponent<WorldEditor::MaterialComponent>(matEntity, name + "_Mat");
    if (teamId == 1) {
        mat.baseColor = isMelee ? Vec3(0.4f, 0.7f, 0.4f) : Vec3(0.3f, 0.5f, 0.7f);
    } else {
        mat.baseColor = isMelee ? Vec3(0.7f, 0.4f, 0.4f) : Vec3(0.7f, 0.5f, 0.3f);
    }
    mesh.materialEntity = matEntity;
    
    LOG_INFO("Created Barracks '{}' ({}) at ({}, {}, {})", name, isMelee ? "Melee" : "Ranged", pos.x, pos.y, pos.z);
    return entity;
}

Entity InGameState::CreateCreepSpawn(const Vec3& pos, i32 teamId, i32 lane, const std::string& name) {
    Entity entity = m_gameWorld->createEntity(name);
    
    auto& transform = m_gameWorld->addComponent<WorldEditor::TransformComponent>(entity);
    transform.position = pos;
    
    auto& obj = m_gameWorld->addComponent<WorldEditor::ObjectComponent>(entity);
    obj.type = WorldEditor::ObjectType::CreepSpawn;
    obj.teamId = teamId;
    obj.spawnLane = lane;
    obj.spawnRadius = 150.0f;  // Scaled for 16000 map
    obj.maxUnits = 4;
    
    LOG_INFO("Created CreepSpawn '{}' lane {} at ({}, {}, {})", name, lane, pos.x, pos.y, pos.z);
    return entity;
}

Entity InGameState::CreateWaypoint(const Vec3& pos, i32 lane, i32 order, const std::string& name) {
    Entity entity = m_gameWorld->createEntity(name);
    
    auto& transform = m_gameWorld->addComponent<WorldEditor::TransformComponent>(entity);
    transform.position = pos;
    
    auto& obj = m_gameWorld->addComponent<WorldEditor::ObjectComponent>(entity);
    obj.type = WorldEditor::ObjectType::Waypoint;
    obj.waypointLane = lane;
    obj.waypointOrder = order;
    
    LOG_INFO("Created Waypoint '{}' lane {} order {} at ({}, {}, {})", name, lane, order, pos.x, pos.y, pos.z);
    return entity;
}

Entity InGameState::CreateHeroSpawn(const Vec3& pos, i32 teamId, i32 spawnIndex, const std::string& name) {
    Entity entity = m_gameWorld->createEntity(name);
    
    auto& transform = m_gameWorld->addComponent<WorldEditor::TransformComponent>(entity);
    transform.position = pos;
    
    auto& obj = m_gameWorld->addComponent<WorldEditor::ObjectComponent>(entity);
    obj.type = WorldEditor::ObjectType::HeroSpawn;
    obj.teamId = teamId;
    obj.spawnLane = spawnIndex;  // Using spawnLane to store spawn index (0-4 for 5 heroes)
    obj.spawnRadius = 100.0f;    // Spawn area radius
    
    // Add a visual marker for the spawn point (small platform)
    auto& mesh = m_gameWorld->addComponent<WorldEditor::MeshComponent>(entity, name);
    WorldEditor::MeshGenerators::GenerateCylinder(mesh, 80.0f, 10.0f, 16);  // Flat platform
    mesh.gpuUploadNeeded = true;
    
    // Material - team colored glow
    Entity matEntity = m_gameWorld->createEntity(name + "_Mat");
    auto& mat = m_gameWorld->addComponent<WorldEditor::MaterialComponent>(matEntity, name + "_Mat");
    mat.baseColor = teamId == 1 ? Vec3(0.2f, 0.6f, 0.3f) : Vec3(0.6f, 0.2f, 0.2f);
    mat.emissiveColor = teamId == 1 ? Vec3(0.1f, 0.4f, 0.2f) : Vec3(0.4f, 0.1f, 0.1f);
    mesh.materialEntity = matEntity;
    
    LOG_INFO("Created HeroSpawn '{}' team {} index {} at ({}, {}, {})", name, teamId, spawnIndex, pos.x, pos.y, pos.z);
    return entity;
}

} // namespace Game
