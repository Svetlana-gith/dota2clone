# Tailwind-Style CSS Шпаргалка для Panorama UI

## 🎨 Быстрый старт

### Базовый пример
```cpp
// Создаем элемент
auto label = std::make_shared<Panorama::CLabel>("Hello", "MyLabel");

// Добавляем utility классы
label->AddClass("text-2xl");      // Размер 28px
label->AddClass("text-gold");     // Золотой цвет
label->AddClass("font-bold");     // Жирный шрифт
label->AddClass("p-4");           // Padding 16px
label->AddClass("bg-card");       // Темный фон
label->AddClass("rounded-lg");    // Скругление 12px
```

## 📏 Spacing

| Класс | Значение | Описание |
|-------|----------|----------|
| `p-0` | 0px | Без padding |
| `p-1` | 4px | Минимальный |
| `p-2` | 8px | Маленький |
| `p-3` | 12px | Средний |
| `p-4` | 16px | Стандартный |
| `p-6` | 24px | Большой |
| `p-8` | 32px | Очень большой |

**Направления:**
- `px-4` - horizontal (left + right)
- `py-4` - vertical (top + bottom)
- `pt-4`, `pb-4`, `pl-4`, `pr-4` - отдельные стороны

## 🎨 Цвета

### Фон
```cpp
.AddClass("bg-dark")        // #0A0E17 - Темный фон
.AddClass("bg-card")        // #12161F - Карточка
.AddClass("bg-input")       // #1A1F2A - Input поле
.AddClass("bg-gold")        // #D4A84B - Золотой
.AddClass("bg-cyan")        // #4ECDC4 - Cyan
.AddClass("bg-error")       // Красный (15% opacity)
.AddClass("bg-success")     // Зеленый (15% opacity)
```

### Текст
```cpp
.AddClass("text-white")     // Белый
.AddClass("text-gray")      // Серый
.AddClass("text-gold")      // Золотой
.AddClass("text-cyan")      // Cyan
.AddClass("text-error")     // Красный
.AddClass("text-success")   // Зеленый
```

## 📝 Типографика

### Размеры
```cpp
.AddClass("text-xs")        // 10px
.AddClass("text-sm")        // 12px
.AddClass("text-base")      // 14px (default)
.AddClass("text-lg")        // 18px
.AddClass("text-xl")        // 22px
.AddClass("text-2xl")       // 28px
.AddClass("text-3xl")       // 36px
.AddClass("text-4xl")       // 48px
```

### Вес шрифта
```cpp
.AddClass("font-light")     // 300
.AddClass("font-normal")    // 400
.AddClass("font-medium")    // 500
.AddClass("font-semibold")  // 600
.AddClass("font-bold")      // 700
```

## 🔲 Границы

### Ширина
```cpp
.AddClass("border")         // 1px
.AddClass("border-2")       // 2px
.AddClass("border-3")       // 3px
.AddClass("border-l-3")     // Левая граница 3px
.AddClass("border-t-2")     // Верхняя граница 2px
```

### Цвета границ
```cpp
.AddClass("border-gold")    // Золотая
.AddClass("border-cyan")    // Cyan
.AddClass("border-error")   // Красная
.AddClass("border-input")   // Темная
```

### Скругление
```cpp
.AddClass("rounded-none")   // 0px
.AddClass("rounded-sm")     // 4px
.AddClass("rounded")        // 6px
.AddClass("rounded-md")     // 8px
.AddClass("rounded-lg")     // 12px
.AddClass("rounded-xl")     // 16px
.AddClass("rounded-full")   // 9999px (круг)
```

## 🎭 Эффекты

### Прозрачность
```cpp
.AddClass("opacity-0")      // 0%
.AddClass("opacity-50")     // 50%
.AddClass("opacity-75")     // 75%
.AddClass("opacity-90")     // 90%
.AddClass("opacity-95")     // 95%
.AddClass("opacity-100")    // 100%
```

### Тени
```cpp
.AddClass("shadow-sm")      // Маленькая тень
.AddClass("shadow")         // Средняя тень
.AddClass("shadow-lg")      // Большая тень
```

## 📐 Layout

### Размеры
```cpp
.AddClass("w-full")         // width: 100%
.AddClass("h-full")         // height: 100%
.AddClass("w-auto")         // width: fit-children
.AddClass("h-auto")         // height: fit-children
```

### Flow (Flexbox-like)
```cpp
.AddClass("flex-col")       // flow-children: down
.AddClass("flex-row")       // flow-children: right
.AddClass("flex-wrap")      // flow-children: right-wrap
```

### Выравнивание
```cpp
.AddClass("items-center")   // vertical-align: center
.AddClass("items-start")    // vertical-align: top
.AddClass("items-end")      // vertical-align: bottom

.AddClass("justify-center") // horizontal-align: center
.AddClass("justify-start")  // horizontal-align: left
.AddClass("justify-end")    // horizontal-align: right
```

## 🎯 Готовые паттерны

### Error Alert
```cpp
errorLabel->AddClass("bg-error");
errorLabel->AddClass("border-l-3");
errorLabel->AddClass("border-error");
errorLabel->AddClass("rounded");
errorLabel->AddClass("text-error");
errorLabel->AddClass("px-3");
```

### Primary Button
```cpp
button->AddClass("bg-gold");
button->AddClass("rounded-md");
button->AddClass("text-lg");
button->AddClass("font-semibold");
button->AddClass("shadow");
```

### Input Field
```cpp
input->AddClass("bg-input");
input->AddClass("border-2");
input->AddClass("border-input");
input->AddClass("rounded-md");
input->AddClass("px-4");
input->AddClass("text-white");
```

### Card Container
```cpp
card->AddClass("bg-card");
card->AddClass("rounded-lg");
card->AddClass("border");
card->AddClass("border-gold-dim");
card->AddClass("p-6");
card->AddClass("shadow-lg");
```

### Success Message
```cpp
success->AddClass("bg-success");
success->AddClass("border-l-3");
success->AddClass("text-success");
success->AddClass("rounded");
success->AddClass("px-3");
success->AddClass("py-2");
```

## 💡 Советы

### ✅ DO:
```cpp
// Комбинируй классы для создания компонентов
label->AddClass("text-2xl");
label->AddClass("text-gold");
label->AddClass("font-bold");

// Используй ID для позиционирования
auto label = std::make_shared<Panorama::CLabel>("Text", "MyLabel");
// В CSS: #MyLabel { x: 50%; y: 50%; }
```

### ❌ DON'T:
```cpp
// Не дублируй стили в CSS если есть utility класс
// Плохо:
#MyLabel {
  color: rgba(212, 168, 75, 1.0);
  font-size: 28px;
}

// Хорошо:
label->AddClass("text-gold");
label->AddClass("text-2xl");
```

## 🔧 Расширение

Добавь свои классы в `resources/styles/base.css`:

```css
/* Новый spacing */
.p-10 { padding: 40px; }

/* Новый цвет */
.bg-purple { background-color: rgba(147, 51, 234, 1.0); }
.text-purple { color: rgba(147, 51, 234, 1.0); }

/* Новый размер */
.text-7xl { font-size: 96px; }
```

## 📚 Полная документация

См. `docs/TailwindCSS_Approach.md` для подробной информации.
