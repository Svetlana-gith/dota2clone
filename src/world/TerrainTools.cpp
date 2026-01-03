#include "TerrainTools.h"
#include "TerrainMesh.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <random>

namespace WorldEditor::TerrainTools {

static inline size_t idx2d(int x, int y, int w) {
    return static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
}

void initTileTerrain(TerrainComponent& terrain, i32 tilesX, i32 tilesZ, f32 tileSize, f32 heightStep) {
    tilesX = std::max<i32>(1, tilesX);
    tilesZ = std::max<i32>(1, tilesZ);
    tileSize = std::max(1.0f, tileSize);
    heightStep = std::max(1.0f, heightStep);

    // Clamp to reasonable limits to prevent memory issues
    tilesX = std::min<i32>(tilesX, 512);
    tilesZ = std::min<i32>(tilesZ, 512);

    terrain.tilesX = tilesX;
    terrain.tilesZ = tilesZ;
    terrain.tileSize = tileSize;
    terrain.heightStep = heightStep;

    // NOTE: current TerrainMesh assumes square `size` for X and Z.
    // For now (Dota-like grid) we use square maps (tilesX == tilesZ).
    terrain.resolution = Vec2i(tilesX + 1, tilesZ + 1);
    terrain.size = static_cast<f32>(tilesX) * tileSize;

    // Dota 2 Tile Editor allows many steps, but for MVP keep a safe positive range.
    terrain.minHeight = 0.0f;
    terrain.maxHeight = 15.0f * heightStep; // 15 steps above base

    // Clear old data first to avoid memory issues
    terrain.heightLevels.clear();
    terrain.heightmap.clear();
    terrain.rampMask.clear();

    const size_t wanted = static_cast<size_t>(terrain.resolution.x) * static_cast<size_t>(terrain.resolution.y);
    if (wanted > 0 && wanted < 1000000) { // Safety: reasonable limit
        terrain.heightLevels.assign(wanted, static_cast<i16>(0));
        terrain.heightmap.assign(wanted, 0.0f);
    }

    const size_t rampSize = static_cast<size_t>(tilesX) * static_cast<size_t>(tilesZ);
    if (rampSize > 0 && rampSize < 1000000) { // Safety: reasonable limit
        terrain.rampMask.assign(rampSize, static_cast<u8>(0));
    }
}

void syncHeightmapFromLevels(TerrainComponent& terrain, const Vec2i& minAffectedIn, const Vec2i& maxAffectedIn) {
    const int w = std::max(2, terrain.resolution.x);
    const int h = std::max(2, terrain.resolution.y);
    if (w < 2 || h < 2) return; // Safety check
    
    const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
    
    // Ensure heightLevels is initialized first (critical for tile terrain)
    if (terrain.heightLevels.size() != wanted) {
        terrain.heightLevels.assign(wanted, static_cast<i16>(0));
    }
    
    // Ensure heightmap matches size
    if (terrain.heightmap.size() != wanted) {
        terrain.heightmap.assign(wanted, 0.0f);
    }
    
    // Safety: if heightLevels is still empty after assignment, something is wrong
    if (terrain.heightLevels.empty() || terrain.heightmap.empty()) {
        return;
    }

    Vec2i minAffected = minAffectedIn;
    Vec2i maxAffected = maxAffectedIn;
    // Check if default parameters (both are (0,0)) - means sync entire map
    // This is safe because if user wants to sync only (0,0) vertex, they would pass (0,0) and (0,0) explicitly
    // Default parameters mean "sync all", so we check if both are exactly (0,0) AND resolution > 1
    if (minAffected.x == 0 && minAffected.y == 0 && maxAffected.x == 0 && maxAffected.y == 0 && w > 1 && h > 1) {
        // Sync entire map
        minAffected = Vec2i(0, 0);
        maxAffected = Vec2i(w - 1, h - 1);
    }
    minAffected.x = std::clamp(minAffected.x, 0, w - 1);
    minAffected.y = std::clamp(minAffected.y, 0, h - 1);
    maxAffected.x = std::clamp(maxAffected.x, 0, w - 1);
    maxAffected.y = std::clamp(maxAffected.y, 0, h - 1);

    const f32 step = std::max(1.0f, terrain.heightStep);
    if (step <= 0.0f) return; // Safety check
    
    const i16 minLevel = static_cast<i16>(std::floor(terrain.minHeight / step));
    const i16 maxLevel = static_cast<i16>(std::ceil(terrain.maxHeight / step));

    // Double-check bounds before accessing arrays
    if (terrain.heightLevels.size() < wanted || terrain.heightmap.size() < wanted) {
        return; // Safety: arrays not properly sized
    }

    for (int y = minAffected.y; y <= maxAffected.y; ++y) {
        for (int x = minAffected.x; x <= maxAffected.x; ++x) {
            const size_t i = idx2d(x, y, w);
            if (i >= wanted) continue; // Safety: bounds check
            
            i16 lvl = terrain.heightLevels[i];
            lvl = std::clamp<i16>(lvl, minLevel, maxLevel);
            terrain.heightLevels[i] = lvl;
            terrain.heightmap[i] = static_cast<f32>(lvl) * step;
        }
    }
}

static Vec2i worldToVertexCoord_Tile(const TerrainComponent& t, const Vec3& worldPos) {
    const float ts = std::max(1.0f, t.tileSize);
    const int vx = static_cast<int>(std::round(worldPos.x / ts));
    const int vy = static_cast<int>(std::round(worldPos.z / ts));
    return Vec2i(vx, vy);
}

static void clampVertexCoord(Vec2i& v, int w, int h) {
    v.x = std::clamp(v.x, 0, w - 1);
    v.y = std::clamp(v.y, 0, h - 1);
}

static ModificationResult applyTileBrushCore(
    TerrainComponent& terrain,
    const Vec3& worldPos,
    i32 radiusTiles,
    const std::function<i16(i16 oldLevel)>& op
) {
    ModificationResult result;

    const int w = std::max(2, terrain.resolution.x);
    const int h = std::max(2, terrain.resolution.y);
    const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (terrain.heightLevels.size() != wanted) {
        terrain.heightLevels.assign(wanted, static_cast<i16>(0));
    }

    radiusTiles = std::max<i32>(1, radiusTiles);
    const float radiusWorld = static_cast<float>(radiusTiles) * std::max(1.0f, terrain.tileSize);

    const Vec2i center = worldToVertexCoord_Tile(terrain, worldPos);
    const int minX = std::max(0, center.x - radiusTiles);
    const int maxX = std::min(w - 1, center.x + radiusTiles);
    const int minY = std::max(0, center.y - radiusTiles);
    const int maxY = std::min(h - 1, center.y + radiusTiles);

    result.minAffected = Vec2i(minX, minY);
    result.maxAffected = Vec2i(maxX, maxY);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = (float(x) - float(center.x)) * terrain.tileSize;
            const float dz = (float(y) - float(center.y)) * terrain.tileSize;
            const float dist = std::sqrt(dx * dx + dz * dz);
            if (dist > radiusWorld) continue;

            const size_t i = idx2d(x, y, w);
            const i16 oldLevel = terrain.heightLevels[i];
            const i16 newLevel = op(oldLevel);
            if (newLevel != oldLevel) {
                terrain.heightLevels[i] = newLevel;
                result.modified = true;
                result.verticesChanged++;
            }
        }
    }

    return result;
}

