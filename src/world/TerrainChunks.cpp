#include "TerrainChunks.h"
#include "TerrainMesh.h"
#include <algorithm>
#include <cmath>

namespace WorldEditor {

bool TerrainChunks::initializeChunks(TerrainComponent& terrain, MeshComponent& mesh) {
    const int w = terrain.resolution.x;
    const int h = terrain.resolution.y;
    
    // Calculate number of chunks needed
    // CHUNK_SIZE is in cells (quads). The terrain has (w-1)x(h-1) cells.
    const int cellsX = (std::max)(1, w - 1);
    const int cellsY = (std::max)(1, h - 1);
    const int chunksX = (cellsX + CHUNK_SIZE - 1) / CHUNK_SIZE;
    const int chunksY = (cellsY + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    if (chunksX * chunksY > MAX_CHUNKS) {
        // Terrain too large for chunk system, fall back to single mesh
        return false;
    }
    
    // Clear existing chunks
    auto& chunks = getChunks(mesh);
    chunks.clear();
    chunks.reserve(chunksX * chunksY);
    
    // Create chunks
    for (int cy = 0; cy < chunksY; ++cy) {
        for (int cx = 0; cx < chunksX; ++cx) {
            TerrainChunk chunk;
            chunk.chunkCoord = Vec2i(cx, cy);
            // Vertex offset matches cell offset (since chunk includes +1 border vertices).
            chunk.vertexOffset = Vec2i(cx * CHUNK_SIZE, cy * CHUNK_SIZE);
            chunk.isDirty = true; // Initial build needed
            chunk.hasGPUBuffers = false;
            
            chunks.push_back(std::move(chunk));
        }
    }
    
    return true;
}

void TerrainChunks::updateDirtyChunks(TerrainComponent& terrain, MeshComponent& mesh, ID3D12Device* device) {
    auto& chunks = getChunks(mesh);
    
    for (auto& chunk : chunks) {
        if (chunk.isDirty) {
            // Build CPU mesh data
            buildChunkMesh(terrain, chunk);
            
            // Update or create GPU buffers
            if (chunk.hasGPUBuffers) {
                updateChunkGPUBuffers(chunk, device);
            } else {
                createChunkGPUBuffers(chunk, device);
                chunk.hasGPUBuffers = true;
            }
            
            chunk.isDirty = false;
        }
    }
}

void TerrainChunks::markChunksDirty(TerrainComponent& terrain, const Vec2i& minAffected, const Vec2i& maxAffected) {
    // This would need access to chunks, but we don't have MeshComponent here
    // Will be called from TerrainTools with proper parameters
}

Vec2i TerrainChunks::getChunkCoord(const TerrainComponent& terrain, const Vec3& worldPos) {
    const float cellSize = terrain.size / float(terrain.resolution.x - 1);
    const int gridX = static_cast<int>(worldPos.x / cellSize);
    const int gridY = static_cast<int>(worldPos.z / cellSize);
    
    // Chunk size is in cells; clamp to avoid indexing beyond last cell.
    const int cellX = (std::max)(0, (std::min)(terrain.resolution.x - 2, gridX));
    const int cellY = (std::max)(0, (std::min)(terrain.resolution.y - 2, gridY));
    const int chunkX = cellX / CHUNK_SIZE;
    const int chunkY = cellY / CHUNK_SIZE;
    
    return Vec2i(chunkX, chunkY);
}

void TerrainChunks::buildChunkMesh(const TerrainComponent& terrain, TerrainChunk& chunk) {
    const int w = terrain.resolution.x;
    const int h = terrain.resolution.y;
    
    // Calculate chunk bounds
    const int startX = chunk.vertexOffset.x;
    const int startY = chunk.vertexOffset.y;
    // +1 vertex overlap so cells on chunk borders are actually rendered (prevents cracks).
    const int endX = std::min(startX + CHUNK_SIZE + 1, w);
    const int endY = std::min(startY + CHUNK_SIZE + 1, h);
    
    const int chunkW = endX - startX;
    const int chunkH = endY - startY;
    
    // Clear existing data
    chunk.vertices.clear();
    chunk.normals.clear();
    chunk.texCoords.clear();
    chunk.indices.clear();
    
    // Reserve space
    const size_t vertexCount = static_cast<size_t>(chunkW) * static_cast<size_t>(chunkH);
    chunk.vertices.reserve(vertexCount);
    chunk.normals.reserve(vertexCount);
    chunk.texCoords.reserve(vertexCount);
    
    // Build vertices
    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const float u = float(x) / float(w - 1);
            const float v = float(y) / float(h - 1);
            const float worldX = u * terrain.size;
            const float worldZ = v * terrain.size;
            
            // Get height
            float height = 0.0f;
            if (x < w && y < h) {
                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                if (idx < terrain.heightmap.size()) {
                    height = terrain.heightmap[idx];
                }
            }
            
            chunk.vertices.push_back(Vec3(worldX, height, worldZ));
            chunk.texCoords.push_back(Vec2(u, v));
            
            // Calculate normal (simplified)
            Vec3 normal(0.0f, 1.0f, 0.0f);
            if (x > 0 && x < w-1 && y > 0 && y < h-1) {
                const float hL = terrain.heightmap[(y * w) + (x-1)];
                const float hR = terrain.heightmap[(y * w) + (x+1)];
                const float hD = terrain.heightmap[((y-1) * w) + x];
                const float hU = terrain.heightmap[((y+1) * w) + x];
                
                const float dx = terrain.size / float(w - 1);
                const float dz = terrain.size / float(h - 1);
                
                normal.x = -(hR - hL) / (2.0f * dx);
                normal.z = -(hU - hD) / (2.0f * dz);
                normal = glm::normalize(normal);
            }
            chunk.normals.push_back(normal);
        }
    }
    
