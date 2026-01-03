#include "DirectXRenderer.h"
#include "LightingSystem.h"
#include "ShadowMapping.h"
#include "SkyRenderer.h"
#include "WireframeGrid.h"
#include <dxgidebug.h>
#include <iostream>

DirectXRenderer::DirectXRenderer()
    : m_width(0)
    , m_height(0)
    , m_frameIndex(0)
    , m_rtvDescriptorSize(0)
    , m_dsvDescriptorSize(0)
    , m_fenceValue(0)
    , m_fenceEvent(nullptr)
{
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        m_fenceValues[i] = 0;
    }
}

DirectXRenderer::~DirectXRenderer() {
    Shutdown();
}

bool DirectXRenderer::Initialize(HWND hwnd, uint32_t width, uint32_t height) {
    try {
        m_hwnd = hwnd;
        m_width = width;
        m_height = height;

        std::cout << "Creating DirectX device..." << std::endl;
        // Создаем устройство
        if (!CreateDevice()) {
            std::cerr << "Failed to create device" << std::endl;
            return false;
        }

        std::cout << "Creating command queue..." << std::endl;
        // Создаем командную очередь
        if (!CreateCommandQueue()) {
            std::cerr << "Failed to create command queue" << std::endl;
            return false;
        }

        std::cout << "Creating swap chain..." << std::endl;
        // Создаем swap chain
        if (!CreateSwapChain(hwnd)) {
            std::cerr << "Failed to create swap chain" << std::endl;
            return false;
        }

        std::cout << "Creating descriptor heaps..." << std::endl;
        // Создаем descriptor heaps
        if (!CreateDescriptorHeaps()) {
            std::cerr << "Failed to create descriptor heaps" << std::endl;
            return false;
        }

        std::cout << "Creating render targets..." << std::endl;
        // Создаем render targets
        if (!CreateRenderTargets()) {
            std::cerr << "Failed to create render targets" << std::endl;
            return false;
        }

        std::cout << "Creating command allocators..." << std::endl;
        // Создаем command allocator и list
        if (!CreateCommandAllocators()) {
            std::cerr << "Failed to create command allocators" << std::endl;
            return false;
        }

        std::cout << "Creating synchronization objects..." << std::endl;
        // Создаем объекты синхронизации
        if (!CreateSynchronizationObjects()) {
            std::cerr << "Failed to create synchronization objects" << std::endl;
            return false;
        }

        std::cout << "Creating triangle resources..." << std::endl;
        // Создаем ресурсы для рендеринга треугольника
        if (!CreateTriangleResources()) {
            std::cerr << "Failed to create triangle resources" << std::endl;
            return false;
        }

        std::cout << "Executing initialization commands..." << std::endl;
        // Выполняем команды инициализации ресурсов
        HRESULT hr = m_commandList->Close();
        if (FAILED(hr)) {
            std::cerr << "Failed to close command list: " << std::hex << hr << std::endl;
            return false;
        }

        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        std::cout << "Waiting for initialization commands to complete..." << std::endl;
        // IMPORTANT: Wait for the initialization command list to finish on the GPU.
        // The old WaitForPreviousFrame() relied on per-frame fence values (which are still 0 during init),
        // so the very first BeginFrame() could reset command allocators while GPU was still executing.
        if (m_commandQueue && m_fence) {
            const uint64_t initFenceValue = m_fenceValue++;
            DX_CHECK(m_commandQueue->Signal(m_fence.Get(), initFenceValue));
            if (m_fence->GetCompletedValue() < initFenceValue) {
                DX_CHECK(m_fence->SetEventOnCompletion(initFenceValue, m_fenceEvent));
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
            // Mark all frame slots as "completed at least initFenceValue" to make BeginFrame waits meaningful.
            for (uint32_t i = 0; i < kFrameCount; ++i) {
                m_fenceValues[i] = initFenceValue;
            }
        }

        // Initialize lighting system
        m_lightingSystem = std::make_unique<WorldEditor::LightingSystem>();
        if (!m_lightingSystem->initialize(m_device.Get())) {
            std::cerr << "Failed to initialize lighting system" << std::endl;
            return false;
        }

        // Initialize wireframe grid
        m_wireframeGrid = std::make_unique<WorldEditor::WireframeGrid>();
        if (!m_wireframeGrid->initialize(m_device.Get())) {
            std::cerr << "Failed to initialize wireframe grid" << std::endl;
            return false;
        }

        // Initialize editor sky renderer (gradient sky + sun disc).
        m_skyRenderer = std::make_unique<WorldEditor::SkyRenderer>();
        if (!m_skyRenderer->initialize(m_device.Get())) {
            std::cerr << "Failed to initialize sky renderer" << std::endl;
            return false;
        }

        // TODO: Initialize shadow mapping later when shaders are ready
        // m_shadowMapping = std::make_unique<WorldEditor::ShadowMapping>();
        // if (!m_shadowMapping->initialize(m_device.Get(), 2048)) {
        //     std::cerr << "Failed to initialize shadow mapping" << std::endl;
        //     return false;
        // }

        std::cout << "DirectX renderer initialized successfully!" << std::endl;
        return true;
    }
    catch (const DirectXException& e) {
        std::cerr << "DirectX initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void DirectXRenderer::Resize(uint32_t width, uint32_t height) {
    if (!m_swapChain || !m_device) {
        return;
    }
    if (width == 0 || height == 0) {
        return; // minimized
    }
    if (width == m_width && height == m_height) {
        return;
    }

    // Make sure GPU is not using current backbuffers.
    WaitForPreviousFrame();

    // Release references to the back buffers.
    for (auto& rt : m_renderTargets) {
        rt.Reset();
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    DX_CHECK(m_swapChain->GetDesc(&desc));

    // Resize swapchain buffers.
    DX_CHECK(m_swapChain->ResizeBuffers(
        kFrameCount,
        width,
        height,
        desc.BufferDesc.Format,
        desc.Flags
    ));

    m_width = width;
    m_height = height;
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate RTVs for back buffers (keep RTV heap; just rewrite first kFrameCount descriptors).
    m_renderTargets.resize(kFrameCount);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; ++i) {
        DX_CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += static_cast<SIZE_T>(m_rtvDescriptorSize);
    }
}

void DirectXRenderer::Shutdown() {
    // Make shutdown safe to call multiple times.
    if (m_isShutdown) {
        return;
    }
    m_isShutdown = true;
    if (!m_device) {
        return;
    }

    try {
        // Ждем завершения всех команд GPU
        WaitForPreviousFrame();

        // Дополнительная синхронизация для безопасности
        if (m_commandQueue && m_fence) {
            const uint64_t finalFenceValue = ++m_fenceValue;
            DX_CHECK(m_commandQueue->Signal(m_fence.Get(), finalFenceValue));
            
            if (m_fence->GetCompletedValue() < finalFenceValue) {
                DX_CHECK(m_fence->SetEventOnCompletion(finalFenceValue, m_fenceEvent));
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
        }

        std::cout << "DirectX renderer shutdown complete" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error during DirectX shutdown: " << e.what() << std::endl;
    }

    // Закрываем event
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    // Release CPU-side references deterministically (ComPtr would do it in destructor too,
    // but doing it here avoids "half-shutdown" states and makes repeated Shutdown() cheap).
    m_wireframeGrid.reset();
    m_skyRenderer.reset();
    m_shadowMapping.reset();
    m_lightingSystem.reset();

    m_deferredReleases.clear();
    m_renderTargets.clear();

    m_viewportRT.Reset();
    m_viewportDS.Reset();
    m_depthStencil.Reset();

    m_vertexBufferUpload.Reset();
    m_indexBufferUpload.Reset();
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_rootSignature.Reset();
    m_pipelineState.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();

    m_commandList.Reset();
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        m_commandAllocators[i].Reset();
        m_fenceValues[i] = 0;
    }
    m_commandQueue.Reset();
    m_swapChain.Reset();

    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_srvHeap.Reset();

    m_fence.Reset();
    m_device.Reset();
}

void DirectXRenderer::SafeReleaseResource(ComPtr<ID3D12Resource>& resource) {
    if (!resource) {
        return;
    }

    // Проверяем состояние устройства перед операциями
    if (!m_device) {
        // Устройство уже уничтожено, просто сбрасываем ресурс
        resource.Reset();
        return;
    }

    try {
        // Проверяем, не было ли устройство удалено
        HRESULT deviceRemovedReason = m_device->GetDeviceRemovedReason();
        if (FAILED(deviceRemovedReason)) {
            // Устройство было удалено, безопасно сбрасываем ресурс
            resource.Reset();
            return;
        }

        // Ждем завершения текущего кадра перед освобождением ресурса
        WaitForFrame(m_frameIndex);
        resource.Reset();
    }
    catch (const std::exception& e) {
        std::cerr << "Error during safe resource release: " << e.what() << std::endl;
        // Принудительно освобождаем ресурс даже при ошибке
        resource.Reset();
    }
    catch (...) {
        // Ловим любые другие исключения
        std::cerr << "Unknown error during safe resource release" << std::endl;
        resource.Reset();
    }
}

void DirectXRenderer::DeferredReleaseResource(ComPtr<ID3D12Resource> resource) {
    if (!resource || !m_device) {
        return;
    }

    // Добавляем ресурс в список отложенного удаления
    DeferredResource deferred;
    deferred.resource = resource;
    deferred.frameValue = m_fenceValue; // Текущее значение fence
    m_deferredReleases.push_back(deferred);
}

void DirectXRenderer::BeginFrame() {
    // Получаем индекс текущего back buffer и ждём GPU для этого frame-index,
    // чтобы безопасно reset'ить allocator и переиспользовать ресурсы.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Обрабатываем отложенное удаление ресурсов
    ProcessDeferredReleases();

    // Ждем завершения работы GPU для этого frame index
    // Это гарантирует, что command allocator не используется GPU
    WaitForFrame(m_frameIndex);
    
    // Дополнительная проверка: если это первые кадры, подождем немного больше
    if (m_fenceValues[m_frameIndex] == 0) {
        // Для первых кадров ждем завершения всех предыдущих команд
        const uint64_t currentCompleted = m_fence->GetCompletedValue();
        if (currentCompleted < m_fenceValue - 1) {
            // Есть незавершенные команды, ждем их
            HRESULT hr = m_fence->SetEventOnCompletion(m_fenceValue - 1, m_fenceEvent);
            if (SUCCEEDED(hr)) {
                WaitForSingleObject(m_fenceEvent, 1000); // Максимум 1 секунда
            }
        }
    }
    
    // Теперь безопасно сбрасываем command allocator
    HRESULT hr = m_commandAllocators[m_frameIndex]->Reset();
    if (FAILED(hr)) {
        std::cerr << "Failed to reset command allocator for frame " << m_frameIndex 
                  << ": 0x" << std::hex << hr << std::dec << std::endl;
        // Попробуем подождать еще раз
        WaitForPreviousFrame();
        hr = m_commandAllocators[m_frameIndex]->Reset();
        if (FAILED(hr)) {
            throw DirectXException(hr, "Command allocator reset failed after sync");
        }
    }

    // Сбрасываем command list
    DX_CHECK(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

#ifdef DX12_ENABLE_DEBUG_LAYER
    CheckDebugMessages();
#endif
}

void DirectXRenderer::BeginSwapchainPass(float clearColor[4]) {
    // Transition swapchain buffer to RT
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Bind RT
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCurrentRenderTargetView();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Viewport/scissor to window size
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &scissorRect);

    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void DirectXRenderer::EnsureViewportRenderTarget(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    if (m_viewportRT && width == m_viewportRTWidth && height == m_viewportRTHeight) {
        return;
    }

    // Ensure GPU isn't using previous viewport RT.
    WaitForPreviousFrame();

    m_viewportRT.Reset();
    m_viewportDS.Reset();

    m_viewportRTWidth = width;
    m_viewportRTHeight = height;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.1f;
    clearValue.Color[1] = 0.1f;
    clearValue.Color[2] = 0.1f;
    clearValue.Color[3] = 1.0f;

    DX_CHECK(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&m_viewportRT)
    ));

    // RTV: use extra descriptor slot after swapchain RTVs.
    m_viewportRtvHandle = GetViewportRenderTargetView();
    m_device->CreateRenderTargetView(m_viewportRT.Get(), nullptr, m_viewportRtvHandle);

    // SRV: fixed index 0 in SRV heap (reserved for viewport texture).
    m_viewportSrvCpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    m_viewportSrvCpuHandle.ptr += static_cast<SIZE_T>(kViewportSrvIndex) * static_cast<SIZE_T>(m_srvDescriptorSize);
    m_viewportSrvGpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    m_viewportSrvGpuHandle.ptr += static_cast<UINT64>(kViewportSrvIndex) * static_cast<UINT64>(m_srvDescriptorSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(m_viewportRT.Get(), &srvDesc, m_viewportSrvCpuHandle);

    // Depth-stencil for viewport pass (same dimensions).
    EnsureViewportDepthStencil(width, height);
}

void DirectXRenderer::EnsureViewportDepthStencil(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    if (m_viewportDS && width == m_viewportRTWidth && height == m_viewportRTHeight) {
        return;
    }

    // DSV heap has a single descriptor.
    m_viewportDsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_RESOURCE_DESC dsDesc = {};
    dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    dsDesc.Alignment = 0;
    dsDesc.Width = width;
    dsDesc.Height = height;
    dsDesc.DepthOrArraySize = 1;
    dsDesc.MipLevels = 1;
    dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsDesc.SampleDesc.Count = 1;
    dsDesc.SampleDesc.Quality = 0;
    dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    DX_CHECK(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &dsDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_viewportDS)
    ));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    m_device->CreateDepthStencilView(m_viewportDS.Get(), &dsvDesc, m_viewportDsvHandle);
}

