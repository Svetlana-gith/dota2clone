#pragma once
/**
 * CUIRenderer - DirectX 12 Renderer for Panorama UI
 * Handles all 2D rendering with support for Valve-style effects
 * Uses DirectX 12 command lists for rendering
 */

#include "PanoramaTypes.h"
#include "CStyleSheet.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dwrite.h>
#include <d2d1.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace Panorama {

// ============ Render Command Types ============

enum class RenderCommandType {
    DrawRect,
    DrawRectOutline,
    DrawRoundedRect,
    DrawText,
    DrawImage,
    DrawLine,
    DrawCircle,
    DrawArc,
    DrawGradient,
    SetClipRect,
    PopClipRect,
    SetBlur,
    SetSaturation,
    SetOpacity,
    PushTransform,
    PopTransform
};

struct RenderCommand {
    RenderCommandType type;
    Rect2D rect;
    Color color;
    Color color2;  // For gradients
    f32 param1 = 0;
    f32 param2 = 0;
    f32 param3 = 0;
    f32 param4 = 0;
    std::string text;
    std::string texturePath;
    HorizontalAlign textAlign = HorizontalAlign::Left;
    VerticalAlign verticalAlign = VerticalAlign::Top;
    bool bold = false;
};

// ============ Font Info ============

struct FontInfo {
    // Default Panorama font (project-provided).
    std::string family = "Roboto Condensed";
    // Default font size in pixels (should match CStyleManager default).
    f32 size = 16.0f;
    bool bold = false;
    bool italic = false;
    // Extra spacing between glyphs (pixels). Applied between characters on the same line.
    f32 letterSpacing = 0.0f;
};

// ============ UI Renderer ============

class CUIRenderer {
public:
    CUIRenderer();
    ~CUIRenderer();
    
    // Initialization
    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                   ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* srvHeap,
                   f32 width, f32 height);
    void Shutdown();
    
    // Frame management
    void BeginFrame();
    void EndFrame();
    void Flush();
    
    // Screen size
    void SetScreenSize(f32 width, f32 height);
    f32 GetScreenWidth() const { return m_screenWidth; }
    f32 GetScreenHeight() const { return m_screenHeight; }
    
    // ============ Drawing Primitives ============
    
    // Rectangles
    void DrawRect(const Rect2D& rect, const Color& color);
    void DrawRectOutline(const Rect2D& rect, const Color& color, f32 thickness = 1.0f);
    void DrawRoundedRect(const Rect2D& rect, f32 radius, const Color& color);
    void DrawRoundedRect(const Rect2D& rect, const Color& color, f32 radius = 0);  // Overload
    void DrawRoundedRect(const Rect2D& rect, const Color& color, f32 topLeft, f32 topRight, f32 bottomRight, f32 bottomLeft);
    
    // Gradients (Valve style)
    void DrawGradientRect(const Rect2D& rect, const Color& startColor, const Color& endColor, bool vertical = true);
    void DrawRadialGradient(const Rect2D& rect, const Color& centerColor, const Color& edgeColor);
    
    // Text
    void DrawText(const std::string& text, const Rect2D& bounds, const Color& color, const FontInfo& font = {}, HorizontalAlign hAlign = HorizontalAlign::Left, VerticalAlign vAlign = VerticalAlign::Top);
    Vector2D MeasureText(const std::string& text, const FontInfo& font = {});
    
    // Images
    void DrawImage(const std::string& path, const Rect2D& rect, f32 opacity = 1.0f);
    void DrawImageTinted(const std::string& path, const Rect2D& rect, const Color& tint);
    void DrawImageRegion(const std::string& path, const Rect2D& destRect, const Rect2D& srcRect);
    
    // Box shadow
    void DrawBoxShadow(const Rect2D& rect, const Color& color, f32 offsetX, f32 offsetY, f32 blur, f32 spread, bool inset = false);
    
    // Lines and shapes
    void DrawLine(f32 x1, f32 y1, f32 x2, f32 y2, const Color& color, f32 thickness = 1.0f);
    void DrawCircle(f32 x, f32 y, f32 radius, const Color& color, bool filled = true);
    
    // ============ Clipping ============
    void PushClipRect(const Rect2D& rect);
    void PopClipRect();
    
    // ============ Transforms ============
    struct Transform2D {
        f32 translateX = 0, translateY = 0;
        f32 rotation = 0;
        f32 scaleX = 1, scaleY = 1;
        f32 originX = 0, originY = 0;
    };
    
    void PushTransform(const Transform2D& transform);
    void PopTransform();
    void Translate(f32 x, f32 y);
    void Rotate(f32 angle);
    void Scale(f32 sx, f32 sy);
    void SetTransformOrigin(f32 x, f32 y);
    
    // ============ Effects ============
    void SetOpacity(f32 opacity);
    void SetBlur(f32 amount);
    void SetSaturation(f32 amount);
    void SetBrightness(f32 amount);
    void SetContrast(f32 amount);
    void SetWashColor(const Color& color);
    void ClearEffects();
    
    // ============ Clipping ============
    void SetClipEnabled(bool enabled);
    
    // ============ Text Measurement ============
    f32 MeasureTextWidth(const std::string& text, const FontInfo& font = {});
    f32 MeasureTextHeight(const FontInfo& font = {});
    
