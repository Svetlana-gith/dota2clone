#pragma once

#include "core/Types.h"
#include "core/MathUtils.h"

// Forward declarations for DirectX types
#ifdef DIRECTX_RENDERER
// Prevent Windows headers (pulled by d3d12.h) from defining min/max macros that break GLM.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d12.h>
#include <wrl/client.h>

// Forward declaration
class DirectXRenderer;
#endif

namespace WorldEditor {

#ifdef DIRECTX_RENDERER
using Microsoft::WRL::ComPtr;
#endif

// Transform component
struct TransformComponent {
    Vec3 position = Vec3(0.0f);
    Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    Vec3 scale = Vec3(1.0f);

    Mat4 getMatrix() const {
        Mat4 matrix = Mat4(1.0f);
        matrix = Math::translate(matrix, position);
        matrix = matrix * Math::toMat4(rotation);
        matrix = Math::scaleMatrix(matrix, scale);
        return matrix;
    }

    Vec3 transformPoint(const Vec3& point) const {
        return Vec3(getMatrix() * Vec4(point, 1.0f));
    }

    Vec3 transformVector(const Vec3& vector) const {
        return Vec3(getMatrix() * Vec4(vector, 0.0f));
    }
};

// Name component for identification
struct NameComponent {
    String name;

    NameComponent() = default;
    NameComponent(const String& name) : name(name) {}
};

// Terrain component - Tile-based terrain (Dota 2 Workshop Tools style)
struct TerrainComponent {
    // Tile-based terrain parameters (Dota 2 style)
    f32 tileSize = 128.0f;   // tile width/height in world units
    f32 heightStep = 128.0f; // height delta per level in world units
    i32 tilesX = 128;        // tile count X (resolution.x = tilesX+1)
    i32 tilesZ = 128;        // tile count Z (resolution.y = tilesZ+1)
    
    // Derived from tiles: resolution = (tilesX+1, tilesZ+1), size = tilesX * tileSize
    Vec2i resolution = Vec2i(129, 129);
    f32 size = 16384.0f; // tilesX * tileSize
    
    // Height range (derived from heightStep)
    f32 minHeight = 0.0f;
    f32 maxHeight = 15.0f * heightStep; // 15 steps above base

    // Discrete per-vertex height levels (resolution.x * resolution.y).
    // Height in world units: heightmap[i] == heightLevels[i] * heightStep.
    Vector<i16> heightLevels;

    // Float heightmap for rendering (derived from heightLevels via syncHeightmapFromLevels).
    Vector<f32> heightmap;

    // Optional metadata for ramps/paths (per-tile: tilesX * tilesZ).
    // 0 = none, 1 = ramp/path tile.
    Vector<u8> rampMask;

    f32 getHeightAt(i32 x, i32 y) const {
        if (x < 0 || x >= resolution.x || y < 0 || y >= resolution.y) {
            return 0.0f;
        }
        return heightmap[y * resolution.x + x];
    }

    void setHeightAt(i32 x, i32 y, f32 height) {
        if (x >= 0 && x < resolution.x && y >= 0 && y < resolution.y) {
            heightmap[y * resolution.x + x] = height;
        }
    }
};

// Object types for MOBA map editor
enum class ObjectType : u8 {
    None = 0,
    Tower,          // Defense tower spawn point
    CreepSpawn,     // Creep spawn point
    NeutralCamp,    // Neutral monster camp
    Tree,           // Decorative tree
    Rock,           // Decorative rock
    Building,       // Generic building
    Waypoint,       // Navigation waypoint for creeps
    Base,           // Team base (final destination for creeps)
    Custom          // Custom object type
};

// Object component for placed entities
struct ObjectComponent {
    ObjectType type = ObjectType::None;
    String assetPath;
    String layerName = "Default";
    bool isStatic = true;
    
    // MOBA-specific properties
    i32 teamId = 0;              // Team ID for towers/spawns (0 = neutral, 1 = team 1, 2 = team 2)
    f32 spawnRadius = 5.0f;      // Spawn radius for creeps/camps
    i32 maxUnits = 3;            // Max units for camps/spawns
    i32 spawnLane = -1;          // Lane for creep spawn (-1 = all lanes, 0 = Top, 1 = Middle, 2 = Bottom)
    String customData;            // JSON string for custom properties