void DirectXRenderer::BeginOffscreenPass(uint32_t width, uint32_t height, float clearColor[4]) {
    EnsureViewportRenderTarget(width, height);
    if (!m_viewportRT) {
        return;
    }

    // Transition viewport RT to RT state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_viewportRT.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Bind viewport RT + depth
    m_commandList->OMSetRenderTargets(1, &m_viewportRtvHandle, FALSE, &m_viewportDsvHandle);

    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &scissorRect);

    m_commandList->ClearRenderTargetView(m_viewportRtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_viewportDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void DirectXRenderer::EndOffscreenPass() {
    if (!m_viewportRT) {
        return;
    }

    // Transition viewport RT back to SRV
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_viewportRT.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);
}

void DirectXRenderer::EndFrame() {
    // Resource barrier обратно в present state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_commandList->ResourceBarrier(1, &barrier);

    // Закрываем command list
    DX_CHECK(m_commandList->Close());
}

bool DirectXRenderer::Present() {
    // Выполняем command list
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present с VSync (1 = включен)
    HRESULT hr = m_swapChain->Present(1, 0);
    if (FAILED(hr)) {
        // Log the error but don't throw exception for present failures
        // These are often non-fatal (device removed, window closed, etc.)
        std::cerr << "Present failed with HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        if (m_device) {
            const HRESULT removed = m_device->GetDeviceRemovedReason();
            if (FAILED(removed)) {
                std::cerr << "DeviceRemovedReason: 0x" << std::hex << removed << std::dec << std::endl;
            }
        }
        return false;
    }

    // Сигнализируем fence для синхронизации
    const uint64_t currentFenceValue = m_fenceValue;
    DX_CHECK(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));
    m_fenceValues[m_frameIndex] = currentFenceValue;
    m_fenceValue++;

