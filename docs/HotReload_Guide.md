# CSS Hot Reload System

## 🔥 Обзор

Система hot-reload автоматически отслеживает изменения в CSS файлах и перезагружает стили без перезапуска игры. Это значительно ускоряет итерацию по UI дизайну.

## 🚀 Быстрый старт

### 1. Включить hot-reload (Debug режим)

```cpp
// В OnEnter() вашего GameState
#ifdef _DEBUG
Panorama::CUIEngine::Instance().EnableHotReload(true);
Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/login.css");
Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/base.css");
#endif
```

### 2. Редактировать CSS

Просто сохраните изменения в CSS файле - стили обновятся автоматически через ~0.5 секунды!

```css
/* Измените цвет в login.css */
#ErrorLabel {
  background-color: rgba(255, 107, 107, 0.25); /* Было 0.15 */
}
```

**Сохраните файл** → Игра автоматически перезагрузит стили! ✨

## 📋 API Reference

### CUIEngine методы

```cpp
// Включить/выключить hot-reload
void EnableHotReload(bool enabled = true);

// Добавить файл для отслеживания
void WatchStyleSheet(const std::string& path);

// Проверить статус
bool IsHotReloadEnabled() const;
```

### CStyleHotReload (продвинутое использование)

```cpp
auto& hotReload = Panorama::CStyleHotReload::Instance();

// Настройка
hotReload.Enable(true);
hotReload.SetCheckInterval(0.5f);  // Проверять каждые 0.5 сек

// Отслеживание файлов
hotReload.WatchFile("resources/styles/login.css");
hotReload.WatchFile("resources/styles/base.css");

// Кастомный callback при изменении
hotReload.WatchFile("resources/styles/custom.css", [](const std::string& path) {
    LOG_INFO("Custom reload for: {}", path);
    // Ваша логика перезагрузки
});

// Управление
hotReload.UnwatchFile("resources/styles/login.css");
hotReload.UnwatchAll();
hotReload.CheckNow();  // Принудительная проверка

// Статистика
auto stats = hotReload.GetStats();
LOG_INFO("Total reloads: {}", stats.totalReloads);
LOG_INFO("Failed reloads: {}", stats.failedReloads);
LOG_INFO("Last reloaded: {}", stats.lastReloadedFile);
```

## 🎯 Примеры использования

### Пример 1: LoginState

```cpp
void LoginState::OnEnter() {
    // Load stylesheet
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/login.css");
    
    // Enable hot reload (Debug only)
    #ifdef _DEBUG
    Panorama::CUIEngine::Instance().EnableHotReload(true);
    Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/login.css");
    Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/base.css");
    LOG_INFO("Hot reload enabled for CSS files");
    #endif
    
    CreateUI();
}
```

### Пример 2: MainMenuState

```cpp
void MainMenuState::OnEnter() {
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/main_menu.css");
    
    #ifdef _DEBUG
    auto& hotReload = Panorama::CStyleHotReload::Instance();
    hotReload.Enable(true);
    hotReload.WatchFile("resources/styles/main_menu.css");
    hotReload.WatchFile("resources/styles/base.css");
    #endif
    
    CreateUI();
}
```

### Пример 3: Кастомный callback

```cpp
// Отслеживать изменения и показывать уведомление
Panorama::CStyleHotReload::Instance().WatchFile(
    "resources/styles/login.css",
    [](const std::string& path) {
        LOG_INFO("Styles reloaded: {}", path);
        
        // Показать уведомление в игре
        ShowNotification("Styles reloaded!");
        
        // Кастомная логика
        ReapplyCustomStyles();
    }
);
```

## ⚙️ Конфигурация

### Интервал проверки

По умолчанию файлы проверяются каждые **0.5 секунды**. Можно изменить:

```cpp
Panorama::CStyleHotReload::Instance().SetCheckInterval(1.0f);  // 1 секунда
```

### Debug vs Release

Рекомендуется включать hot-reload **только в Debug режиме**:

```cpp
#ifdef _DEBUG
    // Hot reload только для разработки
    Panorama::CUIEngine::Instance().EnableHotReload(true);
#endif
```

## 🔍 Как это работает

### 1. Мониторинг файлов

Система использует `std::filesystem::last_write_time()` для отслеживания изменений:

```cpp
auto currentTime = std::filesystem::last_write_time(filePath);
if (currentTime != lastWriteTime) {
    // Файл изменился - перезагрузить!
}
```

### 2. Перезагрузка стилей

При обнаружении изменений:

1. Перезагружается CSS файл через `CStyleManager`
2. Вызывается `InvalidateStyle()` для всех панелей
3. Стили пересчитываются при следующем рендере

```cpp
void DefaultReloadCallback(const std::string& path) {
    // Reload stylesheet
    CStyleManager::Instance().LoadGlobalStyles(path);
    
    // Invalidate all panel styles
    auto* root = CUIEngine::Instance().GetRoot();
    std::function<void(CPanel2D*)> invalidate = [&](CPanel2D* panel) {
        panel->InvalidateStyle();
        for (auto* child : panel->GetChildren()) {
            invalidate(child);
        }
    };
    invalidate(root);
}
```

### 3. Update Loop

Hot-reload проверяется в основном цикле игры:

