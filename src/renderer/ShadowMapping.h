#pragma once

#include "core/Types.h"

#ifdef DIRECTX_RENDERER
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace WorldEditor {

// Shadow mapping system for realistic shadows
class ShadowMapping {
public:
    ShadowMapping();
    ~ShadowMapping();
    
    // Initialize shadow mapping with DirectX device
    bool initialize(ID3D12Device* device, uint32_t shadowMapSize = 2048);
    void shutdown();
    
    // Update shadow camera matrix from light direction
    void updateShadowCamera(const Vec3& lightDirection, const Vec3& sceneCenter, float sceneRadius);
    
    // Begin shadow pass - render to shadow map
    void beginShadowPass(ID3D12GraphicsCommandList* commandList);
    void endShadowPass(ID3D12GraphicsCommandList* commandList);
    
    // Get shadow map for binding in main pass
    ID3D12Resource* getShadowMap() const { return shadowMap_.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE getShadowMapSRV() const { return shadowMapSrvGpuHandle_; }
    
    // Get light view-projection matrix for shadow mapping
    const Mat4& getLightViewProjMatrix() const { return lightViewProjMatrix_; }
    
    // Shadow mapping constants for shaders
    struct ShadowConstants {
        Mat4 lightViewProjMatrix;
        Vec4 shadowMapSize = Vec4(2048.0f, 2048.0f, 1.0f/2048.0f, 1.0f/2048.0f); // size, inv_size
        Vec4 shadowParams = Vec4(0.001f, 0.5f, 0.0f, 0.0f); // bias, strength, unused, unused
    };
    
    const ShadowConstants& getShadowConstants() const { return shadowConstants_; }
    
private:
    uint32_t shadowMapSize_ = 2048;
    Mat4 lightViewProjMatrix_;
    ShadowConstants shadowConstants_;
    
#ifdef DIRECTX_RENDERER
    ID3D12Device* device_ = nullptr;
    
    // Shadow map resources
    ComPtr<ID3D12Resource> shadowMap_;
    ComPtr<ID3D12DescriptorHeap> shadowMapDsvHeap_;
    ComPtr<ID3D12DescriptorHeap> shadowMapSrvHeap_;
    
    D3D12_CPU_DESCRIPTOR_HANDLE shadowMapDsvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE shadowMapSrvCpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE shadowMapSrvGpuHandle_{};
    
    // Shadow pass viewport and scissor
    D3D12_VIEWPORT shadowViewport_{};
    D3D12_RECT shadowScissorRect_{};
#endif
    
    bool createShadowMap();
    bool createShadowMapViews();
};

} // namespace WorldEditor