#pragma once

// Prevent Windows headers (pulled by d3d12.h) from defining min/max macros that break GLM.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

using Microsoft::WRL::ComPtr;

// Forward declaration
namespace WorldEditor {
    class LightingSystem;
    class ShadowMapping;
    class SkyRenderer;
    class WireframeGrid;
}

// Структура вершины для треугольника
struct Vertex {
    glm::vec3 position;
    glm::vec4 color;
};

class DirectXRenderer {
public:
    DirectXRenderer();
    ~DirectXRenderer();

    // Инициализация и завершение
    bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    // Resize swapchain/backbuffer to match window client size.
    void Resize(uint32_t width, uint32_t height);

    // Рендеринг
    void BeginFrame();
    void BeginSwapchainPass(float clearColor[4]);
    void BeginOffscreenPass(uint32_t width, uint32_t height, float clearColor[4]);
    void EndOffscreenPass();
    void EndFrame();
    bool Present(); // Returns true on success, false on error

    // Получение интерфейсов для внешнего использования
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }

    // Безопасное освобождение GPU ресурсов (для использования в деструкторах компонентов)
    void SafeReleaseResource(ComPtr<ID3D12Resource>& resource);
    
    // Отложенное освобождение ресурсов (более безопасно)
    void DeferredReleaseResource(ComPtr<ID3D12Resource> resource);

    // Размеры окна
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    // SRV descriptor heap access for ImGui
    ID3D12DescriptorHeap* GetSrvHeap() const { return m_srvHeap.Get(); }
    uint32_t GetSrvDescriptorSize() const { return m_srvDescriptorSize; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle() const {
        return m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const {
        return m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    }
    
    // Current render target view (for UI rendering)
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;

    // Offscreen viewport SRV (descriptor index 0 in SRV heap).
    D3D12_GPU_DESCRIPTOR_HANDLE GetViewportSrvGpuHandle() const { return m_viewportSrvGpuHandle; }

    // Lighting system access
    WorldEditor::LightingSystem* GetLightingSystem() const { return m_lightingSystem.get(); }
    WorldEditor::ShadowMapping* GetShadowMapping() const { return m_shadowMapping.get(); }
    void UpdateLighting(const glm::vec3& cameraPosition, float time);
    
    // Editor sky background (Unreal-like viewport)
    WorldEditor::SkyRenderer* GetSkyRenderer() const { return m_skyRenderer.get(); }

    // Wireframe grid access
    WorldEditor::WireframeGrid* GetWireframeGrid() const { return m_wireframeGrid.get(); }
    
    // GPU synchronization for safe resource cleanup
    void WaitForPreviousFrame();

private:
    static constexpr uint32_t kFrameCount = 3;
    static constexpr uint32_t kRtvExtraCount = 1; // offscreen RT
    static constexpr uint32_t kRtvCount = kFrameCount + kRtvExtraCount;

    static constexpr uint32_t kViewportSrvIndex = 0;
    static constexpr uint32_t kViewportRtvIndex = kFrameCount;

    // Основные DirectX объекты
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain4> m_swapChain;

    // Command management
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[kFrameCount];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Descriptor heaps
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    // Render targets
    std::vector<ComPtr<ID3D12Resource>> m_renderTargets;
    ComPtr<ID3D12Resource> m_depthStencil;

    // Offscreen viewport render target
    ComPtr<ID3D12Resource> m_viewportRT;
    uint32_t m_viewportRTWidth = 0;
    uint32_t m_viewportRTHeight = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_viewportRtvHandle{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_viewportSrvCpuHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_viewportSrvGpuHandle{};

    // Offscreen viewport depth stencil (DSV heap has 1 slot reserved for this).
    ComPtr<ID3D12Resource> m_viewportDS;
    D3D12_CPU_DESCRIPTOR_HANDLE m_viewportDsvHandle{};

    // Synchronization objects
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    uint64_t m_fenceValue;
    uint64_t m_fenceValues[kFrameCount];

    // Guard to make Shutdown() idempotent (GameMain used to call Shutdown() + delete,
    // while ~DirectXRenderer() also calls Shutdown()).
    bool m_isShutdown = false;

    // Triangle rendering resources
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3DBlob> m_vertexShader;
    ComPtr<ID3DBlob> m_pixelShader;

    // Upload buffers for initialization
    ComPtr<ID3D12Resource> m_vertexBufferUpload;
    ComPtr<ID3D12Resource> m_indexBufferUpload;

    // Размеры окна
    HWND m_hwnd = nullptr;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_frameIndex;

    // Descriptor sizes
    uint32_t m_rtvDescriptorSize;
    uint32_t m_dsvDescriptorSize;
    uint32_t m_srvDescriptorSize;

    // Отложенное удаление ресурсов для безопасности
    struct DeferredResource {
        ComPtr<ID3D12Resource> resource;
        uint64_t frameValue;
    };
    std::vector<DeferredResource> m_deferredReleases;

    // Lighting system
    std::unique_ptr<WorldEditor::LightingSystem> m_lightingSystem;
    std::unique_ptr<WorldEditor::ShadowMapping> m_shadowMapping;
    std::unique_ptr<WorldEditor::SkyRenderer> m_skyRenderer;
    std::unique_ptr<WorldEditor::WireframeGrid> m_wireframeGrid;

    // Private методы инициализации
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd);
    bool CreateDescriptorHeaps();
    bool CreateRenderTargets();
    bool CreateCommandAllocators();
    bool CreateSynchronizationObjects();

    // Triangle rendering методы
    bool CreateTriangleResources();
    bool LoadShaders();
    bool CreateRootSignature();
    bool CreatePipelineState();
    bool CreateVertexBuffer();
    bool CreateIndexBuffer();
    void PopulateTriangleCommandList();

    // Helper методы
    void WaitForFrame(uint32_t frameIndex);
    void ProcessDeferredReleases();
    void EnsureViewportRenderTarget(uint32_t width, uint32_t height);
    void EnsureViewportDepthStencil(uint32_t width, uint32_t height);
    void PopulateCommandList();
    D3D12_CPU_DESCRIPTOR_HANDLE GetViewportRenderTargetView() const;
    
#ifdef DX12_ENABLE_DEBUG_LAYER
    void CheckDebugMessages();
#endif
};

class DirectXException : public std::exception {
public:
    DirectXException(HRESULT hr, const std::string& message);

    HRESULT GetHRESULT() const { return m_hr; }
    const char* what() const override { return m_message.c_str(); }

private:
    HRESULT m_hr;
    std::string m_message;
};

#define DX_CHECK(hr) \
    if (FAILED(hr)) { \
        throw DirectXException(hr, std::string(__FUNCTION__) + " failed"); \
    }
