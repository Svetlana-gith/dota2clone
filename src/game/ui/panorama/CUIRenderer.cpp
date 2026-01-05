#include "CUIRenderer.h"
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <cmath>
#include <codecvt>
#include <locale>
#include <spdlog/spdlog.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)

namespace Panorama {

CUIRenderer::CUIRenderer() = default;

CUIRenderer::~CUIRenderer() {
    Shutdown();
}

bool CUIRenderer::Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                             ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* srvHeap,
                             f32 width, f32 height) {
    m_device = device;
    m_commandQueue = commandQueue;
    m_commandList = commandList;
    m_srvHeap = srvHeap;
    m_screenWidth = width;
    m_screenHeight = height;
    
    if (!CreateRootSignature()) {
        LOG_ERROR("CUIRenderer: Failed to create root signature");
        return false;
    }
    if (!CompileShaders()) {
        LOG_ERROR("CUIRenderer: Failed to compile shaders");
        return false;
    }
    if (!CreatePipelineState()) {
        LOG_ERROR("CUIRenderer: Failed to create pipeline state");
        return false;
    }
    if (!CreateBuffers()) {
        LOG_ERROR("CUIRenderer: Failed to create buffers");
        return false;
    }
    if (!InitializeDirectWrite()) {
        LOG_WARN("CUIRenderer: DirectWrite init failed (text measurement may be inaccurate)");
    }
    
    m_transformStack.push_back(Transform2D{});
    
    LOG_INFO("CUIRenderer (DX12) initialized: {}x{}", (int)width, (int)height);
    return true;
}

bool CUIRenderer::InitializeDirectWrite() {
    HRESULT hr;
    
    // Create D2D1 Factory
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
    if (FAILED(hr)) return false;
    
    // Create DirectWrite factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr)) return false;
    
    return true;
}

void CUIRenderer::Shutdown() {
    ShutdownDirectWrite();
    ClearTextureCache();
    
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        m_vertexBuffers[i].Reset();
    }
    
    m_pipelineState.Reset();
    m_pipelineStateTextured.Reset();
    m_rootSignature.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_pixelShaderTextured.Reset();
}

void CUIRenderer::ShutdownDirectWrite() {
    m_textFormatCache.clear();
    m_dwriteFactory.Reset();
    m_d2dFactory.Reset();
}

bool CUIRenderer::CreateRootSignature() {
    // Root signature with one constant buffer (screen size)
    D3D12_ROOT_PARAMETER rootParams[1] = {};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants.ShaderRegister = 0;
    rootParams[0].Constants.RegisterSpace = 0;
    rootParams[0].Constants.Num32BitValues = 4; // screenWidth, screenHeight, padding x2
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &signature, &error);
    if (FAILED(hr)) return false;
    
    hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                        IID_PPV_ARGS(&m_rootSignature));
    return SUCCEEDED(hr);
}

bool CUIRenderer::CompileShaders() {
    const char* shaderCode = R"(
        cbuffer Constants : register(b0) {
            float2 screenSize;
            float2 padding;
        };
        
        struct VS_INPUT {
            float2 pos : POSITION;
            float2 uv : TEXCOORD0;
            float4 color : COLOR0;
        };
        
        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD0;
            float4 color : COLOR0;
        };
        
        PS_INPUT VS(VS_INPUT input) {
            PS_INPUT output;
            output.pos.x = (input.pos.x / screenSize.x) * 2.0 - 1.0;
            output.pos.y = 1.0 - (input.pos.y / screenSize.y) * 2.0;
            output.pos.z = 0.0;
            output.pos.w = 1.0;
            output.uv = input.uv;
            output.color = input.color;
            return output;
        }
        
        float4 PS(PS_INPUT input) : SV_TARGET {
            return input.color;
        }
        
        float4 PS_Textured(PS_INPUT input) : SV_TARGET {
            return input.color;
        }
    )";
    
    ComPtr<ID3DBlob> errorBlob;
    
    HRESULT hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                           "VS", "vs_5_0", 0, 0, &m_vertexShader, &errorBlob);
    if (FAILED(hr)) return false;
    
    hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                   "PS", "ps_5_0", 0, 0, &m_pixelShader, &errorBlob);
    if (FAILED(hr)) return false;
    
    hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                   "PS_Textured", "ps_5_0", 0, 0, &m_pixelShaderTextured, &errorBlob);
    if (FAILED(hr)) return false;
    
    return true;
}