    // Build indices
    const int quadsX = chunkW - 1;
    const int quadsY = chunkH - 1;
    chunk.indices.reserve(static_cast<size_t>(quadsX) * static_cast<size_t>(quadsY) * 6);
    
    for (int y = 0; y < quadsY; ++y) {
        for (int x = 0; x < quadsX; ++x) {
            const u32 i0 = static_cast<u32>(y * chunkW + x);
            const u32 i1 = static_cast<u32>(y * chunkW + (x + 1));
            const u32 i2 = static_cast<u32>((y + 1) * chunkW + x);
            const u32 i3 = static_cast<u32>((y + 1) * chunkW + (x + 1));
            
            // Two triangles per quad
            chunk.indices.push_back(i0);
            chunk.indices.push_back(i2);
            chunk.indices.push_back(i1);
            
            chunk.indices.push_back(i1);
            chunk.indices.push_back(i2);
            chunk.indices.push_back(i3);
        }
    }
}

Vector<TerrainChunks::TerrainChunk>& TerrainChunks::getChunks(MeshComponent& mesh) {
    // Store chunks in mesh component's name field as a hack for now
    // In real implementation, would extend MeshComponent with chunks field
    static Vector<TerrainChunk> globalChunks;
    return globalChunks;
}

void TerrainChunks::createChunkGPUBuffers(TerrainChunk& chunk, ID3D12Device* device) {
#ifdef DIRECTX_RENDERER
    if (!device || chunk.vertices.empty()) return;
    
    // Create vertex buffer
    const UINT vertexBufferSize = static_cast<UINT>(chunk.vertices.size() * sizeof(MeshVertex));
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = vertexBufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&chunk.vertexBuffer)
    );
    
    if (SUCCEEDED(hr)) {
        // Map and copy vertex data
        void* mappedData = nullptr;
        hr = chunk.vertexBuffer->Map(0, nullptr, &mappedData);
        if (SUCCEEDED(hr)) {
            MeshVertex* vertices = static_cast<MeshVertex*>(mappedData);
            for (size_t i = 0; i < chunk.vertices.size(); ++i) {
                vertices[i].position = chunk.vertices[i];
                vertices[i].normal = chunk.normals[i];
                vertices[i].texCoord = chunk.texCoords[i];
            }
            chunk.vertexBuffer->Unmap(0, nullptr);
            
            // Setup vertex buffer view
            chunk.vertexBufferView.BufferLocation = chunk.vertexBuffer->GetGPUVirtualAddress();
            chunk.vertexBufferView.StrideInBytes = sizeof(MeshVertex);
            chunk.vertexBufferView.SizeInBytes = vertexBufferSize;
        }
    }
    
    // Create index buffer
    if (!chunk.indices.empty()) {
        const UINT indexBufferSize = static_cast<UINT>(chunk.indices.size() * sizeof(u32));
        
        bufferDesc.Width = indexBufferSize;
        hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&chunk.indexBuffer)
        );
        
        if (SUCCEEDED(hr)) {
            void* mappedData = nullptr;
            hr = chunk.indexBuffer->Map(0, nullptr, &mappedData);
            if (SUCCEEDED(hr)) {
                memcpy(mappedData, chunk.indices.data(), indexBufferSize);
                chunk.indexBuffer->Unmap(0, nullptr);
                
                // Setup index buffer view
                chunk.indexBufferView.BufferLocation = chunk.indexBuffer->GetGPUVirtualAddress();
                chunk.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
                chunk.indexBufferView.SizeInBytes = indexBufferSize;
            }
        }
    }
#endif
}

void TerrainChunks::updateChunkGPUBuffers(TerrainChunk& chunk, ID3D12Device* device) {
    (void)device;
#ifdef DIRECTX_RENDERER
    // IMPORTANT: Do NOT recreate buffers every edit. Releasing D3D12 resources while GPU is still using them
    // can trigger device removal / debug-layer exceptions and "soft crashes" on the first sculpt stroke.
    // Chunks have stable topology after initialization, so we can update the upload-heap buffers in-place.
    if (!chunk.vertexBuffer || !chunk.indexBuffer) {
        // Fallback: if something went wrong, recreate once.
        createChunkGPUBuffers(chunk, device);
        return;
    }

    // Update vertex buffer contents
    void* mappedVB = nullptr;
    HRESULT hr = chunk.vertexBuffer->Map(0, nullptr, &mappedVB);
    if (SUCCEEDED(hr) && mappedVB) {
        MeshVertex* vtx = static_cast<MeshVertex*>(mappedVB);
        const size_t n = chunk.vertices.size();
        for (size_t i = 0; i < n; ++i) {
            vtx[i].position = chunk.vertices[i];
            vtx[i].normal = (i < chunk.normals.size()) ? chunk.normals[i] : Vec3(0.0f, 1.0f, 0.0f);
            vtx[i].texCoord = (i < chunk.texCoords.size()) ? chunk.texCoords[i] : Vec2(0.0f, 0.0f);
        }
        chunk.vertexBuffer->Unmap(0, nullptr);
    }

    // Update index buffer contents (should be stable, but keep it safe)
    if (chunk.indexBuffer && !chunk.indices.empty()) {
        void* mappedIB = nullptr;
        hr = chunk.indexBuffer->Map(0, nullptr, &mappedIB);
        if (SUCCEEDED(hr) && mappedIB) {
            const UINT indexBufferSize = static_cast<UINT>(chunk.indices.size() * sizeof(u32));
            memcpy(mappedIB, chunk.indices.data(), indexBufferSize);
            chunk.indexBuffer->Unmap(0, nullptr);
        }
    }
#endif
}

} // namespace WorldEditor