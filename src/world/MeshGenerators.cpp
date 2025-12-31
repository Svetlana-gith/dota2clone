#include "MeshGenerators.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace WorldEditor::MeshGenerators {

void GenerateCylinder(MeshComponent& mesh, float radius, float height, int segments) {
    mesh.vertices.clear();
    mesh.normals.clear();
    mesh.texCoords.clear();
    mesh.indices.clear();
    
    const float halfHeight = height * 0.5f;
    const float angleStep = 2.0f * glm::pi<float>() / segments;
    
    // Bottom center
    mesh.vertices.push_back(Vec3(0.0f, -halfHeight, 0.0f));
    mesh.normals.push_back(Vec3(0.0f)); // Will be recalculated
    mesh.texCoords.push_back(Vec2(0.5f, 0.5f));
    
    // Top center
    mesh.vertices.push_back(Vec3(0.0f, halfHeight, 0.0f));
    mesh.normals.push_back(Vec3(0.0f)); // Will be recalculated
    mesh.texCoords.push_back(Vec2(0.5f, 0.5f));
    
    // Generate side vertices
    for (int i = 0; i <= segments; ++i) {
        float angle = i * angleStep;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        
        // Bottom ring
        mesh.vertices.push_back(Vec3(x, -halfHeight, z));
        mesh.normals.push_back(Vec3(0.0f)); // Will be recalculated
        mesh.texCoords.push_back(Vec2(static_cast<float>(i) / segments, 0.0f));
        
        // Top ring
        mesh.vertices.push_back(Vec3(x, halfHeight, z));
        mesh.normals.push_back(Vec3(0.0f)); // Will be recalculated
        mesh.texCoords.push_back(Vec2(static_cast<float>(i) / segments, 1.0f));
    }
    
    // Bottom face indices (counter-clockwise when viewed from below)
    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(0);
        mesh.indices.push_back(2 + ((i + 1) % (segments + 1)) * 2);
        mesh.indices.push_back(2 + i * 2);
    }
    
    // Top face indices (counter-clockwise when viewed from above)
    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(1);
        mesh.indices.push_back(2 + i * 2 + 1);
        mesh.indices.push_back(2 + ((i + 1) % (segments + 1)) * 2 + 1);
    }
    
    // Side face indices (counter-clockwise)
    for (int i = 0; i < segments; ++i) {
        int base = 2 + i * 2;
        int nextBase = 2 + ((i + 1) % (segments + 1)) * 2;
        // First triangle
        mesh.indices.push_back(base);
        mesh.indices.push_back(nextBase);
        mesh.indices.push_back(base + 1);
        // Second triangle
        mesh.indices.push_back(nextBase);
        mesh.indices.push_back(nextBase + 1);
        mesh.indices.push_back(base + 1);
    }
    
    // Recalculate normals from faces
    mesh.normals.assign(mesh.vertices.size(), Vec3(0.0f));
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        Vec3 v0 = mesh.vertices[mesh.indices[i]];
        Vec3 v1 = mesh.vertices[mesh.indices[i + 1]];
        Vec3 v2 = mesh.vertices[mesh.indices[i + 2]];
        Vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        mesh.normals[mesh.indices[i]] += normal;
        mesh.normals[mesh.indices[i + 1]] += normal;
        mesh.normals[mesh.indices[i + 2]] += normal;
    }
    // Normalize all normals
    for (auto& n : mesh.normals) {
        if (glm::length(n) > 0.001f) {
            n = glm::normalize(n);
        } else {
            n = Vec3(0.0f, 1.0f, 0.0f); // Fallback
        }
    }
}

