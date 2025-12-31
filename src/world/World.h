#pragma once

#include "core/Types.h"
#include "EntityManager.h"
#include "System.h"

#ifdef DIRECTX_RENDERER
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace WorldEditor {

// Forward declarations
class LightingSystem;
class WireframeGrid;
class CreepSystem;
class CollisionSystem;

class RenderSystem : public System {
public:
    RenderSystem(EntityManager& entityManager, ID3D12Device* device);
    ~RenderSystem() override;

    void update(f32 deltaTime) override {}
    String getName() const override { return "RenderSystem"; }

    // Render all entities with Mesh components
    void render(ID3D12GraphicsCommandList* commandList,
                const Mat4& viewProjMatrix,
                const Vec3& cameraPosition,
                bool showPathLines = true);

    // Set lighting system for rendering
    void setLightingSystem(LightingSystem* lightingSystem) { lightingSystem_ = lightingSystem; }
    
    // Set wireframe grid for rendering
    void setWireframeGrid(class WireframeGrid* wireframeGrid) { wireframeGrid_ = wireframeGrid; }
    
    // Set wireframe enabled state
    void setWireframeEnabled(bool enabled) { wireframeEnabled_ = enabled; }
    
    // Path visualization
    void renderPaths(ID3D12GraphicsCommandList* commandList,
                     const Mat4& viewProjMatrix,
                     const Vec3& cameraPosition,
                     bool enabled = true);
    
    // Path rendering resources (temporary buffers for line rendering)
    void createPathBuffers(const Vector<Vec3>& vertices, const Vector<u32>& indices);
    void renderPathLines(ID3D12GraphicsCommandList* commandList,
                        const Mat4& viewProjMatrix,
                        const Vector<Vec3>& vertices,
                        const Vector<u32>& indices);

private:
    EntityManager& entityManager_;
    ID3D12Device* device_;
    LightingSystem* lightingSystem_ = nullptr;
    class WireframeGrid* wireframeGrid_ = nullptr;
    bool wireframeEnabled_ = false;

    // Path rendering resources
#ifdef DIRECTX_RENDERER
    ComPtr<ID3D12Resource> pathVertexBuffer_;
    ComPtr<ID3D12Resource> pathIndexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW pathVertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW pathIndexBufferView_{};
    bool pathBuffersCreated_ = false;
#endif

    // DirectX resources for rendering
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipelineState_;
    ComPtr<ID3DBlob> vertexShader_;
    ComPtr<ID3DBlob> pixelShader_;

    // Helper methods
    void ensureMeshBuffers(Entity entity, MeshComponent& mesh, ID3D12GraphicsCommandList* commandList);
    bool createVertexBuffer(MeshComponent& mesh, ID3D12GraphicsCommandList* commandList);
    bool createIndexBuffer(MeshComponent& mesh, ID3D12GraphicsCommandList* commandList);

    void ensureConstantBuffers(Entity entity, MeshComponent& mesh, const TransformComponent& transform,
                              const Mat4& viewProjMatrix, ID3D12GraphicsCommandList* commandList);
    bool createPerObjectConstantBuffer(MeshComponent& mesh);
    bool createMaterialConstantBuffer(MaterialComponent& material);
    void updatePerObjectConstants(MeshComponent& mesh, const TransformComponent& transform, const Mat4& viewProjMatrix);
    void updateMaterialConstants(MaterialComponent& material);

    void renderMesh(ID3D12GraphicsCommandList* commandList,
                   const MeshComponent& mesh,
                   const TransformComponent& transform,
                   const Mat4& viewProjMatrix);

    bool initializeShaders();
    bool createRootSignature(ID3D12Device* device);
    bool createPipelineState(ID3D12Device* device);
};

class World {
public:
    World();
    World(ID3D12Device* device);
    ~World();

    // Entity management
    Entity createEntity(const String& name = "Entity") {
        return entityManager_.createEntity(name);
    }

    void destroyEntity(Entity entity) {
        entityManager_.destroyEntity(entity);
    }

    bool isValid(Entity entity) const {
        return entityManager_.isValid(entity);
    }

    // Component management (forwarding to EntityManager)
    template<typename Component, typename... Args>
    Component& addComponent(Entity entity, Args&&... args) {
        return entityManager_.addComponent<Component>(entity, std::forward<Args>(args)...);
    }

    template<typename Component>
    void removeComponent(Entity entity) {
        entityManager_.removeComponent<Component>(entity);
    }

    template<typename Component>
    bool hasComponent(Entity entity) const {
        return entityManager_.hasComponent<Component>(entity);
    }

    template<typename Component>
    Component& getComponent(Entity entity) {
        return entityManager_.getComponent<Component>(entity);
    }

    template<typename Component>
    const Component& getComponent(Entity entity) const {
        return entityManager_.getComponent<Component>(entity);
    }

    // System management
    void addSystem(UniquePtr<System> system);
    void removeSystem(const String& name);
    System* getSystem(const String& name);

    // Main loop
    void update(f32 deltaTime, bool gameModeActive = false);
    void render(ID3D12GraphicsCommandList* commandList,
                const Mat4& viewProjMatrix,
                const Vec3& cameraPosition,
                bool showPathLines = true);

    // World operations
    void clear();
    void clearEntities();
    size_t getEntityCount() const;

    // Access to underlying managers
    EntityManager& getEntityManager() { return entityManager_; }
    const EntityManager& getEntityManager() const { return entityManager_; }

private:
    ID3D12Device* device_;
    EntityManager entityManager_;
    Map<String, UniquePtr<System>> systems_;
};

}
