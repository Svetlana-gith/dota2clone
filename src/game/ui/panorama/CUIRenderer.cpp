#include "CUIRenderer.h"
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <cmath>
#include <codecvt>
#include <locale>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Panorama {

CUIRenderer::CUIRenderer() = default;

CUIRenderer::~CUIRenderer() {
    Shutdown();
}

bool CUIRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, f32 width, f32 height) {
    m_device = device;
    m_context = context;
    m_screenWidth = width;
    m_screenHeight = height;
    
    CreateShaders();
    CreateBuffers();
    CreateRenderStates();
    
    m_transformStack.push_back(Transform2D{});
    
    // Initialize D2D/DirectWrite interop with D3D11 (optional - text rendering)
    // If this fails, we can still render shapes, just not text
    InitializeD2DInterop();
    
    return true;
}

bool CUIRenderer::InitializeD2DInterop() {
    HRESULT hr;
    
    // Create D2D1 Factory (must be single-threaded for DXGI interop)
    D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, m_d2dFactory.GetAddressOf());
    if (FAILED(hr)) return false;
    
    // Get DXGI device from D3D11 device
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (FAILED(hr)) return false;
    
    // Create D2D device from DXGI device
    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), m_d2dDevice.GetAddressOf());
    if (FAILED(hr)) return false;
    
    // Create D2D device context
    hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_d2dContext.GetAddressOf());
    if (FAILED(hr)) return false;
    
    // Set text antialiasing mode to ClearType for sharper text
    m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    
    // Create DirectWrite factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory1),
                             reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr)) return false;
    
    // Configure DirectWrite rendering parameters for better quality
    ComPtr<IDWriteRenderingParams> defaultParams;
    hr = m_dwriteFactory->CreateRenderingParams(&defaultParams);
    if (SUCCEEDED(hr)) {
        // Create custom rendering parameters with enhanced settings
        ComPtr<IDWriteRenderingParams> customParams;
        hr = m_dwriteFactory->CreateCustomRenderingParams(
            defaultParams->GetGamma(),                    // Use default gamma
            defaultParams->GetEnhancedContrast(),         // Use default enhanced contrast
            1.0f,                                          // ClearType level (1.0 = full ClearType)
            DWRITE_PIXEL_GEOMETRY_RGB,                    // Standard RGB pixel geometry
            DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC,  // Best quality ClearType mode
            &customParams
        );
        
        if (SUCCEEDED(hr)) {
            m_d2dContext->SetTextRenderingParams(customParams.Get());
        }
    }
    
    // Create default text format
    hr = m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        16.0f,
        L"en-us",
        m_defaultTextFormat.GetAddressOf()
    );
    if (FAILED(hr)) return false;
    
    // Create text brush
    hr = m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), m_textBrush.GetAddressOf());
    if (FAILED(hr)) return false;
    
    return true;
}

bool CUIRenderer::SetRenderTarget(ID3D11Texture2D* backBuffer) {
    if (!backBuffer) return false;
    
    // Release old D3D11 render target
    if (m_renderTargetView) {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
    
    // Create D3D11 render target view (always needed)
    HRESULT hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTargetView);
    if (FAILED(hr)) return false;
    
    // Get surface description for screen size
    D3D11_TEXTURE2D_DESC texDesc;
    backBuffer->GetDesc(&texDesc);
    m_screenWidth = static_cast<f32>(texDesc.Width);
    m_screenHeight = static_cast<f32>(texDesc.Height);
    
    // D2D setup is optional - if it fails, we still have D3D11 rendering
    if (m_d2dContext) {
        // Release old D2D targets
        m_d2dContext->SetTarget(nullptr);
        m_d2dTargetBitmap.Reset();
        m_dxgiSurface.Reset();
        
        // Get DXGI surface from back buffer
        hr = backBuffer->QueryInterface(IID_PPV_ARGS(&m_dxgiSurface));
        if (SUCCEEDED(hr)) {
            // Get surface description
            DXGI_SURFACE_DESC surfaceDesc;
            m_dxgiSurface->GetDesc(&surfaceDesc);
            
            // Create D2D bitmap properties for DXGI surface
            D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(surfaceDesc.Format, D2D1_ALPHA_MODE_PREMULTIPLIED)
            );
            
            // Create D2D bitmap from DXGI surface
            hr = m_d2dContext->CreateBitmapFromDxgiSurface(
                m_dxgiSurface.Get(),
                &bitmapProps,
                m_d2dTargetBitmap.GetAddressOf()
            );
            if (SUCCEEDED(hr)) {
                // Set as render target
                m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
            }
        }
    }
    
    return true;
}