ModificationResult applyTileLevelDeltaBrush(TerrainComponent& terrain, const Vec3& worldPos, i32 deltaLevels, i32 radiusTiles) {
    deltaLevels = std::clamp<i32>(deltaLevels, -1, 1);
    if (deltaLevels == 0) return ModificationResult{};
    
    const int w = std::max(2, terrain.resolution.x);
    const int h = std::max(2, terrain.resolution.y);
    const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (terrain.heightLevels.size() != wanted) {
        terrain.heightLevels.assign(wanted, static_cast<i16>(0));
    }

    radiusTiles = std::max<i32>(1, radiusTiles);
    
    ModificationResult result;
    
    // For 1x1 tile (Hammer Editor style), modify only the single tile under cursor
    if (radiusTiles == 1) {
        // Determine the exact tile under cursor (using floor to get tile index)
        const float tileSize = std::max(1.0f, terrain.tileSize);
        const int tileX = static_cast<int>(std::floor(worldPos.x / tileSize));
        const int tileZ = static_cast<int>(std::floor(worldPos.z / tileSize));
        
        // Clamp to valid tile range (tiles are 0-based, vertices are 0-based)
        // One tile consists of 4 vertices: (tileX, tileZ), (tileX+1, tileZ), (tileX, tileZ+1), (tileX+1, tileZ+1)
        const int vx0 = std::clamp(tileX, 0, w - 1);
        const int vx1 = std::clamp(tileX + 1, 0, w - 1);
        const int vz0 = std::clamp(tileZ, 0, h - 1);
        const int vz1 = std::clamp(tileZ + 1, 0, h - 1);
        
        // Check if tile is flat (all 4 vertices have the same level)
        // Only allow raising/lowering flat tiles - prevent editing sloped tiles
        const i16 level0 = terrain.heightLevels[idx2d(vx0, vz0, w)];
        const i16 level1 = terrain.heightLevels[idx2d(vx1, vz0, w)];
        const i16 level2 = terrain.heightLevels[idx2d(vx0, vz1, w)];
        const i16 level3 = terrain.heightLevels[idx2d(vx1, vz1, w)];
        
        // If tile is not flat (has slopes), don't allow editing
        if (level0 != level1 || level0 != level2 || level0 != level3) {
            return result; // Return without modification
        }
        
        // Tile is flat - all vertices have the same level (level0)
        
        // Check if any neighboring tile (within 1 tile radius) is raised higher
        // This prevents editing tiles adjacent to raised tiles (creates pattern of every 2nd tile)
        const int tilesX = terrain.tilesX;
        const int tilesZ = terrain.tilesZ;
        const int currentTileLevel = static_cast<int>(level0);
        
        // Check all 8 neighboring tiles (4 orthogonal + 4 diagonal)
        const int neighborOffsets[8][2] = {
            {-1, -1}, {0, -1}, {1, -1},  // Top row
            {-1,  0},          {1,  0},  // Middle row (skip self)
            {-1,  1}, {0,  1}, {1,  1}   // Bottom row
        };
        
        for (int i = 0; i < 8; ++i) {
            const int nTileX = tileX + neighborOffsets[i][0];
            const int nTileZ = tileZ + neighborOffsets[i][1];
            
            // Skip if neighbor is out of bounds
            if (nTileX < 0 || nTileX >= tilesX || nTileZ < 0 || nTileZ >= tilesZ) {
                continue;
            }
            
            // Get neighbor tile vertices
            const int nvx0 = std::clamp(nTileX, 0, w - 1);
            const int nvx1 = std::clamp(nTileX + 1, 0, w - 1);
            const int nvz0 = std::clamp(nTileZ, 0, h - 1);
            const int nvz1 = std::clamp(nTileZ + 1, 0, h - 1);
            
            // Check if neighbor tile is flat and raised higher
            const i16 nlevel0 = terrain.heightLevels[idx2d(nvx0, nvz0, w)];
            const i16 nlevel1 = terrain.heightLevels[idx2d(nvx1, nvz0, w)];
            const i16 nlevel2 = terrain.heightLevels[idx2d(nvx0, nvz1, w)];
            const i16 nlevel3 = terrain.heightLevels[idx2d(nvx1, nvz1, w)];
            
            // If neighbor is flat (all vertices same level) and higher than current tile, block editing
            if (nlevel0 == nlevel1 && nlevel0 == nlevel2 && nlevel0 == nlevel3) {
                if (static_cast<int>(nlevel0) > currentTileLevel) {
                    return result; // Block editing - neighbor is raised
                }
            }
        }
        
        // Find the highest level among the 4 vertices of THIS tile only
        // This ensures the tile becomes flat regardless of neighboring tiles
        // IMPORTANT: We look at current vertex levels to determine the base level,
        // then set ALL 4 vertices to the same level (maxLevel + delta)
        i16 maxLevel = level0;
        
        // Calculate target level: max level in this tile + delta
        // This ensures the tile is completely flat - all 4 vertices at the same level
        const i16 targetLevel = static_cast<i16>(std::clamp<i32>(static_cast<i32>(maxLevel) + deltaLevels, -32768, 32767));
        
        result.minAffected = Vec2i(vx0, vz0);
        result.maxAffected = Vec2i(vx1, vz1);
        
        // Update all 4 vertices of the tile to the SAME level (ensures flat tile)
        // CRITICAL: All 4 vertices MUST be at the same level for a flat tile
        const Vec2i vertices[4] = {
            Vec2i(vx0, vz0), Vec2i(vx1, vz0),
            Vec2i(vx0, vz1), Vec2i(vx1, vz1)
        };
        
        // Force all 4 vertices to the exact same level (no slopes within tile)
        for (const Vec2i& v : vertices) {
            const size_t i = idx2d(v.x, v.y, w);
            // Always set to targetLevel - this ensures the tile is completely flat
            // Even if vertex was already at targetLevel, we set it again for consistency
            const i16 oldLevel = terrain.heightLevels[i];
            terrain.heightLevels[i] = targetLevel;
            if (oldLevel != targetLevel) {
                result.modified = true;
                result.verticesChanged++;
            }
        }
        
        // Ensure we mark as modified if any vertex changed
        if (result.verticesChanged > 0) {
            result.modified = true;
        }
    } else {
        // For larger radius, use the original logic
        const float radiusWorld = static_cast<float>(radiusTiles) * std::max(1.0f, terrain.tileSize);
        const Vec2i center = worldToVertexCoord_Tile(terrain, worldPos);
        const int minX = std::max(0, center.x - radiusTiles);
        const int maxX = std::min(w - 1, center.x + radiusTiles);
        const int minY = std::max(0, center.y - radiusTiles);
        const int maxY = std::min(h - 1, center.y + radiusTiles);

        // Find the highest level in the brush area
        i16 maxLevel = terrain.heightLevels[idx2d(center.x, center.y, w)];
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const float dx = (float(x) - float(center.x)) * terrain.tileSize;
                const float dz = (float(y) - float(center.y)) * terrain.tileSize;
                const float dist = std::sqrt(dx * dx + dz * dz);
                if (dist > radiusWorld) continue;
                
                const size_t i = idx2d(x, y, w);
                maxLevel = std::max(maxLevel, terrain.heightLevels[i]);
            }
        }
        
        // Set all vertices in the brush area to the same flat level (maxLevel + delta)
        const i16 targetLevel = static_cast<i16>(std::clamp<i32>(static_cast<i32>(maxLevel) + deltaLevels, -32768, 32767));
        
        result.minAffected = Vec2i(minX, minY);
        result.maxAffected = Vec2i(maxX, maxY);
        
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const float dx = (float(x) - float(center.x)) * terrain.tileSize;
                const float dz = (float(y) - float(center.y)) * terrain.tileSize;
                const float dist = std::sqrt(dx * dx + dz * dz);
                if (dist > radiusWorld) continue;

                const size_t i = idx2d(x, y, w);
                if (terrain.heightLevels[i] != targetLevel) {
                    terrain.heightLevels[i] = targetLevel;
                    result.modified = true;
                    result.verticesChanged++;
                }
            }
        }
    }
    
    return result;
}

