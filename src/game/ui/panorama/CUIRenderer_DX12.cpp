#include "CUIRenderer.h"
#include "PanoramaTypes.h"
#include "FontAtlas.h"
#include <spdlog/spdlog.h>
#include <d3dcompiler.h>
#include <dwrite.h>
#include <d2d1.h>
#include <cmath>
#include <locale>
#include <codecvt>
#include <unordered_set>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Panorama {

// Vertex structure for UI rendering
struct UIVertex {
    f32 x, y;           // Position
    f32 u, v;           // UV coordinates
    f32 r, g, b, a;     // Color
};

// SDF text configuration.
// We generate a single SDF atlas at a stable base pixel size and scale geometry for other font sizes.
static constexpr float kSdfBaseFontSizePx = 32.0f;
// Debug mode for text PS:
// 0 = normal SDF rendering
// 1 = visualize dist in grayscale (alpha=1)
// 2 = solid magenta quads (alpha=1) to confirm glyph quads are being drawn
static constexpr float kSdfDebugMode = 0.0f;

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
    
    // Initialize transform stack with identity
    m_transformStack.push_back(Transform2D{});
    
    if (!CreateRootSignature()) {
        LOG_ERROR("Failed to create UI root signature");
        return false;
    }
    
    if (!CompileShaders()) {
        LOG_ERROR("Failed to compile UI shaders");
        return false;
    }
    
    if (!CreatePipelineState()) {
        LOG_ERROR("Failed to create UI pipeline state");
        return false;
    }
    
    if (!CreateBuffers()) {
        LOG_ERROR("Failed to create UI buffers");
        return false;
    }
    
    if (!InitializeDirectWrite()) {
        LOG_WARN("Failed to initialize DirectWrite - text rendering will be limited");
    }
    
    // Initialize font system (Dota 2 style)
    if (srvHeap) {
        FontManager::Instance().Initialize(device, commandQueue, commandList, srvHeap);
        // Don't pre-load fonts here - command list is already closed
        // Fonts will be loaded on-demand during first DrawText call
        LOG_INFO("FontManager initialized (fonts will load on-demand)");
    }
    
    LOG_INFO("CUIRenderer (DX12) initialized: {}x{}", width, height);
    return true;
}

void CUIRenderer::Shutdown() {
    FontManager::Instance().Shutdown();
    ClearTextureCache();
    ShutdownDirectWrite();
    m_device = nullptr;
    m_commandQueue = nullptr;
    m_commandList = nullptr;
    m_srvHeap = nullptr;
}

void CUIRenderer::SetScreenSize(f32 width, f32 height) {
    m_screenWidth = width;
    m_screenHeight = height;
}

void CUIRenderer::BeginFrame() {
    // Rotate to next frame's vertex buffer
    m_currentFrameIndex = (m_currentFrameIndex + 1) % kFrameCount;
    
    // Clear render queue
    m_renderCommands.clear();
    m_vertices.clear();
    m_textVertices.clear();
    m_indices.clear();
    m_textUploadCursorVertices = 0;
    
    // Reset effects
    ClearEffects();
    
    // Set viewport and scissor ONCE per frame
    if (m_commandList) {
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, m_screenWidth, m_screenHeight, 0.0f, 1.0f };
        D3D12_RECT scissor = { 0, 0, (LONG)m_screenWidth, (LONG)m_screenHeight };
        m_commandList->RSSetViewports(1, &viewport);
        m_commandList->RSSetScissorRects(1, &scissor);
    }
    
    m_frameCount++;
}

void CUIRenderer::EndFrame() {
    // Flush solid geometry first
    FlushBatch();
    
    // Then flush text
    FlushTextBatch();
}

void CUIRenderer::Flush() {
    FlushBatch();
}