#ifdef DX12_ENABLE_DEBUG_LAYER
    CheckDebugMessages();
#endif

    return true;
}

// Private implementation methods

bool DirectXRenderer::CreateDevice() {
#ifdef DX12_ENABLE_DEBUG_LAYER
    // Включаем debug layer только в специальных debug builds
    // Для обычной разработки отключаем - слишком агрессивный
    /*
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        std::cout << "DX12 Debug Layer enabled" << std::endl;
        
        // Включаем GPU-based validation (опционально, только в Debug)
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController.As(&debugController1))) {
#ifdef _DEBUG
            // GPU validation очень медленная и строгая - только для глубокой отладки
            // debugController1->SetEnableGPUBasedValidation(TRUE);
            // std::cout << "GPU-based validation enabled" << std::endl;
            std::cout << "GPU-based validation disabled (too strict for development)" << std::endl;
#endif
        }
    }
    */
    std::cout << "DX12 Debug Layer disabled for stability" << std::endl;
#endif

    // Создаем DXGI factory
    ComPtr<IDXGIFactory4> factory;
    DX_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

    // Pick a hardware adapter that actually supports D3D12.
    // The previous logic relied on DedicatedVideoMemory > 0 and could fail on some iGPUs.
    ComPtr<IDXGIAdapter1> bestAdapter;
    SIZE_T bestDedicatedVideoMemory = 0;

    for (UINT adapterIndex = 0;; ++adapterIndex) {
        ComPtr<IDXGIAdapter1> candidate;
        if (factory->EnumAdapters1(adapterIndex, &candidate) == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc{};
        candidate->GetDesc1(&desc);

        // Log adapter info (helps diagnose "works on one PC, fails on another").
        char descA[256]{};
        WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, descA, static_cast<int>(sizeof(descA)), nullptr, nullptr);
        std::cout << "DXGI Adapter[" << adapterIndex << "]: " << descA
                  << " (DedicatedVRAM=" << (unsigned long long)desc.DedicatedVideoMemory
                  << ", Shared=" << (unsigned long long)desc.SharedSystemMemory
                  << ", Flags=0x" << std::hex << desc.Flags << std::dec << ")"
                  << std::endl;

        // Skip software adapters here; we'll fallback to WARP below if needed.
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            std::cout << "  -> skipped (software adapter)" << std::endl;
            continue;
        }

        // Test whether the adapter can create a D3D12 device.
        const HRESULT testHr = D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
        if (FAILED(testHr)) {
            std::cout << "  -> D3D12CreateDevice test failed: 0x" << std::hex << testHr << std::dec << std::endl;
            continue;
        }
        std::cout << "  -> D3D12CreateDevice test OK" << std::endl;

        // Prefer the adapter with the most dedicated VRAM, but accept 0 (common on integrated GPUs).
        if (!bestAdapter || desc.DedicatedVideoMemory > bestDedicatedVideoMemory) {
            bestAdapter = candidate;
            bestDedicatedVideoMemory = desc.DedicatedVideoMemory;
        }
    }

    if (!bestAdapter) {
        // Fallback: WARP software device (lets the editor at least start on machines without D3D12 GPU/driver).
        ComPtr<IDXGIAdapter> warpAdapter;
        if (SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)))) {
            std::cout << "No hardware D3D12 adapter found. Falling back to WARP." << std::endl;
            DX_CHECK(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
            return true;
        }
        std::cerr << "No suitable DXGI adapter found and WARP fallback failed." << std::endl;
        return false;
    }

    DX_CHECK(D3D12CreateDevice(bestAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

#ifdef DX12_ENABLE_DEBUG_LAYER
    // Настраиваем Info Queue для умной обработки ошибок
    // ОТКЛЮЧЕНО: слишком агрессивно для разработки
    /*
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_device.As(&infoQueue))) {
        std::cout << "Setting up DX12 Info Queue..." << std::endl;
        
        // НЕ прерываем выполнение при ошибках - только логируем
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);  // Только критические
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);      // Ошибки - логируем
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);    // Предупреждения - логируем
        
        // Фильтруем известные "ложные" ошибки
        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
            // Добавим другие известные ложные срабатывания по мере необходимости
        };
        
        D3D12_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        infoQueue->AddStorageFilterEntries(&filter);
        
        std::cout << "DX12 Info Queue configured (non-breaking mode)" << std::endl;
    }
    */
    std::cout << "DX12 Info Queue disabled for stability" << std::endl;
#endif

    return true;
}

bool DirectXRenderer::CreateCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    DX_CHECK(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    return true;
}

bool DirectXRenderer::CreateSwapChain(HWND hwnd) {
    ComPtr<IDXGIFactory4> factory;
    DX_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 3;  // Увеличиваем до 3 буферов для лучшей производительности
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    DX_CHECK(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    DX_CHECK(swapChain.As(&m_swapChain));
    return true;
}

bool DirectXRenderer::CreateDescriptorHeaps() {
    // RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = kRtvCount; // swapchain RTVs + offscreen RTV
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    DX_CHECK(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // DSV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    DX_CHECK(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // SRV descriptor heap for ImGui and other shader resources
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 64; // Reserve space for ImGui textures and other resources
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    DX_CHECK(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
    m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return true;
}

bool DirectXRenderer::CreateRenderTargets() {
    m_renderTargets.resize(3);  // 3 буфера вместо 2

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < 3; i++) {  // Цикл до 3
        DX_CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));

        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += static_cast<SIZE_T>(m_rtvDescriptorSize);
    }

    return true;
}

bool DirectXRenderer::CreateCommandAllocators() {
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        DX_CHECK(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
    }
    DX_CHECK(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    // Don't close here - we'll close it after initialization commands

    return true;
}

bool DirectXRenderer::CreateSynchronizationObjects() {
    DX_CHECK(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr) {
        return false;
    }

    return true;
}

void DirectXRenderer::WaitForFrame(uint32_t frameIndex) {
    const uint64_t fenceValueForFrame = m_fenceValues[frameIndex];
    if (fenceValueForFrame == 0) {
        return; // Frame has not been submitted yet.
    }
    if (m_fence->GetCompletedValue() < fenceValueForFrame) {
        HRESULT hr = m_fence->SetEventOnCompletion(fenceValueForFrame, m_fenceEvent);
        if (FAILED(hr)) {
            std::cerr << "Failed to set fence event: " << std::hex << hr << std::endl;
            throw DirectXException(hr, "Fence event setup failed");
        }
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void DirectXRenderer::WaitForPreviousFrame() {
    // Wait for all queued GPU work (best-effort).
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        WaitForFrame(i);
    }
    if (m_swapChain) {
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    }
}

void DirectXRenderer::ProcessDeferredReleases() {
    if (!m_fence) {
        return;
    }

    const uint64_t completedValue = m_fence->GetCompletedValue();
    
    // Удаляем ресурсы, которые больше не используются GPU
    auto it = m_deferredReleases.begin();
    while (it != m_deferredReleases.end()) {
        if (completedValue >= it->frameValue) {
            // GPU завершил работу с этим ресурсом, можно безопасно удалить
            it = m_deferredReleases.erase(it);
        } else {
            ++it;
        }
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXRenderer::GetCurrentRenderTargetView() const {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * static_cast<SIZE_T>(m_rtvDescriptorSize);
    return rtvHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXRenderer::GetViewportRenderTargetView() const {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(kViewportRtvIndex) * static_cast<SIZE_T>(m_rtvDescriptorSize);
    return rtvHandle;
}

// Triangle rendering implementation

bool DirectXRenderer::CreateTriangleResources() {
    if (!LoadShaders()) {
        return false;
    }

    if (!CreateRootSignature()) {
        return false;
    }

    if (!CreatePipelineState()) {
        return false;
    }

    if (!CreateVertexBuffer()) {
        return false;
    }

    if (!CreateIndexBuffer()) {
        return false;
    }

    return true;
}

bool DirectXRenderer::LoadShaders() {
    // Vertex shader HLSL код
    const char* vertexShaderCode = R"(
        struct VSInput {
            float3 position : POSITION;
            float4 color : COLOR;
        };

        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        VSOutput main(VSInput input) {
            VSOutput output;
            output.position = float4(input.position, 1.0f);
            output.color = input.color;
            return output;
        }
    )";

    // Pixel shader HLSL код
    const char* pixelShaderCode = R"(
        struct PSInput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        float4 main(PSInput input) : SV_TARGET {
            return input.color;
        }
    )";

    UINT compileFlags = 0;
#ifdef DX12_ENABLE_DEBUG_LAYER
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Компилируем vertex shader
    ComPtr<ID3DBlob> vertexShaderError;
    HRESULT hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr,
                           "main", "vs_5_0", compileFlags, 0, &m_vertexShader, &vertexShaderError);
    if (FAILED(hr)) {
        if (vertexShaderError) {
            std::cerr << "Vertex shader compilation failed: " << (char*)vertexShaderError->GetBufferPointer() << std::endl;
        }
        return false;
    }

    // Компилируем pixel shader
    ComPtr<ID3DBlob> pixelShaderError;
    hr = D3DCompile(pixelShaderCode, strlen(pixelShaderCode), nullptr, nullptr, nullptr,
                   "main", "ps_5_0", compileFlags, 0, &m_pixelShader, &pixelShaderError);
    if (FAILED(hr)) {
        if (pixelShaderError) {
            std::cerr << "Pixel shader compilation failed: " << (char*)pixelShaderError->GetBufferPointer() << std::endl;
        }
        return false;
    }

    return true;
}

bool DirectXRenderer::CreateRootSignature() {
    // Создаем пустую root signature (для простого треугольника не нужны ресурсы)
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            std::cerr << "Root signature serialization failed: " << (char*)error->GetBufferPointer() << std::endl;
        }
        return false;
    }

    DX_CHECK(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                         IID_PPV_ARGS(&m_rootSignature)));

    return true;
}

