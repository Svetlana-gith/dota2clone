# Tailwind-Inspired CSS System для Panorama UI

## Обзор

Мы создали **utility-first CSS систему** вдохновленную TailwindCSS, но адаптированную для кастомной Panorama UI системы в C++ игровом движке.

## Структура

```
resources/styles/
├── base.css              # Utility классы (Tailwind-style)
├── login.css             # Традиционный подход (ID-based)
└── login-tailwind.css    # Tailwind-inspired подход
```

## Доступные Utility Классы

### Spacing (Padding)
```css
.p-0, .p-1, .p-2, .p-3, .p-4, .p-6, .p-8
.px-2, .px-3, .px-4  /* horizontal */
.py-2, .py-3, .py-4  /* vertical */
```

### Spacing (Margin)
```css
.m-0, .m-1, .m-2, .m-3, .m-4
.mx-auto  /* center horizontally */
.mt-2, .mt-3, .mt-4, .mb-2, .mb-3, .mb-4
```

### Colors (Background)
```css
.bg-transparent, .bg-dark, .bg-card, .bg-input
.bg-gold, .bg-gold-light, .bg-cyan
.bg-error, .bg-success, .bg-warning
```

### Colors (Text)
```css
.text-white, .text-gray, .text-muted
.text-gold, .text-gold-light, .text-cyan
.text-error, .text-success, .text-warning
```

### Typography
```css
/* Size Scale */
.text-xs, .text-sm, .text-base, .text-lg, .text-xl
.text-2xl, .text-3xl, .text-4xl, .text-5xl, .text-6xl

/* Semantic */
.small, .caption, .body, .subheading, .heading
.title, .display, .hero

/* Weight */
.font-light, .font-normal, .font-medium, .font-semibold, .font-bold

/* Alignment */
.text-left, .text-center, .text-right
```

### Borders
```css
.border, .border-2, .border-3
.border-l-3, .border-t-2
.border-gold, .border-gold-dim, .border-cyan
.border-input, .border-error
```

### Border Radius
```css
.rounded-none, .rounded-sm, .rounded, .rounded-md
.rounded-lg, .rounded-xl, .rounded-full
```

### Layout
```css
.w-full, .h-full, .w-auto, .h-auto
.flex-col, .flex-row, .flex-wrap
.items-center, .items-start, .items-end
.justify-center, .justify-start, .justify-end
```

### Effects
```css
.opacity-0, .opacity-50, .opacity-75, .opacity-90, .opacity-95, .opacity-100
.shadow-sm, .shadow, .shadow-lg
```

## Примеры использования

### Пример 1: Error Label (Традиционный vs Tailwind)

**Традиционный подход (login.css):**
```css
#ErrorLabel {
  width: 87%;
  height: 7%;
  background-color: rgba(255, 107, 107, 0.15);
  border-left-width: 3px;
  border-color: rgba(255, 107, 107, 1.0);
  border-radius: 6px;
  color: rgba(255, 107, 107, 1.0);
  font-size: 15px;
  padding-left: 12px;
  padding-right: 12px;
  x: 6%;
  y: 49%;
}
```

**Tailwind-inspired подход:**
```cpp
// C++ код
m_errorLabel = std::make_shared<Panorama::CLabel>("", "ErrorLabel");
m_errorLabel->AddClass("bg-error");
m_errorLabel->AddClass("border-l-3");
m_errorLabel->AddClass("border-error");
m_errorLabel->AddClass("rounded");
m_errorLabel->AddClass("text-error");
m_errorLabel->AddClass("text-base");
m_errorLabel->AddClass("px-3");
m_errorLabel->SetVisible(false);
m_container->AddChild(m_errorLabel);
```

```css
/* CSS - только позиционирование */
#ErrorLabel {
  width: 87%;
  height: 7%;
  x: 6%;
  y: 49%;
}
```

### Пример 2: Input Field

**C++ код:**
```cpp
m_usernameInput = std::make_shared<Panorama::CTextEntry>("UsernameInput");
m_usernameInput->AddClass("bg-input");
m_usernameInput->AddClass("border-2");
m_usernameInput->AddClass("border-input");
m_usernameInput->AddClass("rounded-md");
m_usernameInput->AddClass("px-4");
m_usernameInput->AddClass("text-lg");
m_usernameInput->AddClass("text-white");
m_usernameInput->SetPlaceholder("Enter your username");
```