void GenerateSphere(MeshComponent& mesh, float radius, int segments) {
    mesh.vertices.clear();
    mesh.normals.clear();
    mesh.texCoords.clear();
    mesh.indices.clear();
    
    const int rings = segments;
    const float pi = glm::pi<float>();
    
    // Generate vertices
    for (int i = 0; i <= rings; ++i) {
        float theta = i * pi / rings;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);
        
        for (int j = 0; j <= segments; ++j) {
            float phi = j * 2.0f * pi / segments;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);
            
            Vec3 pos(cosPhi * sinTheta, cosTheta, sinPhi * sinTheta);
            mesh.vertices.push_back(pos * radius);
            mesh.normals.push_back(Vec3(0.0f)); // Will be recalculated
            mesh.texCoords.push_back(Vec2(static_cast<float>(j) / segments, static_cast<float>(i) / rings));
        }
    }
    
    // Generate indices (counter-clockwise for front face)
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            int first = i * (segments + 1) + j;
            int second = first + segments + 1;
            
            // First triangle (counter-clockwise)
            mesh.indices.push_back(first);
            mesh.indices.push_back(first + 1);
            mesh.indices.push_back(second);
            
            // Second triangle (counter-clockwise)
            mesh.indices.push_back(second);
            mesh.indices.push_back(first + 1);
            mesh.indices.push_back(second + 1);
        }
    }
    
    // Recalculate normals from faces (smooth normals)
    mesh.normals.assign(mesh.vertices.size(), Vec3(0.0f));
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        Vec3 v0 = mesh.vertices[mesh.indices[i]];
        Vec3 v1 = mesh.vertices[mesh.indices[i + 1]];
        Vec3 v2 = mesh.vertices[mesh.indices[i + 2]];
        Vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        mesh.normals[mesh.indices[i]] += normal;
        mesh.normals[mesh.indices[i + 1]] += normal;
        mesh.normals[mesh.indices[i + 2]] += normal;
    }
    // Normalize all normals
    for (auto& n : mesh.normals) {
        if (glm::length(n) > 0.001f) {
            n = glm::normalize(n);
        } else {
            n = Vec3(0.0f, 1.0f, 0.0f); // Fallback
        }
    }
}

void GenerateCone(MeshComponent& mesh, float radius, float height, int segments) {
    mesh.vertices.clear();
    mesh.normals.clear();
    mesh.texCoords.clear();
    mesh.indices.clear();
    
    const float angleStep = 2.0f * glm::pi<float>() / segments;
    
    // Apex
    mesh.vertices.push_back(Vec3(0.0f, height * 0.5f, 0.0f));
    mesh.normals.push_back(Vec3(0.0f)); // Will be recalculated
    mesh.texCoords.push_back(Vec2(0.5f, 1.0f));
    
    // Base center
    mesh.vertices.push_back(Vec3(0.0f, -height * 0.5f, 0.0f));
    mesh.normals.push_back(Vec3(0.0f)); // Will be recalculated
    mesh.texCoords.push_back(Vec2(0.5f, 0.5f));
    
    // Base ring
    for (int i = 0; i <= segments; ++i) {
        float angle = i * angleStep;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        Vec3 pos(x, -height * 0.5f, z);
        
        mesh.vertices.push_back(pos);
        mesh.normals.push_back(Vec3(0.0f)); // Will be recalculated
        mesh.texCoords.push_back(Vec2(static_cast<float>(i) / segments, 0.0f));
    }
    
    // Base face indices (counter-clockwise when viewed from below)
    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(1);
        mesh.indices.push_back(2 + i);
        mesh.indices.push_back(2 + ((i + 1) % (segments + 1)));
    }
    
    // Side face indices (counter-clockwise)
    for (int i = 0; i < segments; ++i) {
        mesh.indices.push_back(0);
        mesh.indices.push_back(2 + ((i + 1) % (segments + 1)));
        mesh.indices.push_back(2 + i);
    }
    
    // Recalculate normals from faces
    mesh.normals.assign(mesh.vertices.size(), Vec3(0.0f));
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        Vec3 v0 = mesh.vertices[mesh.indices[i]];
        Vec3 v1 = mesh.vertices[mesh.indices[i + 1]];
        Vec3 v2 = mesh.vertices[mesh.indices[i + 2]];
        Vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        mesh.normals[mesh.indices[i]] += normal;
        mesh.normals[mesh.indices[i + 1]] += normal;
        mesh.normals[mesh.indices[i + 2]] += normal;
    }
    // Normalize all normals
    for (auto& n : mesh.normals) {
        if (glm::length(n) > 0.001f) {
            n = glm::normalize(n);
        } else {
            n = Vec3(0.0f, 1.0f, 0.0f); // Fallback
        }
    }
}