bool DirectXRenderer::CreatePipelineState() {
    // Описываем input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Описываем PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
    psoDesc.PS = { m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize() };

    // Rasterizer state
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Blend state
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
        psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    DX_CHECK(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

    return true;
}

bool DirectXRenderer::CreateVertexBuffer() {
    // Создаем вершины треугольника
    Vertex triangleVertices[] = {
        { glm::vec3(0.0f, 0.5f, 0.0f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) }, // Верхняя вершина (красная)
        { glm::vec3(0.5f, -0.5f, 0.0f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f) }, // Правая вершина (зеленая)
        { glm::vec3(-0.5f, -0.5f, 0.0f), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f) }  // Левая вершина (синяя)
    };

    const UINT vertexBufferSize = sizeof(triangleVertices);

    // Создаем vertex buffer
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = vertexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    DX_CHECK(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                             D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                             IID_PPV_ARGS(&m_vertexBuffer)));

    // Создаем upload buffer для копирования данных
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    DX_CHECK(m_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&m_vertexBufferUpload)));

    // Копируем данные в upload buffer
    D3D12_RANGE readRange = { 0, 0 };
    void* pData;
    DX_CHECK(m_vertexBufferUpload->Map(0, &readRange, &pData));
    memcpy(pData, triangleVertices, vertexBufferSize);
    m_vertexBufferUpload->Unmap(0, nullptr);

    // Копируем из upload buffer в vertex buffer
    m_commandList->CopyResource(m_vertexBuffer.Get(), m_vertexBufferUpload.Get());

    // Переходим vertex buffer в состояние для чтения
    D3D12_RESOURCE_BARRIER vertexBarrier = {};
    vertexBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    vertexBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    vertexBarrier.Transition.pResource = m_vertexBuffer.Get();
    vertexBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    vertexBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    vertexBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &vertexBarrier);

    // Создаем vertex buffer view
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vertexBufferSize;

    return true;
}

