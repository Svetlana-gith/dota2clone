#include "TerrainTools.h"
#include "TerrainMesh.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace WorldEditor::TerrainTools {

ModificationResult TerrainBrush::applyBrush(
    TerrainComponent& terrain,
    const Vec3& worldPos,
    const BrushSettings& settings,
    float deltaTime
) {
    ModificationResult result;
    
    // Более строгие ограничения для стабильности
    BrushSettings safeSettings = settings;
    safeSettings.strength = std::clamp(safeSettings.strength, 0.01f, 5.0f);  // Уменьшил максимум
    safeSettings.radius = std::clamp(safeSettings.radius, 0.1f, 20.0f);     // Уменьшил максимум
    
    // Ограничиваем deltaTime для предотвращения резких скачков
    deltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
    
    const int w = terrain.resolution.x;
    const int h = terrain.resolution.y;
    
    if (w < 2 || h < 2 || terrain.heightmap.empty()) {
        return result;
    }

    // Convert world position to terrain grid coordinates
    const float cellSize = terrain.size / float(w - 1);
    const float gridX = worldPos.x / cellSize;
    const float gridY = worldPos.z / cellSize;
    
    const int centerX = static_cast<int>(std::round(gridX));
    const int centerY = static_cast<int>(std::round(gridY));
    
    const int radiusCells = static_cast<int>(std::ceil(safeSettings.radius / cellSize));
    
    // Calculate affected region
    const int minX = std::max(0, centerX - radiusCells);
    const int maxX = std::min(w - 1, centerX + radiusCells);
    const int minY = std::max(0, centerY - radiusCells);
    const int maxY = std::min(h - 1, centerY + radiusCells);
    
    result.minAffected = Vec2i(minX, minY);
    result.maxAffected = Vec2i(maxX, maxY);
    
    // Максимальное изменение высоты за один кадр
    const float maxHeightChange = 0.5f;
    
    // Apply brush effect
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = (float(x) - gridX) * cellSize;
            const float dy = (float(y) - gridY) * cellSize;
            const float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance > safeSettings.radius) continue;
            
            const float falloff = calculateFalloff(distance, safeSettings.radius, safeSettings.falloff);
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            
            float& height = terrain.heightmap[idx];
            const float oldHeight = height;
            float heightChange = 0.0f;
            
            switch (safeSettings.type) {
                case BrushType::Raise:
                    heightChange = safeSettings.strength * deltaTime * falloff;
                    break;
                    
                case BrushType::Lower:
                    // Only lower if current height is above 0
                    if (height > 0.0f) {
                        heightChange = -safeSettings.strength * deltaTime * falloff;
                        // Ensure we don't go below 0
                        heightChange = std::max(heightChange, -height);
                    }
                    break;
                    
                case BrushType::Flatten: {
                    const float diff = safeSettings.targetHeight - height;
                    heightChange = diff * safeSettings.strength * deltaTime * falloff * 0.05f; // Еще медленнее
                    break;
                }
                
                case BrushType::Smooth: {
                    const float smoothed = smoothHeight(terrain, x, y, safeSettings.smoothFactor);
                    const float diff = smoothed - height;
                    heightChange = diff * safeSettings.strength * deltaTime * falloff * 0.05f; // Еще медленнее
                    break;
                }
                
                case BrushType::Noise: {
                    const float noise = sampleNoise(float(x) * safeSettings.noiseScale, 
                                                  float(y) * safeSettings.noiseScale, 
                                                  NoiseSettings{});
                    heightChange = noise * safeSettings.strength * deltaTime * falloff * 0.02f; // Еще медленнее
                    break;
                }
                
                case BrushType::Erode:
                    // Simplified erosion - lower peaks, fill valleys
                    if (height > 0.0f) {
                        heightChange = -safeSettings.strength * deltaTime * falloff * 0.3f;
                    } else {
                        heightChange = safeSettings.strength * deltaTime * falloff * 0.1f;
                    }
                    break;
            }
            
            // Ограничиваем максимальное изменение за кадр
            heightChange = std::clamp(heightChange, -maxHeightChange, maxHeightChange);
            
            // Применяем изменение
            height += heightChange;
            
            // Clamp to terrain height range with safety margins
            // Minimum height is always 0.0 (no holes below ground level)
            const float safetyMargin = 2.0f;
            const float minSafe = 0.0f; // Never allow height below 0
            const float maxSafe = terrain.maxHeight - safetyMargin;
            height = std::clamp(height, minSafe, maxSafe);
            
            if (std::abs(height - oldHeight) > 0.001f) {
                result.modified = true;
                result.verticesChanged++;
            }
        }
    }
    
    return result;
}

