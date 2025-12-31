#pragma once

#include "core/Types.h"

// Prevent Windows headers (pulled by d3d12.h) from defining min/max macros that break GLM.
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

// Editor-only sky background renderer (gradient + sun disk).
class SkyRenderer {
public:
    SkyRenderer() = default;
    ~SkyRenderer() { shutdown(); }

    bool initialize(ID3D12Device* device);
    void shutdown();

    // Draws a full-screen triangle into the currently bound render target.
    void render(ID3D12GraphicsCommandList* commandList,
                const Mat4& invViewProj,
                const Vec3& sunDirection,
                const Vec3& sunColor);

private:
#ifdef DIRECTX_RENDERER
    ID3D12Device* device_ = nullptr;
    bool initialized_ = false;

    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
    ComPtr<ID3D12Resource> constantBuffer_;

    bool createPipeline();
    void updateConstantBuffer(const Mat4& invViewProj, const Vec3& sunDirection, const Vec3& sunColor);
#endif
};

} // namespace WorldEditor


