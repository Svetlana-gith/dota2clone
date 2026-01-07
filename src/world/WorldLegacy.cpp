#include "WorldLegacy.h"
#include "CreepSystem.h"
#include "ProjectileSystem.h"
#include "TowerSystem.h"
#include "CreepSpawnSystem.h"
#include "CollisionSystem.h"
#include "HeroSystem.h"
#include "../renderer/LightingSystem.h"
#include "../renderer/WireframeGrid.h"
#include "TerrainChunks.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <iostream>
#include <map>
#include <memory>
#include <algorithm>

namespace WorldEditor {

WorldLegacy::WorldLegacy(ID3D12Device* device) : device_(device) {
    // Set world reference in entity manager
    // entityManager_.setWorld(this); // WorldLegacy is deprecated
    
    // Add render system
    addSystem(std::make_unique<RenderSystem>(entityManager_, device_));
    
    // Add collision system
    {
        auto collisionSystem = std::make_unique<CollisionSystem>(entityManager_);
        System* systemPtr = collisionSystem.release();
        addSystem(UniquePtr<System>(systemPtr));
    }
    
    // Initialize core MOBA systems
    addSystem(std::make_unique<CreepSystem>(entityManager_));
    addSystem(std::make_unique<ProjectileSystem>(entityManager_));
    addSystem(std::make_unique<TowerSystem>(entityManager_));
    addSystem(std::make_unique<CreepSpawnSystem>(entityManager_));
    
    // Add Hero System
    auto heroSystem = std::make_unique<HeroSystem>(entityManager_);
    // heroSystem->setWorld(this); // WorldLegacy is deprecated
    addSystem(std::move(heroSystem));
    
    LOG_INFO("World initialized");
}

WorldLegacy::WorldLegacy() : device_(nullptr) {
    // Set world reference in entity manager
    // entityManager_.setWorld(this); // WorldLegacy is deprecated
    
    // Initialize core MOBA systems without DirectX device
    addSystem(std::make_unique<CreepSystem>(entityManager_));
    addSystem(std::make_unique<ProjectileSystem>(entityManager_));
    addSystem(std::make_unique<TowerSystem>(entityManager_));
    addSystem(std::make_unique<CreepSpawnSystem>(entityManager_));
    
    // Add Hero System
    auto heroSystem = std::make_unique<HeroSystem>(entityManager_);
    // heroSystem->setWorld(this); // WorldLegacy is deprecated
    addSystem(std::move(heroSystem));
    
    LOG_INFO("World initialized (device not available yet)");
}

WorldLegacy::~WorldLegacy() {
    LOG_INFO("World destroyed");
}

void WorldLegacy::addSystem(UniquePtr<System> system) {
    if (!system) return;

    String name = system->getName();
    if (systems_.find(name) != systems_.end()) {
        LOG_WARNING("System '{}' already exists, replacing", name);
    }

    systems_[name] = std::move(system);
    LOG_INFO("Added system: {}", name);
}

void WorldLegacy::removeSystem(const String& name) {
    auto it = systems_.find(name);
    if (it != systems_.end()) {
        systems_.erase(it);
        LOG_INFO("Removed system: {}", name);
    }
}

System* WorldLegacy::getSystem(const String& name) {
    auto it = systems_.find(name);
    return (it != systems_.end()) ? it->second.get() : nullptr;
}

const System* WorldLegacy::getSystem(const String& name) const {
    auto it = systems_.find(name);
    return (it != systems_.end()) ? it->second.get() : nullptr;
}

void WorldLegacy::update(f32 deltaTime, bool gameModeActive) {
    for (auto& pair : systems_) {
        // Only update game systems (like CreepSystem) when game mode is active
        String systemName = pair.second->getName();
        if (systemName == "CreepSystem" && !gameModeActive) {
            continue; // Skip CreepSystem when not in game mode
        }
        pair.second->update(deltaTime);
    }
}

void WorldLegacy::render(ID3D12GraphicsCommandList* commandList,
                   const Mat4& viewProjMatrix,
                   const Vec3& cameraPosition,
                   bool showPathLines) {
    // Get render system and delegate rendering
    RenderSystem* renderSystem = static_cast<RenderSystem*>(getSystem("RenderSystem"));
    if (renderSystem) {
        renderSystem->render(commandList, viewProjMatrix, cameraPosition, showPathLines);
    } else {
        LOG_ERROR("RenderSystem not found");
    }
}

void WorldLegacy::clear() {
    systems_.clear();
    entityManager_.clear();
    LOG_INFO("World cleared");
}

void WorldLegacy::clearEntities() {
    entityManager_.clear();
    LOG_INFO("World entities cleared");
}

size_t WorldLegacy::getEntityCount() const {
    return entityManager_.getEntityCount();
}

// RenderSystem implementation

RenderSystem::RenderSystem(EntityManager& entityManager, ID3D12Device* device)
    : entityManager_(entityManager), device_(device) {
    LOG_INFO("RenderSystem initialized with device: {}", device != nullptr ? "valid" : "null");

    if (device_) {
        // Initialize DirectX resources
        if (!initializeShaders()) {
            LOG_ERROR("Failed to initialize shaders");
        }

        if (!createRootSignature(device_)) {
            LOG_ERROR("Failed to create root signature");
        }

        if (!createPipelineState(device_)) {
            LOG_ERROR("Failed to create pipeline state");
        }

        LOG_INFO("RenderSystem DirectX resources initialized successfully");
    } else {
        LOG_INFO("RenderSystem initialized without device - DirectX resources not created");
    }
}

RenderSystem::~RenderSystem() {
    LOG_INFO("RenderSystem destroyed");
}

void RenderSystem::render(ID3D12GraphicsCommandList* commandList,
                         const Mat4& viewProjMatrix,
                         const Vec3& cameraPosition,
                         bool showPathLines) {
    // TODO: Implement proper mesh buffer creation and management
    // For now, render a simple hardcoded cube as demonstration

    if (!commandList) {
        LOG_ERROR("RenderSystem::render called with null command list");
        return;
    }

    if (!device_) {
        LOG_ERROR("RenderSystem::render called without a valid D3D12 device");
        return;
    }

    // Lazy init (should happen once). Keep it deterministic.
    if (!rootSignature_ || !pipelineState_) {
        const bool shadersOk = vertexShader_ && pixelShader_ ? true : initializeShaders();
        const bool rsOk = rootSignature_ ? true : createRootSignature(device_);
        const bool psoOk = pipelineState_ ? true : createPipelineState(device_);
        if (!(shadersOk && rsOk && psoOk)) {
            LOG_ERROR("RenderSystem pipeline not ready (shaders={}, rootsig={}, pso={})",
                      shadersOk, rsOk, psoOk);
            return;
        }
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Bind lighting constant buffer (register b1)
    if (lightingSystem_ && lightingSystem_->getLightingConstantBuffer()) {
        commandList->SetGraphicsRootConstantBufferView(1, 
            lightingSystem_->getLightingConstantBuffer()->GetGPUVirtualAddress());
    }

    // Render all entities with Mesh and Transform components
    size_t renderedCount = 0;
    entityManager_.forEach<MeshComponent, TransformComponent>(
        [this, commandList, &viewProjMatrix, &renderedCount](Entity entity,
                                           MeshComponent& mesh,
                                           TransformComponent& transform) {
            if (mesh.visible) {
                ensureMeshBuffers(entity, mesh, commandList);
                ensureConstantBuffers(entity, mesh, transform, viewProjMatrix, commandList);
                renderMesh(commandList, mesh, transform, viewProjMatrix);
                renderedCount++;
            }
        }
    );
    
    // Render wireframe grid for terrain entities if enabled
    if (wireframeEnabled_ && wireframeGrid_) {
        // Find first terrain entity and render wireframe grid once
        bool wireframeRendered = false;
        entityManager_.forEach<TerrainComponent, MeshComponent>(
            [this, commandList, &viewProjMatrix, &cameraPosition, &wireframeRendered](Entity entity,
                                               TerrainComponent& terrain,
                                               MeshComponent& mesh) {
                if (mesh.visible && !wireframeRendered) {
                    // Terrain mesh is transformed by TransformComponent during normal rendering; apply the same transform to wireframe grid.
                    const Mat4 worldMatrix = entityManager_.hasComponent<TransformComponent>(entity)
                        ? entityManager_.getComponent<TransformComponent>(entity).getMatrix()
                        : Mat4(1.0f);
                    wireframeGrid_->render(commandList, worldMatrix, viewProjMatrix, cameraPosition);
                    wireframeRendered = true;
                }
            }
        );
    }
    
    // Render path visualization if enabled
    renderPaths(commandList, viewProjMatrix, cameraPosition, showPathLines);
}

void RenderSystem::renderPaths(ID3D12GraphicsCommandList* commandList,
                               const Mat4& viewProjMatrix,
                               const Vec3& cameraPosition,
                               bool enabled) {
    if (!enabled || !commandList || !wireframeGrid_) {
        return;
    }
    
    // Get CreepSystem to build paths
    // For now, we'll build paths directly here
    auto& reg = entityManager_.getRegistry();
    
    // Find all waypoints and group by team/lane
    struct WaypointInfo {
        Entity entity;
        Vec3 position;
        i32 order;
        i32 teamId;
        i32 lane;
    };
    Vector<WaypointInfo> waypoints;
    
    auto waypointView = reg.view<ObjectComponent, TransformComponent>();
    for (auto entity : waypointView) {
        const auto& objComp = waypointView.get<ObjectComponent>(entity);
        const auto& transform = waypointView.get<TransformComponent>(entity);
        
        if (objComp.type == ObjectType::Waypoint) {
            waypoints.push_back({
                entity,
                transform.position,
                objComp.waypointOrder,
                objComp.teamId,
                objComp.waypointLane
            });
        }
    }
    
    // Group by team and lane, sort by order
    struct PathKey {
        i32 teamId;
        i32 lane;
        bool operator<(const PathKey& other) const {
            if (teamId != other.teamId) return teamId < other.teamId;
            return lane < other.lane;
        }
    };
    std::map<PathKey, Vector<Vec3>> paths;
    
    for (const auto& wp : waypoints) {
        PathKey key = {wp.teamId, wp.lane};
        if (paths.find(key) == paths.end()) {
            paths[key] = Vector<Vec3>();
        }
        paths[key].push_back(wp.position);
    }
    
    // Sort each path by order (we need to keep order info)
    for (auto& [key, path] : paths) {
        // Re-sort based on waypoint order
        Vector<std::pair<Vec3, i32>> sortedPath;
        for (const auto& wp : waypoints) {
            if (wp.teamId == key.teamId && wp.lane == key.lane) {
                sortedPath.push_back({wp.position, wp.order});
            }
        }
        std::sort(sortedPath.begin(), sortedPath.end(),
                 [](const std::pair<Vec3, i32>& a, const std::pair<Vec3, i32>& b) {
                     return a.second < b.second;
                 });
        path.clear();
        for (const auto& [pos, order] : sortedPath) {
            path.push_back(pos);
        }
    }
    
    // Render paths as lines between waypoints
    // We'll use a simple approach: render lines directly using the same pipeline as WireframeGrid
    if (paths.empty()) {
        return;
    }
    
    // Use wireframe grid's pipeline for rendering lines
    // We need to create temporary geometry for paths
    // For now, we'll render paths using a simple line list
    
    // Build line geometry for all paths
    Vector<Vec3> pathVertices;
    Vector<u32> pathIndices;
    
    u32 vertexOffset = 0;
    for (const auto& [key, path] : paths) {
        if (path.size() < 2) continue; // Need at least 2 points for a line
        
        // Add vertices for this path
        for (const auto& pos : path) {
            pathVertices.push_back(pos);
        }
        
        // Add indices for lines (connect consecutive waypoints)
        for (size_t i = 0; i < path.size() - 1; ++i) {
            pathIndices.push_back(static_cast<u32>(vertexOffset + i));
            pathIndices.push_back(static_cast<u32>(vertexOffset + i + 1));
        }
        
        vertexOffset += static_cast<u32>(path.size());
    }
    
    if (pathVertices.empty() || pathIndices.empty()) {
        return;
    }
    
    // Use wireframe grid's rendering pipeline
    // We'll need to create temporary buffers or use a different approach
    // For now, we'll use the wireframe grid's system by temporarily replacing its geometry
    // This is a simplified approach - in a full implementation, we'd have a separate path renderer
    
    // Note: This is a placeholder - full implementation would create proper GPU buffers
    // and use the wireframe pipeline to render the lines
}

bool RenderSystem::initializeShaders() {
    // Terrain vertex shader with lighting support
    const char* vertexShaderCode = R"(
        cbuffer PerObjectConstants : register(b0) {
            float4x4 worldMatrix;
            float4x4 viewProjMatrix;
        };

        cbuffer LightingConstants : register(b1) {
            float4 lightDirection;
            float4 lightColor;
            float4 ambientColor;
            float4 cameraPosition;
            float4 materialParams;
        };

        struct VSInput {
            float3 position : POSITION;
            float3 normal : NORMAL;
            float2 texCoord : TEXCOORD0;
        };

        struct VSOutput {
            float4 position : SV_POSITION;
            float3 worldPos : WORLD_POSITION;
            float3 normal : NORMAL;
            float2 texCoord : TEXCOORD0;
            float3 viewDir : VIEW_DIR;
        };

        VSOutput main(VSInput input) {
            VSOutput output;
            
            // Transform to world space
            float4 worldPos = mul(worldMatrix, float4(input.position, 1.0f));
            output.worldPos = worldPos.xyz;
            
            // Transform to clip space
            output.position = mul(viewProjMatrix, worldPos);
            
            // Transform normal to world space
            output.normal = normalize(mul((float3x3)worldMatrix, input.normal));
            
            // Pass through texture coordinates
            output.texCoord = input.texCoord;
            
            // Calculate view direction
            output.viewDir = normalize(cameraPosition.xyz - worldPos.xyz);
            
            return output;
        }
    )";

    // Terrain pixel shader with Phong lighting
    const char* pixelShaderCode = R"(
        cbuffer LightingConstants : register(b1) {
            float4 lightDirection;
            float4 lightColor;
            float4 ambientColor;
            float4 cameraPosition;
            float4 materialParams;
        };

        cbuffer MaterialConstants : register(b2) {
            float4 baseColor_metallic;
            float4 emissive_roughness;
        };

        struct PSInput {
            float4 position : SV_POSITION;
            float3 worldPos : WORLD_POSITION;
            float3 normal : NORMAL;
            float2 texCoord : TEXCOORD0;
            float3 viewDir : VIEW_DIR;
        };

        float4 main(PSInput input) : SV_TARGET {
            // Normalize interpolated vectors
            float3 normal = normalize(input.normal);
            float3 viewDir = normalize(input.viewDir);
            float3 lightDir = normalize(-lightDirection.xyz);
            
            // Material properties
            float3 baseColor = baseColor_metallic.rgb;

            // Editor-only checker terrain style (Unreal-like viewport).
            // Apply checkerboard only to terrain (detected by green color ~0.25, 0.6, 0.25).
            // materialParams.w: checker cell size in world units; 0 disables.
            float cellSize = materialParams.w;
            if (cellSize > 0.0f) {
                // Check if this is terrain material (green color indicates terrain)
                float3 terrainColor = float3(0.25, 0.6, 0.25);
                float colorDiff = length(baseColor - terrainColor);
                // If color is close to terrain color (within 0.3), apply checkerboard
                if (colorDiff < 0.3f) {
                    float2 c = floor(input.worldPos.xz / cellSize);
                    float checker = fmod(c.x + c.y, 2.0);
                    float3 lightGray = float3(0.45, 0.45, 0.45);
                    float3 darkGray  = float3(0.32, 0.32, 0.32);
                    baseColor = lerp(lightGray, darkGray, checker);
                }
            }
            float diffuseStrength = materialParams.x;
            float specularStrength = materialParams.y;
            float shininess = materialParams.z;
            
            // Ambient lighting (always present)
            float3 ambient = ambientColor.rgb * baseColor;
            
            // Diffuse lighting (Lambert)
            float NdotL = max(dot(normal, lightDir), 0.0);
            float3 diffuse = lightColor.rgb * baseColor * NdotL * diffuseStrength;
            
            // Specular lighting (Phong)
            float3 reflectDir = reflect(-lightDir, normal);
            float RdotV = max(dot(reflectDir, viewDir), 0.0);
            float spec = pow(RdotV, shininess);
            float3 specular = lightColor.rgb * spec * specularStrength;
            
            // Combine lighting components
            float3 finalColor = ambient + diffuse + specular;
            
            // Add slight emissive for better visibility
            finalColor += emissive_roughness.rgb * 0.1;
            
            // Simple gamma correction
            finalColor = pow(finalColor, 1.0/2.2);
            
            return float4(finalColor, 1.0);
        }
    )";

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Compile vertex shader
    ComPtr<ID3DBlob> vertexShaderError;
    HRESULT hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr,
                           "main", "vs_5_0", compileFlags, 0, &vertexShader_, &vertexShaderError);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to compile vertex shader");
        if (vertexShaderError) {
            LOG_ERROR("Vertex shader error: {}", (char*)vertexShaderError->GetBufferPointer());
        }
        return false;
    }

    // Compile pixel shader
    ComPtr<ID3DBlob> pixelShaderError;
    hr = D3DCompile(pixelShaderCode, strlen(pixelShaderCode), nullptr, nullptr, nullptr,
                   "main", "ps_5_0", compileFlags, 0, &pixelShader_, &pixelShaderError);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to compile pixel shader");
        if (pixelShaderError) {
            LOG_ERROR("Pixel shader error: {}", (char*)pixelShaderError->GetBufferPointer());
        }
        return false;
    }

    return true;
}

