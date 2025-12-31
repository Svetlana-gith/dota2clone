#include "TerrainRaycast.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WorldEditor::TerrainRaycast {

static inline size_t idx(int x, int y, int w) {
    return static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
}

static bool intersectTriangleMT(const Math::Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2, float& outT, float& outU, float& outV) {
    // Möller–Trumbore
    const Vec3 e1 = v1 - v0;
    const Vec3 e2 = v2 - v0;
    const Vec3 p = Math::cross(ray.direction, e2);
    const float det = Math::dot(e1, p);

    if (std::abs(det) < 1e-8f) return false;
    const float invDet = 1.0f / det;

    const Vec3 tvec = ray.origin - v0;
    outU = Math::dot(tvec, p) * invDet;
    if (outU < 0.0f || outU > 1.0f) return false;

    const Vec3 q = Math::cross(tvec, e1);
    outV = Math::dot(ray.direction, q) * invDet;
    if (outV < 0.0f || (outU + outV) > 1.0f) return false;

    outT = Math::dot(e2, q) * invDet;
    return outT >= 0.0f;
}

static bool intersectAABB(const Math::Ray& ray, const Vec3& bmin, const Vec3& bmax, float& tMin, float& tMax) {
    // Slab method, robust against zero dir components.
    tMin = 0.0f;
    tMax = std::numeric_limits<float>::infinity();

    for (int axis = 0; axis < 3; ++axis) {
        const float ro = (&ray.origin.x)[axis];
        const float rd = (&ray.direction.x)[axis];
        const float mn = (&bmin.x)[axis];
        const float mx = (&bmax.x)[axis];

        if (std::abs(rd) < 1e-8f) {
            if (ro < mn || ro > mx) return false;
            continue;
        }

        const float inv = 1.0f / rd;
        float t0 = (mn - ro) * inv;
        float t1 = (mx - ro) * inv;
        if (t0 > t1) std::swap(t0, t1);
        tMin = (std::max)(tMin, t0);
        tMax = (std::min)(tMax, t1);
        if (tMax < tMin) return false;
    }

    return true;
}

