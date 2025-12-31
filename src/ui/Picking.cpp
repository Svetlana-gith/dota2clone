#include "Picking.h"

#include "world/World.h"
#include "world/Components.h"

#include <limits>

namespace WorldEditor {

namespace {

Math::AABB computeLocalAABB(const MeshComponent& mesh) {
    Math::AABB aabb{};
    if (mesh.vertices.empty()) {
        aabb.min = Vec3(0.0f);
        aabb.max = Vec3(0.0f);
        return aabb;
    }

    Vec3 mn = mesh.vertices[0];
    Vec3 mx = mesh.vertices[0];
    for (const Vec3& v : mesh.vertices) {
        mn = glm::min(mn, v);
        mx = glm::max(mx, v);
    }
    aabb.min = mn;
    aabb.max = mx;
    return aabb;
}

Math::AABB transformAABB(const Math::AABB& local, const Mat4& worldMtx) {
    // Conservative world AABB by transforming 8 corners.
    const Vec3 c0(local.min.x, local.min.y, local.min.z);
    const Vec3 c1(local.max.x, local.min.y, local.min.z);
    const Vec3 c2(local.min.x, local.max.y, local.min.z);
    const Vec3 c3(local.max.x, local.max.y, local.min.z);
    const Vec3 c4(local.min.x, local.min.y, local.max.z);
    const Vec3 c5(local.max.x, local.min.y, local.max.z);
    const Vec3 c6(local.min.x, local.max.y, local.max.z);
    const Vec3 c7(local.max.x, local.max.y, local.max.z);

    Vec3 corners[8] = { c0,c1,c2,c3,c4,c5,c6,c7 };
    Vec3 mn(std::numeric_limits<float>::infinity());
    Vec3 mx(-std::numeric_limits<float>::infinity());
    for (const Vec3& c : corners) {
        const Vec4 w = worldMtx * Vec4(c, 1.0f);
        const Vec3 p(w);
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }

    Math::AABB out{};
    out.min = mn;
    out.max = mx;
    return out;
}

} // namespace

Entity pickEntityAABB(World& world, const Math::Ray& ray, float* outTMin) {
    auto& em = world.getEntityManager();
    auto& reg = em.getRegistry();

    Entity best = INVALID_ENTITY;
    float bestT = std::numeric_limits<float>::infinity();

    auto view = reg.view<MeshComponent, TransformComponent>();
    for (auto entity : view) {
        auto& mesh = view.get<MeshComponent>(entity);
        auto& tr = view.get<TransformComponent>(entity);

        if (!mesh.visible || mesh.vertices.empty()) {
            continue;
        }

        const Math::AABB local = computeLocalAABB(mesh);
        const Math::AABB worldAabb = transformAABB(local, tr.getMatrix());

        float tMin = 0.0f;
        float tMax = 0.0f;
        if (Math::rayAABBIntersection(ray, worldAabb, tMin, tMax)) {
            const float hitT = (tMin >= 0.0f) ? tMin : tMax;
            if (hitT >= 0.0f && hitT < bestT) {
                bestT = hitT;
                best = entity;
            }
        }
    }

    if (outTMin) {
        *outTMin = bestT;
    }
    return best;
}

} // namespace WorldEditor