bool RenderSystem::createRootSignature(ID3D12Device* device) {
    // Create root signature for mesh rendering with lighting (no shadows for now)
    // Three constant buffers: per-object, lighting, and material
    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    // Per-object constant buffer (world matrix, view-proj matrix) - register(b0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Lighting constant buffer (light direction, colors, camera pos) - register(b1)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    rootParameters[1].Descriptor.RegisterSpace = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Material constant buffer (base color, metallic, roughness, emissive) - register(b2)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 2;
    rootParameters[2].Descriptor.RegisterSpace = 0;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Root signature description
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = _countof(rootParameters);
    rootSigDesc.pParameters = rootParameters;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to serialize root signature");
        if (error) {
            LOG_ERROR("Root signature error: {}", (char*)error->GetBufferPointer());
        }
        return false;
    }

    hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                   IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create root signature");
        return false;
    }

    return true;
}

bool RenderSystem::createPipelineState(ID3D12Device* device) {
    // Input layout for mesh vertices
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Pipeline state description
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = { vertexShader_->GetBufferPointer(), vertexShader_->GetBufferSize() };
    psoDesc.PS = { pixelShader_->GetBufferPointer(), pixelShader_->GetBufferSize() };

    // Rasterizer state
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    // Use NONE culling for objects to see both sides (objects are small and should be visible from all angles)
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Blend state
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
        psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    // Depth stencil state (enabled): required for terrain (many triangles) to render correctly.
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // Use LESS instead of LESS_EQUAL for better depth precision
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create pipeline state");
        return false;
    }

    return true;
}