ModificationResult applyTileSetLevelBrush(TerrainComponent& terrain, const Vec3& worldPos, i32 setLevel, i32 radiusTiles) {
    setLevel = std::clamp<i32>(setLevel, -32768, 32767);
    return applyTileBrushCore(terrain, worldPos, radiusTiles, [&](i16 /*oldLevel*/) -> i16 {
        return static_cast<i16>(setLevel);
    });
}

ModificationResult enforceCliffConstraints(TerrainComponent& terrain, const Vec2i& minA, const Vec2i& maxA, i32 maxDeltaLevels) {
    ModificationResult result;

    const int w = std::max(2, terrain.resolution.x);
    const int h = std::max(2, terrain.resolution.y);
    const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (terrain.heightLevels.size() != wanted) {
        terrain.heightLevels.assign(wanted, static_cast<i16>(0));
    }

    Vec2i minV = minA;
    Vec2i maxV = maxA;
    clampVertexCoord(minV, w, h);
    clampVertexCoord(maxV, w, h);
    if (minV.x > maxV.x) std::swap(minV.x, maxV.x);
    if (minV.y > maxV.y) std::swap(minV.y, maxV.y);

    result.minAffected = minV;
    result.maxAffected = maxV;

    maxDeltaLevels = std::max(0, maxDeltaLevels);
    if (maxDeltaLevels == 0) return result;

    // A couple of relaxation passes are enough for a brush-sized region.
    for (int pass = 0; pass < 3; ++pass) {
        bool any = false;
        for (int y = minV.y; y <= maxV.y; ++y) {
            for (int x = minV.x; x <= maxV.x; ++x) {
                const size_t i = idx2d(x, y, w);
                i16 lvl = terrain.heightLevels[i];

                const auto clampToNeighbor = [&](int nx, int ny) {
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) return;
                    const i16 n = terrain.heightLevels[idx2d(nx, ny, w)];
                    const i32 diff = static_cast<i32>(lvl) - static_cast<i32>(n);
                    if (diff > maxDeltaLevels) lvl = static_cast<i16>(static_cast<i32>(n) + maxDeltaLevels);
                    if (diff < -maxDeltaLevels) lvl = static_cast<i16>(static_cast<i32>(n) - maxDeltaLevels);
                };

                clampToNeighbor(x - 1, y);
                clampToNeighbor(x + 1, y);
                clampToNeighbor(x, y - 1);
                clampToNeighbor(x, y + 1);

                if (lvl != terrain.heightLevels[i]) {
                    terrain.heightLevels[i] = lvl;
                    any = true;
                    result.modified = true;
                    result.verticesChanged++;
                }
            }
        }
        if (!any) break;
    }

    return result;
}