**CSS для focus state:**
```css
.LoginInput:focus {
  border-color: rgba(212, 168, 75, 0.8);
}
```

### Пример 3: Primary Button

**C++ код:**
```cpp
m_primaryButton = std::make_shared<Panorama::CButton>("ENTER THE GAME", "PrimaryButton");
m_primaryButton->AddClass("bg-gold");
m_primaryButton->AddClass("rounded-md");
m_primaryButton->AddClass("text-lg");
m_primaryButton->AddClass("font-semibold");
m_primaryButton->SetOnActivate([this]() {
    if (m_onSubmit) m_onSubmit();
});
```

**CSS для hover/active:**
```css
#PrimaryButton:hover {
  background-color: rgba(245, 215, 142, 1.0);
}

#PrimaryButton:active {
  background-color: rgba(139, 105, 20, 1.0);
}
```

### Пример 4: Form Title

**C++ код:**
```cpp
m_titleLabel = std::make_shared<Panorama::CLabel>("WELCOME BACK", "FormTitle");
m_titleLabel->AddClass("text-2xl");
m_titleLabel->AddClass("text-white");
m_titleLabel->AddClass("font-semibold");
```

## Преимущества Tailwind-подхода

### ✅ Плюсы:
1. **Переиспользуемость** - классы можно комбинировать
2. **Консистентность** - единая система spacing/colors
3. **Быстрая разработка** - не нужно придумывать имена классов
4. **Легко менять** - просто добавить/убрать класс
5. **Меньше CSS** - utility классы переиспользуются

### ❌ Минусы:
1. **Много классов в C++** - код может быть длиннее
2. **Нет автокомплита** - в отличие от веб-Tailwind
3. **Позиционирование** - все равно нужны ID для x/y координат

## Гибридный подход (Рекомендуется)

Используй **комбинацию** обоих подходов:

```cpp
// ID для позиционирования и уникальных свойств
m_errorLabel = std::make_shared<Panorama::CLabel>("", "ErrorLabel");

// Utility классы для визуального стиля
m_errorLabel->AddClass("bg-error");
m_errorLabel->AddClass("border-l-3");
m_errorLabel->AddClass("border-error");
m_errorLabel->AddClass("rounded");
m_errorLabel->AddClass("text-error");
m_errorLabel->AddClass("px-3");
```

```css
/* ID - позиционирование */
#ErrorLabel {
  width: 87%;
  height: 7%;
  x: 6%;
  y: 49%;
}

/* Utility классы - стиль (в base.css) */
.bg-error { background-color: rgba(255, 107, 107, 0.15); }
.border-l-3 { border-left-width: 3px; }
.border-error { border-color: rgba(255, 107, 107, 1.0); }
.rounded { border-radius: 6px; }
.text-error { color: rgba(255, 107, 107, 1.0); }
.px-3 { padding-left: 12px; padding-right: 12px; }
```

## Расширение системы

Чтобы добавить новые utility классы, редактируй `resources/styles/base.css`:

```css
/* Новые spacing значения */
.p-10 { padding-left: 40px; padding-right: 40px; padding-top: 40px; padding-bottom: 40px; }

/* Новые цвета */
.bg-purple { background-color: rgba(147, 51, 234, 1.0); }
.text-purple { color: rgba(147, 51, 234, 1.0); }

/* Новые размеры */
.text-7xl { font-size: 96px; }
```

## Миграция существующего кода

Не нужно переписывать весь код сразу. Используй Tailwind-подход для:
- ✅ Новых компонентов
- ✅ Часто меняющихся элементов
- ✅ Переиспользуемых паттернов

Оставь традиционный подход для:
- ✅ Сложного позиционирования
- ✅ Уникальных компонентов
- ✅ Анимаций и transitions

## Заключение

Tailwind-inspired подход дает тебе **гибкость и скорость разработки**, сохраняя при этом возможность использовать традиционный CSS где это нужно.

Выбирай подход в зависимости от задачи! 🎨
