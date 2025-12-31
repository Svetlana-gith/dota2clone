#pragma once

#include "core/MathUtils.h"
#include "Components.h"

namespace WorldEditor::TerrainRaycast {

// Raycasts against the heightfield triangles (derived from TerrainComponent heightmap).
// MVP assumptions:
// - Terrain is axis-aligned in XZ.
// - Only Transform.position is applied (no rotation/scale yet).
// Returns true if hit; outputs world-space hit point (and optional normal / t).
bool raycastHeightfield(
    const TerrainComponent& terrain,
    const TransformComponent& transform,
    const Math::Ray& rayWorld,
    Vec3& outHitWorld,
    Vec3* outNormalWorld = nullptr,
    float* outTWorld = nullptr
);

} // namespace WorldEditor::TerrainRaycast


