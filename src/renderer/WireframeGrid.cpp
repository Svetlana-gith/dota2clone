// Prevent Windows min/max macros from conflicting with other headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "WireframeGrid.h"
#include "world/Components.h"
#include <iostream>
#include <algorithm>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace WorldEditor {

WireframeGrid::WireframeGrid() = default;

WireframeGrid::~WireframeGrid() {
    shutdown();
}

bool WireframeGrid::initialize(ID3D12Device* device) {
    if (!device) {
        std::cerr << "WireframeGrid: Invalid device" << std::endl;
        return false;
    }

    device_ = device;
    
    if (!createWireframeShaders()) {
        std::cerr << "WireframeGrid: Failed to create shaders" << std::endl;
        return false;
    }

    if (!createWireframePipeline()) {
        std::cerr << "WireframeGrid: Failed to create pipeline" << std::endl;
        return false;
    }

    std::cout << "WireframeGrid initialized successfully" << std::endl;
    return true;
}

void WireframeGrid::shutdown() {
    // GPU resources will be automatically released by ComPtr
    gpuResourcesCreated_ = false;
    device_ = nullptr;
}

bool WireframeGrid::generateGrid(const TerrainComponent& terrain, const MeshComponent& mesh) {
    if (!device_) {
        return false;
    }

    const int w = terrain.resolution.x;
    const int h = terrain.resolution.y;
    
    if (w < 2 || h < 2) {
        return false;
    }

    // Safety: skip wireframe for very large terrains to prevent GPU buffer overflow.
    // For 129x129, wireframe would create ~66k vertices which can crash the driver.
    const int maxSafeResolution = 65;
    if (w > maxSafeResolution || h > maxSafeResolution) {
        // Clear existing grid instead of generating new one
        gridVertices_.clear();
        gridIndices_.clear();
        return false;
    }

    // Очищаем предыдущие данные
    gridVertices_.clear();
    gridIndices_.clear();

    // Create a grid matching terrain local space: terrain mesh uses [0..size] in XZ.
    const float stepX = terrain.size / float(w - 1);
    const float stepZ = terrain.size / float(h - 1);
    const float startX = 0.0f;
    const float startZ = 0.0f;
    // Small vertical offset to reduce z-fighting against the terrain surface.
    const float yEps = 0.01f * (std::min)(stepX, stepZ);

    // Горизонтальные линии
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w - 1; ++x) {
            const float x1 = startX + x * stepX;
            const float x2 = startX + (x + 1) * stepX;
            const float z = startZ + y * stepZ;
            
            // Получаем высоты из heightmap если доступен
            float y1 = 0.0f, y2 = 0.0f;
            if (!terrain.heightmap.empty()) {
                const size_t idx1 = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                const size_t idx2 = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x + 1);
                if (idx1 < terrain.heightmap.size()) y1 = terrain.heightmap[idx1];
                if (idx2 < terrain.heightmap.size()) y2 = terrain.heightmap[idx2];
            }
            y1 += yEps;
            y2 += yEps;
            
            gridVertices_.push_back(Vec3(x1, y1, z));
            gridVertices_.push_back(Vec3(x2, y2, z));
            
            const u32 baseIdx = static_cast<u32>(gridVertices_.size()) - 2;
            gridIndices_.push_back(baseIdx);
            gridIndices_.push_back(baseIdx + 1);
        }
    }

    // Вертикальные линии
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h - 1; ++y) {
            const float x_pos = startX + x * stepX;
            const float z1 = startZ + y * stepZ;
            const float z2 = startZ + (y + 1) * stepZ;
            
            // Получаем высоты из heightmap если доступен
            float y1 = 0.0f, y2 = 0.0f;
            if (!terrain.heightmap.empty()) {
                const size_t idx1 = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                const size_t idx2 = static_cast<size_t>(y + 1) * static_cast<size_t>(w) + static_cast<size_t>(x);
                if (idx1 < terrain.heightmap.size()) y1 = terrain.heightmap[idx1];
                if (idx2 < terrain.heightmap.size()) y2 = terrain.heightmap[idx2];
            }
            y1 += yEps;
            y2 += yEps;
            
            gridVertices_.push_back(Vec3(x_pos, y1, z1));
            gridVertices_.push_back(Vec3(x_pos, y2, z2));
            
            const u32 baseIdx = static_cast<u32>(gridVertices_.size()) - 2;
            gridIndices_.push_back(baseIdx);
            gridIndices_.push_back(baseIdx + 1);
        }
    }

    // Создаем GPU ресурсы
    return createGPUResources();
}

void WireframeGrid::render(ID3D12GraphicsCommandList* commandList,
                          const Mat4& worldMatrix,
                          const Mat4& viewProjMatrix,
                          const Vec3& cameraPosition) {
    if (!enabled_ || !gpuResourcesCreated_ || !commandList || gridIndices_.empty()) {
        return;
    }

    // Устанавливаем pipeline state
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());

    // Обновляем constant buffer
    updateConstantBuffer(worldMatrix, viewProjMatrix, cameraPosition);

    // Устанавливаем constant buffer
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());

    // Устанавливаем vertex buffer
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    // Рендерим
    commandList->DrawIndexedInstanced(static_cast<UINT>(gridIndices_.size()), 1, 0, 0, 0);
}

