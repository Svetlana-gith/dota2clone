#include "LightingSystem.h"

#ifdef DIRECTX_RENDERER
#include "DirectXRenderer.h"
#include <cmath>
#endif

namespace WorldEditor {

LightingSystem::LightingSystem() {
    // Настройка дефолтного освещения
    lightingConstants_.lightDirection = Vec4(-0.5f, -0.8f, -0.3f, 0.0f);
    lightingConstants_.lightColor = Vec4(1.0f, 0.95f, 0.8f, 1.5f); // немного ярче
    lightingConstants_.ambientColor = Vec4(0.3f, 0.35f, 0.4f, 1.0f); // больше ambient для лучшей видимости
    lightingConstants_.materialParams = Vec4(0.8f, 0.3f, 16.0f, 1.0f);
}

LightingSystem::~LightingSystem() {
    shutdown();
}

bool LightingSystem::initialize(ID3D12Device* device) {
#ifdef DIRECTX_RENDERER
    if (!device) return false;
    
    device_ = device;
    
    // Create constant buffer for lighting
    const UINT bufferSize = sizeof(LightingConstants);
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = bufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&lightingConstantBuffer_)
    );
    
    if (FAILED(hr)) {
        return false;
    }
    
    lightingConstantBuffer_->SetName(L"LightingConstantBuffer");
    
    // Upload initial data
    updateConstantBuffer();
    gpuBufferCreated_ = true;
    
    return true;
#else
    return false;
#endif
}

void LightingSystem::shutdown() {
#ifdef DIRECTX_RENDERER
    lightingConstantBuffer_.Reset();
    lightingConstantBufferUpload_.Reset();
    gpuBufferCreated_ = false;
    device_ = nullptr;
#endif
}

void LightingSystem::updateLighting(const Vec3& cameraPosition, float time) {
    // Update camera position
    lightingConstants_.cameraPosition = Vec4(cameraPosition, 1.0f);
    
    // Animate light direction slightly for more dynamic lighting
    const float lightRotation = time * 0.1f; // медленное вращение
    const float baseX = -0.5f;
    const float baseZ = -0.3f;
    lightingConstants_.lightDirection.x = baseX + 0.2f * std::sin(lightRotation);
    lightingConstants_.lightDirection.z = baseZ + 0.1f * std::cos(lightRotation);
    
    // Normalize light direction
    Vec3 lightDir = Vec3(lightingConstants_.lightDirection);
    lightDir = glm::normalize(lightDir);
    lightingConstants_.lightDirection = Vec4(lightDir, 0.0f);
    
    updateConstantBuffer();
}

void LightingSystem::setDirectionalLight(const Vec3& direction, const Vec3& color) {
    Vec3 normalizedDir = glm::normalize(direction);
    lightingConstants_.lightDirection = Vec4(normalizedDir, 0.0f);
    lightingConstants_.lightColor = Vec4(color, lightingConstants_.lightColor.w);
    updateConstantBuffer();
}

void LightingSystem::setAmbientLight(const Vec3& color) {
    lightingConstants_.ambientColor = Vec4(color, lightingConstants_.ambientColor.w);
    updateConstantBuffer();
}

void LightingSystem::setEditorCheckerCellSize(float cellSize) {
    // Use materialParams.w as an editor-only parameter (0 disables checker shading).
    lightingConstants_.materialParams.w = (std::max)(0.0f, cellSize);
    updateConstantBuffer();
}

void LightingSystem::updateConstantBuffer() {
#ifdef DIRECTX_RENDERER
    if (!gpuBufferCreated_ || !lightingConstantBuffer_) return;
    
    // Map and copy data
    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 }; // We don't read from this resource on CPU
    
    HRESULT hr = lightingConstantBuffer_->Map(0, &readRange, &mappedData);
    if (SUCCEEDED(hr)) {
        memcpy(mappedData, &lightingConstants_, sizeof(LightingConstants));
        lightingConstantBuffer_->Unmap(0, nullptr);
    }
#endif
}

} // namespace WorldEditor