void CUIRenderer::FlushBatch() {
    if (m_vertices.empty() || !m_commandList || !m_pipelineState) return;
    
    // Use current frame's vertex buffer
    auto& vertexBuffer = m_vertexBuffers[m_currentFrameIndex];
    
    // Upload vertex data to first half of buffer (offset 0)
    void* pData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr = vertexBuffer->Map(0, &readRange, &pData);
    if (SUCCEEDED(hr)) {
        size_t dataSize = m_vertices.size() * sizeof(UIVertex);
        memcpy(pData, m_vertices.data(), dataSize);
        vertexBuffer->Unmap(0, nullptr);
    } else {
        return;
    }
    
    // Set pipeline state
    m_commandList->SetPipelineState(m_pipelineState.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    
    // Set root constants
    float screenConstants[4] = { m_screenWidth, m_screenHeight, 0, 0 };
    m_commandList->SetGraphicsRoot32BitConstants(0, 4, screenConstants, 0);
    
    float opacityConstant = m_currentOpacity;
    m_commandList->SetGraphicsRoot32BitConstants(1, 1, &opacityConstant, 0);

    // SDF constant slot exists in the root signature; solid PS doesn't use it.
    // Keep it deterministic anyway.
    float sdfConsts[2] = { 1.0f, 0.0f };
    m_commandList->SetGraphicsRoot32BitConstants(2, 2, sdfConsts, 0);
    
    // Create vertex buffer view for solid geometry (starts at 0)
    D3D12_VERTEX_BUFFER_VIEW solidView = {};
    solidView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    solidView.SizeInBytes = (UINT)(m_vertices.size() * sizeof(UIVertex));
    solidView.StrideInBytes = sizeof(UIVertex);
    
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &solidView);
    
    // Draw
    m_commandList->DrawInstanced((UINT)m_vertices.size(), 1, 0, 0);
    
    // Clear batch
    m_vertices.clear();
    m_indices.clear();
}

void CUIRenderer::FlushTextBatch() {
    if (m_textVertices.empty() || !m_commandList || !m_pipelineStateTextured) return;
    if (!m_currentFont) return;

    // Log EVERY flush to diagnose batching issues
    // Use current frame's vertex buffer
    auto& vertexBuffer = m_vertexBuffers[m_currentFrameIndex];
    
    // Upload vertex data to the second half of the buffer.
    // IMPORTANT: We can flush text multiple times per frame (e.g. different font sizes).
    // Each flush must use a unique offset to avoid overwriting data referenced by earlier draw calls.
    static constexpr size_t kMaxVerticesPerFrame = 120000;
    static constexpr size_t kTextBaseVertexOffset = 60000;
    const size_t uploadVertexOffset = kTextBaseVertexOffset + m_textUploadCursorVertices;
    const size_t uploadVertexCount = m_textVertices.size();
    if (uploadVertexOffset + uploadVertexCount > kMaxVerticesPerFrame) {
        LOG_ERROR("FlushTextBatch overflow: need {} vertices at offset {}, but max is {}. Dropping text batch.",
            uploadVertexCount, uploadVertexOffset, kMaxVerticesPerFrame);
        m_textVertices.clear();
        return;
    }

    const size_t textOffsetBytes = uploadVertexOffset * sizeof(UIVertex);
    
    void* pData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr = vertexBuffer->Map(0, &readRange, &pData);
    if (SUCCEEDED(hr)) {
        size_t dataSize = m_textVertices.size() * sizeof(UIVertex);
        memcpy((uint8_t*)pData + textOffsetBytes, m_textVertices.data(), dataSize);
        vertexBuffer->Unmap(0, nullptr);
    } else {
        return;
    }
    
    // Set textured pipeline state
    m_commandList->SetPipelineState(m_pipelineStateTextured.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    
    // Set root constants
    float screenConstants[4] = { m_screenWidth, m_screenHeight, 0, 0 };
    m_commandList->SetGraphicsRoot32BitConstants(0, 4, screenConstants, 0);
    
    float opacityConstant = m_currentOpacity;
    m_commandList->SetGraphicsRoot32BitConstants(1, 1, &opacityConstant, 0);

    // Text smoothing scale: for SDF rendering, this affects edge smoothness.
    // Since we batch all font sizes together now, we use a default scale of 1.0.
    // TODO: For proper SDF rendering, consider passing scale per-vertex or using screen-space derivatives.
    float sdfScale = 1.0f;
    float sdfConsts[2] = { sdfScale, kSdfDebugMode };
    m_commandList->SetGraphicsRoot32BitConstants(2, 2, sdfConsts, 0);
    
    // Set font texture
    if (m_srvHeap) {
        ID3D12DescriptorHeap* heaps[] = { m_srvHeap };
        m_commandList->SetDescriptorHeaps(1, heaps);
        m_commandList->SetGraphicsRootDescriptorTable(3, m_currentFont->GetSRV());
    }
    
    // Create vertex buffer view for text (starts at our per-flush offset)
    D3D12_VERTEX_BUFFER_VIEW textView = {};
    textView.BufferLocation = vertexBuffer->GetGPUVirtualAddress() + textOffsetBytes;
    textView.SizeInBytes = (UINT)(m_textVertices.size() * sizeof(UIVertex));
    textView.StrideInBytes = sizeof(UIVertex);
    
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &textView);
    
    // Draw
    m_commandList->DrawInstanced((UINT)m_textVertices.size(), 1, 0, 0);
    
    // Clear text batch
    m_textUploadCursorVertices += uploadVertexCount;
    m_textVertices.clear();
}