bool CUIRenderer::CreateD2DRenderTarget(ID3D11Texture2D* backBuffer) {
    return SetRenderTarget(backBuffer);
}

void CUIRenderer::Shutdown() {
    ClearTextureCache();
    ShutdownD2DInterop();
    
    #define SAFE_RELEASE(x) if (x) { x->Release(); x = nullptr; }
    SAFE_RELEASE(m_renderTargetView);
    SAFE_RELEASE(m_blendState);
    SAFE_RELEASE(m_blendStateAdditive);
    SAFE_RELEASE(m_rasterizerState);
    SAFE_RELEASE(m_depthStencilState);
    SAFE_RELEASE(m_samplerState);
    SAFE_RELEASE(m_vertexShader);
    SAFE_RELEASE(m_pixelShader);
    SAFE_RELEASE(m_pixelShaderTextured);
    SAFE_RELEASE(m_pixelShaderBlur);
    SAFE_RELEASE(m_inputLayout);
    SAFE_RELEASE(m_vertexBuffer);
    SAFE_RELEASE(m_indexBuffer);
    SAFE_RELEASE(m_constantBuffer);
    #undef SAFE_RELEASE
}

void CUIRenderer::ShutdownD2DInterop() {
    m_textFormatCache.clear();
    m_textBrush.Reset();
    m_defaultTextFormat.Reset();
    m_dwriteFactory.Reset();
    m_d2dTargetBitmap.Reset();
    m_dxgiSurface.Reset();
    m_d2dContext.Reset();
    m_d2dDevice.Reset();
    m_d2dFactory.Reset();
}

std::wstring CUIRenderer::ToWideString(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

IDWriteTextFormat* CUIRenderer::GetOrCreateTextFormat(const FontInfo& font) {
    // Create cache key
    std::string key = font.family + "_" + std::to_string(font.size) + 
                      (font.bold ? "_b" : "") + (font.italic ? "_i" : "");
    
    auto it = m_textFormatCache.find(key);
    if (it != m_textFormatCache.end()) {
        return it->second.Get();
    }
    
    // Create new text format
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
    
    if (FAILED(hr)) {
        return m_defaultTextFormat.Get();
    }
    
    // Enable better text quality settings
    textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);  // Prevent wrapping for cleaner rendering
    
    m_textFormatCache[key] = textFormat;
    return textFormat.Get();
}

void CUIRenderer::SetScreenSize(f32 width, f32 height) {
    m_screenWidth = width;
    m_screenHeight = height;
}

void CUIRenderer::CreateShaders() {
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
        
        Texture2D tex : register(t0);
        SamplerState samp : register(s0);
        
        float4 PS(PS_INPUT input) : SV_TARGET {
            return input.color;
        }
        
        float4 PS_Textured(PS_INPUT input) : SV_TARGET {
            return tex.Sample(samp, input.uv) * input.color;
        }
    )";
    
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    HRESULT hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                           "VS", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (SUCCEEDED(hr) && vsBlob) {
        m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                     nullptr, &m_vertexShader);
        
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        m_device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(), &m_inputLayout);
        vsBlob->Release();
    }
    if (errorBlob) errorBlob->Release();
    
    hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                   "PS", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (SUCCEEDED(hr) && psBlob) {
        m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                    nullptr, &m_pixelShader);
        psBlob->Release();
    }
    if (errorBlob) errorBlob->Release();
    
    hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                   "PS_Textured", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (SUCCEEDED(hr) && psBlob) {
        m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                    nullptr, &m_pixelShaderTextured);
        psBlob->Release();
    }
    if (errorBlob) errorBlob->Release();
}