```cpp
void CUIEngine::Update(f32 deltaTime) {
    // Check for CSS changes
    CStyleHotReload::Instance().Update(deltaTime);
    
    // Update UI
    if (m_root) {
        UpdatePanelRecursive(m_root.get(), deltaTime);
    }
}
```

## 📊 Performance

### Overhead

- **Проверка файлов:** ~0.1ms каждые 0.5 сек (незаметно)
- **Перезагрузка стилей:** ~5-10ms (происходит только при изменении)
- **Влияние на FPS:** Минимальное (<1%)

### Оптимизация

```cpp
// Увеличить интервал для меньшего overhead
hotReload.SetCheckInterval(2.0f);  // Проверять каждые 2 секунды

// Отключить в Release
#ifndef _DEBUG
    hotReload.Enable(false);
#endif
```

## 🐛 Troubleshooting

### Стили не перезагружаются

**Проблема:** Изменения в CSS не применяются

**Решения:**
1. Проверьте что hot-reload включен:
   ```cpp
   LOG_INFO("Hot reload enabled: {}", 
       Panorama::CUIEngine::Instance().IsHotReloadEnabled());
   ```

2. Проверьте что файл отслеживается:
   ```cpp
   auto files = Panorama::CStyleHotReload::Instance().GetWatchedFiles();
   for (const auto& file : files) {
       LOG_INFO("Watching: {}", file);
   }
   ```

3. Проверьте путь к файлу:
   ```cpp
   // Используйте относительный путь от рабочей директории
   WatchStyleSheet("resources/styles/login.css");  // ✅
   WatchStyleSheet("login.css");                   // ❌
   ```

### Ошибки при перезагрузке

**Проблема:** CSS файл с синтаксической ошибкой

**Решение:** Проверьте логи:
```
[error] CStyleHotReload: Failed to reload 'login.css': Parse error at line 42
```

Исправьте ошибку и сохраните файл снова.

### Файл не найден

**Проблема:** `File not found: resources/styles/login.css`

**Решение:** Проверьте рабочую директорию:
```cpp
LOG_INFO("Current directory: {}", 
    std::filesystem::current_path().u8string());
```

Убедитесь что путь правильный относительно рабочей директории.

## 💡 Best Practices

### ✅ DO:

```cpp
// Включать только в Debug
#ifdef _DEBUG
    EnableHotReload(true);
#endif

// Отслеживать все используемые CSS файлы
WatchStyleSheet("resources/styles/base.css");
WatchStyleSheet("resources/styles/login.css");

// Использовать относительные пути
WatchStyleSheet("resources/styles/login.css");
```

### ❌ DON'T:

```cpp
// Не включать в Release
EnableHotReload(true);  // Без #ifdef _DEBUG

// Не использовать абсолютные пути
WatchStyleSheet("C:/Projects/Game/resources/styles/login.css");

// Не забывать отслеживать base.css
WatchStyleSheet("resources/styles/login.css");
// base.css тоже нужен!
```

## 🎨 Workflow для UI дизайна

### Типичный процесс:

1. **Запустить игру в Debug режиме**
   ```powershell
   cmake --build build --config Debug --target Game
   ./build/bin/Debug/Game.exe
   ```

2. **Открыть CSS файл в редакторе**
   ```
   resources/styles/login.css
   ```

3. **Изменить стили**
   ```css
   #ErrorLabel {
     background-color: rgba(255, 107, 107, 0.25);
     border-left-width: 4px;  /* Было 3px */
   }
   ```

4. **Сохранить (Ctrl+S)**
   - Игра автоматически перезагрузит стили
   - Изменения видны сразу!

5. **Итерировать**
   - Повторять шаги 3-4 пока не достигнете нужного результата
   - Не нужно перезапускать игру!

## 📈 Статистика

Посмотреть статистику hot-reload:

```cpp
auto stats = Panorama::CStyleHotReload::Instance().GetStats();

LOG_INFO("=== Hot Reload Stats ===");
LOG_INFO("Total reloads: {}", stats.totalReloads);
LOG_INFO("Failed reloads: {}", stats.failedReloads);
LOG_INFO("Last reloaded: {}", stats.lastReloadedFile);
LOG_INFO("Success rate: {:.1f}%", 
    100.0f * stats.totalReloads / (stats.totalReloads + stats.failedReloads));
```

## 🔧 Расширение

### Добавить поддержку других файлов

Можно расширить систему для отслеживания других типов файлов:

```cpp
// Отслеживать JSON конфиги
hotReload.WatchFile("config/ui_config.json", [](const std::string& path) {
    ReloadUIConfig(path);
});

// Отслеживать layout файлы
hotReload.WatchFile("layouts/main_menu.xml", [](const std::string& path) {
    ReloadLayout(path);
});
```

## 🎉 Заключение

Hot-reload система значительно ускоряет разработку UI:

- ⚡ **Мгновенная обратная связь** - видишь изменения сразу
- 🚀 **Быстрая итерация** - не нужно перезапускать игру
- 🎨 **Удобный workflow** - редактируй CSS и сохраняй
- 🐛 **Легкая отладка** - быстро тестируй разные варианты

Используй hot-reload для быстрой разработки UI! 🔥
