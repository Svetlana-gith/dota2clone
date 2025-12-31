// Prevent Windows headers (pulled by d3d12.h) from defining min/max macros that break GLM.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "SkyRenderer.h"

#ifdef DIRECTX_RENDERER
#include <d3dcompiler.h>
#include <cstring>
#include <iostream>
#pragma comment(lib, "d3dcompiler.lib")
#endif

namespace WorldEditor {

bool SkyRenderer::initialize(ID3D12Device* device) {
#ifdef DIRECTX_RENDERER
    if (!device) return false;
    device_ = device;

    if (!createPipeline()) {
        std::cerr << "SkyRenderer: Failed to create pipeline" << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
#else
    (void)device;
    return false;
#endif
}

void SkyRenderer::shutdown() {
#ifdef DIRECTX_RENDERER
    constantBuffer_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    device_ = nullptr;
    initialized_ = false;
#endif
}

void SkyRenderer::render(ID3D12GraphicsCommandList* commandList,
                         const Mat4& invViewProj,
                         const Vec3& sunDirection,
                         const Vec3& sunColor) {
#ifdef DIRECTX_RENDERER
    if (!initialized_ || !commandList || !pipelineState_ || !rootSignature_ || !constantBuffer_) {
        return;
    }

    updateConstantBuffer(invViewProj, sunDirection, sunColor);

    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());

    // Full-screen triangle via SV_VertexID, no vertex buffers.
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
#else
    (void)commandList;
    (void)invViewProj;
    (void)sunDirection;
    (void)sunColor;
#endif
}

#ifdef DIRECTX_RENDERER
bool SkyRenderer::createPipeline() {
    if (!device_) return false;

    // Root signature: one CBV at b0.
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 1;
    rsDesc.pParameters = &rootParam;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            std::cerr << "SkyRenderer root signature error: " << (char*)error->GetBufferPointer() << std::endl;
        }
        return false;
    }
    hr = device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                      IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr)) return false;

    // Shaders (embedded).
    const char* vsSrc = R"(
        struct VSOut {
            float4 pos : SV_POSITION;
            float2 uv  : TEXCOORD0;
        };

        VSOut main(uint vid : SV_VertexID) {
            // Full-screen triangle in NDC.
            float2 p;
            if (vid == 0) p = float2(-1.0, -1.0);
            else if (vid == 1) p = float2(-1.0,  3.0);
            else p = float2( 3.0, -1.0);

            VSOut o;
            o.pos = float4(p, 0.0, 1.0);
            // Map NDC [-1..1] to UV [0..1]. Note: p goes beyond [-1..1] for the big triangle.
            o.uv = p * 0.5 + 0.5;
            return o;
        }
    )";

    const char* psSrc = R"(
        cbuffer Constants : register(b0)
        {
            float4x4 invViewProj;
            float3   sunDir;      float sunIntensity;
            float3   sunColor;    float _pad0;
        };

        struct PSIn {
            float4 pos : SV_POSITION;
            float2 uv  : TEXCOORD0;
        };

        float3 computeRayDir(float2 uv) {
            float2 ndc = uv * 2.0 - 1.0;

            float4 nearH = mul(invViewProj, float4(ndc, 0.0, 1.0));
            float4 farH  = mul(invViewProj, float4(ndc, 1.0, 1.0));
            float3 nearP = nearH.xyz / max(nearH.w, 1e-6);
            float3 farP  = farH.xyz / max(farH.w, 1e-6);
            return normalize(farP - nearP);
        }

        float4 main(PSIn i) : SV_TARGET {
            float3 ray = computeRayDir(i.uv);

            // Simple sky gradient.
            float t = saturate(ray.y * 0.5 + 0.5);
            float3 horizon = float3(0.70, 0.78, 0.90);
            float3 zenith  = float3(0.18, 0.38, 0.75);
            float3 sky = lerp(horizon, zenith, t);

            // Sun disk + glow.
            float mu = saturate(dot(ray, normalize(sunDir)));
            float sunDisk = pow(mu, 1500.0);
            float sunGlow = pow(mu, 25.0) * 0.25;
            float3 sun = (sunDisk + sunGlow) * sunColor * sunIntensity;

            return float4(sky + sun, 1.0);
        }
    )";

    ComPtr<ID3DBlob> vs;
    ComPtr<ID3DBlob> ps;
    hr = D3DCompile(vsSrc, strlen(vsSrc), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vs, nullptr);
    if (FAILED(hr)) return false;
    hr = D3DCompile(psSrc, strlen(psSrc), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &ps, nullptr);
    if (FAILED(hr)) return false;

    // Constant buffer (upload heap).
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = 256; // CBV alignment
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                          IID_PPV_ARGS(&constantBuffer_));
    if (FAILED(hr)) return false;

    // PSO.
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = rootSignature_.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    // Blend state (opaque).
    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    pso.BlendState.RenderTarget[0].BlendEnable = FALSE;
    pso.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    pso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
    pso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
    pso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    pso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    pso.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.SampleMask = UINT_MAX;
    // Rasterizer state.
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    pso.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    pso.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.RasterizerState.MultisampleEnable = FALSE;
    pso.RasterizerState.AntialiasedLineEnable = FALSE;
    pso.RasterizerState.ForcedSampleCount = 0;
    pso.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Depth/stencil state (disabled for sky background).
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    hr = device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr)) return false;
    return true;
}

void SkyRenderer::updateConstantBuffer(const Mat4& invViewProj, const Vec3& sunDirection, const Vec3& sunColor) {
    struct alignas(16) Constants {
        Mat4 invViewProj;
        Vec3 sunDir;
        float sunIntensity;
        Vec3 sunColor;
        float pad0;
    };
    static_assert(sizeof(Constants) <= 256, "SkyRenderer constants must fit in 256 bytes");

    Constants c{};
    c.invViewProj = invViewProj;
    c.sunDir = glm::normalize(sunDirection);
    c.sunIntensity = 1.0f;
    c.sunColor = sunColor;
    c.pad0 = 0.0f;

    void* dst = nullptr;
    HRESULT hr = constantBuffer_->Map(0, nullptr, &dst);
    if (SUCCEEDED(hr)) {
        memcpy(dst, &c, sizeof(c));
        constantBuffer_->Unmap(0, nullptr);
    }
}
#endif

} // namespace WorldEditor