bool DirectXRenderer::CreateIndexBuffer() {
    // Создаем индексы для треугольника
    uint32_t triangleIndices[] = {
        0, 1, 2  // Один треугольник
    };

    const UINT indexBufferSize = sizeof(triangleIndices);

    // Создаем index buffer
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = indexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    DX_CHECK(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                             D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                             IID_PPV_ARGS(&m_indexBuffer)));

    // Создаем upload buffer для копирования данных
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    DX_CHECK(m_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&m_indexBufferUpload)));

    // Копируем данные в upload buffer
    D3D12_RANGE readRange = { 0, 0 };
    void* pData;
    DX_CHECK(m_indexBufferUpload->Map(0, &readRange, &pData));
    memcpy(pData, triangleIndices, indexBufferSize);
    m_indexBufferUpload->Unmap(0, nullptr);

    // Копируем из upload buffer в index buffer
    m_commandList->CopyResource(m_indexBuffer.Get(), m_indexBufferUpload.Get());

    // Переходим index buffer в состояние для чтения
    D3D12_RESOURCE_BARRIER indexBarrier = {};
    indexBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    indexBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    indexBarrier.Transition.pResource = m_indexBuffer.Get();
    indexBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    indexBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    indexBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &indexBarrier);

    // Создаем index buffer view
    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = indexBufferSize;

    return true;
}