// ============ Transform Stack ============

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

void CUIRenderer::PushTransform(const Transform2D& transform) {
    if (m_transformStack.empty()) {
        m_transformStack.push_back(transform);
    } else {
        m_transformStack.push_back(m_transformStack.back());
        auto& t = m_transformStack.back();
        t.translateX += transform.translateX;
        t.translateY += transform.translateY;
        t.rotation += transform.rotation;
        t.scaleX *= transform.scaleX;
        t.scaleY *= transform.scaleY;
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

// ============ Clipping ============

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

// ============ Geometry Generation ============

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

// ============ Drawing Primitives ============

void CUIRenderer::DrawRect(const Rect2D& rect, const Color& color) {
    AddQuad(rect, color);
}

void CUIRenderer::DrawRectOutline(const Rect2D& rect, const Color& color, f32 thickness) {
    AddQuad({ rect.x, rect.y, rect.width, thickness }, color);
    AddQuad({ rect.x, rect.y + rect.height - thickness, rect.width, thickness }, color);
    AddQuad({ rect.x, rect.y + thickness, thickness, rect.height - thickness * 2 }, color);
    AddQuad({ rect.x + rect.width - thickness, rect.y + thickness, thickness, rect.height - thickness * 2 }, color);
}

void CUIRenderer::DrawRoundedRect(const Rect2D& rect, f32 radius, const Color& color) {
    // Simplified: just draw regular rect for now
    // TODO: Implement proper rounded corners with tessellation or SDF
    AddQuad(rect, color);
}

void CUIRenderer::DrawRoundedRect(const Rect2D& rect, const Color& color, f32 radius) {
    DrawRoundedRect(rect, radius, color);
}

void CUIRenderer::DrawRoundedRect(const Rect2D& rect, const Color& color, f32 tl, f32 tr, f32 br, f32 bl) {
    // Simplified: just draw regular rect for now
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

void CUIRenderer::DrawLine(f32 x1, f32 y1, f32 x2, f32 y2, const Color& color, f32 thickness) {
    f32 dx = x2 - x1;
    f32 dy = y2 - y1;
    f32 len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    
    f32 nx = -dy / len * thickness * 0.5f;
    f32 ny = dx / len * thickness * 0.5f;
    
    auto p0 = TransformPoint(x1 + nx, y1 + ny);
    auto p1 = TransformPoint(x2 + nx, y2 + ny);
    auto p2 = TransformPoint(x2 - nx, y2 - ny);
    auto p3 = TransformPoint(x1 - nx, y1 - ny);
    
    UIVertex v[6];
    v[0] = { p0.x, p0.y, 0, 0, color.r, color.g, color.b, color.a };
    v[1] = { p1.x, p1.y, 0, 0, color.r, color.g, color.b, color.a };
    v[2] = { p2.x, p2.y, 0, 0, color.r, color.g, color.b, color.a };
    v[3] = { p0.x, p0.y, 0, 0, color.r, color.g, color.b, color.a };
    v[4] = { p2.x, p2.y, 0, 0, color.r, color.g, color.b, color.a };
    v[5] = { p3.x, p3.y, 0, 0, color.r, color.g, color.b, color.a };
    
    for (int i = 0; i < 6; i++) m_vertices.push_back(v[i]);
}

void CUIRenderer::DrawCircle(f32 x, f32 y, f32 radius, const Color& color, bool filled) {
    const int segments = 32;
    const f32 PI = 3.14159265f;
    
    if (filled) {
        for (int i = 0; i < segments; i++) {
            f32 a1 = (f32)i / segments * 2 * PI;
            f32 a2 = (f32)(i + 1) / segments * 2 * PI;
            
            auto p0 = TransformPoint(x, y);
            auto p1 = TransformPoint(x + std::cos(a1) * radius, y + std::sin(a1) * radius);
            auto p2 = TransformPoint(x + std::cos(a2) * radius, y + std::sin(a2) * radius);
            
            UIVertex v[3];
            v[0] = { p0.x, p0.y, 0, 0, color.r, color.g, color.b, color.a };
            v[1] = { p1.x, p1.y, 0, 0, color.r, color.g, color.b, color.a };
            v[2] = { p2.x, p2.y, 0, 0, color.r, color.g, color.b, color.a };
            
            for (int j = 0; j < 3; j++) m_vertices.push_back(v[j]);
        }
    } else {
        for (int i = 0; i < segments; i++) {
            f32 a1 = (f32)i / segments * 2 * PI;
            f32 a2 = (f32)(i + 1) / segments * 2 * PI;
            DrawLine(x + std::cos(a1) * radius, y + std::sin(a1) * radius,
                    x + std::cos(a2) * radius, y + std::sin(a2) * radius, color, 1);
        }
    }
}

// ============ Text Rendering ============

void CUIRenderer::DrawText(const std::string& text, const Rect2D& bounds, const Color& color, 
                           const FontInfo& font, HorizontalAlign hAlign, VerticalAlign vAlign) {
    if (text.empty()) return;
    
    // Debug: log all DrawText calls to understand rendering order
    // Try to use SDF font atlas if available.
    // We keep a single atlas at kSdfBaseFontSizePx and scale geometry for other font sizes.
    const float roundedSize = std::max(1.0f, std::round(font.size));
    const float geomScale = (kSdfBaseFontSizePx > 0.0f) ? (roundedSize / kSdfBaseFontSizePx) : 1.0f;

    // IMPORTANT:
    // Text is batched into m_textVertices and flushed once per frame with ONE SRV (m_currentFont).
    // Since we use a single SDF atlas (kSdfBaseFontSizePx) for all font sizes, we only need to
    // flush when the font FAMILY changes, not the size. The size only affects geometry scaling.
    bool currentMatches = (m_currentFont && m_currentFontFamily == font.family);

    if (!currentMatches) {
        if (!m_textVertices.empty() && m_currentFont) {
            FlushTextBatch();
        }
        m_currentFont = nullptr;
        m_currentFontFamily.clear();
    }

    FontAtlas* fontAtlas = m_currentFont;
    if (!fontAtlas && m_srvHeap) {
        // Always fetch the base SDF atlas; geometry/shader are scaled by requested size.
        fontAtlas = FontManager::Instance().GetFont(font.family, kSdfBaseFontSizePx);
        if (fontAtlas) {
            m_currentFont = fontAtlas;
            m_currentFontFamily = font.family;
        } else {
            // Log only once per font family to avoid spam
            static std::unordered_set<std::string> s_loggedMissingFonts;
            if (s_loggedMissingFonts.find(font.family) == s_loggedMissingFonts.end()) {
                s_loggedMissingFonts.insert(font.family);
                LOG_ERROR("DrawText: FontAtlas is NULL for family='{}' size={} (base={}). Text='{}' will use fallback.",
                    font.family, roundedSize, kSdfBaseFontSizePx, text.substr(0, 30));
            }
        }
    }
    
    if (fontAtlas) {
        // Render using font atlas (Dota 2 style)
        const float letterSpacing = std::max(0.0f, font.letterSpacing);

        // Minimal UTF-8 decoder: returns next Unicode codepoint and advances index.
        // Invalid sequences yield U+FFFD and advance by 1 byte.
        auto nextCodepoint = [](const std::string& s, size_t& i) -> uint32_t {
            const size_t n = s.size();
            if (i >= n) return 0;
            const uint8_t c0 = static_cast<uint8_t>(s[i]);
            if (c0 < 0x80) {
                i += 1;
                return c0;
            }
            // 2-byte
            if ((c0 & 0xE0) == 0xC0 && i + 1 < n) {
                const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                if ((c1 & 0xC0) == 0x80) {
                    i += 2;
                    return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
                }
            }
            // 3-byte
            if ((c0 & 0xF0) == 0xE0 && i + 2 < n) {
                const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
                if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80)) {
                    i += 3;
                    return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
                }
            }
            // 4-byte
            if ((c0 & 0xF8) == 0xF0 && i + 3 < n) {
                const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
                const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
                const uint8_t c3 = static_cast<uint8_t>(s[i + 3]);
                if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
                    i += 4;
                    return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                }
            }
            i += 1;
            return 0xFFFD;
        };
        
        // Measure text with basic multiline + letter spacing.
        auto measureWithSpacing = [&](const std::string& s) -> Vector2D {
            float maxWidth = 0.0f;
            float lineWidth = 0.0f;
            float height = fontAtlas->GetLineHeight() * geomScale;
            bool firstInLine = true;
            
            // Use space glyph for tabs/unknown spacing
            const FontGlyph* spaceGlyph = fontAtlas->GetGlyph((uint32_t)' ');
            const float spaceAdvanceBase = spaceGlyph ? spaceGlyph->advance : (fontAtlas->GetFontSize() * 0.5f);
            const float spaceAdvance = spaceAdvanceBase * geomScale;
            
            size_t i = 0;
            while (i < s.size()) {
                const uint32_t cp = nextCodepoint(s, i);
                if (cp == '\r') continue;
                if (cp == '\n') {
                    maxWidth = std::max(maxWidth, lineWidth);
                    lineWidth = 0.0f;
                    height += fontAtlas->GetLineHeight() * geomScale;
                    firstInLine = true;
                    continue;
                }
                
                if (!firstInLine) lineWidth += letterSpacing;
                firstInLine = false;
                
                if (cp == '\t') {
                    lineWidth += spaceAdvance * 4.0f;
                    continue;
                }
                
                const FontGlyph* g = fontAtlas->GetGlyph(cp);
                if (g) {
                    // stb_truetype advance can be smaller than the bitmap box for some fonts/sizes,
                    // which makes text look "crushed" due to overlap. Enforce a minimum advance
                    // that fits the glyph's drawn box (offsetX + width).
                    const float minAdvance = std::max(0.0f, (g->offsetX + g->width) * geomScale);
                    lineWidth += std::max(g->advance * geomScale, minAdvance);
                } else {
                    lineWidth += spaceAdvance;
                }
            }
            
            maxWidth = std::max(maxWidth, lineWidth);
            return { maxWidth, height };
        };
        
        Vector2D textSize = measureWithSpacing(text);
        
        // Calculate position based on alignment
        float textX = bounds.x;
        float textY = bounds.y;
        
        switch (hAlign) {
            case HorizontalAlign::Center:
                textX = bounds.x + (bounds.width - textSize.x) * 0.5f;
                break;
            case HorizontalAlign::Right:
                textX = bounds.x + bounds.width - textSize.x;
                break;
            default:
                break;
        }
        
        switch (vAlign) {
            case VerticalAlign::Center:
                textY = bounds.y + (bounds.height - textSize.y) * 0.5f;
                break;
            case VerticalAlign::Bottom:
                textY = bounds.y + bounds.height - textSize.y;
                break;
            default:
                break;
        }
        
        // Render each character as a textured quad (supports basic multiline via '\n')
        const float baseX = textX;
        float cursorX = textX;
        const float lineStep = fontAtlas->GetLineHeight() * geomScale;

        // Snap ONLY the baseline per line. Rounding each glyph Y individually looks worse
        // because glyph offsetY differs slightly per character (causes 1px jitter).
        const float baseLineY0 = std::round(textY + fontAtlas->GetAscent() * geomScale);
        int lineIndex = 0;
        float baselineY = baseLineY0;

        bool firstInLine = true;
        static bool s_loggedFirstGlyph = false;
        size_t ti = 0;
        while (ti < text.size()) {
            const uint32_t cp = nextCodepoint(text, ti);
            if (cp == '\r') continue;
            if (cp == '\n') {
                cursorX = baseX;
                lineIndex++;
                baselineY = std::round(baseLineY0 + (float)lineIndex * lineStep);
                firstInLine = true;
                continue;
            }
            
            if (!firstInLine) cursorX += letterSpacing;
            firstInLine = false;
            
            const FontGlyph* glyph = fontAtlas->GetGlyph(cp);
            
            if (cp == ' ') {
                const FontGlyph* sg = glyph ? glyph : fontAtlas->GetGlyph((uint32_t)' ');
                cursorX += sg ? (sg->advance * geomScale) : (fontAtlas->GetFontSize() * 0.5f * geomScale);
                continue;
            }
            if (cp == '\t') {
                // Simple tab = 4 spaces.
                const FontGlyph* sg = fontAtlas->GetGlyph((uint32_t)' ');
                const float adv = sg ? (sg->advance * geomScale) : (fontAtlas->GetFontSize() * 0.5f * geomScale);
                cursorX += adv * 4.0f;
                continue;
            }

            if (!glyph) {
                // Unknown glyph: advance by space width so layout doesn't collapse.
                const FontGlyph* sg = fontAtlas->GetGlyph((uint32_t)' ');
                cursorX += sg ? (sg->advance * geomScale) : (fontAtlas->GetFontSize() * 0.5f * geomScale);
                continue;
            }

            if (!s_loggedFirstGlyph) {
                s_loggedFirstGlyph = true;
                LOG_INFO("DrawText first glyph: cp=U+{:04X} w={} h={} u0={:.4f} v0={:.4f} u1={:.4f} v1={:.4f} bounds=({:.1f},{:.1f},{:.1f},{:.1f})",
                    (uint32_t)cp,
                    glyph->width, glyph->height,
                    glyph->u0, glyph->v0, glyph->u1, glyph->v1,
                    bounds.x, bounds.y, bounds.width, bounds.height);
            }
            
            // Calculate glyph position (keep per-glyph Y fractional to avoid jitter).
            float glyphX = cursorX + glyph->offsetX * geomScale;
            float glyphY = baselineY + glyph->offsetY * geomScale;
            
            // Add textured quad for this glyph
            auto p0 = TransformPoint(glyphX, glyphY);
            auto p1 = TransformPoint(glyphX + glyph->width * geomScale, glyphY);
            auto p2 = TransformPoint(glyphX + glyph->width * geomScale, glyphY + glyph->height * geomScale);
            auto p3 = TransformPoint(glyphX, glyphY + glyph->height * geomScale);
            
            UIVertex v[6];
            v[0] = { p0.x, p0.y, glyph->u0, glyph->v0, color.r, color.g, color.b, color.a };
            v[1] = { p1.x, p1.y, glyph->u1, glyph->v0, color.r, color.g, color.b, color.a };
            v[2] = { p2.x, p2.y, glyph->u1, glyph->v1, color.r, color.g, color.b, color.a };
            v[3] = { p0.x, p0.y, glyph->u0, glyph->v0, color.r, color.g, color.b, color.a };
            v[4] = { p2.x, p2.y, glyph->u1, glyph->v1, color.r, color.g, color.b, color.a };
            v[5] = { p3.x, p3.y, glyph->u0, glyph->v1, color.r, color.g, color.b, color.a };
            
            for (int i = 0; i < 6; i++) {
                m_textVertices.push_back(v[i]); // Add to text batch
            }
            
            {
                // See note in measureWithSpacing(): avoid overlaps.
                const float minAdvance = std::max(0.0f, (glyph->offsetX + glyph->width) * geomScale);
                cursorX += std::max(glyph->advance * geomScale, minAdvance);
            }
        }
        
        // Don't flush here - will flush at end of frame
        
    } else {
        // Fallback: use DirectWrite measurement with block visualization
        Vector2D textSize = MeasureText(text, font);
        
        float textX = bounds.x;
        float textY = bounds.y;
        
        switch (hAlign) {
            case HorizontalAlign::Center:
                textX = bounds.x + (bounds.width - textSize.x) * 0.5f;
                break;
            case HorizontalAlign::Right:
                textX = bounds.x + bounds.width - textSize.x;
                break;
            default:
                break;
        }
        
        switch (vAlign) {
            case VerticalAlign::Center:
                textY = bounds.y + (bounds.height - textSize.y) * 0.5f;
                break;
            case VerticalAlign::Bottom:
                textY = bounds.y + bounds.height - textSize.y;
                break;
            default:
                break;
        }
        
        // Draw text background
        Rect2D textBounds = { textX - 2, textY - 2, textSize.x + 4, textSize.y + 4 };
        Color bgColor = color;
        bgColor.a *= 0.15f;
        DrawRect(textBounds, bgColor);
        
        // Draw character blocks
        float charWidth = font.size * 0.55f;
        float charHeight = font.size * 0.85f;
        float charSpacing = font.size * 0.05f;
        
        for (size_t i = 0; i < text.length() && i < 100; ++i) {
            char c = text[i];
            if (c == '\r' || c == '\n') continue; // Keep fallback strictly single-line
            if (c == ' ') continue;
            
            float cx = textX + i * (charWidth + charSpacing);
            if (cx + charWidth > bounds.x + bounds.width) break;
           
            // IMPORTANT: Keep the fallback rendering stable on Y (no per-character wobble).
            // This is only a debug/placeholder renderer, but jitter makes UI text unreadable.
            Rect2D charRect = { cx, textY, charWidth, charHeight };
            
            Color charColor = color;
            charColor.a *= 0.7f;
            DrawRect(charRect, charColor);
        }
    }
}