bool raycastHeightfield(
    const TerrainComponent& terrain,
    const TransformComponent& transform,
    const Math::Ray& rayWorld,
    Vec3& outHitWorld,
    Vec3* outNormalWorld,
    float* outTWorld
) {
    const int w = std::max(2, terrain.resolution.x);
    const int h = std::max(2, terrain.resolution.y);
    const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (terrain.heightmap.size() != wanted) return false;
    if (terrain.size <= 0.0f) return false;

    // Transform ray to terrain local space (MVP: only translation).
    Math::Ray ray;
    ray.origin = rayWorld.origin - transform.position;
    ray.direction = rayWorld.direction;

    // Quick reject with AABB around terrain in local space.
    // Y range follows TerrainComponent's clamp range.
    const float yMin = (std::min)(terrain.minHeight, terrain.maxHeight);
    const float yMax = (std::max)(terrain.minHeight, terrain.maxHeight);
    const Vec3 bmin(0.0f, yMin, 0.0f);
    const Vec3 bmax(terrain.size, yMax, terrain.size);
    float tEnter = 0.0f, tExit = 0.0f;
    if (!intersectAABB(ray, bmin, bmax, tEnter, tExit)) return false;
    if (tExit < 0.0f) return false;

    const float cellX = terrain.size / float(w - 1);
    const float cellZ = terrain.size / float(h - 1);

    // Start marching from entry point in XZ.
    const float t0 = (std::max)(tEnter, 0.0f);
    Vec3 p = ray.pointAt(t0);

    int cx = static_cast<int>(std::floor(p.x / cellX));
    int cz = static_cast<int>(std::floor(p.z / cellZ));
    cx = (std::max)(0, (std::min)(w - 2, cx));
    cz = (std::max)(0, (std::min)(h - 2, cz));

    const float dirX = ray.direction.x;
    const float dirZ = ray.direction.z;

    int stepX = 0, stepZ = 0;
    float tMaxX = std::numeric_limits<float>::infinity();
    float tMaxZ = std::numeric_limits<float>::infinity();
    float tDeltaX = std::numeric_limits<float>::infinity();
    float tDeltaZ = std::numeric_limits<float>::infinity();

    if (std::abs(dirX) > 1e-8f) {
        stepX = (dirX > 0.0f) ? 1 : -1;
        const float nextBoundaryX = (stepX > 0) ? (float(cx + 1) * cellX) : (float(cx) * cellX);
        tMaxX = t0 + (nextBoundaryX - p.x) / dirX;
        tDeltaX = cellX / std::abs(dirX);
    }
    if (std::abs(dirZ) > 1e-8f) {
        stepZ = (dirZ > 0.0f) ? 1 : -1;
        const float nextBoundaryZ = (stepZ > 0) ? (float(cz + 1) * cellZ) : (float(cz) * cellZ);
        tMaxZ = t0 + (nextBoundaryZ - p.z) / dirZ;
        tDeltaZ = cellZ / std::abs(dirZ);
    }

    float bestT = std::numeric_limits<float>::infinity();
    Vec3 bestN(0.0f, 1.0f, 0.0f);

    // Traverse cells until we leave the AABB interval.
    float tCur = t0;
    while (tCur <= tExit) {
        // Build 2 triangles for this cell in local space.
        const int x0 = cx;
        const int z0 = cz;
        const int x1 = cx + 1;
        const int z1 = cz + 1;

        const float px0 = float(x0) * cellX;
        const float px1 = float(x1) * cellX;
        const float pz0 = float(z0) * cellZ;
        const float pz1 = float(z1) * cellZ;

        const float h00 = terrain.heightmap[idx(x0, z0, w)];
        const float h10 = terrain.heightmap[idx(x1, z0, w)];
        const float h01 = terrain.heightmap[idx(x0, z1, w)];
        const float h11 = terrain.heightmap[idx(x1, z1, w)];

        const Vec3 v00(px0, h00, pz0);
        const Vec3 v10(px1, h10, pz0);
        const Vec3 v01(px0, h01, pz1);
        const Vec3 v11(px1, h11, pz1);

        float tHit, uHit, vHit;
        // Match mesh winding: (x,y) (x,y+1) (x+1,y) and (x+1,y) (x,y+1) (x+1,y+1)
        if (intersectTriangleMT(ray, v00, v01, v10, tHit, uHit, vHit)) {
            if (tHit >= tCur && tHit < bestT) {
                bestT = tHit;
                bestN = Math::normalize(Math::cross(v01 - v00, v10 - v00));
            }
        }
        if (intersectTriangleMT(ray, v10, v01, v11, tHit, uHit, vHit)) {
            if (tHit >= tCur && tHit < bestT) {
                bestT = tHit;
                bestN = Math::normalize(Math::cross(v01 - v10, v11 - v10));
            }
        }

        // If we found a hit inside this cell before we exit it, early-out.
        const float tNext = (std::min)(tMaxX, tMaxZ);
        if (bestT <= tNext) {
            break;
        }

        // Advance to next cell boundary.
        if (tMaxX < tMaxZ) {
            cx += stepX;
            tCur = tMaxX;
            tMaxX += tDeltaX;
            if (cx < 0 || cx >= w - 1) break;
        } else {
            cz += stepZ;
            tCur = tMaxZ;
            tMaxZ += tDeltaZ;
            if (cz < 0 || cz >= h - 1) break;
        }
    }

    if (!std::isfinite(bestT)) return false;

    const Vec3 hitLocal = ray.pointAt(bestT);
    outHitWorld = hitLocal + transform.position;
    if (outNormalWorld) *outNormalWorld = bestN; // local == world for MVP (no rotation)
    if (outTWorld) *outTWorld = bestT;
    return true;
}

} // namespace WorldEditor::TerrainRaycast


