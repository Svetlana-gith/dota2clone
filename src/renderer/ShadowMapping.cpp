#include "ShadowMapping.h"

#ifdef DIRECTX_RENDERER
#include "DirectXRenderer.h"
#include <iostream>
#endif

namespace WorldEditor {

ShadowMapping::ShadowMapping() {
    // Initialize shadow constants
    shadowConstants_.shadowMapSize = Vec4(2048.0f, 2048.0f, 1.0f/2048.0f, 1.0f/2048.0f);
    shadowConstants_.shadowParams = Vec4(0.001f, 0.8f, 0.0f, 0.0f); // bias, strength
}

ShadowMapping::~ShadowMapping() {
    shutdown();
}

bool ShadowMapping::initialize(ID3D12Device* device, uint32_t shadowMapSize) {
#ifdef DIRECTX_RENDERER
    if (!device) return false;
    
    device_ = device;
    shadowMapSize_ = shadowMapSize;
    
    // Update shadow constants
    shadowConstants_.shadowMapSize = Vec4(
        float(shadowMapSize_), float(shadowMapSize_), 
        1.0f/float(shadowMapSize_), 1.0f/float(shadowMapSize_)
    );
    
    // Create shadow map resources
    if (!createShadowMap()) {
        std::cerr << "Failed to create shadow map" << std::endl;
        return false;
    }
    
    if (!createShadowMapViews()) {
        std::cerr << "Failed to create shadow map views" << std::endl;
        return false;
    }
    
    // Setup shadow viewport
    shadowViewport_.TopLeftX = 0.0f;
    shadowViewport_.TopLeftY = 0.0f;
    shadowViewport_.Width = float(shadowMapSize_);
    shadowViewport_.Height = float(shadowMapSize_);
    shadowViewport_.MinDepth = 0.0f;
    shadowViewport_.MaxDepth = 1.0f;
    
    shadowScissorRect_.left = 0;
    shadowScissorRect_.top = 0;
    shadowScissorRect_.right = shadowMapSize_;
    shadowScissorRect_.bottom = shadowMapSize_;
    
    std::cout << "Shadow mapping initialized with " << shadowMapSize_ << "x" << shadowMapSize_ << " shadow map" << std::endl;
    return true;
#else
    return false;
#endif
}

void ShadowMapping::shutdown() {
#ifdef DIRECTX_RENDERER
    shadowMap_.Reset();
    shadowMapDsvHeap_.Reset();
    shadowMapSrvHeap_.Reset();
    device_ = nullptr;
#endif
}

void ShadowMapping::updateShadowCamera(const Vec3& lightDirection, const Vec3& sceneCenter, float sceneRadius) {
    // Create orthographic projection for directional light
    const float shadowDistance = sceneRadius * 2.0f;
    const Mat4 lightProjection = glm::ortho(
        -sceneRadius, sceneRadius,    // left, right
        -sceneRadius, sceneRadius,    // bottom, top
        0.1f, shadowDistance         // near, far
    );
    
    // Create view matrix looking from light position towards scene center
    const Vec3 lightPos = sceneCenter - glm::normalize(lightDirection) * (shadowDistance * 0.5f);
    const Vec3 up = std::abs(lightDirection.y) > 0.9f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
    const Mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);
    
    // Combine view and projection
    lightViewProjMatrix_ = lightProjection * lightView;
    shadowConstants_.lightViewProjMatrix = lightViewProjMatrix_;
}

void ShadowMapping::beginShadowPass(ID3D12GraphicsCommandList* commandList) {
#ifdef DIRECTX_RENDERER
    if (!commandList || !shadowMap_) return;
    
    // Transition shadow map to depth write
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = shadowMap_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    
    // Set shadow map as depth target
    commandList->OMSetRenderTargets(0, nullptr, FALSE, &shadowMapDsvHandle_);
    
    // Clear shadow map
    commandList->ClearDepthStencilView(shadowMapDsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    
    // Set shadow viewport
    commandList->RSSetViewports(1, &shadowViewport_);
    commandList->RSSetScissorRects(1, &shadowScissorRect_);
#endif
}

void ShadowMapping::endShadowPass(ID3D12GraphicsCommandList* commandList) {
#ifdef DIRECTX_RENDERER
    if (!commandList || !shadowMap_) return;
    
    // Transition shadow map to shader resource
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = shadowMap_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
#endif
}

bool ShadowMapping::createShadowMap() {
#ifdef DIRECTX_RENDERER
    if (!device_) return false;
    
    // Create shadow map texture (depth buffer)
    D3D12_RESOURCE_DESC shadowMapDesc = {};
    shadowMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    shadowMapDesc.Width = shadowMapSize_;
    shadowMapDesc.Height = shadowMapSize_;
    shadowMapDesc.DepthOrArraySize = 1;
    shadowMapDesc.MipLevels = 1;
    shadowMapDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Typeless for both DSV and SRV
    shadowMapDesc.SampleDesc.Count = 1;
    shadowMapDesc.SampleDesc.Quality = 0;
    shadowMapDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    shadowMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;
    
    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &shadowMapDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&shadowMap_)
    );
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create shadow map texture" << std::endl;
        return false;
    }
    
    shadowMap_->SetName(L"ShadowMap");
    return true;
#else
    return false;
#endif
}

bool ShadowMapping::createShadowMapViews() {
#ifdef DIRECTX_RENDERER
    if (!device_ || !shadowMap_) return false;
    
    // Create DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    
    HRESULT hr = device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&shadowMapDsvHeap_));
    if (FAILED(hr)) {
        std::cerr << "Failed to create shadow map DSV heap" << std::endl;
        return false;
    }
    
    // Create SRV heap
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    hr = device_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&shadowMapSrvHeap_));
    if (FAILED(hr)) {
        std::cerr << "Failed to create shadow map SRV heap" << std::endl;
        return false;
    }
    
    // Create DSV
    shadowMapDsvHandle_ = shadowMapDsvHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    device_->CreateDepthStencilView(shadowMap_.Get(), &dsvDesc, shadowMapDsvHandle_);
    
    // Create SRV
    shadowMapSrvCpuHandle_ = shadowMapSrvHeap_->GetCPUDescriptorHandleForHeapStart();
    shadowMapSrvGpuHandle_ = shadowMapSrvHeap_->GetGPUDescriptorHandleForHeapStart();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device_->CreateShaderResourceView(shadowMap_.Get(), &srvDesc, shadowMapSrvCpuHandle_);
    
    return true;
#else
    return false;
#endif
}

} // namespace WorldEditor