// Mesh buffer management
void RenderSystem::ensureMeshBuffers(Entity entity, MeshComponent& mesh, ID3D12GraphicsCommandList* commandList) {
#ifdef DIRECTX_RENDERER
    // Check if buffers are already created
    if (mesh.gpuBuffersCreated) {
        return; // Buffers already exist
    }

    LOG_DEBUG("Creating GPU buffers for mesh entity {} ({} vertices, {} indices)",
              static_cast<u32>(entity), mesh.getVertexCount(), mesh.getIndexCount());

    // Validate mesh data
    if (mesh.vertices.empty()) {
        LOG_ERROR("Mesh entity {} has no vertices", static_cast<u32>(entity));
        return;
    }

    if (mesh.indices.empty()) {
        LOG_ERROR("Mesh entity {} has no indices", static_cast<u32>(entity));
        return;
    }

    // Create vertex and index buffers
    if (!createVertexBuffer(mesh, commandList)) {
        LOG_ERROR("Failed to create vertex buffer for mesh entity {}", static_cast<u32>(entity));
        return;
    }

    if (!createIndexBuffer(mesh, commandList)) {
        LOG_ERROR("Failed to create index buffer for mesh entity {}", static_cast<u32>(entity));
        return;
    }

    mesh.gpuBuffersCreated = true;
    LOG_DEBUG("GPU buffers created successfully for mesh entity {}", static_cast<u32>(entity));

#endif
}