Vector2D CUIRenderer::MeasureText(const std::string& text, const FontInfo& font) {
    if (text.empty() || !m_dwriteFactory) {
        return {0, font.size};
    }
    
    IDWriteTextFormat* textFormat = GetOrCreateTextFormat(font);
    if (!textFormat) {
        // Fallback estimation
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

f32 CUIRenderer::MeasureTextWidth(const std::string& text, const FontInfo& font) {
    return text.length() * font.size * 0.6f;
}

f32 CUIRenderer::MeasureTextHeight(const FontInfo& font) {
    return font.size;
}

// ============ Image Rendering ============

void CUIRenderer::DrawImage(const std::string& path, const Rect2D& rect, f32 opacity) {
    // TODO: Load and render texture
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

// ============ Effects ============

void CUIRenderer::SetOpacity(f32 opacity) {
    m_currentOpacity = opacity;
}

void CUIRenderer::SetBlur(f32 amount) { 
    m_currentBlur = amount; 
}

void CUIRenderer::SetSaturation(f32 amount) { 
    m_currentSaturation = amount; 
}

void CUIRenderer::SetBrightness(f32 amount) { 
    m_currentBrightness = amount; 
}

void CUIRenderer::SetContrast(f32 amount) { 
    m_currentContrast = amount; 
}

void CUIRenderer::SetWashColor(const Color& color) { 
    m_currentWashColor = color; 
}

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

// ============ Texture Management ============

void CUIRenderer::ClearTextureCache() {
    // TODO: Release DX12 texture resources
    m_textureCache.clear();
}

// ============ DX12 Resource Creation ============

bool CUIRenderer::CreateRootSignature() {
    // Root parameters:
    //  - b0: ScreenConstants
    //  - b1: OpacityConstants
    //  - b2: SdfConstants (text only)
    //  - t0: Texture SRV (font atlas)
    //  - s0: static sampler
    D3D12_ROOT_PARAMETER rootParams[4] = {};
    
    // b0: Screen constants
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants.ShaderRegister = 0;
    rootParams[0].Constants.RegisterSpace = 0;
    rootParams[0].Constants.Num32BitValues = 4; // float2 screenSize + float2 padding
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    
    // b1: Transform/opacity constants
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[1].Constants.ShaderRegister = 1;
    rootParams[1].Constants.RegisterSpace = 0;
    rootParams[1].Constants.Num32BitValues = 1; // float opacity
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    // b2: SDF constants (used by the text PS)
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[2].Constants.ShaderRegister = 2;
    rootParams[2].Constants.RegisterSpace = 0;
    rootParams[2].Constants.Num32BitValues = 2; // float sdfScale + float debugMode
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // t0: Texture (descriptor table)
    D3D12_DESCRIPTOR_RANGE descRange = {};
    descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange.NumDescriptors = 1;
    descRange.BaseShaderRegister = 0;
    descRange.RegisterSpace = 0;
    descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[3].DescriptorTable.pDescriptorRanges = &descRange;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    // Static sampler
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    // Font atlas is a single-channel SDF field (R8). Linear filtering is required for proper SDF
    // reconstruction (smoothstep around the 0.5 edge). Point sampling produces jagged edges.
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 4;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &sampler;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            LOG_ERROR("Root signature serialization failed: {}", (char*)error->GetBufferPointer());
        }
        return false;
    }
    
    hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), 
                                       IID_PPV_ARGS(&m_rootSignature));
    return SUCCEEDED(hr);
}

