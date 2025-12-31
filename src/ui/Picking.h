#pragma once

#include "core/Types.h"
#include "core/MathUtils.h"

namespace WorldEditor {

class World;

// Returns the closest hit entity (Mesh+Transform) using a simple world-space AABB test.
// outTMin is optional and returns the hit distance along the ray.
Entity pickEntityAABB(World& world, const Math::Ray& ray, float* outTMin = nullptr);

} // namespace WorldEditor


