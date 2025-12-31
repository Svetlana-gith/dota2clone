#pragma once

#include "core/Types.h"
#include "world/Components.h"

namespace WorldEditor {

// Lighting constants for shaders
struct alignas(256) LightingConstants {
    // Directional light
    Vec4 lightDirection = Vec4(-0.5f, -0.8f, -0.3f, 0.0f);  // направление света
    Vec4 lightColor = Vec4(1.0f, 0.95f, 0.8f, 1.0f);       // цвет света (теплый белый)
    Vec4 ambientColor = Vec4(0.2f, 0.25f, 0.35f, 1.0f);    // ambient (холодный)
    
    // Camera position for specular
    Vec4 cameraPosition = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    
    // Material properties
    Vec4 materialParams = Vec4(0.8f, 0.2f, 32.0f, 1.0f);   // diffuse, specular, shininess, unused
    
    // Padding to 256 bytes
    f32 _padding[44] = {};
};

static_assert(sizeof(LightingConstants) == 256, "LightingConstants must be 256 bytes");

class LightingSystem {
public:
    LightingSystem();
    ~LightingSystem();
    
    // Initialize lighting system with DirectX device
    bool initialize(ID3D12Device* device);
    void shutdown();
    
    // Update lighting parameters
    void updateLighting(const Vec3& cameraPosition, float time);
    
    // Get lighting constant buffer for binding
    ID3D12Resource* getLightingConstantBuffer() const { return lightingConstantBuffer_.Get(); }
    
    // Get current lighting constants
    const LightingConstants& getLightingConstants() const { return lightingConstants_; }
    
    // Set directional light parameters
    void setDirectionalLight(const Vec3& direction, const Vec3& color);
    void setAmbientLight(const Vec3& color);

    // Editor viewport styling: when > 0, terrain shader uses this as checker cell size in world units.
    // Set to 0 to disable checker shading.
    void setEditorCheckerCellSize(float cellSize);
    
private:
    LightingConstants lightingConstants_;
    
#ifdef DIRECTX_RENDERER
    ComPtr<ID3D12Resource> lightingConstantBuffer_;
    ComPtr<ID3D12Resource> lightingConstantBufferUpload_;
    bool gpuBufferCreated_ = false;
    ID3D12Device* device_ = nullptr;
#endif
    
    void updateConstantBuffer();
};

} // namespace WorldEditor