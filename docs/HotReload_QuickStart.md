# CSS Hot Reload - Quick Start

## 🔥 3 шага для включения

### 1. Добавь в OnEnter()

```cpp
void YourState::OnEnter() {
    // Load stylesheet
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/your_style.css");
    
    // Enable hot reload (Debug only)
    #ifdef _DEBUG
    Panorama::CUIEngine::Instance().EnableHotReload(true);
    Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/your_style.css");
    Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/base.css");
    #endif
    
    CreateUI();
}
```

### 2. Запусти игру в Debug

```powershell
cmake --build build --config Debug --target Game
./build/bin/Debug/Game.exe
```

### 3. Редактируй CSS и сохраняй!

```css
/* Измени что-то в your_style.css */
#MyButton {
  background-color: rgba(255, 0, 0, 1.0);  /* Красный! */
}
```

**Сохрани (Ctrl+S)** → Стили обновятся автоматически! ✨

---

## 📋 Шпаргалка команд

```cpp
// Включить hot-reload
Panorama::CUIEngine::Instance().EnableHotReload(true);

// Отслеживать файл
Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/login.css");

// Проверить статус
bool enabled = Panorama::CUIEngine::Instance().IsHotReloadEnabled();

// Продвинутое использование
auto& hr = Panorama::CStyleHotReload::Instance();
hr.SetCheckInterval(1.0f);           // Интервал проверки
hr.CheckNow();                       // Принудительная проверка
hr.UnwatchFile("path/to/file.css"); // Перестать отслеживать
hr.UnwatchAll();                     // Перестать отслеживать все

// Статистика
auto stats = hr.GetStats();
LOG_INFO("Reloads: {}", stats.totalReloads);
```

---

## 🎯 Типичный workflow

1. **Запусти игру** (Debug режим)
2. **Открой CSS** в редакторе
3. **Измени стили**
4. **Сохрани** (Ctrl+S)
5. **Смотри результат** в игре
6. **Повторяй** шаги 3-5

Не нужно перезапускать игру! 🚀

---

## ⚠️ Важно

- ✅ Включай **только в Debug** режиме (`#ifdef _DEBUG`)
- ✅ Отслеживай **base.css** если используешь utility классы
- ✅ Используй **относительные пути** от рабочей директории
- ❌ Не включай в **Release** сборке

---

## 🐛 Не работает?

### Проверь логи:

```
[info] CStyleHotReload: Watching file 'resources/styles/login.css'
[info] CStyleHotReload: File changed, reloading: resources/styles/login.css
[info] CStyleHotReload: Successfully reloaded 'resources/styles/login.css'
```

### Если нет логов:

1. Проверь что hot-reload включен
2. Проверь путь к файлу
3. Проверь что файл существует
4. Проверь рабочую директорию

---

## 📚 Полная документация

См. `docs/HotReload_Guide.md` для подробной информации.