bool CUIRenderer::CreatePipelineState() {
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
    psoDesc.PS = { m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize() };
    
    // Rasterizer state
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    
    // Blend state (alpha blending)
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    
    // Depth stencil state (disabled)
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    
    HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    if (FAILED(hr)) return false;
    
    // Create textured pipeline state
    psoDesc.PS = { m_pixelShaderTextured->GetBufferPointer(), m_pixelShaderTextured->GetBufferSize() };
    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateTextured));
    
    return SUCCEEDED(hr);
}

bool CUIRenderer::CreateBuffers() {
    // Create per-frame vertex buffers
    const uint32_t vertexBufferSize = sizeof(UIVertex) * 40000; // 20k for shapes, 20k for text
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = vertexBufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_vertexBuffers[i]));
        if (FAILED(hr)) return false;
        
        m_vertexBufferViews[i].BufferLocation = m_vertexBuffers[i]->GetGPUVirtualAddress();
        m_vertexBufferViews[i].SizeInBytes = vertexBufferSize;
        m_vertexBufferViews[i].StrideInBytes = sizeof(UIVertex);
    }
    
    return true;
}

void CUIRenderer::SetScreenSize(f32 width, f32 height) {
    m_screenWidth = width;
    m_screenHeight = height;
}

std::wstring CUIRenderer::ToWideString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

IDWriteTextFormat* CUIRenderer::GetOrCreateTextFormat(const FontInfo& font) {
    std::string key = font.family + "_" + std::to_string(font.size) + 
                      (font.bold ? "_b" : "") + (font.italic ? "_i" : "");
    
    auto it = m_textFormatCache.find(key);
    if (it != m_textFormatCache.end()) {
        return it->second.Get();
    }
    
    ComPtr<IDWriteTextFormat> textFormat;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        ToWideString(font.family).c_str(),
        nullptr,
        font.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        font.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font.size,
        L"en-us",
        textFormat.GetAddressOf()
    );
    
    if (FAILED(hr)) return nullptr;
    
    m_textFormatCache[key] = textFormat;
    return textFormat.Get();
}

void CUIRenderer::BeginFrame() {
    m_vertices.clear();
    m_textVertices.clear();
    m_textUploadCursorVertices = 0;
    
    // Advance frame index
    m_currentFrameIndex = (m_currentFrameIndex + 1) % kFrameCount;
    
    // Set pipeline state
    if (m_commandList && m_pipelineState && m_rootSignature) {
        m_commandList->SetPipelineState(m_pipelineState.Get());
        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        
        // Set screen size constants
        float constants[4] = { m_screenWidth, m_screenHeight, 0, 0 };
        m_commandList->SetGraphicsRoot32BitConstants(0, 4, constants, 0);
        
        // Set viewport
        D3D12_VIEWPORT viewport = {};
        viewport.Width = m_screenWidth;
        viewport.Height = m_screenHeight;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        m_commandList->RSSetViewports(1, &viewport);
        
        // Set scissor rect
        D3D12_RECT scissor = { 0, 0, (LONG)m_screenWidth, (LONG)m_screenHeight };
        m_commandList->RSSetScissorRects(1, &scissor);
    }
    
    ClearEffects();
    m_frameCount++;
}

void CUIRenderer::EndFrame() {
    Flush();
}

void CUIRenderer::Flush() {
    FlushBatch();
    FlushTextBatch();
}

void CUIRenderer::FlushBatch() {
    if (m_vertices.empty() || !m_commandList) return;
    
    auto& vertexBuffer = m_vertexBuffers[m_currentFrameIndex];
    if (!vertexBuffer) return;
    
    // Log first few flushes
    static int flushCount = 0;
    if (flushCount < 5) {
        LOG_INFO("CUIRenderer::FlushBatch: {} vertices, frame {}", m_vertices.size(), m_currentFrameIndex);
        flushCount++;
    }
    
    // Map and copy vertices
    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr = vertexBuffer->Map(0, &readRange, &mappedData);
    if (SUCCEEDED(hr)) {
        memcpy(mappedData, m_vertices.data(), m_vertices.size() * sizeof(UIVertex));
        vertexBuffer->Unmap(0, nullptr);
    } else {
        LOG_ERROR("CUIRenderer::FlushBatch: Failed to map vertex buffer");
        return;
    }
    
    // Draw
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferViews[m_currentFrameIndex]);
    m_commandList->DrawInstanced((UINT)m_vertices.size(), 1, 0, 0);
    
    m_vertices.clear();
}

