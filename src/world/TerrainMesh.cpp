#include "TerrainMesh.h"
#include "TerrainTools.h"
#include "Components.h"

#ifdef DIRECTX_RENDERER
#include "../renderer/DirectXRenderer.h"
#endif

#include <algorithm>
#include <cmath>

namespace WorldEditor::TerrainMesh {

void ensureHeightmap(TerrainComponent& terrain) {
    // Terrain is always tile-based now - sync heightmap from heightLevels
    WorldEditor::TerrainTools::syncHeightmapFromLevels(terrain);
}

static inline size_t idx(int x, int y, int w) {
    return static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
}

template <class HeightFn>
static Vec3 computeNormal(const TerrainComponent& t, int x, int y, HeightFn&& heightAt) {
    const int w = t.resolution.x;
    const int h = t.resolution.y;
    const float dx = t.size / float(w - 1);
    const float dz = t.size / float(h - 1);

    const int x0 = std::max(0, x - 1);
    const int x1 = std::min(w - 1, x + 1);
    const int y0 = std::max(0, y - 1);
    const int y1 = std::min(h - 1, y + 1);

    const float hl = heightAt(x0, y);
    const float hr = heightAt(x1, y);
    const float hd = heightAt(x, y0);
    const float hu = heightAt(x, y1);

    const float dhdx = (hr - hl) / (float(x1 - x0) * dx);
    const float dhdz = (hu - hd) / (float(y1 - y0) * dz);

    // Left-handed: X right, Y up, Z forward.
    // Normal points up, correcting slopes in X/Z.
    Vec3 n(-dhdx, 1.0f, -dhdz);
    return glm::normalize(n);
}

void buildMesh(const TerrainComponent& terrainIn, MeshComponent& mesh) {
    const int w = std::max(2, terrainIn.resolution.x);
    const int h = std::max(2, terrainIn.resolution.y);
    const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);

    TerrainComponent t = terrainIn;
    t.resolution = Vec2i(w, h);

    auto heightAt = [&](int x, int y) -> float {
        if (x < 0 || x >= w || y < 0 || y >= h) return 0.0f;
        if (terrainIn.heightmap.size() != wanted) return 0.0f;
        return terrainIn.heightmap[idx(x, y, w)];
    };

    mesh.vertices.clear();
    mesh.normals.clear();
    mesh.texCoords.clear();
    mesh.indices.clear();

    mesh.vertices.reserve(wanted);
    mesh.normals.reserve(wanted);
    mesh.texCoords.reserve(wanted);

    for (int y = 0; y < h; ++y) {
        const float v = float(y) / float(h - 1);
        const float z = v * t.size;
        for (int x = 0; x < w; ++x) {
            const float u = float(x) / float(w - 1);
            const float xx = u * t.size;
            const float hh = heightAt(x, y);
            mesh.vertices.push_back(Vec3(xx, hh, z));
            mesh.normals.push_back(computeNormal(t, x, y, heightAt));
            mesh.texCoords.push_back(Vec2(u, v));
        }
    }

    // Two triangles per grid cell.
    const int quadsX = w - 1;
    const int quadsY = h - 1;
    mesh.indices.reserve(static_cast<size_t>(quadsX) * static_cast<size_t>(quadsY) * 6);

    for (int y = 0; y < quadsY; ++y) {
        for (int x = 0; x < quadsX; ++x) {
            const u32 i0 = static_cast<u32>(idx(x, y, w));
            const u32 i1 = static_cast<u32>(idx(x + 1, y, w));
            const u32 i2 = static_cast<u32>(idx(x, y + 1, w));
            const u32 i3 = static_cast<u32>(idx(x + 1, y + 1, w));

            // Winding for LH: clockwise by default in D3D is front-facing.
            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i1);

            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }

    invalidateGpu(mesh);
}

void invalidateGpu(MeshComponent& mesh) {
#ifdef DIRECTX_RENDERER
    // Use deferred release to avoid deleting resources while GPU is using them
    if (MeshComponent::s_renderer) {
        if (mesh.vertexBuffer) {
            MeshComponent::s_renderer->DeferredReleaseResource(mesh.vertexBuffer);
        }
        if (mesh.indexBuffer) {
            MeshComponent::s_renderer->DeferredReleaseResource(mesh.indexBuffer);
        }
        if (mesh.vertexBufferUpload) {
            MeshComponent::s_renderer->DeferredReleaseResource(mesh.vertexBufferUpload);
        }
        if (mesh.indexBufferUpload) {
            MeshComponent::s_renderer->DeferredReleaseResource(mesh.indexBufferUpload);
        }
        if (mesh.perObjectConstantBuffer) {
            MeshComponent::s_renderer->DeferredReleaseResource(mesh.perObjectConstantBuffer);
        }
        if (mesh.perObjectConstantBufferUpload) {
            MeshComponent::s_renderer->DeferredReleaseResource(mesh.perObjectConstantBufferUpload);
        }
    }
    
    mesh.gpuBuffersCreated = false;
    mesh.gpuConstantBuffersCreated = false;
    mesh.vertexBuffer.Reset();
    mesh.indexBuffer.Reset();
    mesh.vertexBufferUpload.Reset();
    mesh.indexBufferUpload.Reset();
    mesh.perObjectConstantBuffer.Reset();
    mesh.perObjectConstantBufferUpload.Reset();
#endif
}

} // namespace WorldEditor::TerrainMesh