private:
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
    ID3D12DescriptorHeap* m_srvHeap = nullptr;
    
    f32 m_screenWidth = 1920.0f;
    f32 m_screenHeight = 1080.0f;
    
    // Debug
    int m_frameCount = 0;
    
    // Clip stack
    std::vector<Rect2D> m_clipStack;
    bool m_clipEnabled = true;
    
    // Transform stack
    std::vector<Transform2D> m_transformStack;
    
    // Current effects
    f32 m_currentOpacity = 1.0f;
    f32 m_currentBlur = 0.0f;
    f32 m_currentSaturation = 1.0f;
    f32 m_currentBrightness = 1.0f;
    f32 m_currentContrast = 1.0f;
    Color m_currentWashColor = Color::Transparent();
    
    // Render commands queue
    std::vector<RenderCommand> m_renderCommands;
    
    // Vertex batching
    struct UIVertex {
        f32 x, y;
        f32 u, v;
        f32 r, g, b, a;
    };
    std::vector<UIVertex> m_vertices;
    std::vector<UIVertex> m_textVertices; // Separate batch for text
    std::vector<uint16_t> m_indices;

    // Text upload cursor (in vertices) within the per-frame dynamic vertex buffer.
    // We use the second half of the buffer for text. Multiple text flushes can occur per frame
    // (e.g. different font sizes); this cursor prevents overwriting earlier batches before GPU executes.
    size_t m_textUploadCursorVertices = 0;
    
    // Texture cache
    std::unordered_map<std::string, void*> m_textureCache;
    
    // Helper methods
    Vector2D TransformPoint(f32 x, f32 y) const;
    void AddQuad(const Rect2D& rect, const Color& color, f32 u0 = 0, f32 v0 = 0, f32 u1 = 1, f32 v1 = 1);
    void FlushBatch();
    void FlushTextBatch(); // For textured text rendering
    void UpdateScissorRect();
    void ClearTextureCache();
    
    // DX12 resource creation
    bool CreateRootSignature();
    bool CompileShaders();
    bool CreatePipelineState();
    bool CreateBuffers();
    
    // DX12 resources
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12PipelineState> m_pipelineStateTextured; // For text rendering
    ComPtr<ID3DBlob> m_vertexShader;
    ComPtr<ID3DBlob> m_pixelShader;
    ComPtr<ID3DBlob> m_pixelShaderTextured; // For text rendering
    
    // Per-frame vertex buffers to avoid GPU/CPU sync issues
    static constexpr uint32_t kFrameCount = 3;
    ComPtr<ID3D12Resource> m_vertexBuffers[kFrameCount];
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferViews[kFrameCount] = {};
    uint32_t m_currentFrameIndex = 0;
    
    // DirectWrite resources
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<ID2D1Factory> m_d2dFactory;
    std::unordered_map<std::string, ComPtr<IDWriteTextFormat>> m_textFormatCache;
    
    // Font atlas for text rendering (Dota 2 style)
    class FontAtlas* m_currentFont = nullptr;
    std::string m_currentFontFamily;
    f32 m_currentFontSize = 0.0f;
    
    // Text rendering helpers
    bool InitializeDirectWrite();
    void ShutdownDirectWrite();
    IDWriteTextFormat* GetOrCreateTextFormat(const FontInfo& font);
    std::wstring ToWideString(const std::string& str);
};

} // namespace Panorama