void CUIRenderer::FlushTextBatch() {
    if (m_textVertices.empty() || !m_commandList) return;
    
    auto& vertexBuffer = m_vertexBuffers[m_currentFrameIndex];
    if (!vertexBuffer) return;
    
    // Text uses second half of buffer
    const size_t textOffset = 20000 * sizeof(UIVertex);
    
    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr = vertexBuffer->Map(0, &readRange, &mappedData);
    if (SUCCEEDED(hr)) {
        char* dest = static_cast<char*>(mappedData) + textOffset + m_textUploadCursorVertices * sizeof(UIVertex);
        memcpy(dest, m_textVertices.data(), m_textVertices.size() * sizeof(UIVertex));
        vertexBuffer->Unmap(0, nullptr);
    }
    
    // Draw text with textured pipeline
    if (m_pipelineStateTextured) {
        m_commandList->SetPipelineState(m_pipelineStateTextured.Get());
    }
    
    D3D12_VERTEX_BUFFER_VIEW textView = m_vertexBufferViews[m_currentFrameIndex];
    textView.BufferLocation += textOffset + m_textUploadCursorVertices * sizeof(UIVertex);
    textView.SizeInBytes = (UINT)(m_textVertices.size() * sizeof(UIVertex));
    
    m_commandList->IASetVertexBuffers(0, 1, &textView);
    m_commandList->DrawInstanced((UINT)m_textVertices.size(), 1, 0, 0);
    
    // Restore normal pipeline
    if (m_pipelineState) {
        m_commandList->SetPipelineState(m_pipelineState.Get());
    }
    
    m_textUploadCursorVertices += m_textVertices.size();
    m_textVertices.clear();
}

Vector2D CUIRenderer::TransformPoint(f32 x, f32 y) const {
    if (m_transformStack.empty()) return {x, y};
    
    const auto& t = m_transformStack.back();
    
    f32 px = x - t.originX;
    f32 py = y - t.originY;
    
    px *= t.scaleX;
    py *= t.scaleY;
    
    if (t.rotation != 0) {
        f32 rad = t.rotation * 3.14159265f / 180.0f;
        f32 cos_r = std::cos(rad);
        f32 sin_r = std::sin(rad);
        f32 rx = px * cos_r - py * sin_r;
        f32 ry = px * sin_r + py * cos_r;
        px = rx;
        py = ry;
    }
    
    return {px + t.originX + t.translateX, py + t.originY + t.translateY};
}

void CUIRenderer::AddQuad(const Rect2D& rect, const Color& color, f32 u0, f32 v0, f32 u1, f32 v1) {
    auto p0 = TransformPoint(rect.x, rect.y);
    auto p1 = TransformPoint(rect.x + rect.width, rect.y);
    auto p2 = TransformPoint(rect.x + rect.width, rect.y + rect.height);
    auto p3 = TransformPoint(rect.x, rect.y + rect.height);
    
    // Apply current opacity
    Color c = color;
    c.a *= m_currentOpacity;
    
    UIVertex v[6];
    v[0] = { p0.x, p0.y, u0, v0, c.r, c.g, c.b, c.a };
    v[1] = { p1.x, p1.y, u1, v0, c.r, c.g, c.b, c.a };
    v[2] = { p2.x, p2.y, u1, v1, c.r, c.g, c.b, c.a };
    v[3] = { p0.x, p0.y, u0, v0, c.r, c.g, c.b, c.a };
    v[4] = { p2.x, p2.y, u1, v1, c.r, c.g, c.b, c.a };
    v[5] = { p3.x, p3.y, u0, v1, c.r, c.g, c.b, c.a };
    
    for (int i = 0; i < 6; i++) {
        m_vertices.push_back(v[i]);
    }
}

void CUIRenderer::DrawRect(const Rect2D& rect, const Color& color) {
    AddQuad(rect, color);
}

void CUIRenderer::DrawRectOutline(const Rect2D& rect, const Color& color, f32 thickness) {
    AddQuad({ rect.x, rect.y, rect.width, thickness }, color);
    AddQuad({ rect.x, rect.y + rect.height - thickness, rect.width, thickness }, color);
    AddQuad({ rect.x, rect.y + thickness, thickness, rect.height - thickness * 2 }, color);
    AddQuad({ rect.x + rect.width - thickness, rect.y + thickness, thickness, rect.height - thickness * 2 }, color);
}