bool CUIRenderer::CompileShaders() {
    const char* shaderCode = R"(
        cbuffer ScreenConstants : register(b0) {
            float2 screenSize;
            float2 padding;
        };
        
        cbuffer OpacityConstants : register(b1) {
            float opacity;
            float3 padding2;
        };
        
        cbuffer SdfConstants : register(b2) {
            float sdfScale;      // fontSize / SDF_BASE_SIZE
            float debugMode;     // see kSdfDebugMode
            float2 padding3;
        };
        
        struct VSInput {
            float2 position : POSITION;
            float2 uv : TEXCOORD0;
            float4 color : COLOR0;
        };
        
        struct PSInput {
            float4 position : SV_POSITION;
            float2 uv : TEXCOORD0;
            float4 color : COLOR0;
        };
        
        PSInput VSMain(VSInput input) {
            PSInput output;
            float2 pos = input.position;
            pos.x = (pos.x / screenSize.x) * 2.0 - 1.0;
            pos.y = 1.0 - (pos.y / screenSize.y) * 2.0;
            output.position = float4(pos, 0.0, 1.0);
            output.uv = input.uv;
            output.color = input.color;
            return output;
        }
        
        float4 PSMain(PSInput input) : SV_TARGET {
            return input.color * opacity;
        }
        
        Texture2D fontTexture : register(t0);
        SamplerState fontSampler : register(s0);
        
        float4 PSMainTextured(PSInput input) : SV_TARGET {
            // Single-channel SDF atlas in R8_UNORM (0.5 ~= edge).
            // Scale fwidth by sdfScale so smoothing remains correct when glyph geometry is scaled.
            float dist = fontTexture.Sample(fontSampler, input.uv).r;

            if (debugMode > 1.5) {
                return float4(1.0, 0.0, 1.0, 1.0);
            }
            if (debugMode > 0.5) {
                return float4(dist, dist, dist, 1.0);
            }

            float w = max(fwidth(dist) * sdfScale, 0.0005);
            float alpha = smoothstep(0.5 - w, 0.5 + w, dist);
            return float4(input.color.rgb, input.color.a * alpha * opacity);
        }
    )";
    
    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    
    ComPtr<ID3DBlob> error;
    
    HRESULT hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                           "VSMain", "vs_5_0", compileFlags, 0, &m_vertexShader, &error);
    if (FAILED(hr)) {
        if (error) {
            LOG_ERROR("Vertex shader compilation failed: {}", (char*)error->GetBufferPointer());
        }
        return false;
    }
    
    hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                   "PSMain", "ps_5_0", compileFlags, 0, &m_pixelShader, &error);
    if (FAILED(hr)) {
        if (error) {
            LOG_ERROR("Pixel shader compilation failed: {}", (char*)error->GetBufferPointer());
        }
        return false;
    }
    
    hr = D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr,
                   "PSMainTextured", "ps_5_0", compileFlags, 0, &m_pixelShaderTextured, &error);
    if (FAILED(hr)) {
        if (error) {
            LOG_ERROR("Textured pixel shader compilation failed: {}", (char*)error->GetBufferPointer());
        }
        return false;
    }
    
    return true;
}