    // Tower combat (used when type == Tower)
    // Note: these are serialized so you can tune them per-map.
    // IMPORTANT: our test map coordinates are ~0..300, so 700 would cover the whole map.
    f32 attackRange = 20.0f;     // world units (scaled for our map)
    f32 attackDamage = 120.0f;   // base damage per shot
    f32 attackSpeed = 1.0f;      // shots per second
    
    // Waypoint-specific properties
    i32 waypointOrder = 0;       // Order in the path (0 = first, higher = later)
    i32 waypointLane = -1;       // Lane for waypoint (-1 = all lanes, 0 = Top, 1 = Middle, 2 = Bottom)

    ObjectComponent() = default;
    ObjectComponent(ObjectType type) : type(type) {}
    ObjectComponent(const String& assetPath) : assetPath(assetPath) {}
};

// Runtime-only tower state (not serialized).
struct TowerRuntimeComponent {
    f32 attackCooldown = 0.0f;
};

// Navigation mesh component
struct NavMeshComponent {
    Vector<Vec3> vertices;
    Vector<i32> triangles;
    Vector<Vec3> normals;

    // Simplified navmesh data - in real implementation would use Recast/Detour
    bool isWalkable(const Vec3& position) const {
        // Simple implementation - check if position is above terrain
        // In real navmesh would check proper navigation polygons
        return position.y > 0.0f;
    }
};

// Layer component for grouping
struct LayerComponent {
    String name;
    bool visible = true;
    bool locked = false;

    LayerComponent() = default;
    LayerComponent(const String& name) : name(name) {}
};

// Camera component for editor cameras
struct CameraComponent {
    f32 fov = 60.0f;
    f32 nearPlane = 0.1f;
    f32 farPlane = 1000.0f;
    f32 aspectRatio = 16.0f / 9.0f;

    Mat4 getProjectionMatrix() const {
        return glm::perspective(Math::radians(fov), aspectRatio, nearPlane, farPlane);
    }
};

// Vertex structure for GPU buffers
struct MeshVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texCoord;
};

// Constant buffer structures for shader parameters
struct PerObjectConstants {
    Mat4 worldMatrix;
    Mat4 viewProjMatrix;
    // Padding to 256 bytes (64 + 64 + 128)
    f32 padding[32];
};

struct alignas(256) MaterialConstants {
    // Pack into float4 to keep layout deterministic across compilers/GLM settings.
    // baseColor_metallic.xyz = baseColor, baseColor_metallic.w = metallic
    Vec4 baseColor_metallic = Vec4(1.0f);
    // emissive_roughness.xyz = emissiveColor, emissive_roughness.w = roughness
    Vec4 emissive_roughness = Vec4(0.0f, 0.0f, 0.0f, 0.5f);
    // Pad to 256 bytes for D3D12 constant buffer alignment.
    f32 _padding[56] = {};
};

// Creep component for MOBA creeps
enum class CreepLane : u8 {
    Top = 0,
    Middle = 1,
    Bottom = 2
};

enum class CreepType : u8 {
    Melee = 0,      // Melee creep (метчик)
    Ranged = 1,     // Ranged creep (маг)
    Siege = 2,      // Siege creep (катапульта)
    LargeMelee = 3, // Large melee (большой метчик)
    LargeRanged = 4,// Large ranged (большой маг)
    LargeSiege = 5, // Large siege (большая катапульта)
    MegaMelee = 6,  // Mega melee (мега метчик)
    MegaRanged = 7, // Mega ranged (мега маг)
    MegaSiege = 8   // Mega siege (мега катапульта)
};

enum class CreepState : u8 {
    Idle = 0,
    Moving = 1,
    Attacking = 2,
    Dead = 3
};

struct CreepComponent {
    // Basic stats
    f32 maxHealth = 550.0f;
    f32 currentHealth = 550.0f;
    f32 damage = 19.0f;
    // IMPORTANT: our test map coordinates are ~0..300, so "Dota-like" 600-700 is too big here.
    f32 attackRange = 5.0f;
    f32 attackSpeed = 1.0f; // attacks per second
    f32 moveSpeed = 5.0f; // Very slow speed for better visibility and control
    f32 armor = 0.0f;
    
    // Team and lane
    i32 teamId = 0; // 1 = Radiant, 2 = Dire
    CreepLane lane = CreepLane::Middle;
    CreepType type = CreepType::Melee; // Type of creep
    
    // State
    CreepState state = CreepState::Moving;
    Entity targetEntity = INVALID_ENTITY; // Current attack target
    Vec3 targetPosition = Vec3(0.0f); // Movement target (current waypoint)
    Vec3 laneDirection = Vec3(1.0f, 0.0f, 1.0f); // Direction along lane (fallback)
    i32 currentWaypointIndex = 0; // Current waypoint index in path
    Vector<Vec3> path; // Path of waypoints to follow
    
