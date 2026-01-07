#include "FontAtlas.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
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
    
    // Generate atlas for common UI ranges.
    // NOTE: The UI uses UTF-8 strings; limiting to 32..255 breaks Cyrillic/Unicode UI text.
    // We pack:
    //  - Latin-1-ish range: 32..255 (matches legacy behavior)
    //  - Cyrillic: U+0400..U+04FF (covers Russian/UA/BY/etc basics, incl. Ё/ё)
    const uint32_t latinFirst = 32;
    const uint32_t latinLast = 255;
    const uint32_t latinCount = latinLast - latinFirst + 1;
    const uint32_t cyrFirst = 0x0400;
    const uint32_t cyrLast = 0x04FF;
    const uint32_t cyrCount = cyrLast - cyrFirst + 1;
    const uint32_t totalGlyphs = latinCount + cyrCount;
    
    // Calculate atlas size (power of 2).
    // With extra Unicode ranges we need a bit more room; for larger font sizes,
    // a 1024 atlas can be borderline and result in many zero-sized packed glyphs.
    uint32_t atlasSize = 512;
    if (totalGlyphs > 256) atlasSize = 1024;
    if (totalGlyphs >= 480) atlasSize = 2048;
    if (fontSize >= 28.0f) atlasSize = std::max(atlasSize, 2048u);
    if (fontSize > 64.0f) atlasSize = 4096;

    std::vector<uint8_t> finalAtlas;
    m_glyphs.clear();

    if (!useSDF) {
        // ===== Bitmap (coverage) atlas =====
        m_isSDF = false;

        std::vector<stbtt_packedchar> packedLatin(latinCount);
        std::vector<stbtt_packedchar> packedCyr(cyrCount);
        std::vector<uint8_t> atlasData(atlasSize * atlasSize);

        stbtt_pack_context packContext;
        if (!stbtt_PackBegin(&packContext, atlasData.data(), atlasSize, atlasSize, 0, 1, nullptr)) {
            LOG_ERROR("Failed to begin font packing");
            return false;
        }

        // Oversampling often produces fractional bearings that can make small glyphs look like
        // they're "wobbling" on Y in pixel-aligned UI. Prefer stable pixel metrics.
        stbtt_PackSetOversampling(&packContext, 1, 1);

        stbtt_pack_range ranges[2]{};
        ranges[0].font_size = fontSize;
        ranges[0].first_unicode_codepoint_in_range = (int)latinFirst;
        ranges[0].num_chars = (int)latinCount;
        ranges[0].chardata_for_range = packedLatin.data();

        ranges[1].font_size = fontSize;
        ranges[1].first_unicode_codepoint_in_range = (int)cyrFirst;
        ranges[1].num_chars = (int)cyrCount;
        ranges[1].chardata_for_range = packedCyr.data();

        if (!stbtt_PackFontRanges(&packContext, fontData.data(), 0, ranges, 2)) {
            LOG_ERROR("Failed to pack font ranges (latin={}, cyrillic={})", (int)latinCount, (int)cyrCount);
            stbtt_PackEnd(&packContext);
            return false;
        }

        stbtt_PackEnd(&packContext);

        m_atlasWidth = atlasSize;
        m_atlasHeight = atlasSize;
        finalAtlas = std::move(atlasData);

        // Create glyph map from stbtt packed ranges
        auto addGlyphs = [&](uint32_t first, const std::vector<stbtt_packedchar>& pcs) {
            for (uint32_t i = 0; i < (uint32_t)pcs.size(); ++i) {
                uint32_t codepoint = first + i;
                const auto& pc = pcs[i];

                FontGlyph glyph;
                const f32 inv = 1.0f / (f32)atlasSize;
                const f32 inset = 0.5f * inv;
                glyph.u0 = (pc.x0 * inv) + inset;
                glyph.v0 = (pc.y0 * inv) + inset;
                glyph.u1 = (pc.x1 * inv) - inset;
                glyph.v1 = (pc.y1 * inv) - inset;
                if (glyph.u1 < glyph.u0) glyph.u1 = glyph.u0;
                if (glyph.v1 < glyph.v0) glyph.v1 = glyph.v0;
                glyph.width = pc.x1 - pc.x0;
                glyph.height = pc.y1 - pc.y0;
                glyph.offsetX = pc.xoff;
                glyph.offsetY = pc.yoff;
                glyph.advance = pc.xadvance;
                glyph.codepoint = codepoint;

                m_glyphs[codepoint] = glyph;
            }
        };

        addGlyphs(latinFirst, packedLatin);
        addGlyphs(cyrFirst, packedCyr);

    } else {
        // ===== Fast SDF atlas (per-glyph SDF via stb_truetype) =====
        // This avoids brute-forcing a full-image distance transform (too slow for 2048^2+).
        m_isSDF = true;

        // SDF parameters (in pixels of the base font size).
        // Larger spread gives smoother scaling but requires more padding and atlas space.
        const float spreadPx = 8.0f;
        const int paddingPx = (int)std::ceil(spreadPx);
        const unsigned char onedge = 128; // maps to 0.5 in UNORM
        const float pixelDistScale = (spreadPx > 0.0f) ? ((float)onedge / spreadPx) : 16.0f;

        struct TempGlyph {
            uint32_t cp = 0;
            int w = 0, h = 0;
            int xoff = 0, yoff = 0;
            float advance = 0.0f;
            std::vector<uint8_t> sdf; // w*h
            int x0 = 0, y0 = 0;       // packed location
        };

        std::vector<TempGlyph> glyphs;
        glyphs.reserve(totalGlyphs);

        auto addCodepoint = [&](uint32_t cp) {
            int advW = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(&fontInfo, (int)cp, &advW, &lsb);
            TempGlyph tg;
            tg.cp = cp;
            tg.advance = advW * scale;

            // Space and control chars often have no visible shape; keep advance only.
            if (cp == (uint32_t)' ' || cp == '\t' || cp == '\n' || cp == '\r') {
                glyphs.push_back(std::move(tg));
                return;
            }

            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char* bmp = stbtt_GetCodepointSDF(
                &fontInfo,
                scale,
                (int)cp,
                paddingPx,
                onedge,
                pixelDistScale,
                &w, &h, &xoff, &yoff
            );

            if (!bmp || w <= 0 || h <= 0) {
                if (bmp) STBTT_free(bmp, nullptr);
                glyphs.push_back(std::move(tg));
                return;
            }

            tg.w = w;
            tg.h = h;
            tg.xoff = xoff;
            tg.yoff = yoff;
            tg.sdf.assign(bmp, bmp + (size_t)w * (size_t)h);
            STBTT_free(bmp, nullptr);
            glyphs.push_back(std::move(tg));
        };

        for (uint32_t cp = latinFirst; cp <= latinLast; ++cp) addCodepoint(cp);
        for (uint32_t cp = cyrFirst; cp <= cyrLast; ++cp) addCodepoint(cp);

        auto tryPack = [&](uint32_t size) -> bool {
            finalAtlas.assign((size_t)size * (size_t)size, 0);
            int x = 1, y = 1;
            int rowH = 0;
            const int gap = 1;

            for (auto& g : glyphs) {
                if (g.sdf.empty()) continue;
                if (x + g.w + gap >= (int)size) {
                    x = 1;
                    y += rowH + gap;
                    rowH = 0;
                }
                if (y + g.h + gap >= (int)size) {
                    return false;
                }

                g.x0 = x;
                g.y0 = y;

                // Copy rows
                for (int yy = 0; yy < g.h; ++yy) {
                    uint8_t* dst = finalAtlas.data() + (size_t)(y + yy) * (size_t)size + (size_t)x;
                    const uint8_t* src = g.sdf.data() + (size_t)yy * (size_t)g.w;
                    memcpy(dst, src, (size_t)g.w);
                }

                x += g.w + gap;
                rowH = std::max(rowH, g.h);
            }
            return true;
        };

        // Try requested size, then grow once if needed.
        if (!tryPack(atlasSize)) {
            if (atlasSize < 4096) {
                LOG_WARN("SDF atlas pack overflow at {}x{}; retrying with 4096x4096", atlasSize, atlasSize);
                atlasSize = 4096;
                if (!tryPack(atlasSize)) {
                    LOG_ERROR("Failed to pack SDF atlas even at 4096x4096");
                    return false;
                }
            } else {
                LOG_ERROR("Failed to pack SDF atlas at {}x{}", atlasSize, atlasSize);
                return false;
            }
        }

        m_atlasWidth = atlasSize;
        m_atlasHeight = atlasSize;

        // Build glyph map from packed temp glyphs
        const f32 inv = 1.0f / (f32)atlasSize;
        const f32 inset = 0.5f * inv;
        for (const auto& tg : glyphs) {
            FontGlyph glyph{};
            glyph.codepoint = tg.cp;
            glyph.advance = tg.advance;

            if (!tg.sdf.empty() && tg.w > 0 && tg.h > 0) {
                glyph.u0 = (tg.x0 * inv) + inset;
                glyph.v0 = (tg.y0 * inv) + inset;
                glyph.u1 = ((tg.x0 + tg.w) * inv) - inset;
                glyph.v1 = ((tg.y0 + tg.h) * inv) - inset;
                if (glyph.u1 < glyph.u0) glyph.u1 = glyph.u0;
                if (glyph.v1 < glyph.v0) glyph.v1 = glyph.v0;

                glyph.width = (f32)tg.w;
                glyph.height = (f32)tg.h;
                glyph.offsetX = (f32)tg.xoff;
                glyph.offsetY = (f32)tg.yoff;
            } else {
                // No bitmap: keep advance only.
                glyph.u0 = glyph.v0 = glyph.u1 = glyph.v1 = 0.0f;
                glyph.width = glyph.height = 0.0f;
                glyph.offsetX = glyph.offsetY = 0.0f;
            }

            m_glyphs[glyph.codepoint] = glyph;
        }
    }

    // Debug stats to diagnose "invisible text" issues.
    // If max is 0, the atlas has no ink; if avg is ~255, the atlas is likely inverted/filled.
    {
        uint8_t minV = 255, maxV = 0;
        uint64_t sum = 0;
        uint32_t nonZero = 0;
        for (uint8_t v : finalAtlas) {
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
            sum += v;
            if (v != 0) nonZero++;
        }
        const double avg = finalAtlas.empty() ? 0.0 : (double)sum / (double)finalAtlas.size();
        const double pct = finalAtlas.empty() ? 0.0 : (100.0 * (double)nonZero / (double)finalAtlas.size());
        // LOG_INFO("Font atlas stats: min={} max={} avg={:.2f} nonZero={:.2f}%", (int)minV, (int)maxV, avg, pct);
    }
    
    // Sanity-check: if common glyphs ended up with 0 size, packing likely failed or atlas is too small.
    auto checkGlyph = [&](uint32_t cp, const char* name) {
        auto it = m_glyphs.find(cp);
        if (it == m_glyphs.end()) {
            LOG_WARN("Font atlas sanity: missing glyph '{}' (U+{:04X})", name, cp);
            return;
        }
        const auto& g = it->second;
        if (g.width <= 0 || g.height <= 0) {
            LOG_ERROR("Font atlas sanity: glyph '{}' packed with zero size (w={}, h={}, u0={}, v0={}, u1={}, v1={})",
                name, g.width, g.height, g.u0, g.v0, g.u1, g.v1);
        }
    };
    checkGlyph((uint32_t)'A', "A");
    checkGlyph((uint32_t)'W', "W");
    
    // Upload to GPU
    if (!GenerateAtlasTexture(device, commandQueue, commandList, finalAtlas, atlasSize, atlasSize)) {
        LOG_ERROR("Failed to create atlas texture");
        return false;
    }
    
    // LOG_INFO("Font atlas generated: {} glyphs (latin={}, cyrillic={}), {}x{}, SDF={}",
    //          totalGlyphs, latinCount, cyrCount, atlasSize, atlasSize, useSDF);
    
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

    auto nextCodepoint = [](const std::string& s, size_t& i) -> uint32_t {
        const size_t n = s.size();
        if (i >= n) return 0;
        const uint8_t c0 = static_cast<uint8_t>(s[i]);
        if (c0 < 0x80) { i += 1; return c0; }
        if ((c0 & 0xE0) == 0xC0 && i + 1 < n) {
            const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
            if ((c1 & 0xC0) == 0x80) { i += 2; return ((c0 & 0x1F) << 6) | (c1 & 0x3F); }
        }
        if ((c0 & 0xF0) == 0xE0 && i + 2 < n) {
            const uint8_t c1 = static_cast<uint8_t>(s[i + 1]);
            const uint8_t c2 = static_cast<uint8_t>(s[i + 2]);
            if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80)) {
                i += 3;
                return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            }
        }
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

    size_t i = 0;
    while (i < text.size()) {
        const uint32_t cp = nextCodepoint(text, i);
        if (cp == '\r') continue;
        if (cp == '\n') {
            maxWidth = std::max(maxWidth, lineWidth);
            lineWidth = 0.0f;
            height += m_lineHeight;
            continue;
        }

        const FontGlyph* glyph = GetGlyph(cp);
        if (glyph) {
            if (cp == '\t') lineWidth += glyph->advance * 4.0f;
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
                                       fontName, fontSize, true)) {
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
