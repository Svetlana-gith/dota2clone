# Panorama UI System

Valve-style UI framework с CSS-подобными стилями, XML layouts, и системой событий.

## Архитектура

```
src/game/ui/panorama/
├── PanoramaTypes.h      - Базовые типы (Color, Length, Enums)
├── CStyleSheet.h/cpp    - CSS-подобные стили и парсер
├── CPanel2D.h/cpp       - Базовый класс панелей
├── CPanelWidgets.cpp    - Виджеты (Label, Button, etc.)
├── CLayoutFile.h/cpp    - XML layout парсер
├── GameEvents.h/cpp     - Система событий (как в Dota 2)
├── CUIRenderer.h/cpp    - DirectX 11 рендерер
└── CUIEngine.h/cpp      - Главный движок UI
```

## Game States

```
src/game/
├── GameState.h          - Интерфейс состояний
├── GameStateManager.cpp - Менеджер состояний
└── states/
    ├── MainMenuState.cpp   - Главное меню
    ├── LoadingState.cpp    - Экран загрузки
    └── InGameState.cpp     - Игровой HUD
```

## Использование

### Инициализация

```cpp
// В main.cpp или GameMain.cpp
#include "game/GameState.h"
#include "game/ui/panorama/CUIEngine.h"

// Инициализация UI
Panorama::UIEngineConfig config;
config.screenWidth = 1920;
config.screenHeight = 1080;
Panorama::CUIEngine::Instance().Initialize(device, context, config);

// Инициализация состояний
Game::GameStateManager::Instance().Initialize();

// Главный цикл
while (running) {
    Game::GameStateManager::Instance().Update(deltaTime);
    Game::GameStateManager::Instance().Render();
}
```

### Создание панелей (C++)

```cpp
auto& engine = Panorama::CUIEngine::Instance();

// Создание кнопки
auto button = std::make_shared<Panorama::CButton>("Play", "play-btn");
button->AddClass("MenuButton");
button->GetStyle().width = Panorama::Length::Px(200);
button->GetStyle().height = Panorama::Length::Px(50);
button->GetStyle().backgroundColor = Panorama::Color(0.2f, 0.5f, 0.2f, 0.9f);
button->SetOnActivate([]() {
    Game::ChangeGameState(Game::EGameState::Loading);
});

engine.GetRoot()->AddChild(button);
```

### XML Layout (Valve-style)

```xml
<root>
    <styles>
        <include src="file://{resources}/styles/menu.css" />
    </styles>
    <Panel class="MainMenu">
        <Panel id="MenuContainer" class="MenuContainer">
            <Button id="PlayButton" class="MenuButton" text="PLAY" />
            <Button id="SettingsButton" class="MenuButton" text="SETTINGS" />
            <Button id="ExitButton" class="MenuButton" text="EXIT" />
        </Panel>
    </Panel>
</root>
```

### CSS-подобные стили

```css
.MainMenu {
    width: fill-parent-flow;
    height: fill-parent-flow;
    background-color: rgba(10, 10, 15, 0.95);
}

.MenuButton {
    width: 300px;
    height: 60px;
    margin-bottom: 15px;
    background-color: rgba(40, 40, 50, 0.9);
    border-radius: 8px;
    font-size: 22px;
    color: white;
    transition: background-color 0.2s ease-out;
}

.MenuButton:hover {
    background-color: rgba(60, 60, 70, 0.95);
}
```

### События (Valve-style)

```cpp
// Подписка на игровое событие
Panorama::GameEvents_Subscribe("Player_HealthChanged", [](const Panorama::CGameEventData& data) {
    f32 health = data.GetFloat("current");
    f32 maxHealth = data.GetFloat("max");
    // Обновить UI
});

// Отправка события
Panorama::CGameEventData data;
data.SetFloat("current", 80.0f);
data.SetFloat("max", 100.0f);
Panorama::GameEvents_Fire("Player_HealthChanged", data);

// Отправка на сервер
Panorama::GameEvents_SendToServer("Player_RequestAbility", data);
```

## Виджеты

| Класс | Описание |
|-------|----------|
| CPanel2D | Базовый контейнер |
| CLabel | Текстовая метка |
| CImage | Изображение |
| CButton | Кнопка с onClick |
| CProgressBar | Полоса прогресса |
| CTextEntry | Текстовое поле |
| CSlider | Слайдер |
| CDropDown | Выпадающий список |

## Состояния игры

```cpp
// Переключение состояния
Game::ChangeGameState(Game::EGameState::MainMenu);
Game::ChangeGameState(Game::EGameState::Loading);
Game::ChangeGameState(Game::EGameState::InGame);

// Проверка текущего состояния
if (Game::GetCurrentGameState() == Game::EGameState::InGame) {
    // В игре
}

// Подписка на смену состояния
Game::GetGameStateManager().AddStateChangeListener([](Game::EGameState old, Game::EGameState newState) {
    // Реакция на смену состояния
});
```

## Easing функции

- `Linear` - линейная
- `EaseIn` / `EaseOut` / `EaseInOut`
- `EaseInQuad` / `EaseOutQuad`
- `EaseInCubic` / `EaseOutCubic`
- `EaseInBack` / `EaseOutBack`
- `EaseInBounce` / `EaseOutBounce`
- `Spring`