    // Timing
    f32 attackCooldown = 0.0f;
    f32 spawnTime = 0.0f;
    f32 deathTime = 0.0f; // Time when creep died (for cleanup delay)
    f32 deathDelay = 2.0f; // Time before removing dead creep (seconds)

    // Perf: throttle expensive per-tick queries
    f32 targetSearchCooldown = 0.0f; // Seconds until next full target scan
    f32 pathCheckCooldown = 0.0f;    // Seconds until next path-clear check
    bool lastPathClear = true;       // Cached result from last path check
    f32 waypointStuckTime = 0.0f;    // Time stuck at current waypoint (to detect circling)
    
    // Spawn info
    Entity spawnPoint = INVALID_ENTITY; // CreepSpawn entity that spawned this creep
    i32 formationIndex = 0; // Index in wave formation for spacing
    
    CreepComponent() = default;
    CreepComponent(i32 team, CreepLane laneType) : teamId(team), lane(laneType) {}
};

// Runtime-only projectile for ranged creep attacks (not serialized).
struct ProjectileComponent {
    Entity attacker = INVALID_ENTITY;
    Entity target = INVALID_ENTITY;
    i32 teamId = 0;
    bool active = false;
    bool isTower = false; // distinguish tower projectiles for pooling/material

    // Movement
    f32 speed = 80.0f;    // world units per second
    f32 hitRadius = 1.0f; // how close is considered a hit

    // Damage payload (applied on hit)
    f32 baseDamage = 0.0f;

    // Lifetime
    f32 life = 0.0f;
    f32 maxLife = 5.0f;
};

// Health component for towers, buildings, and other structures
struct HealthComponent {
    f32 maxHealth = 1000.0f;
    f32 currentHealth = 1000.0f;
    f32 armor = 5.0f;
    f32 magicResistance = 0.0f;
    bool isDead = false;
    
    HealthComponent() = default;
    HealthComponent(f32 maxHP) : maxHealth(maxHP), currentHealth(maxHP) {}
};

// Collision component for physics and collision detection
enum class CollisionShape : u8 {
    None = 0,
    Box = 1,
    Sphere = 2,
    Capsule = 3
};

struct CollisionComponent {
    CollisionShape shape = CollisionShape::Box;
    
    // Box dimensions (used when shape == Box)
    Vec3 boxSize = Vec3(1.0f, 1.0f, 1.0f);
    
    // Sphere radius (used when shape == Sphere)
    f32 radius = 0.5f;
    
    // Capsule dimensions (used when shape == Capsule)
    f32 capsuleHeight = 2.0f;
    f32 capsuleRadius = 0.5f;
    
    // Collision flags
    bool isStatic = false;      // Static objects don't move
    bool isTrigger = false;      // Trigger volumes don't block movement
    bool blocksMovement = true;  // Whether this object blocks other objects
    
    // Offset from transform position
    Vec3 offset = Vec3(0.0f);
    
    CollisionComponent() = default;
    CollisionComponent(CollisionShape s) : shape(s) {}
    
    // Helper to get bounding box
    Math::AABB getAABB(const Vec3& position) const {
        Vec3 center = position + offset;
        Math::AABB aabb;
        
        switch (shape) {
            case CollisionShape::Box: {
                Vec3 halfSize = boxSize * 0.5f;
                aabb.min = center - halfSize;
                aabb.max = center + halfSize;
                break;
            }
            case CollisionShape::Sphere: {
                Vec3 r(radius);
                aabb.min = center - r;
                aabb.max = center + r;
                break;
            }
            case CollisionShape::Capsule: {
                Vec3 r(capsuleRadius);
                Vec3 halfHeight(0.0f, capsuleHeight * 0.5f, 0.0f);
                aabb.min = center - r - halfHeight;
                aabb.max = center + r + halfHeight;
                break;
            }
            default:
                aabb.min = center;
                aabb.max = center;
                break;
        }
        
        return aabb;
    }
    
