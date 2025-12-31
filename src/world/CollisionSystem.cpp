#include "CollisionSystem.h"
#include "World.h"
#include "EntityManager.h"
#include "core/Types.h"
#include "core/MathUtils.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace WorldEditor {

CollisionSystem::CollisionSystem(EntityManager& entityManager)
    : entityManager_(entityManager) {
    spdlog::info("CollisionSystem initialized");
}

CollisionSystem::~CollisionSystem() {
    spdlog::info("CollisionSystem destroyed");
}

bool CollisionSystem::checkCollision(Entity entity1, Entity entity2) const {
    if (!entityManager_.hasComponent<CollisionComponent>(entity1) ||
        !entityManager_.hasComponent<CollisionComponent>(entity2)) {
        return false;
    }
    
    if (!entityManager_.hasComponent<TransformComponent>(entity1) ||
        !entityManager_.hasComponent<TransformComponent>(entity2)) {
        return false;
    }
    
    const auto& col1 = entityManager_.getComponent<CollisionComponent>(entity1);
    const auto& col2 = entityManager_.getComponent<CollisionComponent>(entity2);
    const auto& trans1 = entityManager_.getComponent<TransformComponent>(entity1);
    const auto& trans2 = entityManager_.getComponent<TransformComponent>(entity2);
    
    // Skip triggers
    if (col1.isTrigger || col2.isTrigger) {
        return false;
    }
    
    // Both boxes
    if (col1.shape == CollisionShape::Box && col2.shape == CollisionShape::Box) {
        return checkBoxBoxCollision(col1, trans1, col2, trans2);
    }
    
    // Both spheres
    if (col1.shape == CollisionShape::Sphere && col2.shape == CollisionShape::Sphere) {
        return checkSphereSphereCollision(col1, trans1, col2, trans2);
    }
    
    // Mixed shapes - convert to AABB and check
    Math::AABB aabb1 = col1.getAABB(trans1.position);
    Math::AABB aabb2 = col2.getAABB(trans2.position);
    return aabb1.intersects(aabb2);
}

bool CollisionSystem::checkPointCollision(Entity entity, const Vec3& point) const {
    if (!entityManager_.hasComponent<CollisionComponent>(entity) ||
        !entityManager_.hasComponent<TransformComponent>(entity)) {
        return false;
    }
    
    const auto& col = entityManager_.getComponent<CollisionComponent>(entity);
    const auto& trans = entityManager_.getComponent<TransformComponent>(entity);
    
    return col.containsPoint(point, trans.position);
}

bool CollisionSystem::checkSphereCollision(Entity entity, const Vec3& center, f32 radius) const {
    if (!entityManager_.hasComponent<CollisionComponent>(entity) ||
        !entityManager_.hasComponent<TransformComponent>(entity)) {
        return false;
    }
    
    const auto& col = entityManager_.getComponent<CollisionComponent>(entity);
    const auto& trans = entityManager_.getComponent<TransformComponent>(entity);
    
    Vec3 colCenter = getCollisionCenter(col, trans);
    f32 colRadius = getCollisionRadius(col);
    
    f32 distance = glm::length(center - colCenter);
    return distance < (radius + colRadius);
}

void CollisionSystem::resolveCollision(Entity entity1, Entity entity2, f32 deltaTime) {
    if (!entityManager_.hasComponent<CollisionComponent>(entity1) ||
        !entityManager_.hasComponent<CollisionComponent>(entity2)) {
        return;
    }
    
    if (!entityManager_.hasComponent<TransformComponent>(entity1) ||
        !entityManager_.hasComponent<TransformComponent>(entity2)) {
        return;
    }
    
    const auto& col1 = entityManager_.getComponent<CollisionComponent>(entity1);
    const auto& col2 = entityManager_.getComponent<CollisionComponent>(entity2);
    
    // Don't resolve if either is static or trigger
    if (col1.isStatic || col2.isStatic || col1.isTrigger || col2.isTrigger) {
        return;
    }
    
    auto& trans1 = entityManager_.getComponent<TransformComponent>(entity1);
    auto& trans2 = entityManager_.getComponent<TransformComponent>(entity2);
    
    Vec3 center1 = getCollisionCenter(col1, trans1);
    Vec3 center2 = getCollisionCenter(col2, trans2);
    Vec3 diff = center1 - center2;
    f32 distance = glm::length(diff);
    
    if (distance < 0.001f) {
        // Entities are on top of each other, push apart
        diff = Vec3(1.0f, 0.0f, 0.0f);
        distance = 1.0f;
    }
    
    f32 radius1 = getCollisionRadius(col1);
    f32 radius2 = getCollisionRadius(col2);
    f32 overlap = (radius1 + radius2) - distance;
    
    if (overlap > 0.0f) {
        Vec3 direction = glm::normalize(diff);
        Vec3 separation = direction * overlap * 0.5f;
        
        // Push both entities apart
        trans1.position += separation;
        trans2.position -= separation;
    }
}