bool WireframeGrid::createGPUResources() {
    if (!device_ || gridVertices_.empty() || gridIndices_.empty()) {
        return false;
    }

    try {
        // Создаем vertex buffer
        const UINT vertexBufferSize = static_cast<UINT>(gridVertices_.size() * sizeof(Vec3));
        
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = vertexBufferSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vertexBuffer_)
        );

        if (FAILED(hr)) {
            std::cerr << "Failed to create vertex buffer for wireframe grid" << std::endl;
            return false;
        }

        // Копируем данные в vertex buffer
        void* vertexDataPtr = nullptr;
        hr = vertexBuffer_->Map(0, nullptr, &vertexDataPtr);
        if (SUCCEEDED(hr)) {
            memcpy(vertexDataPtr, gridVertices_.data(), vertexBufferSize);
            vertexBuffer_->Unmap(0, nullptr);
        }

        // Создаем vertex buffer view
        vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
        vertexBufferView_.StrideInBytes = sizeof(Vec3);
        vertexBufferView_.SizeInBytes = vertexBufferSize;

        // Создаем index buffer
        const UINT indexBufferSize = static_cast<UINT>(gridIndices_.size() * sizeof(u32));
        
        resourceDesc.Width = indexBufferSize;
        
        hr = device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&indexBuffer_)
        );

        if (FAILED(hr)) {
            std::cerr << "Failed to create index buffer for wireframe grid" << std::endl;
            return false;
        }

        // Копируем данные в index buffer
        void* indexDataPtr = nullptr;
        hr = indexBuffer_->Map(0, nullptr, &indexDataPtr);
        if (SUCCEEDED(hr)) {
            memcpy(indexDataPtr, gridIndices_.data(), indexBufferSize);
            indexBuffer_->Unmap(0, nullptr);
        }

        // Создаем index buffer view
        indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
        indexBufferView_.SizeInBytes = indexBufferSize;

        // Создаем constant buffer
        const UINT constantBufferSize = 256; // Aligned to 256 bytes
        
        resourceDesc.Width = constantBufferSize;
        
        hr = device_->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&constantBuffer_)
        );

        if (FAILED(hr)) {
            std::cerr << "Failed to create constant buffer for wireframe grid" << std::endl;
            return false;
        }

        gpuResourcesCreated_ = true;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in createGPUResources: " << e.what() << std::endl;
        return false;
    }
}

bool WireframeGrid::createWireframeShaders() {
    // Простые шейдеры для wireframe
    const char* vertexShaderSource = R"(
        cbuffer Constants : register(b0)
        {
            float4x4 worldMatrix;
            float4x4 viewProjMatrix;
            float3 cameraPosition;
            float padding;
        };

        struct VSInput
        {
            float3 position : POSITION;
        };

        struct VSOutput
        {
            float4 position : SV_POSITION;
            float3 worldPos : WORLD_POS;
        };

        VSOutput main(VSInput input)
        {
            VSOutput output;
            float4 worldPos = mul(worldMatrix, float4(input.position, 1.0f));
            output.worldPos = worldPos.xyz;
            output.position = mul(viewProjMatrix, worldPos);
            return output;
        }
    )";

    const char* pixelShaderSource = R"(
        struct PSInput
        {
            float4 position : SV_POSITION;
            float3 worldPos : WORLD_POS;
        };

        float4 main(PSInput input) : SV_TARGET
        {
            // Простой wireframe цвет - зеленый с альфой
            return float4(0.0f, 1.0f, 0.0f, 0.8f);
        }
    )";

    // Компилируем vertex shader
    HRESULT hr = D3DCompile(
        vertexShaderSource,
        strlen(vertexShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        0,
        0,
        &vertexShader_,
        nullptr
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to compile wireframe vertex shader" << std::endl;
        return false;
    }

    // Компилируем pixel shader
    hr = D3DCompile(
        pixelShaderSource,
        strlen(pixelShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        0,
        0,
        &pixelShader_,
        nullptr
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to compile wireframe pixel shader" << std::endl;
        return false;
    }

    return true;
}

bool WireframeGrid::createWireframePipeline() {
    if (!device_ || !vertexShader_ || !pixelShader_) {
        return false;
    }

    try {
        // Создаем root signature
        D3D12_ROOT_PARAMETER rootParam = {};
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParam.Descriptor.ShaderRegister = 0;
        rootParam.Descriptor.RegisterSpace = 0;
        rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.NumParameters = 1;
        rootSigDesc.pParameters = &rootParam;
        rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        
        HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(hr)) {
            if (error) {
                std::cerr << "Root signature serialization error: " << (char*)error->GetBufferPointer() << std::endl;
            }
            return false;
        }

        hr = device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
        if (FAILED(hr)) {
            std::cerr << "Failed to create root signature for wireframe" << std::endl;
            return false;
        }

        // Создаем pipeline state
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = rootSignature_.Get();
        psoDesc.VS = { vertexShader_->GetBufferPointer(), vertexShader_->GetBufferSize() };
        psoDesc.PS = { pixelShader_->GetBufferPointer(), pixelShader_->GetBufferSize() };
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Не записываем в depth buffer
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        hr = device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
        if (FAILED(hr)) {
            std::cerr << "Failed to create pipeline state for wireframe" << std::endl;
            return false;
        }

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in createWireframePipeline: " << e.what() << std::endl;
        return false;
    }
}

void WireframeGrid::updateConstantBuffer(const Mat4& worldMatrix, const Mat4& viewProjMatrix, const Vec3& cameraPosition) {
    if (!constantBuffer_) {
        return;
    }

    struct Constants {
        Mat4 worldMatrix;
        Mat4 viewProjMatrix;
        Vec3 cameraPosition;
        float padding;
    };

    Constants constants;
    constants.worldMatrix = worldMatrix;
    constants.viewProjMatrix = viewProjMatrix;
    constants.cameraPosition = cameraPosition;
    constants.padding = 0.0f;

    void* dataPtr = nullptr;
    HRESULT hr = constantBuffer_->Map(0, nullptr, &dataPtr);
    if (SUCCEEDED(hr)) {
        memcpy(dataPtr, &constants, sizeof(Constants));
        constantBuffer_->Unmap(0, nullptr);
    }
}

} // namespace WorldEditor