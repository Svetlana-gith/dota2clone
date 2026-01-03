#include "FontAtlas.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <Windows.h>

// Undefine Windows macros
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// stb_truetype for font loading
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

// Helper function for texture upload
inline UINT64 GetRequiredIntermediateSize(ID3D12Resource* pDestinationResource, UINT FirstSubresource, UINT NumSubresources) {
    D3D12_RESOURCE_DESC desc = pDestinationResource->GetDesc();
    UINT64 requiredSize = 0;
    ID3D12Device* pDevice = nullptr;
    pDestinationResource->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
    pDevice->GetCopyableFootprints(&desc, FirstSubresource, NumSubresources, 0, nullptr, nullptr, nullptr, &requiredSize);
    pDevice->Release();
    return requiredSize;
}

namespace Panorama {

static std::string ResolveRobotoCondensedPath() {
    namespace fs = std::filesystem;

    auto tryPath = [](const fs::path& p) -> std::string {
        std::error_code ec;
        if (!p.empty() && fs::exists(p, ec) && !ec) {
            return p.u8string();
        }
        return {};
    };

    // 1) Workspace-relative (when cwd is repo root)
    if (auto p = tryPath(fs::current_path() / "src" / "fonts" / "Roboto Condensed" / "RobotoCondensed.ttf"); !p.empty()) {
        return p;
    }

    // 2) Relative to executable directory (when running from build/bin/Debug)
    char exePathA[MAX_PATH]{};
    DWORD len = GetModuleFileNameA(nullptr, exePathA, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        fs::path exeDir = fs::path(exePathA).parent_path();
        // build/bin/Debug -> ../../../src/...
        if (auto p = tryPath(exeDir / ".." / ".." / ".." / "src" / "fonts" / "Roboto Condensed" / "RobotoCondensed.ttf"); !p.empty()) {
            return p;
        }
        // Some launchers set a different working dir; try one more level.
        if (auto p = tryPath(exeDir / ".." / ".." / ".." / ".." / "src" / "fonts" / "Roboto Condensed" / "RobotoCondensed.ttf"); !p.empty()) {
            return p;
        }
    }

    return {};
}

FontAtlas::FontAtlas() = default;
FontAtlas::~FontAtlas() = default;

bool FontAtlas::GenerateFromSystemFont(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                                      ID3D12GraphicsCommandList* commandList, 
                                      const std::string& fontName, f32 fontSize, bool useSDF) {
    namespace fs = std::filesystem;

    // If caller passed a path directly, use it.
    {
        std::error_code ec;
        fs::path p(fontName);
        if (fs::exists(p, ec) && !ec) {
            return Generate(device, commandQueue, commandList, p.u8string(), fontSize, useSDF);
        }
    }

    // Project-provided font aliasing
    std::string fontPath;
    if (fontName == "Roboto Condensed" || fontName == "RobotoCondensed" || fontName == "Radiance") {
        fontPath = ResolveRobotoCondensedPath();
        if (fontPath.empty()) {
            LOG_WARN("Requested font '{}' but RobotoCondensed.ttf not found; falling back to Segoe UI", fontName);
        }
    }

    // Fallback: use a default system font path
    // TODO: Query system fonts properly
    if (fontPath.empty()) {
        fontPath = "C:/Windows/Fonts/segoeui.ttf"; // Segoe UI
    }
    
    if (fontName == "Arial") {
        fontPath = "C:/Windows/Fonts/arial.ttf";
    } else if (fontName == "Consolas") {
        fontPath = "C:/Windows/Fonts/consola.ttf";
    }
    
    return Generate(device, commandQueue, commandList, fontPath, fontSize, useSDF);
}

bool FontAtlas::Generate(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                        ID3D12GraphicsCommandList* commandList,
                        const std::string& fontPath, f32 fontSize, bool useSDF) {
    m_fontSize = fontSize;
    m_isSDF = useSDF;
    
    // Load font file
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open font file: {}", fontPath);
        return false;
    }
    
    size_t fileSize = file.tellg();
    file.seekg(0);
    
    std::vector<uint8_t> fontData(fileSize);
    file.read(reinterpret_cast<char*>(fontData.data()), fileSize);
    file.close();
    
