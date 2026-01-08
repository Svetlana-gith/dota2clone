# CSS System - Quick Reference

## 🎨 Что доступно

### 1. Улучшенные стили логина
- Более просторные input поля (13% высоты, 17px шрифт)
- Профессиональный ErrorLabel с фоном и границей
- Оптимизированная раскладка

### 2. Tailwind-Inspired Utility System
- 100+ переиспользуемых CSS классов
- Spacing, colors, typography, borders, layout, effects
- Быстрое прототипирование компонентов

### 3. CSS Hot Reload
- Автоматическая перезагрузка при изменении CSS
- Без перезапуска игры
- Debug-only режим

---

## ⚡ Quick Start

### Hot Reload (3 шага)

```cpp
// 1. В OnEnter() вашего State
#ifdef _DEBUG
Panorama::CUIEngine::Instance().EnableHotReload(true);
Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/your_style.css");
#endif

// 2. Запусти игру в Debug
// 3. Редактируй CSS и сохраняй - изменения применятся автоматически!
```

### Tailwind-Style классы

```cpp
// Создай элемент
auto label = std::make_shared<Panorama::CLabel>("Text", "MyLabel");

// Добавь utility классы
label->AddClass("text-2xl");      // Размер 28px
label->AddClass("text-gold");     // Золотой цвет
label->AddClass("font-bold");     // Жирный
label->AddClass("p-4");           // Padding 16px
label->AddClass("bg-card");       // Темный фон
label->AddClass("rounded-lg");    // Скругление 12px
```

---

## 📚 Документация

| Файл | Описание |
|------|----------|
| `HotReload_QuickStart.md` | Hot reload за 3 шага |
| `HotReload_Guide.md` | Полное руководство по hot-reload |
| `Tailwind_CheatSheet.md` | Шпаргалка по utility классам |
| `TailwindCSS_Approach.md` | Полное руководство по Tailwind-style |
| `CSS_Comparison.md` | Сравнение подходов |
| `Visual_Comparison.txt` | Визуальное сравнение |
| `Session_Summary.md` | Полный summary сессии |

---

## 🎯 Примеры

### Error Alert
```cpp
errorLabel->AddClass("bg-error border-l-3 border-error rounded text-error px-3");
```

### Primary Button
```cpp
button->AddClass("bg-gold rounded-md text-lg font-semibold shadow");
```

### Input Field
```cpp
input->AddClass("bg-input border-2 border-input rounded-md px-4 text-white");
```

---

## 🔧 Файлы

### CSS
- `resources/styles/base.css` - Utility классы
- `resources/styles/login.css` - Улучшенные стили логина
- `resources/styles/login-tailwind.css` - Tailwind версия

### Code
- `src/game/ui/panorama/core/CStyleHotReload.h` - Hot reload система
- `src/game/ui/login/LoginForm_Tailwind_Example.cpp` - Примеры

---

## 💡 Tips

- ✅ Используй hot-reload только в Debug режиме
- ✅ Отслеживай base.css если используешь utility классы
- ✅ Комбинируй utility классы для создания компонентов
- ✅ Используй ID для позиционирования, классы для стиля

---

**Начни с:** `docs/HotReload_QuickStart.md` или `docs/Tailwind_CheatSheet.md`