float TerrainBrush::calculateFalloff(float distance, float radius, FalloffType type) {
    if (distance >= radius) return 0.0f;
    
    const float t = 1.0f - (distance / radius);
    
    switch (type) {
        case FalloffType::Linear:
            return t;
            
        case FalloffType::Smooth:
            return t * t * (3.0f - 2.0f * t); // smoothstep
            
        case FalloffType::Gaussian:
            return std::exp(-distance * distance / (2.0f * radius * radius * 0.25f));
            
        case FalloffType::Sharp:
            return t > 0.8f ? 1.0f : 0.0f;
            
        default:
            return t;
    }
}

float TerrainBrush::smoothHeight(const TerrainComponent& terrain, int x, int y, float factor) {
    const int w = terrain.resolution.x;
    const int h = terrain.resolution.y;
    
    float sum = 0.0f;
    int count = 0;
    
    // Sample 3x3 neighborhood
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx;
            const int ny = y + dy;
            
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                const size_t idx = static_cast<size_t>(ny) * static_cast<size_t>(w) + static_cast<size_t>(nx);
                sum += terrain.heightmap[idx];
                count++;
            }
        }
    }
    
    return count > 0 ? sum / float(count) : 0.0f;
}

float TerrainBrush::sampleNoise(float x, float y, const NoiseSettings& settings) {
    // Simplified Perlin-like noise
    static std::mt19937 rng(12345);
    static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    float result = 0.0f;
    float amplitude = settings.amplitude;
    float frequency = settings.frequency;
    
    for (int i = 0; i < settings.octaves; ++i) {
        // Simple noise approximation
        const float nx = x * frequency;
        const float ny = y * frequency;
        const float noise = std::sin(nx * 12.9898f + ny * 78.233f) * 43758.5453f;
        result += (noise - std::floor(noise)) * amplitude;
        
        amplitude *= settings.persistence;
        frequency *= settings.lacunarity;
    }
    
    return result;
}

ModificationResult TerrainBrush::generateNoise(
    TerrainComponent& terrain,
    const NoiseSettings& settings
) {
    ModificationResult result;
    
    const int w = terrain.resolution.x;
    const int h = terrain.resolution.y;
    
    TerrainMesh::ensureHeightmap(terrain);
    
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float worldX = (float(x) / float(w - 1)) * terrain.size;
            const float worldY = (float(y) / float(h - 1)) * terrain.size;
            
            const float noise = sampleNoise(worldX, worldY, settings);
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            
            terrain.heightmap[idx] = std::clamp(noise, terrain.minHeight, terrain.maxHeight);
        }
    }
    
    result.modified = true;
    result.minAffected = Vec2i(0, 0);
    result.maxAffected = Vec2i(w - 1, h - 1);
    result.verticesChanged = static_cast<size_t>(w) * static_cast<size_t>(h);
    
    return result;
}

