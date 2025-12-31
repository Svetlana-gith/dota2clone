# Terrain Tools Documentation

## 🏗️ Архитектура системы

Система Terrain Tools построена по принципам кастомного движка с модульной архитектурой:

```
TerrainToolSystem
├── BrushSystem (кисти разных типов)
├── HeightmapOperations (алгоритмы модификации)
├── TexturePainting (многослойное текстурирование) 
├── NoiseGeneration (процедурная генерация)
└── ImportExport (RAW/PNG heightmaps)
```

## 🎯 Основные компоненты

### TerrainBrush - Система кистей
Профессиональная система sculpting с различными типами кистей:

#### Типы кистей (BrushType):
- **Raise** - Поднимает terrain
- **Lower** - Опускает terrain  
- **Flatten** - Выравнивает до target height
- **Smooth** - Сглаживает неровности
- **Noise** - Добавляет процедурный шум
- **Erode** - Симуляция эрозии

#### Типы затухания (FalloffType):
- **Linear** - Линейное затухание
- **Smooth** - Smooth step (3x²-2x³)
- **Gaussian** - Гауссово распределение  
- **Sharp** - Резкие границы

### TexturePainter - Многослойное текстурирование
Система для рисования текстур на terrain с поддержкой до 4 слоев:

- Per-vertex blend weights
- Автоматическая нормализация весов
- Настраиваемый tiling для каждого слоя
- Поддержка diffuse + normal maps

## 🎮 Управление

### Height Sculpting
- **Ctrl + LMB** - Sculpting кистью
- **Shift + Ctrl + LMB** - Инвертирует эффект кисти
- Настройки в панели "Terrain Tools"

### Texture Painting  
- **T + LMB** - Рисование активным слоем текстуры
- Настройки в панели "Texture Painting"

## 🔧 API Reference

### BrushSettings
```cpp
struct BrushSettings {
    BrushType type = BrushType::Raise;
    FalloffType falloff = FalloffType::Gaussian;
    float radius = 4.0f;        // world units
    float strength = 6.0f;      // units per second
    float targetHeight = 0.0f;  // for Flatten brush
    float noiseScale = 1.0f;    // for Noise brush
    float smoothFactor = 0.5f;  // for Smooth brush
};
```

### Основные методы
```cpp
// Применить кисть к terrain
ModificationResult TerrainBrush::applyBrush(
    TerrainComponent& terrain,
    const Vec3& worldPos,
    const BrushSettings& settings,
    float deltaTime
);

// Генерация процедурного шума
ModificationResult TerrainBrush::generateNoise(
    TerrainComponent& terrain,
    const NoiseSettings& settings
);

// Импорт/экспорт heightmap
ModificationResult TerrainBrush::importHeightmap(
    TerrainComponent& terrain,
    const String& filePath
);

bool TerrainBrush::exportHeightmap(
    const TerrainComponent& terrain,
    const String& filePath,
    bool normalize = true
);
```

### Texture Painting
```cpp
bool TexturePainter::paintTexture(
    TerrainMaterial& material,
    const TerrainComponent& terrain,
    const Vec3& worldPos,
    int layerIndex,
    float radius,
    float strength,
    float deltaTime
);
```

## 🚀 Производительность

### Оптимизации:
- **Локальная модификация** - изменяются только затронутые вершины
- **Batch updates** - группировка изменений для GPU upload
- **Deferred mesh rebuild** - перестроение mesh только при необходимости
- **Efficient falloff calculation** - оптимизированные алгоритмы затухания

### Метрики:
- `ModificationResult.verticesChanged` - количество измененных вершин
- `ModificationResult.minAffected/maxAffected` - область изменений
- Автоматический clamp к terrain height range

## 🎨 UI Integration

### Terrain Panel
- Выбор типа кисти и затухания
- Настройка радиуса и силы
- Tool-specific параметры (target height, smooth factor, etc.)
- Быстрые операции (Generate Noise, Import/Export)

### Texture Painting Panel  
- Управление слоями текстур
- Настройка blend weights
- Добавление новых слоев (до 4)
- Предварительный просмотр результата

## 🔮 Следующие шаги

1. **Реализация stb_image** для реального импорта/экспорта heightmaps
2. **Advanced erosion** - гидравлическая и термальная эрозия
3. **Texture streaming** - динамическая загрузка больших текстур
4. **Undo/Redo system** - история изменений terrain
5. **Multi-threading** - параллельная обработка больших terrain
6. **GPU compute shaders** - перенос алгоритмов на GPU

## 🧠 Архитектурные решения

### Почему кастомная система?
- **Полный контроль** над алгоритмами и производительностью
- **Модульность** - легко добавлять новые типы кистей
- **Интеграция с ECS** - terrain как обычные компоненты
- **DirectX 12 optimization** - прямая работа с GPU ресурсами

### Дизайн-паттерны:
- **Strategy Pattern** - различные типы кистей и затухания
- **Command Pattern** - для undo/redo (будущая реализация)
- **Component System** - terrain как набор компонентов
- **RAII** - автоматическое управление GPU ресурсами