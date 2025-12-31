#pragma once

#include "Components.h"

namespace WorldEditor::MeshGenerators {

// Generate cylinder mesh
void GenerateCylinder(MeshComponent& mesh, float radius, float height, int segments = 16);

// Generate sphere mesh
void GenerateSphere(MeshComponent& mesh, float radius, int segments = 16);

// Generate cone mesh
void GenerateCone(MeshComponent& mesh, float radius, float height, int segments = 16);

// Generate irregular rock mesh
void GenerateIrregularRock(MeshComponent& mesh, float size);

// Generate cube mesh
void GenerateCube(MeshComponent& mesh, const Vec3& size);

} // namespace WorldEditor::MeshGenerators