ModificationResult TerrainBrush::importHeightmap(
    TerrainComponent& terrain,
    const String& filePath
) {
    ModificationResult result;
    
    // TODO: Implement actual image loading using stb_image
    // For now, generate test pattern
    const int w = terrain.resolution.x;
    const int h = terrain.resolution.y;
    
    TerrainMesh::ensureHeightmap(terrain);
    
    // Generate test heightmap pattern
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float fx = float(x) / float(w - 1);
            const float fy = float(y) / float(h - 1);
            
            // Simple test pattern - hills and valleys
            const float height = std::sin(fx * 6.28f) * std::cos(fy * 6.28f) * 10.0f;
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            
            terrain.heightmap[idx] = std::clamp(height, terrain.minHeight, terrain.maxHeight);
        }
    }
    
    result.modified = true;
    result.minAffected = Vec2i(0, 0);
    result.maxAffected = Vec2i(w - 1, h - 1);
    result.verticesChanged = static_cast<size_t>(w) * static_cast<size_t>(h);
    
    return result;
}

bool TerrainBrush::exportHeightmap(
    const TerrainComponent& terrain,
    const String& filePath,
    bool normalize
) {
    // TODO: Implement actual image saving using stb_image_write
    // For now, just return success
    return true;
}

// Texture painting implementation
bool TexturePainter::paintTexture(
    TerrainMaterial& material,
    const TerrainComponent& terrain,
    const Vec3& worldPos,
    int layerIndex,
    float radius,
    float strength,
    float deltaTime
) {
    if (layerIndex < 0 || layerIndex >= static_cast<int>(material.layers.size())) {
        return false;
    }
    
    const int w = terrain.resolution.x;
    const int h = terrain.resolution.y;
    const size_t vertexCount = static_cast<size_t>(w) * static_cast<size_t>(h);
    
    // Ensure blend weights are properly sized
    const size_t expectedSize = vertexCount * material.layers.size();
    if (material.blendWeights.size() != expectedSize) {
        material.blendWeights.resize(expectedSize, 0.0f);
        
        // Initialize first layer to full strength
        if (!material.layers.empty()) {
            for (size_t i = 0; i < vertexCount; ++i) {
                material.blendWeights[i * material.layers.size()] = 1.0f;
            }
        }
    }
    
    // Convert world position to grid coordinates
    const float cellSize = terrain.size / float(w - 1);
    const float gridX = worldPos.x / cellSize;
    const float gridY = worldPos.z / cellSize;
    
    const int centerX = static_cast<int>(std::round(gridX));
    const int centerY = static_cast<int>(std::round(gridY));
    const int radiusCells = static_cast<int>(std::ceil(radius / cellSize));
    
    bool modified = false;
    
    for (int y = std::max(0, centerY - radiusCells); 
         y <= std::min(h - 1, centerY + radiusCells); ++y) {
        for (int x = std::max(0, centerX - radiusCells); 
             x <= std::min(w - 1, centerX + radiusCells); ++x) {
            
            const float dx = (float(x) - gridX) * cellSize;
            const float dy = (float(y) - gridY) * cellSize;
            const float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance > radius) continue;
            
            const float falloff = TerrainBrush::calculateFalloff(distance, radius, FalloffType::Smooth);
            const size_t vertexIdx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            const size_t weightIdx = vertexIdx * material.layers.size() + static_cast<size_t>(layerIndex);
            
            // Increase weight for target layer
            material.blendWeights[weightIdx] += strength * deltaTime * falloff;
            material.blendWeights[weightIdx] = std::min(1.0f, material.blendWeights[weightIdx]);
            
            // Normalize weights for this vertex
            normalizeWeights(material, vertexIdx);
            modified = true;
        }
    }
    
    return modified;
}

void TexturePainter::normalizeWeights(TerrainMaterial& material, size_t vertexIndex) {
    const size_t layerCount = material.layers.size();
    if (layerCount == 0) return;
    
    const size_t baseIdx = vertexIndex * layerCount;
    
    // Calculate sum of weights
    float sum = 0.0f;
    for (size_t i = 0; i < layerCount; ++i) {
        sum += material.blendWeights[baseIdx + i];
    }
    
    // Normalize if sum > 0
    if (sum > 0.001f) {
        for (size_t i = 0; i < layerCount; ++i) {
            material.blendWeights[baseIdx + i] /= sum;
        }
    }
}

} // namespace WorldEditor::TerrainTools