bool RenderSystem::createVertexBuffer(MeshComponent& mesh, ID3D12GraphicsCommandList* commandList) {
#ifdef DIRECTX_RENDERER
    if (!device_) {
        LOG_ERROR("Device not available for vertex buffer creation");
        return false;
    }

    const size_t vertexCount = mesh.getVertexCount();
    const UINT vertexBufferSize = static_cast<UINT>(vertexCount * sizeof(MeshVertex));

    // Create vertex buffer in DEFAULT heap
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = vertexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                IID_PPV_ARGS(&mesh.vertexBuffer));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return false;
    }

    // Create upload buffer
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    hr = device_->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                        IID_PPV_ARGS(&mesh.vertexBufferUpload));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create vertex upload buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return false;
    }

    // Fill upload buffer with vertex data
    std::vector<MeshVertex> gpuVertices;
    gpuVertices.reserve(vertexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        MeshVertex vertex;
        vertex.position = mesh.vertices[i];
        vertex.normal = (i < mesh.normals.size()) ? mesh.normals[i] : Vec3(0, 1, 0);
        vertex.texCoord = (i < mesh.texCoords.size()) ? mesh.texCoords[i] : Vec2(0, 0);
        gpuVertices.push_back(vertex);
    }

    // Copy data to upload buffer
    D3D12_RANGE readRange = { 0, 0 };
    void* pData;
    hr = mesh.vertexBufferUpload->Map(0, &readRange, &pData);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to map vertex upload buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return false;
    }

    memcpy(pData, gpuVertices.data(), vertexBufferSize);
    mesh.vertexBufferUpload->Unmap(0, nullptr);

    // Copy from upload buffer to GPU buffer
    commandList->CopyResource(mesh.vertexBuffer.Get(), mesh.vertexBufferUpload.Get());

    // Transition to vertex buffer state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = mesh.vertexBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    // Create vertex buffer view
    mesh.vertexBufferView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
    mesh.vertexBufferView.StrideInBytes = sizeof(MeshVertex);
    mesh.vertexBufferView.SizeInBytes = vertexBufferSize;

    // Release upload buffer after copy (it will be released when ComPtr goes out of scope)
    // mesh.vertexBufferUpload.Reset(); // Keep for now, will be released later

    return true;