    // Initialize stb_truetype
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontData.data(), 0)) {
        LOG_ERROR("Failed to initialize font: {}", fontPath);
        return false;
    }
    
    // Calculate scale for desired font size
    f32 scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);
    
    // Get font metrics
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    m_ascent = ascent * scale;
    m_descent = -descent * scale;
    m_lineHeight = (ascent - descent + lineGap) * scale;
    
    // Generate atlas for ASCII characters (32-126) + some extended
    const uint32_t firstChar = 32;
    const uint32_t lastChar = 255;
    const uint32_t numChars = lastChar - firstChar + 1;
    
    // Calculate atlas size (power of 2)
    uint32_t atlasSize = 512;
    if (fontSize > 32) atlasSize = 1024;
    if (fontSize > 64) atlasSize = 2048;
    
    m_atlasWidth = atlasSize;
    m_atlasHeight = atlasSize;
    
    // Pack glyphs into atlas
    std::vector<stbtt_packedchar> packedChars(numChars);
    std::vector<uint8_t> atlasData(atlasSize * atlasSize);
    
    stbtt_pack_context packContext;
    if (!stbtt_PackBegin(&packContext, atlasData.data(), atlasSize, atlasSize, 0, 1, nullptr)) {
        LOG_ERROR("Failed to begin font packing");
        return false;
    }
    
    // Oversampling often produces fractional bearings that can make small glyphs look like
    // they're "wobbling" on Y in pixel-aligned UI. Prefer stable pixel metrics.
    stbtt_PackSetOversampling(&packContext, 1, 1);
    
    if (!stbtt_PackFontRange(&packContext, fontData.data(), 0, fontSize, 
                             firstChar, numChars, packedChars.data())) {
        LOG_ERROR("Failed to pack font range");
        stbtt_PackEnd(&packContext);
        return false;
    }
    
    stbtt_PackEnd(&packContext);
    
    // Convert to SDF if requested
    std::vector<uint8_t> finalAtlas;
    if (useSDF) {
        finalAtlas.resize(atlasSize * atlasSize);
        GenerateSDF(atlasData, finalAtlas, atlasSize, atlasSize, 8.0f);
    } else {
        finalAtlas = std::move(atlasData);
    }
    
    // Create glyph map
    for (uint32_t i = 0; i < numChars; ++i) {
        uint32_t codepoint = firstChar + i;
        const auto& pc = packedChars[i];
        
        FontGlyph glyph;
        glyph.u0 = pc.x0 / (f32)atlasSize;
        glyph.v0 = pc.y0 / (f32)atlasSize;
        glyph.u1 = pc.x1 / (f32)atlasSize;
        glyph.v1 = pc.y1 / (f32)atlasSize;
        glyph.width = pc.x1 - pc.x0;
        glyph.height = pc.y1 - pc.y0;
        // Keep subpixel offsets from stb_truetype.
        // Snapping each glyph offset individually can introduce 1px vertical wobble
        // (looks like every character has its own "top"). We instead snap the line
        // baseline in the renderer and keep per-glyph offsets fractional.
        glyph.offsetX = pc.xoff;
        glyph.offsetY = pc.yoff;
        glyph.advance = pc.xadvance;
        glyph.codepoint = codepoint;
        
        m_glyphs[codepoint] = glyph;
    }
    
    // Upload to GPU
    if (!GenerateAtlasTexture(device, commandQueue, commandList, finalAtlas, atlasSize, atlasSize)) {
        LOG_ERROR("Failed to create atlas texture");
        return false;
    }
    
    LOG_INFO("Font atlas generated: {} glyphs, {}x{}, SDF={}", 
             numChars, atlasSize, atlasSize, useSDF);
    
    return true;
}

