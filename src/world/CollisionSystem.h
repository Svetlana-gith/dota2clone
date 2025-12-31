#pragma once

#include "core/Types.h"
#include "System.h"
#include "Components.h"

namespace WorldEditor {

// Forward declarations
class EntityManager;

// Collision system for handling object collisions
class CollisionSystem : public System {
public:
    CollisionSystem(EntityManager& entityManager);
    ~CollisionSystem() override;

    void update(f32 deltaTime) override {}
    String getName() const override { return "CollisionSystem"; }

    // Check if two entities collide
    bool checkCollision(Entity entity1, Entity entity2) const;
    
    // Check if a point collides with an entity
    bool checkPointCollision(Entity entity, const Vec3& point) const;
    
    // Check if a sphere collides with an entity
    bool checkSphereCollision(Entity entity, const Vec3& center, f32 radius) const;
    
    // Resolve collision between two entities (push them apart)
    void resolveCollision(Entity entity1, Entity entity2, f32 deltaTime);
    
    // Check if movement would cause collision and adjust position
    Vec3 checkMovementCollision(Entity entity, const Vec3& desiredPosition, f32 radius = 0.0f) const;
    
    // Get all entities that collide with given position and radius
    Vector<Entity> getCollidingEntities(const Vec3& position, f32 radius) const;

    // Fast check: returns true if any *static* blocking collider overlaps the sphere at position.
    // Avoids allocations and returns early on first hit.
    bool hasBlockingCollisionAt(const Vec3& position, f32 radius, Entity ignoreEntity = INVALID_ENTITY) const;

private:
    EntityManager& entityManager_;
    
    // Helper methods
    bool checkBoxBoxCollision(const CollisionComponent& col1, const TransformComponent& trans1,
                             const CollisionComponent& col2, const TransformComponent& trans2) const;
    bool checkSphereSphereCollision(const CollisionComponent& col1, const TransformComponent& trans1,
                                    const CollisionComponent& col2, const TransformComponent& trans2) const;
    bool checkBoxSphereCollision(const CollisionComponent& col1, const TransformComponent& trans1,
                                  const CollisionComponent& col2, const TransformComponent& trans2) const;
    
    Vec3 getCollisionCenter(const CollisionComponent& col, const TransformComponent& trans) const;
    f32 getCollisionRadius(const CollisionComponent& col) const;
};

} // namespace WorldEditor