#endif
    return false;
}

bool RenderSystem::createIndexBuffer(MeshComponent& mesh, ID3D12GraphicsCommandList* commandList) {
#ifdef DIRECTX_RENDERER
    if (!device_) {
        LOG_ERROR("Device not available for index buffer creation");
        return false;
    }

    const size_t indexCount = mesh.getIndexCount();
    const UINT indexBufferSize = static_cast<UINT>(indexCount * sizeof(uint32_t));

    // Create index buffer in DEFAULT heap
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = indexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                IID_PPV_ARGS(&mesh.indexBuffer));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create index buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return false;
    }

    // Create upload buffer
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    hr = device_->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                        IID_PPV_ARGS(&mesh.indexBufferUpload));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create index upload buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return false;
    }

    // Copy data to upload buffer
    D3D12_RANGE readRange = { 0, 0 };
    void* pData;
    hr = mesh.indexBufferUpload->Map(0, &readRange, &pData);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to map index upload buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return false;
    }

    memcpy(pData, mesh.indices.data(), indexBufferSize);
    mesh.indexBufferUpload->Unmap(0, nullptr);

    // Copy from upload buffer to GPU buffer
    commandList->CopyResource(mesh.indexBuffer.Get(), mesh.indexBufferUpload.Get());

    // Transition to index buffer state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = mesh.indexBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    // Create index buffer view
    mesh.indexBufferView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
    mesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    mesh.indexBufferView.SizeInBytes = indexBufferSize;

    return true;
#endif
    return false;
}

void RenderSystem::renderMesh(ID3D12GraphicsCommandList* commandList,
                             const MeshComponent& mesh,
                             const TransformComponent& transform,
                             const Mat4& viewProjMatrix) {
#ifdef DIRECTX_RENDERER
    // Check if this mesh uses chunk system
    auto& chunks = TerrainChunks::getChunks(const_cast<MeshComponent&>(mesh));
    
    if (!chunks.empty()) {
        // Render using chunk system - inline implementation
        if (!mesh.gpuConstantBuffersCreated) {
            LOG_DEBUG("Mesh constant buffers not ready for chunk rendering");
            return;
        }

        // Per-object constants (b0) - same for all chunks
        if (mesh.perObjectConstantBuffer) {
            D3D12_GPU_VIRTUAL_ADDRESS perObjectCBVAddress = mesh.perObjectConstantBuffer->GetGPUVirtualAddress();
            commandList->SetGraphicsRootConstantBufferView(0, perObjectCBVAddress);
        }

        // Material constants (b2) - same for all chunks
        if (mesh.materialEntity != INVALID_ENTITY &&
            entityManager_.isValid(mesh.materialEntity) &&
            entityManager_.hasComponent<MaterialComponent>(mesh.materialEntity)) {
            auto& material = entityManager_.getComponent<MaterialComponent>(mesh.materialEntity);
            if (material.constantBuffer) {
                D3D12_GPU_VIRTUAL_ADDRESS materialCBVAddress = material.constantBuffer->GetGPUVirtualAddress();
                commandList->SetGraphicsRootConstantBufferView(2, materialCBVAddress);
            }
        }

        // Render each chunk that has GPU buffers
        size_t chunksRendered = 0;
        for (const auto& chunk : chunks) {
            if (chunk.hasGPUBuffers && chunk.vertexBuffer && chunk.indexBuffer && !chunk.indices.empty()) {
                // Set vertex buffer for this chunk
                commandList->IASetVertexBuffers(0, 1, &chunk.vertexBufferView);
                
                // Set index buffer for this chunk
                commandList->IASetIndexBuffer(&chunk.indexBufferView);
                
                // Draw this chunk
                const UINT indexCount = static_cast<UINT>(chunk.indices.size());
                commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
                
                chunksRendered++;
            }
        }

        LOG_DEBUG("Rendered {} chunks out of {} total chunks", chunksRendered, chunks.size());
        return;
    }
    
    // Fall back to traditional mesh rendering
    if (!mesh.gpuBuffersCreated || !mesh.gpuConstantBuffersCreated) {
        LOG_DEBUG("Mesh or constant buffers not ready for rendering");
        return; // Buffers not ready
    }

    // Per-object constants (b0)
    if (mesh.perObjectConstantBuffer) {
        D3D12_GPU_VIRTUAL_ADDRESS perObjectCBVAddress = mesh.perObjectConstantBuffer->GetGPUVirtualAddress();
        commandList->SetGraphicsRootConstantBufferView(0, perObjectCBVAddress);
    }

    // Material constants (b2) - updated for new lighting system
    if (mesh.materialEntity != INVALID_ENTITY &&
        entityManager_.isValid(mesh.materialEntity) &&
        entityManager_.hasComponent<MaterialComponent>(mesh.materialEntity)) {
        auto& material = entityManager_.getComponent<MaterialComponent>(mesh.materialEntity);
        if (!material.gpuBufferCreated) {
            material.gpuBufferCreated = createMaterialConstantBuffer(material);
        } else {
            // For now update every frame; later gate on a dirty flag.
            updateMaterialConstants(material);
        }
        if (material.constantBuffer) {
            D3D12_GPU_VIRTUAL_ADDRESS materialCBVAddress = material.constantBuffer->GetGPUVirtualAddress();
            commandList->SetGraphicsRootConstantBufferView(2, materialCBVAddress); // Changed from 1 to 2
        }
    }

    // Set vertex buffer
    commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);

    // Set index buffer
    commandList->IASetIndexBuffer(&mesh.indexBufferView);

    // Draw the mesh
    const UINT indexCount = static_cast<UINT>(mesh.getIndexCount());
    commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);

    LOG_DEBUG("Rendered mesh with {} vertices, {} indices",
             mesh.getVertexCount(), mesh.getIndexCount());
