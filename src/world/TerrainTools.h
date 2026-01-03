#pragma once

#include "Components.h"
#include "core/Types.h"

namespace WorldEditor::TerrainTools {

// --- Tile-based Terrain (Dota 2 Workshop Tools style) ---
// Terrain uses discrete height levels (multiples of `TerrainComponent::heightStep`)
// on a regular grid. Rendering uses `heightmap` (float) derived from `heightLevels`.

// Initialize terrain as a tile grid (creates/overwrites height buffers).
void initTileTerrain(
    TerrainComponent& terrain,
    i32 tilesX = 128,
    i32 tilesZ = 128,
    f32 tileSize = 128.0f,
    f32 heightStep = 128.0f
);

// Sync float heightmap from discrete heightLevels.
// If `minAffected/maxAffected` are (0,0), syncs the whole map.
void syncHeightmapFromLevels(
    TerrainComponent& terrain,
    const Vec2i& minAffected = Vec2i(0),
    const Vec2i& maxAffected = Vec2i(0)
);

// Terrain modification result
struct ModificationResult {
    bool modified = false;
    Vec2i minAffected = Vec2i(0);
    Vec2i maxAffected = Vec2i(0);
    size_t verticesChanged = 0;
};

// Apply discrete tile height edits.
ModificationResult applyTileLevelDeltaBrush(
    TerrainComponent& terrain,
    const Vec3& worldPos,
    i32 deltaLevels,
    i32 radiusTiles
);

ModificationResult applyTileSetLevelBrush(
    TerrainComponent& terrain,
    const Vec3& worldPos,
    i32 setLevel,
    i32 radiusTiles
);

// Enforce Dota-like cliff constraints: max |deltaLevels| between adjacent vertices.
// Operates in-place on `heightLevels` and returns affected bounds.
ModificationResult enforceCliffConstraints(
    TerrainComponent& terrain,
    const Vec2i& minAffected,
    const Vec2i& maxAffected,
    i32 maxDeltaLevels = 3
);

// Paint a ramp/path from start to end (vertex coords) by interpolating height levels along the line.
// Also sets rampMask on tiles along the path.
ModificationResult applyRampPath(
    TerrainComponent& terrain,
    const Vec3& worldStart,
    const Vec3& worldEnd,
    i32 widthTiles
);


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