bool FontAtlas::GenerateSDF(const std::vector<uint8_t>& bitmap, std::vector<uint8_t>& sdf,
                            uint32_t width, uint32_t height, f32 spread) {
    // Simple SDF generation using brute force distance calculation
    // For production, use msdfgen or similar library
    
    sdf.resize(width * height);
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t idx = y * width + x;
            uint8_t value = bitmap[idx];
            
            // Find distance to nearest edge
            f32 minDist = spread;
            bool inside = value > 127;
            
            // Search in a radius around this pixel
            int searchRadius = (int)spread + 1;
            for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
                for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
                    int nx = (int)x + dx;
                    int ny = (int)y + dy;
                    
                    if (nx < 0 || nx >= (int)width || ny < 0 || ny >= (int)height) continue;
                    
                    uint8_t nvalue = bitmap[ny * width + nx];
                    bool ninside = nvalue > 127;
                    
                    if (inside != ninside) {
                        f32 dist = std::sqrt((f32)(dx * dx + dy * dy));
                        minDist = std::min(minDist, dist);
                    }
                }
            }
            
            // Normalize distance to 0-1 range
            f32 normalizedDist = minDist / spread;
            if (!inside) normalizedDist = -normalizedDist;
            
            // Map to 0-255 range (127 = edge)
            f32 sdfValue = (normalizedDist + 1.0f) * 0.5f * 255.0f;
            sdfValue = std::max(0.0f, std::min(255.0f, sdfValue));
            sdf[idx] = (uint8_t)sdfValue;
        }
    }
    
    return true;
}

bool FontAtlas::GenerateAtlasTexture(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                                     ID3D12GraphicsCommandList* commandList,
                                     const std::vector<uint8_t>& atlasData,
                                     uint32_t width, uint32_t height) {
    // Create texture
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_texture)
    );
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create font atlas texture: 0x{:x}", hr);
        return false;
    }
    
    // Create upload buffer
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
    
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Width = uploadBufferSize;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    
    hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_uploadBuffer)
    );
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create upload buffer: 0x{:x}", hr);
        return false;
    }
    
    // Prepare subresource data and upload
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    UINT numRows;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;
    
    D3D12_RESOURCE_DESC texDescForFootprint = m_texture->GetDesc();
    device->GetCopyableFootprints(&texDescForFootprint, 0, 1, 0, &layout, &numRows, &rowSizeInBytes, &totalBytes);
    
    // Map upload buffer and copy data
    void* pData = nullptr;
    hr = m_uploadBuffer->Map(0, nullptr, &pData);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to map upload buffer: 0x{:x}", hr);
        return false;
    }
    
    // Copy row by row
    BYTE* pDestData = reinterpret_cast<BYTE*>(pData) + layout.Offset;
    const BYTE* pSrcData = atlasData.data();
    for (UINT y = 0; y < height; ++y) {
        memcpy(pDestData + layout.Footprint.RowPitch * y, pSrcData + width * y, width);
    }
    
    m_uploadBuffer->Unmap(0, nullptr);
    
    // Copy from upload buffer to texture
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = m_texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = m_uploadBuffer.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = layout;
    
    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    
    // Transition to shader resource
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
    
    // Note: We don't close/execute the command list here
    // The caller is responsible for managing command list lifecycle
    // The upload buffer will be kept alive until the next frame
    
    LOG_INFO("Font atlas texture uploaded to GPU: {}x{}", width, height);
    return true;
}

const FontGlyph* FontAtlas::GetGlyph(uint32_t codepoint) const {
    auto it = m_glyphs.find(codepoint);
    if (it != m_glyphs.end()) {
        return &it->second;
    }
    
    // Fallback to space character
    it = m_glyphs.find(32);
    if (it != m_glyphs.end()) {
        return &it->second;
    }
    
    return nullptr;
}

Vector2D FontAtlas::MeasureString(const std::string& text) const {
    f32 maxWidth = 0.0f;
    f32 lineWidth = 0.0f;
    f32 height = m_lineHeight;
    
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') {
            maxWidth = std::max(maxWidth, lineWidth);
            lineWidth = 0.0f;
            height += m_lineHeight;
            continue;
        }
        const uint32_t codepoint = static_cast<uint32_t>(static_cast<unsigned char>(c));
        const FontGlyph* glyph = GetGlyph(codepoint);
        if (glyph) {
            if (c == '\t') lineWidth += glyph->advance * 4.0f;
            else lineWidth += glyph->advance;
        }
    }
    
    maxWidth = std::max(maxWidth, lineWidth);
    return {maxWidth, height};
}

// ============ Font Manager ============

FontManager& FontManager::Instance() {
    static FontManager instance;
    return instance;
}

