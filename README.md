# WorldEditor (DirectX 12)

3D редактор карт + игровой режим (creeps/towers) на **ECS (EnTT)** с рендерингом на **DirectX 12** и UI на **ImGui Docking**.

## Требования

- **Windows 10/11 (x64)** и видеокарта с поддержкой **DirectX 12**
- **Visual Studio 2022** (или Build Tools) с компонентами:
  - **Desktop development with C++**
  - **MSVC v143**
  - **Windows 10/11 SDK**
- **CMake 3.20+**
- **Git** (нужен, потому что зависимости тянутся через `FetchContent` в CMake)

Зависимости (glm / stb / spdlog / EnTT / nlohmann-json / imgui docking) скачиваются автоматически при конфигурации CMake.

## Установка / Сборка

Открой PowerShell/Terminal в папке проекта (`e:\worldeditor`) и выполни:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

### Release (без лишних проверок)

Есть батник:

```bash
.\build_release.bat
```

Либо вручную:

```bash
cmake -S . -B build_release -G "Visual Studio 17 2022" -A x64
cmake --build build_release --config Release
```

## Запуск

Debug:

```bash
.\build\bin\Debug\WorldEditor.exe
```

Release:

```bash
.\build_release\bin\Release\WorldEditor.exe
```

При запуске ресурсы (`resources/`) автоматически копируются рядом с бинарником (см. `CMakeLists.txt`).

## Как пользоваться (кратко)

- **Viewport**: основное окно рендера сцены.
- **Game Mode**: включает симуляцию (крипы/башни) и открывает вкладку **Game View** (PIE‑стиль).
- **Game View камера (“как в доте”)**:
  - фиксированный угол (**Yaw=-45, Pitch=-45**)
  - высота камеры по умолчанию **Y=50** (zoom меняет высоту)
  - **edge‑pan** по краям окна, **MMB drag** для перетаскивания, **wheel** для зума
  - стартовая точка камеры — **Radiant Base** (team 1), если она есть на карте

## Структура проекта

- `src/core/` — базовые типы/таймер/математика
- `src/world/` — ECS, компоненты и системы (крипы, коллизии, и т.д.)
- `src/renderer/` — DirectX 12 рендерер
- `src/ui/` — Editor UI / GameMode UI (ImGui)
- `src/serialization/` — загрузка/сохранение карты (JSON)
- `resources/` — шейдеры и ресурсы
- `tests/` — тестовые утилиты/минимальный инспектор дампов

## Частые проблемы

- **CMake не может скачать зависимости**
  - Проверь, что `git` доступен из консоли (`git --version`)
  - Иногда помогает удалить папку `build/` и пересобрать.
- **`WorldEditor.exe` не пересобирается (файл занят)**
  - Закрой запущенный редактор и пересобери.