static void bresenhamLine(Vec2i a, Vec2i b, Vector<Vec2i>& out) {
    out.clear();
    int x0 = a.x, y0 = a.y;
    const int x1 = b.x, y1 = b.y;
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        out.push_back(Vec2i(x0, y0));
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

ModificationResult applyRampPath(TerrainComponent& terrain, const Vec3& worldStart, const Vec3& worldEnd, i32 widthTiles) {
    ModificationResult result;

    const int w = std::max(2, terrain.resolution.x);
    const int h = std::max(2, terrain.resolution.y);
    const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (terrain.heightLevels.size() != wanted) {
        terrain.heightLevels.assign(wanted, static_cast<i16>(0));
    }

    widthTiles = std::clamp<i32>(widthTiles, 1, 32);

    Vec2i a = worldToVertexCoord_Tile(terrain, worldStart);
    Vec2i b = worldToVertexCoord_Tile(terrain, worldEnd);
    clampVertexCoord(a, w, h);
    clampVertexCoord(b, w, h);

    const i16 lvlA = terrain.heightLevels[idx2d(a.x, a.y, w)];
    const i16 lvlB = terrain.heightLevels[idx2d(b.x, b.y, w)];

    Vector<Vec2i> line;
    bresenhamLine(a, b, line);
    if (line.empty()) return result;

    Vec2i minV(w - 1, h - 1);
    Vec2i maxV(0, 0);

    const int n = static_cast<int>(line.size());
    for (int i = 0; i < n; ++i) {
        const float t = (n <= 1) ? 0.0f : (float(i) / float(n - 1));
        const i16 target = static_cast<i16>(std::lround(float(lvlA) * (1.0f - t) + float(lvlB) * t));
        const Vec2i p = line[static_cast<size_t>(i)];

        // Apply to a small square around the line point (width in tiles).
        for (int oy = -widthTiles; oy <= widthTiles; ++oy) {
            for (int ox = -widthTiles; ox <= widthTiles; ++ox) {
                const int vx = p.x + ox;
                const int vy = p.y + oy;
                if (vx < 0 || vx >= w || vy < 0 || vy >= h) continue;
                const size_t vi = idx2d(vx, vy, w);
                if (terrain.heightLevels[vi] != target) {
                    terrain.heightLevels[vi] = target;
                    result.modified = true;
                    result.verticesChanged++;
                }
                minV.x = std::min(minV.x, vx);
                minV.y = std::min(minV.y, vy);
                maxV.x = std::max(maxV.x, vx);
                maxV.y = std::max(maxV.y, vy);
            }
        }

        // Mark ramp/path tiles (tile coords are vertex coords clamped to tiles range).
        if (terrain.tilesX > 0 && terrain.tilesZ > 0) {
            const int tx = std::clamp(p.x, 0, terrain.tilesX - 1);
            const int tz = std::clamp(p.y, 0, terrain.tilesZ - 1);
            const size_t ti = static_cast<size_t>(tz) * static_cast<size_t>(terrain.tilesX) + static_cast<size_t>(tx);
            if (terrain.rampMask.size() != static_cast<size_t>(terrain.tilesX) * static_cast<size_t>(terrain.tilesZ)) {
                terrain.rampMask.assign(static_cast<size_t>(terrain.tilesX) * static_cast<size_t>(terrain.tilesZ), static_cast<u8>(0));
            }
            terrain.rampMask[ti] = 1;
        }
    }

    if (result.modified) {
        result.minAffected = minV;
        result.maxAffected = maxV;
    }
    return result;
}

// Falloff function for texture painting
enum class FalloffType : u8 {
    Linear,
    Smooth,
    Gaussian,
    Sharp
};

static float calculateFalloff(float distance, float radius, FalloffType type) {
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
            
            const float falloff = calculateFalloff(distance, radius, FalloffType::Smooth);
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