# Flexbox Layout System

## Overview

Panorama UI теперь поддерживает CSS Flexbox-подобную систему layout для гибкого позиционирования элементов.

## Основные концепции

### Flex Container
Элемент с `display: flex` становится flex-контейнером. Его дочерние элементы автоматически становятся flex-items.

### Main Axis и Cross Axis
- **Main axis**: основная ось (row = горизонталь, column = вертикаль)
- **Cross axis**: перпендикулярная ось

## CSS Properties

### Container Properties

#### display: flex
Включает flexbox layout для контейнера.

```css
#MyPanel {
    display: flex;
}
```

#### flex-direction
Определяет направление main axis.

```css
#MyPanel {
    display: flex;
    flex-direction: row;    /* горизонтально (default) */
    flex-direction: column; /* вертикально */
}
```

#### justify-content
Выравнивание по main axis.

```css
#MyPanel {
    display: flex;
    justify-content: start;         /* начало (default) */
    justify-content: center;        /* центр */
    justify-content: end;           /* конец */
    justify-content: space-between; /* равномерно с промежутками */
}
```

#### align-items
Выравнивание по cross axis.

```css
#MyPanel {
    display: flex;
    align-items: start;   /* начало (default) */
    align-items: center;  /* центр */
    align-items: end;     /* конец */
    align-items: stretch; /* растянуть на всю высоту/ширину */
}
```

#### gap
Промежуток между элементами (в пикселях).

```css
#MyPanel {
    display: flex;
    gap: 16px;
}
```

### Item Properties

#### flex-grow
Коэффициент роста элемента (как он занимает свободное пространство).

```css
.Item {
    flex-grow: 0; /* не растёт (default) */
    flex-grow: 1; /* занимает доступное пространство */
}
```

#### flex-shrink
Коэффициент сжатия элемента при нехватке места.

```css
.Item {
    flex-shrink: 1; /* может сжиматься (default) */
    flex-shrink: 0; /* не сжимается */
}
```

#### flex-basis
Базовый размер элемента до распределения пространства.

```css
.Item {
    flex-basis: auto;  /* использовать natural size (default) */
    flex-basis: 100px; /* фиксированный размер */
}
```

## Примеры использования

### Горизонтальное меню с центрированием

```css
#MenuBar {
    display: flex;
    flex-direction: row;
    justify-content: center;
    align-items: center;
    gap: 20px;
    height: 60px;
}

.MenuItem {
    flex-grow: 0;
    flex-shrink: 0;
}
```

### Вертикальная форма

```css
#LoginForm {
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    gap: 16px;
    width: 420px;
}

.FormInput {
    width: 100%;
    flex-grow: 0;
}

.SubmitButton {
    width: 100%;
    flex-grow: 0;
}
```

### Растягивающийся контент

```css
#MainLayout {
    display: flex;
    flex-direction: column;
    height: 100%;
}

#Header {
    flex-grow: 0;
    flex-shrink: 0;
    height: 80px;
}

#Content {
    flex-grow: 1; /* занимает всё доступное пространство */
    flex-shrink: 1;
}

#Footer {
    flex-grow: 0;
    flex-shrink: 0;
    height: 60px;
}
```

### Space-between layout

```css
#Toolbar {
    display: flex;
    flex-direction: row;
    justify-content: space-between;
    align-items: center;
    padding: 10px;
}

.ToolbarLeft {
    flex-grow: 0;
}

.ToolbarRight {
    flex-grow: 0;
}
```

## Архитектура реализации

### Фазы Layout

1. **Measure Phase**: Определение natural size каждого элемента
2. **Grow/Shrink Phase**: Распределение свободного пространства или сжатие
3. **Position Phase**: Финальное позиционирование элементов

### Алгоритм

```cpp
// Phase 1: Measure
for each child:
    child->PerformLayout(contentBounds)
    baseSize = child->GetActualSize()
    totalFixed += baseSize

remaining = mainSize - totalFixed - gaps

// Phase 2: Grow/Shrink
if remaining > 0 and totalGrow > 0:
    distribute remaining space proportionally by flex-grow
else if remaining < 0:
    shrink items proportionally by flex-shrink * baseSize

// Phase 3: Position
cursor = ComputeJustifyOffset()
for each item:
    position item at cursor
    apply cross-axis alignment
    cursor += item.finalSize + gap
```

## Совместимость с flow-children

Flexbox и традиционный `flow-children` взаимоисключающие:
- Если `display: flex` установлен, `flow-children` игнорируется
- Если `display: flex` не установлен, используется `flow-children`

## Performance Notes

- Flexbox делает два прохода layout (measure + position)
- Для простых случаев `flow-children` может быть быстрее
- Используйте flexbox когда нужно:
  - Динамическое распределение пространства
  - Центрирование
  - Space-between/around
  - Stretch элементов

## TODO / Future Improvements

- [ ] flex-wrap support
- [ ] align-content для multi-line flex
- [ ] order property для изменения порядка
- [ ] Учёт margins в cross-axis alignment
- [ ] Separate Measure() pass для оптимизации
- [ ] Debug overlay для визуализации flex layout

## Примеры из проекта

См. `resources/styles/login.css` для примеров использования flexbox в login screen.