bool CUIRenderer::CreatePipelineState() {
    // Input layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    
    // Blend state for alpha blending
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    
    // Rasterizer state
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = 0.0f;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    
    // PSO desc for solid rendering
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
    psoDesc.PS = { m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    
    HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create solid PSO: 0x{:x}", hr);
        return false;
    }
    
    // PSO desc for textured rendering (text)
    psoDesc.PS = { m_pixelShaderTextured->GetBufferPointer(), m_pixelShaderTextured->GetBufferSize() };
    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateTextured));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create textured PSO: 0x{:x}", hr);
        return false;
    }
    
    return true;
}

bool CUIRenderer::CreateBuffers() {
    // Create per-frame dynamic vertex buffers to avoid GPU/CPU sync issues
    // UI can generate a lot of vertices, especially for text (6 verts per glyph).
    // Also, we may flush text multiple times per frame when different font sizes are used.
    // Give generous headroom to avoid dropping text.
    const UINT vertexBufferSize = sizeof(UIVertex) * 120000; // Max 120k vertices per frame
    
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = vertexBufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    
    // Create 3 vertex buffers (one per frame in flight)
    for (uint32_t i = 0; i < kFrameCount; ++i) {
        HRESULT hr = m_device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffers[i])
        );
        
        if (FAILED(hr)) {
            LOG_ERROR("Failed to create vertex buffer {}: 0x{:x}", i, hr);
            return false;
        }
        
        m_vertexBufferViews[i].BufferLocation = m_vertexBuffers[i]->GetGPUVirtualAddress();
        m_vertexBufferViews[i].SizeInBytes = vertexBufferSize;
        m_vertexBufferViews[i].StrideInBytes = sizeof(UIVertex);
    }
    
    return true;
}