void DirectXRenderer::PopulateTriangleCommandList() {
    // Устанавливаем pipeline state
    m_commandList->SetPipelineState(m_pipelineState.Get());

    // Устанавливаем root signature
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Устанавливаем primitive topology
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Устанавливаем vertex buffer
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    // Устанавливаем index buffer
    m_commandList->IASetIndexBuffer(&m_indexBufferView);

    // Рендерим треугольник
    std::cout << "Drawing triangle with 3 vertices..." << std::endl;
    m_commandList->DrawIndexedInstanced(3, 1, 0, 0, 0);
}

// DirectXException implementation

DirectXException::DirectXException(HRESULT hr, const std::string& message)
    : m_hr(hr), m_message(message + " (HRESULT: 0x" + std::to_string(hr) + ")")
{
}

#ifdef DX12_ENABLE_DEBUG_LAYER
void DirectXRenderer::CheckDebugMessages() {
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(m_device.As(&infoQueue))) {
        UINT64 messageCount = infoQueue->GetNumStoredMessages();
        
        for (UINT64 i = 0; i < messageCount; ++i) {
            SIZE_T messageLength = 0;
            infoQueue->GetMessage(i, nullptr, &messageLength);
            
            if (messageLength > 0) {
                std::vector<BYTE> messageData(messageLength);
                D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(messageData.data());
                
                if (SUCCEEDED(infoQueue->GetMessage(i, message, &messageLength))) {
                    const char* severityStr = "UNKNOWN";
                    switch (message->Severity) {
                        case D3D12_MESSAGE_SEVERITY_CORRUPTION: severityStr = "CORRUPTION"; break;
                        case D3D12_MESSAGE_SEVERITY_ERROR: severityStr = "ERROR"; break;
                        case D3D12_MESSAGE_SEVERITY_WARNING: severityStr = "WARNING"; break;
                        case D3D12_MESSAGE_SEVERITY_INFO: severityStr = "INFO"; break;
                        case D3D12_MESSAGE_SEVERITY_MESSAGE: severityStr = "MESSAGE"; break;
                    }
                    
                    std::cout << "[DX12 " << severityStr << "] " << message->pDescription << std::endl;
                }
            }
        }
        
        // Очищаем сообщения после вывода
        infoQueue->ClearStoredMessages();
    }
}
#endif

void DirectXRenderer::UpdateLighting(const glm::vec3& cameraPosition, float time) {
    if (m_lightingSystem) {
        m_lightingSystem->updateLighting(cameraPosition, time);
        
        // Update shadow mapping with current light direction
        if (m_shadowMapping) {
            const auto& lightingConstants = m_lightingSystem->getLightingConstants();
            const Vec3 lightDir = Vec3(lightingConstants.lightDirection);
            const Vec3 sceneCenter = Vec3(0.0f, 0.0f, 0.0f); // Центр сцены
            const float sceneRadius = 100.0f; // Радиус сцены
            
            m_shadowMapping->updateShadowCamera(lightDir, sceneCenter, sceneRadius);
        }
    }
}
