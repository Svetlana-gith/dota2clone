# Game Integration Plan
## Интеграция игровой логики с UI системой

### Текущая архитектура

```
Game.exe
├── GameStateManager
│   ├── MainMenuState (UI only)
│   ├── HeroesState (UI only)
│   ├── LoadingState (UI only)
│   └── InGameState (UI + placeholder)
│
├── Panorama UI System
│   └── CUIEngine
│
└── World System (NOT INTEGRATED YET)
    ├── ClientWorld
    ├── ServerWorld
    └── Terrain/Entities
```

### План интеграции

#### 1. LoadingState - Загрузка игрового мира

**Файл:** `src/game/states/LoadingState.cpp`

**Добавить:**
```cpp
#include "../../world/World.h"
#include "../../client/ClientWorld.h"
#include "../../server/ServerWorld.h"

// В LoadingState добавить:
private:
    std::unique_ptr<ClientWorld> m_clientWorld;
    std::unique_ptr<ServerWorld> m_serverWorld;
    
void LoadingState::Update(f32 deltaTime) {
    // Phase 1: Initialize worlds (0-30%)
    if (m_progress < 0.3f && !m_serverWorld) {
        m_statusText = "Initializing server...";
        m_serverWorld = std::make_unique<ServerWorld>();
        m_serverWorld->Initialize();
    }
    
    // Phase 2: Load map (30-60%)
    if (m_progress >= 0.3f && m_progress < 0.6f) {
        m_statusText = "Loading map: " + m_mapName;
        // m_serverWorld->LoadMap(m_mapName);
        // m_clientWorld = std::make_unique<ClientWorld>();
        // m_clientWorld->ConnectToServer("localhost");
    }
    
    // Phase 3: Load assets (60-90%)
    if (m_progress >= 0.6f && m_progress < 0.9f) {
        m_statusText = "Loading assets...";
        // Load textures, models, etc.
    }
    
    // Phase 4: Ready (90-100%)
    if (m_progress >= 0.9f) {
        m_statusText = "Starting game...";
        // Pass worlds to InGameState
        auto* inGameState = m_manager->GetInGameState();
        inGameState->SetWorlds(std::move(m_clientWorld), std::move(m_serverWorld));
    }
    
    if (IsLoadingComplete()) {
        m_manager->ChangeState(EGameState::InGame);
    }
}
```

#### 2. InGameState - Интеграция с игровым миром

**Файл:** `src/game/states/InGameState.cpp`

**Добавить:**
```cpp
#include "../../world/World.h"
#include "../../client/ClientWorld.h"
#include "../../server/ServerWorld.h"
#include "../../renderer/WorldRenderer.h"

// В InGameState добавить:
private:
    std::unique_ptr<ClientWorld> m_clientWorld;
    std::unique_ptr<ServerWorld> m_serverWorld;
    std::unique_ptr<WorldRenderer> m_worldRenderer;
    
public:
    void SetWorlds(std::unique_ptr<ClientWorld> client, 
                   std::unique_ptr<ServerWorld> server) {
        m_clientWorld = std::move(client);
        m_serverWorld = std::move(server);
        
        // Initialize renderer
        m_worldRenderer = std::make_unique<WorldRenderer>();
        m_worldRenderer->Initialize(m_clientWorld.get());
    }

void InGameState::Update(f32 deltaTime) {
    if (m_isPaused) {
        Panorama::CUIEngine::Instance().Update(deltaTime);
        return;
    }
    
    // Update game worlds
    if (m_serverWorld) {
        m_serverWorld->Update(deltaTime);
    }
    
    if (m_clientWorld) {
        m_clientWorld->Update(deltaTime);
    }
    
    // Update HUD with game data
    UpdateHUDFromGameState();
    
    // Update UI
    Panorama::CUIEngine::Instance().Update(deltaTime);
}

void InGameState::RenderWorld() {
    if (m_worldRenderer && m_clientWorld) {
        m_worldRenderer->Render();
    }
}

void InGameState::UpdateHUDFromGameState() {
    // Get player entity from client world
    if (m_clientWorld) {
        auto* player = m_clientWorld->GetLocalPlayer();
        if (player) {
            // Update health bar
            Panorama::CGameEventData healthData;
            healthData.SetFloat("current", player->GetHealth());
            healthData.SetFloat("max", player->GetMaxHealth());
            Panorama::GameEvents_Fire("Player_HealthChanged", healthData);
            
            // Update mana bar
            Panorama::CGameEventData manaData;
            manaData.SetFloat("current", player->GetMana());
            manaData.SetFloat("max", player->GetMaxMana());
            Panorama::GameEvents_Fire("Player_ManaChanged", manaData);
        }
    }
}
```

#### 3. GameMain.cpp - Инициализация рендерера

**Файл:** `src/game/GameMain.cpp`

**Добавить:**
```cpp
#include "../renderer/Renderer.h"

// В main() добавить инициализацию 3D рендерера:
Renderer::Instance().Initialize(device, context, swapChain);

// В игровом цикле:
void GameLoop() {
    // Clear render target
    Renderer::Instance().BeginFrame();
    
    // Update game state
    GameStateManager::Instance().Update(deltaTime);
    
    // Render game state (3D world + UI)
    GameStateManager::Instance().Render();
    
    // Present
    Renderer::Instance().EndFrame();
    swapChain->Present(1, 0);
}
```

#### 4. Структура файлов после интеграции

```
src/game/
├── GameMain.cpp (main loop with 3D + UI rendering)
├── GameState.h
├── GameStateManager.cpp
│
├── states/
│   ├── MainMenuState.cpp (UI only)
│   ├── HeroesState.cpp (UI only)
│   ├── LoadingState.cpp (loads ClientWorld + ServerWorld)
│   └── InGameState.cpp (renders 3D world + HUD)
│
└── ui/panorama/ (UI system)

src/world/
├── World.h/cpp (game logic)
├── Terrain.h/cpp
└── Entity.h/cpp

src/client/
└── ClientWorld.h/cpp (client-side game state)

src/server/
└── ServerWorld.h/cpp (server-side game logic)

src/renderer/
├── Renderer.h/cpp (3D rendering)
└── WorldRenderer.h/cpp (renders game world)
```

### Последовательность действий

1. ✅ **Создана UI система** (Panorama)
2. ✅ **Созданы игровые состояния** (MainMenu, Heroes, Loading, InGame)
3. ⏳ **Интеграция LoadingState** - загрузка ClientWorld/ServerWorld
4. ⏳ **Интеграция InGameState** - рендеринг 3D мира + обновление HUD
5. ⏳ **Связь UI ↔ Game Logic** - события для обновления HP/Mana/etc

### Преимущества такой архитектуры

- **Разделение ответственности**: UI отдельно, игровая логика отдельно
- **Переиспользование**: ClientWorld/ServerWorld можно использовать в редакторе
- **Тестирование**: Можно тестировать UI без игровой логики и наоборот
- **Масштабируемость**: Легко добавлять новые состояния (Shop, Inventory, etc.)

### Следующие шаги

1. Добавить `SetWorlds()` метод в InGameState
2. Модифицировать LoadingState для загрузки миров
3. Интегрировать WorldRenderer в InGameState::RenderWorld()
4. Настроить события для обновления HUD из игровой логики