void FontManager::Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
                            ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* srvHeap) {
    m_device = device;
    m_commandQueue = commandQueue;
    m_commandList = commandList;
    m_srvHeap = srvHeap;

    // Create a fence for shutdown synchronization.
    // Without waiting for the GPU, releasing font textures/upload buffers at exit can crash.
    if (m_device) {
        if (!m_fence) {
            HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
            if (SUCCEEDED(hr)) {
                m_fenceValue = 0;
                if (!m_fenceEvent) {
                    m_fenceEvent = (void*)CreateEvent(nullptr, FALSE, FALSE, nullptr);
                }
            } else {
                m_fence.Reset();
            }
        }
    }
    
    if (m_device && m_srvHeap) {
        m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_srvCpuStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        m_srvGpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    } else {
        m_srvDescriptorSize = 0;
        m_srvCpuStart = {};
        m_srvGpuStart = {};
    }
    
    // Pre-generate common fonts
    // Temporarily disabled until we have proper command list management
    // GetFont("Segoe UI", 16.0f);
    // GetFont("Segoe UI", 20.0f);
    // GetFont("Segoe UI", 24.0f);
    
    LOG_INFO("FontManager initialized");
}

void FontManager::Shutdown() {
    // Ensure the GPU is idle before releasing atlas resources (textures/upload buffers).
    if (m_commandQueue && m_fence && m_fenceEvent) {
        const uint64_t value = ++m_fenceValue;
        if (SUCCEEDED(m_commandQueue->Signal(m_fence.Get(), value))) {
            if (m_fence->GetCompletedValue() < value) {
                m_fence->SetEventOnCompletion(value, (HANDLE)m_fenceEvent);
                WaitForSingleObject((HANDLE)m_fenceEvent, INFINITE);
            }
        }
    }

    m_fonts.clear();
    m_device = nullptr;
    m_commandQueue = nullptr;
    m_commandList = nullptr;
    m_srvHeap = nullptr;

    if (m_fenceEvent) {
        CloseHandle((HANDLE)m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    m_fence.Reset();
    m_fenceValue = 0;
}

FontAtlas* FontManager::GetFont(const std::string& fontName, f32 fontSize) {
    std::string key = MakeFontKey(fontName, fontSize);
    
    auto it = m_fonts.find(key);
    if (it != m_fonts.end()) {
        return it->second.get();
    }
    
    if (!m_device || !m_commandQueue || !m_commandList || !m_srvHeap || m_srvDescriptorSize == 0) {
        LOG_ERROR("FontManager not initialized with valid DX12 objects (device/queue/list/srvHeap)");
        return nullptr;
    }
    
    // Create new font atlas
    auto atlas = std::make_unique<FontAtlas>();
    if (!atlas->GenerateFromSystemFont(m_device, m_commandQueue, m_commandList, 
                                       fontName, fontSize, false)) {
        LOG_ERROR("Failed to generate font atlas for {} {}", fontName, fontSize);
        return nullptr;
    }
    
    // Allocate SRV descriptor slot (0 reserved for viewport texture in DirectXRenderer)
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = m_srvHeap->GetDesc();
    if (m_nextSrvIndex >= heapDesc.NumDescriptors) {
        LOG_ERROR("SRV heap is full (need {}, have {})", m_nextSrvIndex + 1, heapDesc.NumDescriptors);
        return nullptr;
    }
    
    const uint32_t srvIndex = m_nextSrvIndex++;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srvCpuStart;
    cpuHandle.ptr += static_cast<SIZE_T>(srvIndex) * static_cast<SIZE_T>(m_srvDescriptorSize);
    
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvGpuStart;
    gpuHandle.ptr += static_cast<UINT64>(srvIndex) * static_cast<UINT64>(m_srvDescriptorSize);
    
    // Create SRV for font atlas texture (R8_UNORM)
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(atlas->GetTexture(), &srvDesc, cpuHandle);
    
    atlas->SetSRV(gpuHandle);
    LOG_INFO("Font atlas SRV created: '{}' size={} -> srvIndex={}", fontName, fontSize, srvIndex);
    
    FontAtlas* ptr = atlas.get();
    m_fonts[key] = std::move(atlas);
    
    return ptr;
}

std::string FontManager::MakeFontKey(const std::string& name, f32 size) {
    const int px = (int)std::lround(size);
    return name + "_" + std::to_string(px);
}

} // namespace Panorama
