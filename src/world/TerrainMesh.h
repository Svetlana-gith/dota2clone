#pragma once

#include "Components.h"

namespace WorldEditor::TerrainMesh {

// Ensures TerrainComponent::heightmap has correct size (resolution.x * resolution.y).
void ensureHeightmap(TerrainComponent& terrain);

// Builds full mesh data (verts/normals/uvs/indices) from terrain.
void buildMesh(const TerrainComponent& terrain, MeshComponent& mesh);

// Marks GPU buffers as dirty so they will be recreated next render.
void invalidateGpu(MeshComponent& mesh);

} // namespace WorldEditor::TerrainMesh