#endif
}

// Constant buffer management
void RenderSystem::ensureConstantBuffers(Entity entity, MeshComponent& mesh, const TransformComponent& transform,
                                       const Mat4& viewProjMatrix, ID3D12GraphicsCommandList* commandList) {
#ifdef DIRECTX_RENDERER
    // Check if constant buffers are already created
    if (mesh.gpuConstantBuffersCreated) {
        // Update constants if transform changed (for now, update every frame)
        updatePerObjectConstants(mesh, transform, viewProjMatrix);
        return;
    }

    LOG_DEBUG("Creating constant buffers for mesh entity {}", static_cast<u32>(entity));

    // Create per-object constant buffer
    if (!createPerObjectConstantBuffer(mesh)) {
        LOG_ERROR("Failed to create per-object constant buffer for mesh entity {}", static_cast<u32>(entity));
        return;
    }

    // Update constants with initial values
    updatePerObjectConstants(mesh, transform, viewProjMatrix);

    // Handle material constant buffer if material exists
    if (mesh.materialEntity != INVALID_ENTITY) {
        if (entityManager_.isValid(mesh.materialEntity) &&
            entityManager_.hasComponent<MaterialComponent>(mesh.materialEntity)) {
            auto& material = entityManager_.getComponent<MaterialComponent>(mesh.materialEntity);
            if (!material.gpuBufferCreated) {
                material.gpuBufferCreated = createMaterialConstantBuffer(material);
                if (!material.gpuBufferCreated) {
                    LOG_ERROR("Failed to create material constant buffer for entity {}", static_cast<u32>(mesh.materialEntity));
                }
            } else {
                updateMaterialConstants(material);
            }
        }
    }

    mesh.gpuConstantBuffersCreated = true;
    LOG_DEBUG("Constant buffers created successfully for mesh entity {}", static_cast<u32>(entity));

#endif
}

bool RenderSystem::createPerObjectConstantBuffer(MeshComponent& mesh) {
#ifdef DIRECTX_RENDERER
    if (!device_) {
        LOG_ERROR("Device not available for per-object constant buffer creation");
        return false;
    }

    const UINT bufferSize = sizeof(PerObjectConstants);

    // For editor use-cases, keep constant buffers in UPLOAD heap for simplicity.
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                IID_PPV_ARGS(&mesh.perObjectConstantBuffer));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create per-object constant buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return false;
    }

    // Backward-compatible: alias upload pointer to the same resource.
    mesh.perObjectConstantBufferUpload = mesh.perObjectConstantBuffer;

    return true;
#endif
    return false;
}

bool RenderSystem::createMaterialConstantBuffer(MaterialComponent& material) {
#ifdef DIRECTX_RENDERER
    if (!device_) {
        LOG_ERROR("Device not available for material constant buffer creation");
        return false;
    }

    const UINT bufferSize = sizeof(MaterialConstants);

    // For editor use-cases, keep constant buffers in UPLOAD heap for simplicity.
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                IID_PPV_ARGS(&material.constantBuffer));
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create material constant buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return false;
    }

    // Backward-compatible: alias upload pointer to the same resource.
    material.constantBufferUpload = material.constantBuffer;

    // Update material constants
    updateMaterialConstants(material);

    return true;
#endif
    return false;
}