void CUIRenderer::DrawRoundedRect(f32 radius, const Rect2D& rect, const Color& color) {
    // Simplified: just draw regular rect
    AddQuad(rect, color);
}

void CUIRenderer::DrawRoundedRect(const Rect2D& rect, const Color& color, f32 radius) {
    AddQuad(rect, color);
}

void CUIRenderer::DrawRoundedRect(const Rect2D& rect, const Color& color, f32 tl, f32 tr, f32 br, f32 bl) {
    AddQuad(rect, color);
}

void CUIRenderer::DrawGradientRect(const Rect2D& rect, const Color& startColor, const Color& endColor, bool vertical) {
    auto p0 = TransformPoint(rect.x, rect.y);
    auto p1 = TransformPoint(rect.x + rect.width, rect.y);
    auto p2 = TransformPoint(rect.x + rect.width, rect.y + rect.height);
    auto p3 = TransformPoint(rect.x, rect.y + rect.height);
    
    Color c0, c1, c2, c3;
    if (vertical) {
        c0 = c1 = startColor;
        c2 = c3 = endColor;
    } else {
        c0 = c3 = startColor;
        c1 = c2 = endColor;
    }
    
    c0.a *= m_currentOpacity;
    c1.a *= m_currentOpacity;
    c2.a *= m_currentOpacity;
    c3.a *= m_currentOpacity;
    
    UIVertex v[6];
    v[0] = { p0.x, p0.y, 0, 0, c0.r, c0.g, c0.b, c0.a };
    v[1] = { p1.x, p1.y, 1, 0, c1.r, c1.g, c1.b, c1.a };
    v[2] = { p2.x, p2.y, 1, 1, c2.r, c2.g, c2.b, c2.a };
    v[3] = { p0.x, p0.y, 0, 0, c0.r, c0.g, c0.b, c0.a };
    v[4] = { p2.x, p2.y, 1, 1, c2.r, c2.g, c2.b, c2.a };
    v[5] = { p3.x, p3.y, 0, 1, c3.r, c3.g, c3.b, c3.a };
    
    for (int i = 0; i < 6; i++) m_vertices.push_back(v[i]);
}

void CUIRenderer::DrawRadialGradient(const Rect2D& rect, const Color& centerColor, const Color& edgeColor) {
    DrawRect(rect, centerColor);
}

void CUIRenderer::DrawText(const std::string& text, const Rect2D& bounds, const Color& color, 
                           const FontInfo& font, HorizontalAlign hAlign, VerticalAlign vAlign) {
    if (text.empty()) return;
    
    // For DX12, we render text as colored quads (simplified approach)
    // A full implementation would use a font atlas
    
    // Calculate text position based on alignment
    Vector2D textSize = MeasureText(text, font);
    f32 x = bounds.x;
    f32 y = bounds.y;
    
    switch (hAlign) {
        case HorizontalAlign::Center:
            x = bounds.x + (bounds.width - textSize.x) * 0.5f;
            break;
        case HorizontalAlign::Right:
            x = bounds.x + bounds.width - textSize.x;
            break;
        default:
            break;
    }
    
    switch (vAlign) {
        case VerticalAlign::Center:
            y = bounds.y + (bounds.height - textSize.y) * 0.5f;
            break;
        case VerticalAlign::Bottom:
            y = bounds.y + bounds.height - textSize.y;
            break;
        default:
            break;
    }
    
    // Draw each character as a simple rectangle (placeholder for font atlas rendering)
    f32 charWidth = font.size * 0.6f;
    f32 charHeight = font.size;
    Color c = color;
    c.a *= m_currentOpacity;
    
    for (size_t i = 0; i < text.length(); ++i) {
        if (text[i] == ' ') {
            x += charWidth * 0.5f;
            continue;
        }
        
        // For now, draw a small rectangle for each character
        // This is a placeholder - real implementation needs font atlas
        Rect2D charRect = { x, y + charHeight * 0.1f, charWidth * 0.8f, charHeight * 0.8f };
        
        UIVertex v[6];
        v[0] = { charRect.x, charRect.y, 0, 0, c.r, c.g, c.b, c.a };
        v[1] = { charRect.x + charRect.width, charRect.y, 1, 0, c.r, c.g, c.b, c.a };
        v[2] = { charRect.x + charRect.width, charRect.y + charRect.height, 1, 1, c.r, c.g, c.b, c.a };
        v[3] = { charRect.x, charRect.y, 0, 0, c.r, c.g, c.b, c.a };
        v[4] = { charRect.x + charRect.width, charRect.y + charRect.height, 1, 1, c.r, c.g, c.b, c.a };
        v[5] = { charRect.x, charRect.y + charRect.height, 0, 1, c.r, c.g, c.b, c.a };
        
        for (int j = 0; j < 6; j++) m_textVertices.push_back(v[j]);
        
        x += charWidth;
    }
}