    // Helper to check if point is inside
    bool containsPoint(const Vec3& point, const Vec3& position) const {
        Vec3 localPoint = point - (position + offset);
        
        switch (shape) {
            case CollisionShape::Box: {
                Vec3 halfSize = boxSize * 0.5f;
                return std::abs(localPoint.x) <= halfSize.x &&
                       std::abs(localPoint.y) <= halfSize.y &&
                       std::abs(localPoint.z) <= halfSize.z;
            }
            case CollisionShape::Sphere: {
                return glm::length(localPoint) <= radius;
            }
            case CollisionShape::Capsule: {
                f32 yDist = std::abs(localPoint.y);
                f32 halfHeight = capsuleHeight * 0.5f;
                if (yDist > halfHeight) {
                    // Check sphere caps
                    Vec3 capCenter(0.0f, (localPoint.y > 0.0f ? halfHeight : -halfHeight), 0.0f);
                    return glm::length(localPoint - capCenter) <= capsuleRadius;
                } else {
                    // Check cylinder body
                    Vec2 xz(localPoint.x, localPoint.z);
                    return glm::length(xz) <= capsuleRadius;
                }
            }
            default:
                return false;
        }
    }
};

// Static assertions for constant buffer sizes
static_assert(sizeof(PerObjectConstants) == 256, "PerObjectConstants must be 256 bytes");
static_assert(sizeof(MaterialConstants) == 256, "MaterialConstants must be 256 bytes");

// Mesh component for DirectX rendering
struct MeshComponent {
    Vector<Vec3> vertices;
    Vector<Vec3> normals;
    Vector<Vec2> texCoords;
    Vector<u32> indices;

#ifdef DIRECTX_RENDERER
    // DirectX buffers (created by renderer)
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    ComPtr<ID3D12Resource> vertexBufferUpload; // For initialization
    ComPtr<ID3D12Resource> indexBufferUpload;  // For initialization
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    bool gpuBuffersCreated = false;
    // When CPU-side mesh data changes, mark this true so renderer can upload into existing buffers (or recreate if size differs).
    bool gpuUploadNeeded = false;

    // Constant buffers for shader parameters
    ComPtr<ID3D12Resource> perObjectConstantBuffer;
    ComPtr<ID3D12Resource> perObjectConstantBufferUpload;
    bool gpuConstantBuffersCreated = false;

    // Static renderer reference for safe resource cleanup
    static class DirectXRenderer* s_renderer;
#endif

    // Material reference
    Entity materialEntity = INVALID_ENTITY;

    // Mesh properties
    String name;
    bool visible = true;

    MeshComponent() = default;
    MeshComponent(const String& name) : name(name) {}

#ifdef DIRECTX_RENDERER
    // Destructor for safe GPU resource cleanup
    ~MeshComponent();
#endif

    // Helper to get vertex count
    size_t getVertexCount() const { return vertices.size(); }

    // Helper to get index count
    size_t getIndexCount() const { return indices.size(); }
};

// Material component for DirectX rendering
struct MaterialComponent {
    Vec3 baseColor = Vec3(1.0f);
    f32 metallic = 0.0f;
    f32 roughness = 0.5f;
    Vec3 emissiveColor = Vec3(0.0f);

    // Texture references (filenames for now, will be DirectX resources)
    String baseColorTexture;
    String normalTexture;
    String metallicRoughnessTexture;
    String emissiveTexture;

#ifdef DIRECTX_RENDERER
    // DirectX resources (created by renderer)
    ComPtr<ID3D12Resource> constantBuffer;
    ComPtr<ID3D12Resource> constantBufferUpload;
    bool gpuBufferCreated = false;
    // Texture resources would go here
#endif

    String name;

    MaterialComponent() = default;
    MaterialComponent(const String& name) : name(name) {}
};

// Light component
struct LightComponent {
    enum Type { Directional, Point, Spot };

    Type type = Directional;
    Vec3 color = Vec3(1.0f);
    f32 intensity = 1.0f;

    // Directional/Spot light
    Vec3 direction = Vec3(0.0f, -1.0f, 0.0f);

    // Point/Spot light
    f32 range = 10.0f;

    // Spot light
    f32 spotAngle = 30.0f;
};

// Terrain material component for multi-layer texturing
struct TerrainMaterialComponent {
    struct TextureLayer {
        String diffuseTexture;
        String normalTexture;
        f32 tiling = 1.0f;
        f32 strength = 1.0f;
    };
    
    Vector<TextureLayer> layers;
    Vector<f32> blendWeights; // Per-vertex blend weights for each layer
    i32 activeLayer = 0;
    
    TerrainMaterialComponent() {
        // Default grass layer
        layers.push_back({"textures/grass_diffuse.png", "textures/grass_normal.png", 4.0f, 1.0f});
    }
};

}