Vec3 CollisionSystem::checkMovementCollision(Entity entity, const Vec3& desiredPosition, f32 radius) const {
    if (!entityManager_.hasComponent<CollisionComponent>(entity) ||
        !entityManager_.hasComponent<TransformComponent>(entity)) {
        return desiredPosition;
    }
    
    const auto& col = entityManager_.getComponent<CollisionComponent>(entity);
    const auto& trans = entityManager_.getComponent<TransformComponent>(entity);
    
    // Use entity's collision radius if radius not specified
    f32 checkRadius = (radius > 0.0f) ? radius : getCollisionRadius(col);
    
    // Check collisions with all other entities
    Vec3 adjustedPosition = desiredPosition;
    auto& reg = entityManager_.getRegistry();
    auto collisionView = reg.view<CollisionComponent, TransformComponent>();
    
    for (auto otherEntity : collisionView) {
        if (otherEntity == entity) {
            continue;
        }
        
        const auto& otherCol = collisionView.get<CollisionComponent>(otherEntity);
        const auto& otherTrans = collisionView.get<TransformComponent>(otherEntity);
        
        // Skip triggers and non-blocking objects
        if (otherCol.isTrigger || !otherCol.blocksMovement) {
            continue;
        }
        
        // Skip static objects if this entity is also static
        if (col.isStatic && otherCol.isStatic) {
            continue;
        }
        
        Vec3 otherCenter = getCollisionCenter(otherCol, otherTrans);
        f32 otherRadius = getCollisionRadius(otherCol);
        Vec3 toOther = otherCenter - adjustedPosition;
        toOther.y = 0.0f; // Only check horizontal collision
        f32 distance = glm::length(toOther);
        
        if (distance < (checkRadius + otherRadius)) {
            // Push away from collision
            f32 overlap = (checkRadius + otherRadius) - distance;
            if (overlap > 0.0f && distance > 0.001f) {
                Vec3 direction = glm::normalize(-toOther);
                adjustedPosition += direction * overlap;
            }
        }
    }
    
    return adjustedPosition;
}

Vector<Entity> CollisionSystem::getCollidingEntities(const Vec3& position, f32 radius) const {
    Vector<Entity> result;
    auto& reg = entityManager_.getRegistry();
    auto collisionView = reg.view<CollisionComponent, TransformComponent>();
    
    for (auto entity : collisionView) {
        if (checkSphereCollision(entity, position, radius)) {
            result.push_back(entity);
        }
    }
    
    return result;
}

bool CollisionSystem::hasBlockingCollisionAt(const Vec3& position, f32 radius, Entity ignoreEntity) const {
    auto& reg = entityManager_.getRegistry();
    auto collisionView = reg.view<CollisionComponent, TransformComponent>();

    for (auto entity : collisionView) {
        if (entity == ignoreEntity) {
            continue;
        }

        const auto& col = collisionView.get<CollisionComponent>(entity);
        if (col.isTrigger || !col.blocksMovement) {
            continue;
        }
        // IMPORTANT: for path checks we only consider static blockers.
        // Dynamic units (creeps) should not make other units "path around" each other.
        if (!col.isStatic) {
            continue;
        }

        if (checkSphereCollision(entity, position, radius)) {
            return true;
        }
    }

    return false;
}

bool CollisionSystem::checkBoxBoxCollision(const CollisionComponent& col1, const TransformComponent& trans1,
                                           const CollisionComponent& col2, const TransformComponent& trans2) const {
    Math::AABB aabb1 = col1.getAABB(trans1.position);
    Math::AABB aabb2 = col2.getAABB(trans2.position);
    return aabb1.intersects(aabb2);
}

bool CollisionSystem::checkSphereSphereCollision(const CollisionComponent& col1, const TransformComponent& trans1,
                                                 const CollisionComponent& col2, const TransformComponent& trans2) const {
    Vec3 center1 = getCollisionCenter(col1, trans1);
    Vec3 center2 = getCollisionCenter(col2, trans2);
    f32 radius1 = getCollisionRadius(col1);
    f32 radius2 = getCollisionRadius(col2);
    
    f32 distance = glm::length(center1 - center2);
    return distance < (radius1 + radius2);
}

bool CollisionSystem::checkBoxSphereCollision(const CollisionComponent& col1, const TransformComponent& trans1,
                                               const CollisionComponent& col2, const TransformComponent& trans2) const {
    // Simplified: check if sphere center is inside expanded box
    Math::AABB aabb1 = col1.getAABB(trans1.position);
    f32 radius2 = getCollisionRadius(col2);
    Vec3 center2 = getCollisionCenter(col2, trans2);
    
    // Expand AABB by sphere radius
    aabb1.min -= Vec3(radius2);
    aabb1.max += Vec3(radius2);
    
    return aabb1.contains(center2);
}

Vec3 CollisionSystem::getCollisionCenter(const CollisionComponent& col, const TransformComponent& trans) const {
    return trans.position + col.offset;
}

f32 CollisionSystem::getCollisionRadius(const CollisionComponent& col) const {
    switch (col.shape) {
        case CollisionShape::Sphere:
            return col.radius;
        case CollisionShape::Capsule:
            return col.capsuleRadius;
        case CollisionShape::Box: {
            // Use largest dimension as radius approximation
            Vec3 halfSize = col.boxSize * 0.5f;
            return std::max({halfSize.x, halfSize.y, halfSize.z});
        }
        default:
            return 0.5f;
    }
}

} // namespace WorldEditor