Vector2D CUIRenderer::MeasureText(const std::string& text, const FontInfo& font) {
    if (text.empty()) return {0, font.size};
    
    if (!m_dwriteFactory) {
        return {text.length() * font.size * 0.6f, font.size};
    }
    
    IDWriteTextFormat* textFormat = GetOrCreateTextFormat(font);
    if (!textFormat) {
        return {text.length() * font.size * 0.6f, font.size};
    }
    
    std::wstring wtext = ToWideString(text);
    
    ComPtr<IDWriteTextLayout> textLayout;
    HRESULT hr = m_dwriteFactory->CreateTextLayout(
        wtext.c_str(),
        static_cast<UINT32>(wtext.length()),
        textFormat,
        10000.0f,
        10000.0f,
        textLayout.GetAddressOf()
    );
    
    if (FAILED(hr)) {
        return {text.length() * font.size * 0.6f, font.size};
    }
    
    DWRITE_TEXT_METRICS metrics;
    hr = textLayout->GetMetrics(&metrics);
    if (FAILED(hr)) {
        return {text.length() * font.size * 0.6f, font.size};
    }
    
    return {metrics.width, metrics.height};
}

f32 CUIRenderer::MeasureTextWidth(const std::string& text, const FontInfo& font) {
    return MeasureText(text, font).x;
}

f32 CUIRenderer::MeasureTextHeight(const FontInfo& font) {
    return font.size;
}

void CUIRenderer::DrawImage(const std::string& path, const Rect2D& rect, f32 opacity) {
    Color c(1, 1, 1, opacity * m_currentOpacity);
    AddQuad(rect, c);
}

void CUIRenderer::DrawImageTinted(const std::string& path, const Rect2D& rect, const Color& tint) {
    Color c = tint;
    c.a *= m_currentOpacity;
    AddQuad(rect, c);
}

void CUIRenderer::DrawImageRegion(const std::string& path, const Rect2D& destRect, const Rect2D& srcRect) {
    Color c = Color::White();
    c.a *= m_currentOpacity;
    AddQuad(destRect, c, srcRect.x, srcRect.y, srcRect.x + srcRect.width, srcRect.y + srcRect.height);
}

void CUIRenderer::DrawBoxShadow(const Rect2D& rect, const Color& color, f32 offsetX, f32 offsetY, 
                                f32 blur, f32 spread, bool inset) {
    Rect2D shadowRect = rect;
    shadowRect.x += offsetX - spread;
    shadowRect.y += offsetY - spread;
    shadowRect.width += spread * 2;
    shadowRect.height += spread * 2;
    
    Color shadowColor = color;
    shadowColor.a *= 0.5f * m_currentOpacity;
    DrawRect(shadowRect, shadowColor);
}

void CUIRenderer::DrawLine(f32 x1, f32 y1, f32 x2, f32 y2, const Color& color, f32 thickness) {
    f32 dx = x2 - x1;
    f32 dy = y2 - y1;
    f32 len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    
    f32 nx = -dy / len * thickness * 0.5f;
    f32 ny = dx / len * thickness * 0.5f;
    
    Color c = color;
    c.a *= m_currentOpacity;
    
    UIVertex v[6];
    v[0] = { x1 + nx, y1 + ny, 0, 0, c.r, c.g, c.b, c.a };
    v[1] = { x2 + nx, y2 + ny, 0, 0, c.r, c.g, c.b, c.a };
    v[2] = { x2 - nx, y2 - ny, 0, 0, c.r, c.g, c.b, c.a };
    v[3] = { x1 + nx, y1 + ny, 0, 0, c.r, c.g, c.b, c.a };
    v[4] = { x2 - nx, y2 - ny, 0, 0, c.r, c.g, c.b, c.a };
    v[5] = { x1 - nx, y1 - ny, 0, 0, c.r, c.g, c.b, c.a };
    
    for (int i = 0; i < 6; i++) m_vertices.push_back(v[i]);
}

