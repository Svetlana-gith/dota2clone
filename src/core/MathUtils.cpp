#include "MathUtils.h"

namespace WorldEditor {
namespace Math {

bool rayPlaneIntersection(const Ray& ray, const Plane& plane, f32& t) {
    f32 denom = dot(plane.normal, ray.direction);
    if (std::abs(denom) < 1e-6f) {
        return false; // Ray is parallel to plane
    }

    f32 nom = -dot(plane.normal, ray.origin) - plane.distance;
    t = nom / denom;

    return t >= 0.0f;
}

bool rayAABBIntersection(const Ray& ray, const AABB& aabb, f32& tMin, f32& tMax) {
    Vec3 invDir = 1.0f / ray.direction;

    Vec3 t1 = (aabb.min - ray.origin) * invDir;
    Vec3 t2 = (aabb.max - ray.origin) * invDir;

    Vec3 tMinVec = glm::min(t1, t2);
    Vec3 tMaxVec = glm::max(t1, t2);

    tMin = std::max(std::max(tMinVec.x, tMinVec.y), tMinVec.z);
    tMax = std::min(std::min(tMaxVec.x, tMaxVec.y), tMaxVec.z);

    return tMax >= tMin && tMax >= 0.0f;
}

Vec2 worldToScreen(const Vec3& worldPos, const Mat4& viewProj, const Vec2& screenSize) {
    Vec4 clipSpace = viewProj * Vec4(worldPos, 1.0f);
    if (clipSpace.w == 0.0f) {
        return Vec2(-1.0f); // Invalid position
    }

    Vec3 ndc = Vec3(clipSpace) / clipSpace.w;

    // Convert NDC to screen coordinates
    Vec2 screenPos;
    screenPos.x = (ndc.x + 1.0f) * 0.5f * screenSize.x;
    screenPos.y = (1.0f - ndc.y) * 0.5f * screenSize.y; // Flip Y for screen coordinates

    return screenPos;
}

Ray screenToWorldRay(const Vec2& screenPos, const Mat4& invViewProj, const Vec2& screenSize) {
    // Convert screen coordinates to NDC
    Vec2 ndc;
    ndc.x = (2.0f * screenPos.x) / screenSize.x - 1.0f;
    ndc.y = 1.0f - (2.0f * screenPos.y) / screenSize.y; // Flip Y for NDC

    // Create ray in clip space.
    // D3D-style depth range is [0..1] (ZO). With GLM's *_ZO projection variants, NDC.z=0 is near, 1 is far.
    Vec4 nearPoint = Vec4(ndc, 0.0f, 1.0f); // Near plane
    Vec4 farPoint = Vec4(ndc, 1.0f, 1.0f);  // Far plane

    // Transform to world space
    nearPoint = invViewProj * nearPoint;
    farPoint = invViewProj * farPoint;

    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    Ray ray;
    ray.origin = Vec3(nearPoint);
    ray.direction = normalize(Vec3(farPoint - nearPoint));

    return ray;
}

}
}