void GenerateIrregularRock(MeshComponent& mesh, float size) {
    mesh.vertices.clear();
    mesh.normals.clear();
    mesh.texCoords.clear();
    mesh.indices.clear();
    
    // Create irregular shape by perturbing a cube
    const float s = size * 0.5f;
    std::vector<Vec3> baseVerts = {
        Vec3(-s, -s, -s), Vec3(s, -s, -s), Vec3(s, s, -s), Vec3(-s, s, -s),
        Vec3(-s, -s, s), Vec3(s, -s, s), Vec3(s, s, s), Vec3(-s, s, s)
    };
    
    // Perturb vertices for irregularity
    for (auto& v : baseVerts) {
        v.x += (rand() % 100 - 50) * 0.01f * s;
        v.y += (rand() % 100 - 50) * 0.01f * s;
        v.z += (rand() % 100 - 50) * 0.01f * s;
    }
    
    mesh.vertices = baseVerts;
    mesh.normals.assign(8, Vec3(0.0f)); // Will be recalculated
    mesh.texCoords.assign(8, Vec2(0.0f, 0.0f));
    mesh.indices = {
        0, 1, 2, 2, 3, 0, // front
        4, 7, 6, 6, 5, 4, // back
        0, 4, 5, 5, 1, 0, // bottom
        3, 2, 6, 6, 7, 3, // top
        0, 3, 7, 7, 4, 0, // left
        1, 5, 6, 6, 2, 1  // right
    };
    
    // Recalculate normals from faces (smooth normals)
    mesh.normals.assign(mesh.vertices.size(), Vec3(0.0f));
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        Vec3 v0 = mesh.vertices[mesh.indices[i]];
        Vec3 v1 = mesh.vertices[mesh.indices[i + 1]];
        Vec3 v2 = mesh.vertices[mesh.indices[i + 2]];
        Vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        mesh.normals[mesh.indices[i]] += normal;
        mesh.normals[mesh.indices[i + 1]] += normal;
        mesh.normals[mesh.indices[i + 2]] += normal;
    }
    // Normalize all normals
    for (auto& n : mesh.normals) {
        if (glm::length(n) > 0.001f) {
            n = glm::normalize(n);
        } else {
            n = Vec3(0.0f, 1.0f, 0.0f); // Fallback
        }
    }
}

void GenerateCube(MeshComponent& mesh, const Vec3& size) {
    mesh.vertices.clear();
    mesh.normals.clear();
    mesh.texCoords.clear();
    mesh.indices.clear();
    
    const float sx = size.x * 0.5f;
    const float sy = size.y * 0.5f;
    const float sz = size.z * 0.5f;
    
    // Vertices for a cube
    mesh.vertices = {
        // Front face
        Vec3(-sx, -sy, sz), Vec3(sx, -sy, sz), Vec3(sx, sy, sz), Vec3(-sx, sy, sz),
        // Back face
        Vec3(-sx, -sy, -sz), Vec3(sx, -sy, -sz), Vec3(sx, sy, -sz), Vec3(-sx, sy, -sz),
        // Left face
        Vec3(-sx, -sy, -sz), Vec3(-sx, -sy, sz), Vec3(-sx, sy, sz), Vec3(-sx, sy, -sz),
        // Right face
        Vec3(sx, -sy, sz), Vec3(sx, -sy, -sz), Vec3(sx, sy, -sz), Vec3(sx, sy, sz),
        // Top face
        Vec3(-sx, sy, sz), Vec3(sx, sy, sz), Vec3(sx, sy, -sz), Vec3(-sx, sy, -sz),
        // Bottom face
        Vec3(-sx, -sy, -sz), Vec3(sx, -sy, -sz), Vec3(sx, -sy, sz), Vec3(-sx, -sy, sz)
    };
    
    // Normals for a cube (face normals)
    mesh.normals = {
        // Front
        Vec3(0, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 1),
        // Back
        Vec3(0, 0, -1), Vec3(0, 0, -1), Vec3(0, 0, -1), Vec3(0, 0, -1),
        // Left
        Vec3(-1, 0, 0), Vec3(-1, 0, 0), Vec3(-1, 0, 0), Vec3(-1, 0, 0),
        // Right
        Vec3(1, 0, 0), Vec3(1, 0, 0), Vec3(1, 0, 0), Vec3(1, 0, 0),
        // Top
        Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 1, 0),
        // Bottom
        Vec3(0, -1, 0), Vec3(0, -1, 0), Vec3(0, -1, 0), Vec3(0, -1, 0)
    };
    
    // Texture coordinates
    mesh.texCoords.assign(mesh.vertices.size(), Vec2(0.0f, 0.0f));
    
    // Indices (counter-clockwise for front face)
    mesh.indices = {
        0, 1, 2, 2, 3, 0,       // Front
        7, 6, 5, 5, 4, 7,       // Back
        8, 9, 10, 10, 11, 8,    // Left
        12, 13, 14, 14, 15, 12, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };
    
    // Mark GPU buffers for recreation
    mesh.gpuBuffersCreated = false;
    mesh.gpuConstantBuffersCreated = false;
}

} // namespace WorldEditor::MeshGenerators
