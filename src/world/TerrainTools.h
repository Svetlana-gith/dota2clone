#pragma once

#include "Components.h"
#include "core/Types.h"

namespace WorldEditor::TerrainTools {

// Brush types for terrain editing
enum class BrushType : u8 {
    Raise,      // Поднимает terrain
    Lower,      // Опускает terrain
    Flatten,    // Выравнивает до target height
    Smooth,     // Сглаживает неровности
    Noise,      // Добавляет процедурный шум
    Erode       // Симуляция эрозии
};

// Brush falloff patterns
enum class FalloffType : u8 {
    Linear,     // Линейное затухание
    Smooth,     // Smooth step (3x^2 - 2x^3)
    Gaussian,   // Гауссово распределение
    Sharp       // Резкие границы
};

// Brush configuration
struct BrushSettings {
    BrushType type = BrushType::Raise;
    FalloffType falloff = FalloffType::Gaussian;
    float radius = 4.0f;        // world units
    float strength = 6.0f;      // units per second
    float targetHeight = 0.0f;  // for Flatten brush
    float noiseScale = 1.0f;    // for Noise brush
    float smoothFactor = 0.5f;  // for Smooth brush
};

// Noise generation parameters
struct NoiseSettings {
    float frequency = 0.1f;
    float amplitude = 10.0f;
    int octaves = 4;
    float lacunarity = 2.0f;
    float persistence = 0.5f;
    int seed = 12345;
};

// Terrain modification result
struct ModificationResult {
    bool modified = false;
    Vec2i minAffected = Vec2i(0);
    Vec2i maxAffected = Vec2i(0);
    size_t verticesChanged = 0;
};

// Core terrain modification functions
class TerrainBrush {
public:
    static ModificationResult applyBrush(
        TerrainComponent& terrain,
        const Vec3& worldPos,
        const BrushSettings& settings,
        float deltaTime
    );

    static ModificationResult generateNoise(
        TerrainComponent& terrain,
        const NoiseSettings& settings
    );

    static ModificationResult importHeightmap(
        TerrainComponent& terrain,
        const String& filePath
    );

    static bool exportHeightmap(
        const TerrainComponent& terrain,
        const String& filePath,
        bool normalize = true
    );

    // Utility functions (public for TexturePainter)
    static float calculateFalloff(float distance, float radius, FalloffType type);

private:
    static float sampleNoise(float x, float y, const NoiseSettings& settings);
    static float smoothHeight(const TerrainComponent& terrain, int x, int y, float factor);
};

// Texture painting system for multi-layer terrain materials
struct TextureLayer {
    String diffuseTexture;
    String normalTexture;
    float tiling = 1.0f;
    float strength = 1.0f;
};

struct TerrainMaterial {
    Vector<TextureLayer> layers;
    Vector<f32> blendWeights; // Per-vertex blend weights for each layer
    int activeLayer = 0;
};

class TexturePainter {
public:
    static bool paintTexture(
        TerrainMaterial& material,
        const TerrainComponent& terrain,
        const Vec3& worldPos,
        int layerIndex,
        float radius,
        float strength,
        float deltaTime
    );

    static void normalizeWeights(TerrainMaterial& material, size_t vertexIndex);
};

} // namespace WorldEditor::TerrainTools