void RenderSystem::updatePerObjectConstants(MeshComponent& mesh, const TransformComponent& transform, const Mat4& viewProjMatrix) {
#ifdef DIRECTX_RENDERER
    if (!mesh.perObjectConstantBuffer) {
        return; // Buffers not created yet
    }

    PerObjectConstants constants = {};
    constants.worldMatrix = transform.getMatrix();
    constants.viewProjMatrix = viewProjMatrix;
    // Padding is already zero-initialized

    // Copy to upload buffer
    D3D12_RANGE readRange = { 0, 0 };
    void* pData;
    HRESULT hr = mesh.perObjectConstantBuffer->Map(0, &readRange, &pData);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to map per-object constant upload buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return;
    }

    memcpy(pData, &constants, sizeof(PerObjectConstants));
    mesh.perObjectConstantBuffer->Unmap(0, nullptr);
#endif
}

void RenderSystem::updateMaterialConstants(MaterialComponent& material) {
#ifdef DIRECTX_RENDERER
    if (!material.constantBuffer) {
        return; // Buffer not created yet
    }

    MaterialConstants constants = {};
    constants.baseColor_metallic = Vec4(material.baseColor, material.metallic);
    constants.emissive_roughness = Vec4(material.emissiveColor, material.roughness);
    // Padding is already zero-initialized

    // Copy to upload buffer
    D3D12_RANGE readRange = { 0, 0 };
    void* pData;
    HRESULT hr = material.constantBuffer->Map(0, &readRange, &pData);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to map material constant upload buffer: HRESULT 0x{:X}", static_cast<unsigned>(hr));
        return;
    }

    memcpy(pData, &constants, sizeof(MaterialConstants));
    material.constantBuffer->Unmap(0, nullptr);

    // Note: CopyResource should be done via command list
#endif
}

void RenderSystem::renderPathLines(ID3D12GraphicsCommandList* commandList,
                                   const Mat4& viewProjMatrix,
                                   const Vector<Vec3>& vertices,
                                   const Vector<u32>& indices) {
#ifdef DIRECTX_RENDERER
    if (!commandList || !wireframeGrid_ || !device_ || vertices.empty() || indices.empty()) {
        return;
    }
    
    // Check if wireframe grid pipeline is ready
    if (!wireframeGrid_->isPipelineReady()) {
        return;
    }
    
    // Create or update path buffers
    createPathBuffers(vertices, indices);
    if (!pathBuffersCreated_) {
        return;
    }
    
    // Use wireframe grid's pipeline for rendering
    ID3D12RootSignature* rootSig = wireframeGrid_->getRootSignature();
    ID3D12PipelineState* pso = wireframeGrid_->getPipelineState();
    
    if (!rootSig || !pso) {
        return;
    }
    
    // Set pipeline state
    commandList->SetPipelineState(pso);
    commandList->SetGraphicsRootSignature(rootSig);
    
    // Create constant buffer for view/proj matrix
    struct PathConstants {
        Mat4 worldMatrix;
        Mat4 viewProjMatrix;
        Vec3 cameraPosition;
        f32 padding;
    };
    
    PathConstants constants;
    constants.worldMatrix = Mat4(1.0f); // Identity for paths
    constants.viewProjMatrix = viewProjMatrix;
    constants.cameraPosition = Vec3(0.0f, 0.0f, 0.0f);
    
    // Update constant buffer (we'll use a simple upload buffer approach)
    const UINT constantBufferSize = (sizeof(PathConstants) + 255) & ~255; // Align to 256 bytes
    
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = constantBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    ComPtr<ID3D12Resource> tempConstantBuffer;
    HRESULT hr = device_->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&tempConstantBuffer)
    );
    
    if (SUCCEEDED(hr)) {
        void* pData = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        hr = tempConstantBuffer->Map(0, &readRange, &pData);
        if (SUCCEEDED(hr)) {
            memcpy(pData, &constants, sizeof(PathConstants));
            tempConstantBuffer->Unmap(0, nullptr);
        }
        
        // Set constant buffer
        commandList->SetGraphicsRootConstantBufferView(0, tempConstantBuffer->GetGPUVirtualAddress());
    }
    
    // Set up for line rendering
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList->IASetVertexBuffers(0, 1, &pathVertexBufferView_);
    commandList->IASetIndexBuffer(&pathIndexBufferView_);
    
    // Draw the lines
    commandList->DrawIndexedInstanced(static_cast<UINT>(indices.size()), 1, 0, 0, 0);
#endif
}

