# Сборка проекта WorldEditor

## Требования

### Обязательные
- **Windows 10/11 (x64)** с поддержкой **DirectX 12**
- **Visual Studio 2022** (или Build Tools) с компонентами:
  - Desktop development with C++
  - MSVC v143 (или новее)
  - Windows 10/11 SDK (10.0.19041.0 или новее)
- **CMake 3.20+**
- **Git** (для скачивания зависимостей через FetchContent)

### Проверка установки
```powershell
cmake --version    # должно быть 3.20+
git --version      # должен быть установлен
```

## Зависимости (скачиваются автоматически)
- GLM 1.0.1
- spdlog 1.11.0
- EnTT 3.12.2
- nlohmann/json 3.11.3
- ImGui (docking branch)
- Catch2 3.5.0
- SQLite3 3.45.1
- stb (master)

---

## Debug сборка

### 1. Конфигурация
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

### 2. Сборка всех targets
```powershell
cmake --build build --config Debug
```

### 3. Сборка отдельных компонентов
```powershell
# Редактор карт
cmake --build build --config Debug --target WorldEditor

# Игровой клиент
cmake --build build --config Debug --target Game

# Выделенный сервер
cmake --build build --config Debug --target DedicatedServer

# Сервер аутентификации
cmake --build build --config Debug --target AuthServer

# Координатор матчмейкинга
cmake --build build --config Debug --target MatchmakingCoordinator

# Утилита добавления тестовых пользователей
cmake --build build --config Debug --target AddTestUser
cmake --build build --config Debug --target AddMMTestUsers
```

### 4. Расположение бинарников (Debug)
```
build/bin/Debug/WorldEditor.exe      # Редактор
build/bin/Debug/Game.exe             # Игра
build/bin/Debug/DedicatedServer.exe  # Игровой сервер
build/bin/Debug/AuthServer.exe       # Сервер авторизации
build/bin/Debug/MatchmakingCoordinator.exe  # Матчмейкинг
```

---

## Release сборка

### Вариант 1: Использовать батник
```powershell
.\build_release.bat
```

### Вариант 2: Вручную
```powershell
# Конфигурация в отдельную папку
cmake -S . -B build_release -G "Visual Studio 17 2022" -A x64

# Сборка всех targets
cmake --build build_release --config Release

# Или отдельные компоненты
cmake --build build_release --config Release --target Game
cmake --build build_release --config Release --target DedicatedServer
```

### Расположение бинарников (Release)
```
build_release/bin/Release/WorldEditor.exe
build_release/bin/Release/Game.exe
build_release/bin/Release/DedicatedServer.exe
build_release/bin/Release/AuthServer.exe
build_release/bin/Release/MatchmakingCoordinator.exe
```

---

## Запуск

### Редактор карт
```powershell
# Debug
.\build\bin\Debug\WorldEditor.exe

# Release
.\build_release\bin\Release\WorldEditor.exe
```

### Игра (полный стек)

1. **Запустить сервер авторизации:**
```powershell
.\build\bin\Debug\AuthServer.exe
# Слушает порт 27016
```

2. **Создать тестового пользователя (один раз):**
```powershell
.\build\bin\Debug\AddTestUser.exe
# Создаёт пользователя test/test123
```

3. **Запустить координатор матчмейкинга:**
```powershell
.\build\bin\Debug\MatchmakingCoordinator.exe
# Слушает порт 27017
```

4. **Запустить игровой сервер:**
```powershell
.\build\bin\Debug\DedicatedServer.exe
# Слушает порт 27015
```

5. **Запустить игру:**
```powershell
.\build\bin\Debug\Game.exe
```

### Быстрый запуск (батники)
```powershell
.\start_auth_server.bat           # AuthServer
.\test_matchmaking_local.bat      # Локальный тест матчмейкинга
```

---

## Структура targets

| Target | Описание | Тип |
|--------|----------|-----|
| `WorldEditor` | Редактор карт с ImGui | Executable |
| `Game` | Игровой клиент с Panorama UI | Executable |
| `DedicatedServer` | Выделенный игровой сервер | Executable |
| `AuthServer` | Сервер аутентификации | Executable |
| `MatchmakingCoordinator` | Координатор матчмейкинга | Executable |
| `AddTestUser` | Утилита создания тестового пользователя | Executable |
| `AddMMTestUsers` | Утилита создания пользователей для тестов MM | Executable |

### Библиотеки (статические)
- `world_editor_core` - базовые типы, логирование
- `world_editor_common` - общий код клиент/сервер
- `world_editor_world` - ECS компоненты и системы
- `world_editor_renderer` - DirectX 12 рендерер
- `world_editor_network` - UDP сеть
- `world_editor_auth` - аутентификация
- `world_editor_server` - серверная логика
- `world_editor_client` - клиентская логика
- `world_editor_ui` - редакторский UI
- `world_editor_game` - игровые состояния
- `panorama_ui` - Panorama UI система

---

## Частые проблемы

### CMake не находит компилятор
```powershell
# Убедитесь что VS 2022 установлена с C++ компонентами
# Или укажите генератор явно:
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

### Ошибка скачивания зависимостей
```powershell
# Удалите папку build и пересоберите
Remove-Item -Recurse -Force build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

### Файл занят (не пересобирается)
Закройте запущенные приложения (WorldEditor.exe, Game.exe и т.д.)

### Ошибки линковки DirectX
Убедитесь что установлен Windows SDK с DirectX 12 заголовками.

### База данных auth.db заблокирована
Закройте AuthServer перед повторным запуском.

---

## Порты по умолчанию

| Сервис | Порт |
|--------|------|
| Game Server | 27015 |
| Auth Server | 27016 |
| Matchmaking Coordinator | 27017 |
