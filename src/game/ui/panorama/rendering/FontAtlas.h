#pragma once

#include "../core/PanoramaTypes.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace Panorama {

// Glyph information in the atlas
struct FontGlyph {
    // UV coordinates in atlas texture (0-1 range)
    f32 u0, v0, u1, v1;
    
    // Glyph metrics
    f32 width, height;      // Size in pixels
    f32 offsetX, offsetY;   // Offset from baseline
    f32 advance;            // Horizontal advance to next character
    
    // Character code
    uint32_t codepoint;
};

// Font atlas containing pre-rendered glyphs
class FontAtlas {
public:
    FontAtlas();
    ~FontAtlas();
    
    // Generate atlas from TTF font
    bool Generate(ID3D12Device* device, ID3D12CommandQueue* commandQueue, 
                  ID3D12GraphicsCommandList* commandList, const std::string& fontPath, 
                  f32 fontSize, bool useSDF = true);
    
    // Generate from system font
    bool GenerateFromSystemFont(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                               ID3D12GraphicsCommandList* commandList, const std::string& fontName, 
                               f32 fontSize, bool useSDF = true);
    
    // Get glyph info for character
    const FontGlyph* GetGlyph(uint32_t codepoint) const;
    
    // Get atlas texture
    ID3D12Resource* GetTexture() const { return m_texture.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return m_srvHandle; }
    void SetSRV(D3D12_GPU_DESCRIPTOR_HANDLE handle) { m_srvHandle = handle; }
    
    // Font metrics
    f32 GetFontSize() const { return m_fontSize; }
    f32 GetLineHeight() const { return m_lineHeight; }
    f32 GetAscent() const { return m_ascent; }
    f32 GetDescent() const { return m_descent; }
    
    // Measure text
    Vector2D MeasureString(const std::string& text) const;
    
private:
    bool GenerateAtlasTexture(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                             ID3D12GraphicsCommandList* commandList,
                             const std::vector<uint8_t>& atlasData, 
                             uint32_t width, uint32_t height);
    bool GenerateSDF(const std::vector<uint8_t>& bitmap, std::vector<uint8_t>& sdf, 
                     uint32_t width, uint32_t height, f32 spread);
    
    ComPtr<ID3D12Resource> m_texture;
    ComPtr<ID3D12Resource> m_uploadBuffer; // Keep upload buffer alive
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvHandle = {};
    
    std::unordered_map<uint32_t, FontGlyph> m_glyphs;
    
    f32 m_fontSize = 16.0f;
    f32 m_lineHeight = 20.0f;
    f32 m_ascent = 14.0f;
    f32 m_descent = 4.0f;
    
    uint32_t m_atlasWidth = 0;
    uint32_t m_atlasHeight = 0;
    bool m_isSDF = false;
};

// Font manager - caches font atlases
class FontManager {
public:
    static FontManager& Instance();
    
    void Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                   ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* srvHeap);
    void Shutdown();
    
    // Get or create font atlas
    FontAtlas* GetFont(const std::string& fontName, f32 fontSize);
    
private:
    FontManager() = default;
    
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
    ID3D12DescriptorHeap* m_srvHeap = nullptr;

    // GPU sync for safe shutdown. Releasing resources still in-flight can crash on exit.
    ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValue = 0;
    void* m_fenceEvent = nullptr; // HANDLE (kept as void* to avoid Windows.h in header)

    uint32_t m_nextSrvIndex = 1; // 0 is reserved for viewport
    uint32_t m_srvDescriptorSize = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvCpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpuStart = {};
    
    std::unordered_map<std::string, std::unique_ptr<FontAtlas>> m_fonts;
    
    std::string MakeFontKey(const std::string& name, f32 size);
};

} // namespace Panorama
