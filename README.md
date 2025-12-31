# World Editor

3D редактор карт с ECS архитектурой и DirectX 12 рендерингом.

## Сборка

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

## Запуск

```bash
# Из корневой директории проекта
.\build\bin\Debug\WorldEditor.exe
```

Или запустите WorldEditor.exe напрямую из папки build\bin\Debug\

## Особенности

- **ECS архитектура** - Entity Component System
- **DirectX 12 рендеринг** - Современный графический API
- **3D рендеринг** - Куб и другие меши в viewport
- **Обработка ошибок** - Консоль остается открытой при ошибках

## Архитектура

- `src/core/` - Базовые утилиты (математика, логирование)
- `src/world/` - ECS система (сущности, компоненты, системы)
- `src/renderer/` - DirectX 12 рендерер
- `src/ui/` - Зарезервировано для будущего UI
- `src/assets/` - Управление ассетами
- `src/serialization/` - Сериализация данных
- `src/tools/` - Инструменты разработки

## Требования

- Windows 10+
- Visual Studio 2019+ с C++17
- DirectX 12 compatible GPU
- CMake 3.10+

## Бекап

При внесении значительных изменений создается бекап в `../worldeditor_backup_YYYY-MM-DD_HH-mm-ss/`
