# Lighting System Documentation

## 🏗️ Архитектура освещения

Система освещения построена по принципам кастомного движка с модульной архитектурой:

```
LightingSystem
├── LightingConstants (GPU constant buffer)
├── DirectionalLight (основной источник света)
├── AmbientLight (рассеянное освещение)
└── PhongShading (в pixel shader)
```

## 🎯 Основные компоненты

### LightingSystem Class
Управляет всеми параметрами освещения и обновляет GPU constant buffer:

```cpp
struct LightingConstants {
    Vec4 lightDirection;    // направление света
    Vec4 lightColor;        // цвет света (теплый белый)
    Vec4 ambientColor;      // ambient (холодный)
    Vec4 cameraPosition;    // для specular
    Vec4 materialParams;    // diffuse, specular, shininess
};
```

### Terrain Shaders с освещением
- **TerrainVertexShader.hlsl** - трансформация вершин + передача данных для освещения
- **TerrainPixelShader.hlsl** - Phong lighting model с gamma correction

## 🎮 Визуальные улучшения

### Динамическое освещение
- **Directional Light** - имитирует солнечный свет
- **Медленное вращение** света для более динамичной картинки
- **Теплый/холодный контраст** - теплый основной свет, холодный ambient

### Phong Lighting Model
```hlsl
// Ambient component
float3 ambient = ambientColor.rgb * baseColor;

// Diffuse component (Lambert)
float NdotL = max(dot(normal, lightDir), 0.0);
float3 diffuse = lightColor.rgb * baseColor * NdotL * diffuseStrength;

// Specular component (Phong)
float3 reflectDir = reflect(-lightDir, normal);
float RdotV = max(dot(reflectDir, viewDir), 0.0);
float spec = pow(RdotV, shininess);
float3 specular = lightColor.rgb * spec * specularStrength;
```

### Gamma Correction
```hlsl
// Simple gamma correction для более реалистичного вида
finalColor = pow(finalColor, 1.0/2.2);
```

## 🔧 Интеграция с движком

### Constant Buffer Layout
- **b0**: PerObjectConstants (world matrix, view-proj matrix)
- **b1**: LightingConstants (освещение)
- **b2**: MaterialConstants (материал)

### Обновление в реальном времени
```cpp
// В main loop
renderer.UpdateLighting(camera.position, totalTime);
```

### Привязка к RenderSystem
```cpp
// В World.cpp
if (lightingSystem_ && lightingSystem_->getLightingConstantBuffer()) {
    commandList->SetGraphicsRootConstantBufferView(1, 
        lightingSystem_->getLightingConstantBuffer()->GetGPUVirtualAddress());
}
```

## 🚀 Результаты

### Визуальные улучшения:
- **Объемность terrain** - теперь видны подъемы и впадины
- **Реалистичные тени** - четкое понимание рельефа
- **Материальность** - terrain выглядит как настоящая поверхность
- **Динамичность** - медленно меняющееся освещение

### Производительность:
- **Minimal overhead** - один дополнительный constant buffer
- **GPU-friendly** - все вычисления в шейдерах
- **Scalable** - легко добавить point/spot lights

## 🔮 Следующие шаги

1. **Shadow Mapping** - реальные тени от объектов
2. **Multiple Lights** - point lights, spot lights
3. **PBR Materials** - physically based rendering
4. **HDR Pipeline** - high dynamic range
5. **Atmospheric Scattering** - реалистичное небо

## 🧠 Архитектурные решения

### Почему Phong вместо PBR?
- **Простота реализации** - быстрый результат
- **Понятность** - легко настраивать параметры
- **Производительность** - меньше вычислений в pixel shader
- **Основа для PBR** - легко расширить в будущем

### Дизайн-паттерны:
- **Component System** - LightingSystem как отдельный компонент
- **Constant Buffer Management** - централизованное управление GPU данными
- **Shader Modularity** - отдельные шейдеры для terrain
- **Real-time Updates** - динамическое обновление параметров