void CUIRenderer::CreateBuffers() {
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.ByteWidth = sizeof(UIVertex) * 20000;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&vbDesc, nullptr, &m_vertexBuffer);
    
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = 16;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
}

void CUIRenderer::CreateRenderStates() {
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_device->CreateBlendState(&blendDesc, &m_blendState);
    
    D3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.ScissorEnable = TRUE;
    m_device->CreateRasterizerState(&rastDesc, &m_rasterizerState);
    
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = FALSE;
    m_device->CreateDepthStencilState(&dsDesc, &m_depthStencilState);
    
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    m_device->CreateSamplerState(&sampDesc, &m_samplerState);
}

void CUIRenderer::BeginFrame() {
    m_vertices.clear();
    m_indices.clear();
    
    // Set render target (if we have one)
    if (m_renderTargetView) {
        m_context->OMSetRenderTargets(1, &m_renderTargetView, nullptr);
    }
    // Note: If no render target view, we assume the caller has already set one
    
    m_context->OMSetBlendState(m_blendState, nullptr, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(m_depthStencilState, 0);
    m_context->RSSetState(m_rasterizerState);
    
    m_context->VSSetShader(m_vertexShader, nullptr, 0);
    m_context->PSSetShader(m_pixelShader, nullptr, 0);
    m_context->IASetInputLayout(m_inputLayout);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    if (m_samplerState) {
        m_context->PSSetSamplers(0, 1, &m_samplerState);
    }
    
    // Set viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = m_screenWidth;
    vp.Height = m_screenHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (m_constantBuffer && SUCCEEDED(m_context->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        f32* data = (f32*)mapped.pData;
        data[0] = m_screenWidth;
        data[1] = m_screenHeight;
        data[2] = 0;
        data[3] = 0;
        m_context->Unmap(m_constantBuffer, 0);
    }
    if (m_constantBuffer) {
        m_context->VSSetConstantBuffers(0, 1, &m_constantBuffer);
    }
    
    if (m_vertexBuffer) {
        UINT stride = sizeof(UIVertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    }
    
    D3D11_RECT scissor = { 0, 0, (LONG)m_screenWidth, (LONG)m_screenHeight };
    m_context->RSSetScissorRects(1, &scissor);
    
    ClearEffects();
}

void CUIRenderer::EndFrame() {
    Flush();
}

void CUIRenderer::Flush() {
    FlushBatch();
}

void CUIRenderer::FlushBatch() {
    if (m_vertices.empty() || !m_vertexBuffer || !m_context) return;
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_context->Map(m_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, m_vertices.data(), m_vertices.size() * sizeof(UIVertex));
        m_context->Unmap(m_vertexBuffer, 0);
    }
    
    m_context->Draw((UINT)m_vertices.size(), 0);
    m_vertices.clear();
}

Vector2D CUIRenderer::TransformPoint(f32 x, f32 y) const {
    if (m_transformStack.empty()) return {x, y};
    
    const auto& t = m_transformStack.back();
    
    // Apply origin offset
    f32 px = x - t.originX;
    f32 py = y - t.originY;
    
    // Apply scale
    px *= t.scaleX;
    py *= t.scaleY;
    
    // Apply rotation
    if (t.rotation != 0) {
        f32 rad = t.rotation * 3.14159265f / 180.0f;
        f32 cos_r = std::cos(rad);
        f32 sin_r = std::sin(rad);
        f32 rx = px * cos_r - py * sin_r;
        f32 ry = px * sin_r + py * cos_r;
        px = rx;
        py = ry;
    }
    
    // Restore origin and apply translation
    return {px + t.originX + t.translateX, py + t.originY + t.translateY};
}

void CUIRenderer::AddQuad(const Rect2D& rect, const Color& color, f32 u0, f32 v0, f32 u1, f32 v1) {
    auto p0 = TransformPoint(rect.x, rect.y);
    auto p1 = TransformPoint(rect.x + rect.width, rect.y);
    auto p2 = TransformPoint(rect.x + rect.width, rect.y + rect.height);
    auto p3 = TransformPoint(rect.x, rect.y + rect.height);
    
    UIVertex v[6];
    v[0] = { p0.x, p0.y, u0, v0, color.r, color.g, color.b, color.a };
    v[1] = { p1.x, p1.y, u1, v0, color.r, color.g, color.b, color.a };
    v[2] = { p2.x, p2.y, u1, v1, color.r, color.g, color.b, color.a };
    v[3] = { p0.x, p0.y, u0, v0, color.r, color.g, color.b, color.a };
    v[4] = { p2.x, p2.y, u1, v1, color.r, color.g, color.b, color.a };
    v[5] = { p3.x, p3.y, u0, v1, color.r, color.g, color.b, color.a };
    
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

void CUIRenderer::DrawRoundedRect(const Rect2D& rect, const Color& color, f32 radius) {
    DrawRoundedRect(rect, color, radius, radius, radius, radius);
}

void CUIRenderer::DrawRoundedRect(const Rect2D& rect, const Color& color, f32 tl, f32 tr, f32 br, f32 bl) {
    // Simplified: just draw regular rect for now
    // Full implementation would tessellate corners
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
    // Simplified implementation
    DrawRect(rect, centerColor);
}

void CUIRenderer::DrawText(const std::string& text, const Rect2D& bounds, const Color& color, 
                           const FontInfo& font, HorizontalAlign hAlign, VerticalAlign vAlign) {
    if (text.empty() || !m_d2dContext || !m_d2dTargetBitmap) return;
    
    // Flush D3D11 batch before D2D rendering
    FlushBatch();
    
    // Get or create text format
    IDWriteTextFormat* textFormat = GetOrCreateTextFormat(font);
    if (!textFormat) return;
    
    // Set text alignment
    switch (hAlign) {
        case HorizontalAlign::Left:
            textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            break;
        case HorizontalAlign::Center:
            textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            break;
        case HorizontalAlign::Right:
            textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            break;
    }
    
    switch (vAlign) {
        case VerticalAlign::Top:
            textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            break;
        case VerticalAlign::Center:
            textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            break;
        case VerticalAlign::Bottom:
            textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);
            break;
    }
    
    // Apply transform
    D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Identity();
    if (!m_transformStack.empty()) {
        const auto& t = m_transformStack.back();
        transform = D2D1::Matrix3x2F::Translation(t.translateX, t.translateY) *
                    D2D1::Matrix3x2F::Rotation(t.rotation, D2D1::Point2F(t.originX, t.originY)) *
                    D2D1::Matrix3x2F::Scale(t.scaleX, t.scaleY, D2D1::Point2F(t.originX, t.originY));
    }
    
    // Convert text to wide string
    std::wstring wtext = ToWideString(text);
    
    // Set brush color
    m_textBrush->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
    
    // Create layout rect
    D2D1_RECT_F layoutRect = D2D1::RectF(bounds.x, bounds.y, 
                                          bounds.x + bounds.width, 
                                          bounds.y + bounds.height);
    
    // Begin D2D drawing
    m_d2dContext->BeginDraw();
    m_d2dContext->SetTransform(transform);
    
    // Set high-quality text rendering options
    m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    
    // Draw text with enhanced options
    m_d2dContext->DrawText(
        wtext.c_str(),
        static_cast<UINT32>(wtext.length()),
        textFormat,
        layoutRect,
        m_textBrush.Get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT  // Enable color fonts and better rendering
    );
    
    // End D2D drawing
    m_d2dContext->EndDraw();
    
    // Reset transform
    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
}

void CUIRenderer::DrawTextWithShadow(const std::string& text, const Rect2D& bounds, const Color& color,
                                     const Color& shadowColor, f32 shadowOffsetX, f32 shadowOffsetY,
                                     const FontInfo& font) {
    Rect2D shadowBounds = bounds;
    shadowBounds.x += shadowOffsetX;
    shadowBounds.y += shadowOffsetY;
    DrawText(text, shadowBounds, shadowColor, font);
    DrawText(text, bounds, color, font);
}

Vector2D CUIRenderer::MeasureText(const std::string& text, const FontInfo& font) {
    if (text.empty() || !m_dwriteFactory) {
        return {0, font.size};
    }
    
    IDWriteTextFormat* textFormat = GetOrCreateTextFormat(font);
    if (!textFormat) {
        return {text.length() * font.size * 0.6f, font.size};
    }
    
    std::wstring wtext = ToWideString(text);
    
    // Create text layout for measurement
    ComPtr<IDWriteTextLayout> textLayout;
    HRESULT hr = m_dwriteFactory->CreateTextLayout(
        wtext.c_str(),
        static_cast<UINT32>(wtext.length()),
        textFormat,
        10000.0f,  // Max width
        10000.0f,  // Max height
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

void CUIRenderer::DrawImage(const std::string& path, const Rect2D& rect, f32 opacity) {
    // Would load texture and draw
    Color c(1, 1, 1, opacity);
    AddQuad(rect, c);
}

void CUIRenderer::DrawImageTinted(const std::string& path, const Rect2D& rect, const Color& tint) {
    AddQuad(rect, tint);
}

void CUIRenderer::DrawImageRegion(const std::string& path, const Rect2D& destRect, const Rect2D& srcRect) {
    Color c = Color::White();
    AddQuad(destRect, c, srcRect.x, srcRect.y, srcRect.x + srcRect.width, srcRect.y + srcRect.height);
}

void CUIRenderer::DrawLine(const Vector2D& start, const Vector2D& end, const Color& color, f32 thickness) {
    f32 dx = end.x - start.x;
    f32 dy = end.y - start.y;
    f32 len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    
    f32 nx = -dy / len * thickness * 0.5f;
    f32 ny = dx / len * thickness * 0.5f;
    
    UIVertex v[6];
    v[0] = { start.x + nx, start.y + ny, 0, 0, color.r, color.g, color.b, color.a };
    v[1] = { end.x + nx, end.y + ny, 0, 0, color.r, color.g, color.b, color.a };
    v[2] = { end.x - nx, end.y - ny, 0, 0, color.r, color.g, color.b, color.a };
    v[3] = { start.x + nx, start.y + ny, 0, 0, color.r, color.g, color.b, color.a };
    v[4] = { end.x - nx, end.y - ny, 0, 0, color.r, color.g, color.b, color.a };
    v[5] = { start.x - nx, start.y - ny, 0, 0, color.r, color.g, color.b, color.a };
    
    for (int i = 0; i < 6; i++) m_vertices.push_back(v[i]);
}

void CUIRenderer::DrawCircle(const Vector2D& center, f32 radius, const Color& color, bool filled) {
    const int segments = 32;
    const f32 PI = 3.14159265f;
    
    if (filled) {
        for (int i = 0; i < segments; i++) {
            f32 a1 = (f32)i / segments * 2 * PI;
            f32 a2 = (f32)(i + 1) / segments * 2 * PI;
            
            UIVertex v[3];
            v[0] = { center.x, center.y, 0, 0, color.r, color.g, color.b, color.a };
            v[1] = { center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius, 0, 0, color.r, color.g, color.b, color.a };
            v[2] = { center.x + std::cos(a2) * radius, center.y + std::sin(a2) * radius, 0, 0, color.r, color.g, color.b, color.a };
            
            for (int j = 0; j < 3; j++) m_vertices.push_back(v[j]);
        }
    } else {
        for (int i = 0; i < segments; i++) {
            f32 a1 = (f32)i / segments * 2 * PI;
            f32 a2 = (f32)(i + 1) / segments * 2 * PI;
            DrawLine({center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius},
                    {center.x + std::cos(a2) * radius, center.y + std::sin(a2) * radius}, color, 1);
        }
    }
}

void CUIRenderer::DrawArc(const Vector2D& center, f32 radius, f32 startAngle, f32 endAngle, 
                          const Color& color, f32 thickness) {
    const int segments = 32;
    const f32 PI = 3.14159265f;
    f32 startRad = startAngle * PI / 180.0f;
    f32 endRad = endAngle * PI / 180.0f;
    f32 step = (endRad - startRad) / segments;
    
    for (int i = 0; i < segments; i++) {
        f32 a1 = startRad + step * i;
        f32 a2 = startRad + step * (i + 1);
        DrawLine({center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius},
                {center.x + std::cos(a2) * radius, center.y + std::sin(a2) * radius}, color, thickness);
    }
}

void CUIRenderer::DrawPolygon(const std::vector<Vector2D>& points, const Color& color, bool filled) {
    if (points.size() < 3) return;
    
    if (filled) {
        for (size_t i = 1; i < points.size() - 1; i++) {
            UIVertex v[3];
            v[0] = { points[0].x, points[0].y, 0, 0, color.r, color.g, color.b, color.a };
            v[1] = { points[i].x, points[i].y, 0, 0, color.r, color.g, color.b, color.a };
            v[2] = { points[i+1].x, points[i+1].y, 0, 0, color.r, color.g, color.b, color.a };
            for (int j = 0; j < 3; j++) m_vertices.push_back(v[j]);
        }
    } else {
        for (size_t i = 0; i < points.size(); i++) {
            DrawLine(points[i], points[(i + 1) % points.size()], color, 1);
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
    if (!m_clipEnabled || m_clipStack.empty()) {
        D3D11_RECT scissor = { 0, 0, (LONG)m_screenWidth, (LONG)m_screenHeight };
        m_context->RSSetScissorRects(1, &scissor);
    } else {
        const Rect2D& r = m_clipStack.back();
        D3D11_RECT scissor = { (LONG)r.x, (LONG)r.y, (LONG)(r.x + r.width), (LONG)(r.y + r.height) };
        m_context->RSSetScissorRects(1, &scissor);
    }
}

void CUIRenderer::PushTransform() {
    if (m_transformStack.empty()) {
        m_transformStack.push_back(Transform2D{});
    } else {
        m_transformStack.push_back(m_transformStack.back());
    }
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

void CUIRenderer::SetBlur(f32 amount) { m_currentBlur = amount; }
void CUIRenderer::SetSaturation(f32 amount) { m_currentSaturation = amount; }
void CUIRenderer::SetBrightness(f32 amount) { m_currentBrightness = amount; }
void CUIRenderer::SetContrast(f32 amount) { m_currentContrast = amount; }
void CUIRenderer::SetWashColor(const Color& color) { m_currentWashColor = color; }

void CUIRenderer::ClearEffects() {
    m_currentBlur = 0;
    m_currentSaturation = 1;
    m_currentBrightness = 1;
    m_currentContrast = 1;
    m_currentWashColor = Color::Transparent();
}

void CUIRenderer::DrawBoxShadow(const Rect2D& rect, const Color& color, f32 offsetX, f32 offsetY, 
                                f32 blur, f32 spread, bool inset) {
    // Simplified shadow - just draw offset rect with alpha
    Rect2D shadowRect = rect;
    shadowRect.x += offsetX - spread;
    shadowRect.y += offsetY - spread;
    shadowRect.width += spread * 2;
    shadowRect.height += spread * 2;
    
    Color shadowColor = color;
    shadowColor.a *= 0.5f;
    DrawRect(shadowRect, shadowColor);
}

ID3D11ShaderResourceView* CUIRenderer::LoadTexture(const std::string& path) {
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) return it->second;
    
    // Would load texture from file
    // For now, return nullptr
    return nullptr;
}

void CUIRenderer::UnloadTexture(const std::string& path) {
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) {
        if (it->second) it->second->Release();
        m_textureCache.erase(it);
    }
}

void CUIRenderer::ClearTextureCache() {
    for (auto& pair : m_textureCache) {
        if (pair.second) pair.second->Release();
    }
    m_textureCache.clear();
}

} // namespace Panorama