// ============ DirectWrite Integration ============

bool CUIRenderer::InitializeDirectWrite() {
    HRESULT hr;
    
    // Create DirectWrite factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())
    );
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create DirectWrite factory: 0x{:x}", hr);
        return false;
    }
    
    // Create D2D factory (for future use with text rendering)
    D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    
    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        options,
        m_d2dFactory.GetAddressOf()
    );
    
    if (FAILED(hr)) {
        LOG_WARN("Failed to create D2D factory: 0x{:x}", hr);
        // Continue anyway - we can still use DirectWrite for text measurement
    }
    
    LOG_INFO("DirectWrite initialized successfully");
    return true;
}

void CUIRenderer::ShutdownDirectWrite() {
    m_textFormatCache.clear();
    m_dwriteFactory.Reset();
    m_d2dFactory.Reset();
}

IDWriteTextFormat* CUIRenderer::GetOrCreateTextFormat(const FontInfo& font) {
    // Create cache key
    std::string key = font.family + "_" + std::to_string((int)font.size) + 
                      (font.bold ? "_b" : "") + (font.italic ? "_i" : "");
    
    auto it = m_textFormatCache.find(key);
    if (it != m_textFormatCache.end()) {
        return it->second.Get();
    }
    
    // Create new text format
    ComPtr<IDWriteTextFormat> textFormat;
    std::wstring wfamily = ToWideString(font.family);
    
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        wfamily.c_str(),
        nullptr,
        font.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        font.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font.size,
        L"en-us",
        textFormat.GetAddressOf()
    );
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create text format for font '{}': 0x{:x}", font.family, hr);
        return nullptr;
    }
    
    m_textFormatCache[key] = textFormat;
    return textFormat.Get();
}

std::wstring CUIRenderer::ToWideString(const std::string& str) {
    if (str.empty()) return L"";
    
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

} // namespace Panorama