void CUIRenderer::DrawCircle(f32 cx, f32 cy, f32 radius, const Color& color, bool filled) {
    const int segments = 32;
    const f32 PI = 3.14159265f;
    
    Color c = color;
    c.a *= m_currentOpacity;
    
    if (filled) {
        for (int i = 0; i < segments; i++) {
            f32 a1 = (f32)i / segments * 2 * PI;
            f32 a2 = (f32)(i + 1) / segments * 2 * PI;
            
            UIVertex v[3];
            v[0] = { cx, cy, 0, 0, c.r, c.g, c.b, c.a };
            v[1] = { cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, 0, 0, c.r, c.g, c.b, c.a };
            v[2] = { cx + std::cos(a2) * radius, cy + std::sin(a2) * radius, 0, 0, c.r, c.g, c.b, c.a };
            
            for (int j = 0; j < 3; j++) m_vertices.push_back(v[j]);
        }
    } else {
        for (int i = 0; i < segments; i++) {
            f32 a1 = (f32)i / segments * 2 * PI;
            f32 a2 = (f32)(i + 1) / segments * 2 * PI;
            DrawLine(cx + std::cos(a1) * radius, cy + std::sin(a1) * radius,
                    cx + std::cos(a2) * radius, cy + std::sin(a2) * radius, color, 1);
        }
    }
}

void CUIRenderer::PushClipRect(const Rect2D& rect) {
    FlushBatch();
    m_clipStack.push_back(rect);
    UpdateScissorRect();
}

void CUIRenderer::PopClipRect() {
    FlushBatch();
    if (!m_clipStack.empty()) m_clipStack.pop_back();
    UpdateScissorRect();
}

void CUIRenderer::SetClipEnabled(bool enabled) {
    m_clipEnabled = enabled;
    UpdateScissorRect();
}

void CUIRenderer::UpdateScissorRect() {
    if (!m_commandList) return;
    
    D3D12_RECT scissor;
    if (!m_clipEnabled || m_clipStack.empty()) {
        scissor = { 0, 0, (LONG)m_screenWidth, (LONG)m_screenHeight };
    } else {
        const Rect2D& r = m_clipStack.back();
        scissor = { (LONG)r.x, (LONG)r.y, (LONG)(r.x + r.width), (LONG)(r.y + r.height) };
    }
    m_commandList->RSSetScissorRects(1, &scissor);
}

void CUIRenderer::PushTransform(const Transform2D& transform) {
    m_transformStack.push_back(transform);
}

void CUIRenderer::PopTransform() {
    if (m_transformStack.size() > 1) {
        m_transformStack.pop_back();
    }
}

void CUIRenderer::Translate(f32 x, f32 y) {
    if (!m_transformStack.empty()) {
        m_transformStack.back().translateX += x;
        m_transformStack.back().translateY += y;
    }
}

void CUIRenderer::Rotate(f32 angle) {
    if (!m_transformStack.empty()) {
        m_transformStack.back().rotation += angle;
    }
}

void CUIRenderer::Scale(f32 sx, f32 sy) {
    if (!m_transformStack.empty()) {
        m_transformStack.back().scaleX *= sx;
        m_transformStack.back().scaleY *= sy;
    }
}

void CUIRenderer::SetTransformOrigin(f32 x, f32 y) {
    if (!m_transformStack.empty()) {
        m_transformStack.back().originX = x;
        m_transformStack.back().originY = y;
    }
}

void CUIRenderer::SetOpacity(f32 opacity) { m_currentOpacity = opacity; }
void CUIRenderer::SetBlur(f32 amount) { m_currentBlur = amount; }
void CUIRenderer::SetSaturation(f32 amount) { m_currentSaturation = amount; }
void CUIRenderer::SetBrightness(f32 amount) { m_currentBrightness = amount; }
void CUIRenderer::SetContrast(f32 amount) { m_currentContrast = amount; }
void CUIRenderer::SetWashColor(const Color& color) { m_currentWashColor = color; }

void CUIRenderer::ClearEffects() {
    m_currentOpacity = 1.0f;
    m_currentBlur = 0;
    m_currentSaturation = 1;
    m_currentBrightness = 1;
    m_currentContrast = 1;
    m_currentWashColor = Color::Transparent();
}

void CUIRenderer::ClearTextureCache() {
    m_textureCache.clear();
}

} // namespace Panorama
