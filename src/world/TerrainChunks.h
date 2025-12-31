#pragma once

#include "Components.h"
#include "core/Types.h"

namespace WorldEditor {

// Chunk-based terrain system for stable GPU performance
class TerrainChunks {
public:
    // Chunk size in CELLS (quads), not vertices.
    // Each chunk will contain (CHUNK_SIZE + 1) vertices in X/Y so borders overlap and there are no cracks.
    static constexpr int CHUNK_SIZE = 64;  // 64x64 quads per chunk => 65x65 vertices per chunk
    static constexpr int MAX_CHUNKS = 16;  // 4x4 grid of chunks
    
    struct TerrainChunk {
        Vec2i chunkCoord;           // Chunk coordinates in grid
        Vec2i vertexOffset;         // Offset in global heightmap
        bool isDirty = false;       // Needs GPU buffer update
        bool hasGPUBuffers = false; // GPU buffers created
        
        // CPU mesh data
        Vector<Vec3> vertices;
        Vector<Vec3> normals;
        Vector<Vec2> texCoords;
        Vector<u32> indices;
        
#ifdef DIRECTX_RENDERER
        // GPU buffers (reused, not recreated)
        ComPtr<ID3D12Resource> vertexBuffer;
        ComPtr<ID3D12Resource> indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        D3D12_INDEX_BUFFER_VIEW indexBufferView;
#endif
    };
    
    // Initialize chunk system for terrain
    static bool initializeChunks(TerrainComponent& terrain, MeshComponent& mesh);
    
    // Update only dirty chunks (much more efficient)
    static void updateDirtyChunks(TerrainComponent& terrain, MeshComponent& mesh, ID3D12Device* device);
    
    // Mark chunks as dirty in affected area
    static void markChunksDirty(TerrainComponent& terrain, const Vec2i& minAffected, const Vec2i& maxAffected);
    
    // Build mesh data for specific chunk
    static void buildChunkMesh(const TerrainComponent& terrain, TerrainChunk& chunk);
    
    // Get chunk containing world position
    static Vec2i getChunkCoord(const TerrainComponent& terrain, const Vec3& worldPos);
    
    // Get chunks storage from MeshComponent
    static Vector<TerrainChunk>& getChunks(MeshComponent& mesh);
    
private:
    static void createChunkGPUBuffers(TerrainChunk& chunk, ID3D12Device* device);
    static void updateChunkGPUBuffers(TerrainChunk& chunk, ID3D12Device* device);
};

} // namespace WorldEditor