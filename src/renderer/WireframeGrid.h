#pragma once

#include "core/Types.h"

// Prevent Windows min/max macros from conflicting with other headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef DIRECTX_RENDERER
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace WorldEditor {

// Forward declarations
struct TerrainComponent;
struct MeshComponent;

// Wireframe grid system for terrain visualization
class WireframeGrid {
public:
    WireframeGrid();
    ~WireframeGrid();
    
    // Initialize wireframe grid system
    bool initialize(ID3D12Device* device);
    void shutdown();
    
    // Generate wireframe grid for terrain
    bool generateGrid(const TerrainComponent& terrain, const MeshComponent& mesh);
    
    // Render wireframe grid
    void render(ID3D12GraphicsCommandList* commandList,
                const Mat4& worldMatrix,
                const Mat4& viewProjMatrix,
                const Vec3& cameraPosition);
    
    // Enable/disable wireframe rendering
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    
    // Get pipeline resources for external use (e.g., path rendering)
    ID3D12RootSignature* getRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* getPipelineState() const { return pipelineState_.Get(); }
    bool isPipelineReady() const { return gpuResourcesCreated_ && rootSignature_ && pipelineState_; }
    
private:
    bool enabled_ = false;
    
#ifdef DIRECTX_RENDERER
    ID3D12Device* device_ = nullptr;
    
    // Grid geometry
    Vector<Vec3> gridVertices_;
    Vector<u32> gridIndices_;
    
    // GPU resources
    ComPtr<ID3D12Resource> vertexBuffer_;
    ComPtr<ID3D12Resource> indexBuffer_;
    ComPtr<ID3D12Resource> constantBuffer_;
    
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    
    // Shaders for wireframe
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
    ComPtr<ID3DBlob> vertexShader_;
    ComPtr<ID3DBlob> pixelShader_;
    
    bool gpuResourcesCreated_ = false;
#endif
    
    bool createGPUResources();
    bool createWireframeShaders();
    bool createWireframePipeline();
    void updateConstantBuffer(const Mat4& worldMatrix, const Mat4& viewProjMatrix, const Vec3& cameraPosition);
};

} // namespace WorldEditor