void RenderSystem::createPathBuffers(const Vector<Vec3>& vertices, const Vector<u32>& indices) {
#ifdef DIRECTX_RENDERER
    if (!device_ || vertices.empty() || indices.empty()) {
        return;
    }
    
    const UINT vertexBufferSize = static_cast<UINT>(vertices.size() * sizeof(Vec3));
    const UINT indexBufferSize = static_cast<UINT>(indices.size() * sizeof(u32));
    
    // Release old buffers if they exist
    pathVertexBuffer_.Reset();
    pathIndexBuffer_.Reset();
    pathBuffersCreated_ = false;
    
    // Create upload heap for vertex buffer
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    // Create vertex buffer (upload heap for simplicity)
    resourceDesc.Width = vertexBufferSize;
    HRESULT hr = device_->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&pathVertexBuffer_)
    );
    
    if (SUCCEEDED(hr)) {
        // Copy vertex data
        void* vertexDataPtr = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        hr = pathVertexBuffer_->Map(0, &readRange, &vertexDataPtr);
        if (SUCCEEDED(hr)) {
            memcpy(vertexDataPtr, vertices.data(), vertexBufferSize);
            pathVertexBuffer_->Unmap(0, nullptr);
        }
        
        // Create vertex buffer view
        pathVertexBufferView_.BufferLocation = pathVertexBuffer_->GetGPUVirtualAddress();
        pathVertexBufferView_.StrideInBytes = sizeof(Vec3);
        pathVertexBufferView_.SizeInBytes = vertexBufferSize;
    }
    
    // Create index buffer
    resourceDesc.Width = indexBufferSize;
    hr = device_->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(pathIndexBuffer_.GetAddressOf())
    );
    
    if (SUCCEEDED(hr)) {
        // Copy index data
        void* indexDataPtr = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        hr = pathIndexBuffer_->Map(0, &readRange, &indexDataPtr);
        if (SUCCEEDED(hr)) {
            memcpy(indexDataPtr, indices.data(), indexBufferSize);
            pathIndexBuffer_->Unmap(0, nullptr);
        }
        
        // Create index buffer view
        pathIndexBufferView_.BufferLocation = pathIndexBuffer_->GetGPUVirtualAddress();
        pathIndexBufferView_.Format = DXGI_FORMAT_R32_UINT;
        pathIndexBufferView_.SizeInBytes = indexBufferSize;
        
        pathBuffersCreated_ = true;
    }
#endif
}

// MOBA Game Management
void WorldLegacy::startGame() {
    if (auto* spawnSystem = static_cast<CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        spawnSystem->startGame();
    }
    
    // Create player hero if not exists
    if (auto* heroSystem = static_cast<HeroSystem*>(getSystem("HeroSystem"))) {
        if (heroSystem->getPlayerHero() == INVALID_ENTITY) {
            // Find Radiant base for spawn position
            Vec3 playerSpawnPos(50.0f, 1.0f, 50.0f); // Default position
            Vec3 enemySpawnPos(-50.0f, 1.0f, -50.0f); // Default enemy position
            
            auto& registry = entityManager_.getRegistry();
            auto baseView = registry.view<ObjectComponent, TransformComponent>();
            for (auto entity : baseView) {
                auto& obj = baseView.get<ObjectComponent>(entity);
                auto& transform = baseView.get<TransformComponent>(entity);
                if (obj.type == ObjectType::Base) {
                    if (obj.teamId == 1) {
                        playerSpawnPos = transform.position + Vec3(10.0f, 1.0f, 10.0f);
                    } else if (obj.teamId == 2) {
                        enemySpawnPos = transform.position + Vec3(-10.0f, 1.0f, -10.0f);
                    }
                }
            }
            
            // Create Warrior hero for player (Team 1 - Radiant)
            Entity playerHero = heroSystem->createHeroByType("Warrior", 1, playerSpawnPos);
            heroSystem->setPlayerHero(playerHero);
            
            // Give starting items
            heroSystem->giveItem(playerHero, HeroSystem::createItem_Tango());
            heroSystem->giveItem(playerHero, HeroSystem::createItem_IronBranch());
            heroSystem->giveItem(playerHero, HeroSystem::createItem_IronBranch());
            
            // Learn first ability
            heroSystem->learnAbility(playerHero, 0);
            
            LOG_INFO("Player hero created at ({}, {}, {})", playerSpawnPos.x, playerSpawnPos.y, playerSpawnPos.z);
            
            // Create enemy AI hero (Team 2 - Dire)
            Entity enemyHero = heroSystem->createHeroByType("Mage", 2, enemySpawnPos);
            
            // Mark as AI controlled (not player)
            if (entityManager_.hasComponent<HeroComponent>(enemyHero)) {
                auto& enemyComp = entityManager_.getComponent<HeroComponent>(enemyHero);
                enemyComp.isPlayerControlled = false;
                enemyComp.heroName = "Enemy Mage";
                
                // Give enemy some items too
                heroSystem->giveItem(enemyHero, HeroSystem::createItem_IronBranch());
                heroSystem->giveItem(enemyHero, HeroSystem::createItem_IronBranch());
                
                // Learn abilities
                heroSystem->learnAbility(enemyHero, 0);
                heroSystem->learnAbility(enemyHero, 1);
            }
            
            LOG_INFO("Enemy AI hero created at ({}, {}, {})", enemySpawnPos.x, enemySpawnPos.y, enemySpawnPos.z);
        }
    }
}

void WorldLegacy::pauseGame() {
    if (auto* spawnSystem = static_cast<CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        spawnSystem->pauseGame();
    }
}

void WorldLegacy::resetGame() {
    if (auto* spawnSystem = static_cast<CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        spawnSystem->resetGame();
    }
}

bool WorldLegacy::isGameActive() const {
    if (auto* spawnSystem = static_cast<const CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        return spawnSystem->isGameActive();
    }
    return false;
}

f32 WorldLegacy::getGameTime() const {
    if (auto* spawnSystem = static_cast<const CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        return spawnSystem->getGameTime();
    }
    return 0.0f;
}

i32 WorldLegacy::getCurrentWave() const {
    if (auto* spawnSystem = static_cast<const CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        return spawnSystem->getCurrentWave();
    }
    return 0;
}

f32 WorldLegacy::getTimeToNextWave() const {
    if (auto* spawnSystem = static_cast<const CreepSpawnSystem*>(getSystem("CreepSpawnSystem"))) {
        return spawnSystem->getTimeToNextWave();
    }
    return -1.0f;
}

}
