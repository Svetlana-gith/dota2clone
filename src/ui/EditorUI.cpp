#include "EditorUI.h"
#include "GameMode.h"
#include "gameplay/GameplayController.h"

#include "EditorCamera.h"
#include "world/World.h"
#include "world/Components.h"
#include "world/CreepSystem.h"
#include "world/HeroSystem.h"
#include "world/TerrainMesh.h"
#include "world/TerrainChunks.h"
#include "world/MeshGenerators.h"
#include "serialization/MapIO.h"
#include "renderer/DirectXRenderer.h"
#include "renderer/WireframeGrid.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cctype>

namespace WorldEditor {

namespace {

bool DragVec3(const char* label, Vec3& v, float speed = 0.05f) {
    float tmp[3] = { v.x, v.y, v.z };
    const bool changed = ImGui::DragFloat3(label, tmp, speed);
    if (changed) {
        v = Vec3(tmp[0], tmp[1], tmp[2]);
    }
    return changed;
}

bool DragEulerDegrees(const char* label, Quat& q, float speed = 0.25f) {
    Vec3 eulerRad = glm::eulerAngles(q);
    Vec3 eulerDeg = glm::degrees(eulerRad);
    float tmp[3] = { eulerDeg.x, eulerDeg.y, eulerDeg.z };
    const bool changed = ImGui::DragFloat3(label, tmp, speed);
    if (changed) {
        const Vec3 newEulerDeg(tmp[0], tmp[1], tmp[2]);
        q = glm::quat(glm::radians(newEulerDeg));
    }
    return changed;
}

static void FillUnitCube(WorldEditor::MeshComponent& mesh) {
    mesh.name = "Cube";
    mesh.visible = true;

    mesh.vertices = {
        // Front face
        Vec3(-0.5f, -0.5f,  0.5f), Vec3(-0.5f,  0.5f,  0.5f), Vec3( 0.5f,  0.5f,  0.5f), Vec3( 0.5f, -0.5f,  0.5f),
        // Back face
        Vec3(-0.5f, -0.5f, -0.5f), Vec3( 0.5f, -0.5f, -0.5f), Vec3( 0.5f,  0.5f, -0.5f), Vec3(-0.5f,  0.5f, -0.5f),
        // Left face
        Vec3(-0.5f,  0.5f,  0.5f), Vec3(-0.5f,  0.5f, -0.5f), Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, -0.5f,  0.5f),
        // Right face
        Vec3( 0.5f,  0.5f,  0.5f), Vec3( 0.5f, -0.5f,  0.5f), Vec3( 0.5f, -0.5f, -0.5f), Vec3( 0.5f,  0.5f, -0.5f),
        // Top face
        Vec3(-0.5f,  0.5f, -0.5f), Vec3( 0.5f,  0.5f, -0.5f), Vec3( 0.5f,  0.5f,  0.5f), Vec3(-0.5f,  0.5f,  0.5f),
        // Bottom face
        Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, -0.5f,  0.5f), Vec3( 0.5f, -0.5f,  0.5f), Vec3( 0.5f, -0.5f, -0.5f)
    };

    mesh.normals = {
        // Front
        Vec3(0, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 1),
        // Back
        Vec3(0, 0, -1), Vec3(0, 0, -1), Vec3(0, 0, -1), Vec3(0, 0, -1),
        // Left
        Vec3(-1, 0, 0), Vec3(-1, 0, 0), Vec3(-1, 0, 0), Vec3(-1, 0, 0),
        // Right
        Vec3(1, 0, 0), Vec3(1, 0, 0), Vec3(1, 0, 0), Vec3(1, 0, 0),
        // Top
        Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 1, 0),
        // Bottom
        Vec3(0, -1, 0), Vec3(0, -1, 0), Vec3(0, -1, 0), Vec3(0, -1, 0)
    };

    mesh.texCoords = {
        Vec2(0, 0), Vec2(0, 1), Vec2(1, 1), Vec2(1, 0),
        Vec2(0, 0), Vec2(1, 0), Vec2(1, 1), Vec2(0, 1),
        Vec2(0, 1), Vec2(0, 0), Vec2(1, 0), Vec2(1, 1),
        Vec2(0, 1), Vec2(1, 1), Vec2(1, 0), Vec2(0, 0),
        Vec2(0, 0), Vec2(1, 0), Vec2(1, 1), Vec2(0, 1),
        Vec2(0, 0), Vec2(0, 1), Vec2(1, 1), Vec2(1, 0)
    };

    mesh.indices = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    };

#ifdef DIRECTX_RENDERER
    mesh.gpuBuffersCreated = false;
    mesh.gpuConstantBuffersCreated = false;
#endif
}

static Entity EnsureDefaultMaterial(WorldEditor::World& world, const char* name, const Vec3& color) {
    Entity m = world.createEntity(name);
    auto& mat = world.addComponent<WorldEditor::MaterialComponent>(m);
    mat.name = name;
    mat.baseColor = color;
    mat.gpuBufferCreated = false;
    return m;
}

} // namespace

u64 EditorUI::makePropKey(Entity e, ComponentSlot slot, size_t offset) {
    // Simple stable key: [entity (32 bits)] [slot (8 bits)] [offset (24 bits)]
    const u64 ent = static_cast<u64>(static_cast<u32>(e));
    const u64 sl = static_cast<u64>(slot) & 0xFFull;
    const u64 off = static_cast<u64>(offset) & 0xFFFFFFull;
    return (ent << 32) | (sl << 24) | off;
}

void EditorUI::applyCommand(const PropCommand& cmd, bool useAfter) {
    // Apply change to the underlying component by offset.
    // This is intentionally narrow: only Transform/Material and supported kinds.
    if (cmd.entity == INVALID_ENTITY) {
        return;
    }

    // We'll fetch the component via the World registry at call site (draw) to keep this function simple.
    // The caller ensures entity is valid.
    (void)useAfter;
}

bool EditorUI::drawComponentProperties(Entity e, ComponentSlot slot, void* componentPtr) {
    if (!componentPtr) {
        return false;
    }

    const size_t typeId =
        (slot == ComponentSlot::Transform) ? typeid(TransformComponent).hash_code() : typeid(MaterialComponent).hash_code();
    const auto* meta = WorldEditor::Properties::getTypeMeta(typeId);
    if (!meta) {
        return false;
    }

    bool anyChanged = false;
    for (const auto& p : meta->props) {
        ImGui::PushID(static_cast<int>(p.offset));

        const u64 key = makePropKey(e, slot, p.offset);

        // Capture old value on activation.
        if (ImGui::IsItemActivated()) {
            // no-op: activation state is checked after widget creation; handled below.
        }

        bool changed = false;
        PropValue newV{};
        newV.kind = p.kind;

        if (p.kind == WorldEditor::Properties::Kind::Float) {
            float* v = WorldEditor::Properties::ptrFloat(componentPtr, p.offset);
            float before = *v;
            changed = ImGui::DragFloat(p.name, v, p.step, p.minV, p.maxV);
            newV.f = *v;

            if (ImGui::IsItemActivated()) {
                PropValue old{};
                old.kind = p.kind;
                old.f = before;
                activeEditOld_[key] = old;
            }
        } else if (p.kind == WorldEditor::Properties::Kind::Vec3) {
            Vec3* v = WorldEditor::Properties::ptrVec3(componentPtr, p.offset);
            Vec3 before = *v;
            float tmp[3] = { v->x, v->y, v->z };
            changed = ImGui::DragFloat3(p.name, tmp, p.step);
            if (changed) {
                *v = Vec3(tmp[0], tmp[1], tmp[2]);
            }
            newV.v = *v;

            if (ImGui::IsItemActivated()) {
                PropValue old{};
                old.kind = p.kind;
                old.v = before;
                activeEditOld_[key] = old;
            }
        } else if (p.kind == WorldEditor::Properties::Kind::Color3) {
            Vec3* v = WorldEditor::Properties::ptrVec3(componentPtr, p.offset);
            Vec3 before = *v;
            float col[3] = { v->x, v->y, v->z };
            changed = ImGui::ColorEdit3(p.name, col);
            if (changed) {
                *v = Vec3(col[0], col[1], col[2]);
            }
            newV.v = *v;

            if (ImGui::IsItemActivated()) {
                PropValue old{};
                old.kind = p.kind;
                old.v = before;
                activeEditOld_[key] = old;
            }
        }

        if (changed) {
            anyChanged = true;
        }

        // Commit command when user finished editing.
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            auto it = activeEditOld_.find(key);
            if (it != activeEditOld_.end()) {
                PropCommand cmd{};
                cmd.entity = e;
                cmd.component = slot;
                cmd.kind = p.kind;
                cmd.offset = p.offset;
                cmd.before = it->second;
                cmd.after = newV;
                undo_.push_back(cmd);
                redo_.clear();
                activeEditOld_.erase(it);
                dirty_ = true;
            }
        }

        ImGui::PopID();
    }
    return anyChanged;
}

// ============================================================================
// Modern Theme & Styling
// ============================================================================

void EditorUI::applyModernTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Dark theme colors (Unreal Engine inspired)
    colors[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.12f, 0.12f, 0.12f, 0.94f);
    colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.36f, 0.69f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    colors[ImGuiCol_DockingPreview]       = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    
    // Style adjustments
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.IndentSpacing = 20.0f;
}

// ============================================================================
// Toolbar
// ============================================================================

void EditorUI::drawToolbar(World& world) {
    (void)world;
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float toolbarHeight = 40.0f;
    const float menuBarHeight = ImGui::GetFrameHeight(); // Height of main menu bar
    
    // Position toolbar right below the menu bar
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbarHeight));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    
    if (ImGui::Begin("##Toolbar", nullptr, flags)) {
        // Tool selection buttons
        const ImVec2 btnSize(28, 28);
        
        // Select tool
        bool isSelectMode = !tileEditorEnabled_ && !texturePaintEnabled_ && !objectPlacementEnabled_;
        if (isSelectMode) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
        if (ImGui::Button("S##Select", btnSize)) {
            tileEditorEnabled_ = false;
            texturePaintEnabled_ = false;
            objectPlacementEnabled_ = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select (1)");
        if (isSelectMode) ImGui::PopStyleColor();
        
        ImGui::SameLine();
        
        // Terrain sculpt tool
        if (tileEditorEnabled_) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
        if (ImGui::Button("T##Terrain", btnSize)) {
            tileEditorEnabled_ = true;
            texturePaintEnabled_ = false;
            objectPlacementEnabled_ = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Terrain Sculpt (2)");
        if (tileEditorEnabled_) ImGui::PopStyleColor();
        
        ImGui::SameLine();
        
        // Paint tool
        if (texturePaintEnabled_) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
        if (ImGui::Button("P##Paint", btnSize)) {
            tileEditorEnabled_ = false;
            texturePaintEnabled_ = true;
            objectPlacementEnabled_ = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Paint (3)");
        if (texturePaintEnabled_) ImGui::PopStyleColor();
        
        ImGui::SameLine();
        
        // Object placement tool
        if (objectPlacementEnabled_) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
        if (ImGui::Button("O##Object", btnSize)) {
            tileEditorEnabled_ = false;
            texturePaintEnabled_ = false;
            objectPlacementEnabled_ = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Place Objects (4)");
        if (objectPlacementEnabled_) ImGui::PopStyleColor();
        
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        
        // Play/Stop buttons
        bool isPlaying = gameMode_ && gameMode_->isGameModeActive();
        
        if (isPlaying) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Stop", ImVec2(50, 28))) {
                if (gameMode_) {
                    gameMode_->setGameModeActive(false);
                }
                if (gameplayController_) {
                    gameplayController_->resetGame();
                }
                world.resetGame();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("Play", ImVec2(50, 28))) {
                if (gameMode_) {
                    gameMode_->setGameModeActive(true);
                }
                if (gameplayController_) {
                    gameplayController_->startGame();
                }
                world.startGame(); // Start creep spawning!
            }
            ImGui::PopStyleColor();
        }
        
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play/Stop Game Mode");
        
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        
        // Visualization toggles
        ImGui::Checkbox("Wireframe", &showWireframe_);
        ImGui::SameLine();
        ImGui::Checkbox("Paths", &showPathLines_);
        ImGui::SameLine();
        ImGui::Checkbox("Ranges", &showUnitAttackRanges_);
    }
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ============================================================================
// Status Bar
// ============================================================================

void EditorUI::drawStatusBar(World& world) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float statusBarHeight = 24.0f;
    
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusBarHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusBarHeight));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    
    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        // Entity count
        ImGui::Text("Entities: %zu", world.getEntityCount());
        
        ImGui::SameLine(150);
        
        // Selected entity
        if (selected_ != INVALID_ENTITY && world.isValid(selected_)) {
            if (world.hasComponent<NameComponent>(selected_)) {
                ImGui::Text("Selected: %s", world.getComponent<NameComponent>(selected_).name.c_str());
            } else {
                ImGui::Text("Selected: Entity %u", static_cast<u32>(selected_));
            }
        } else {
            ImGui::TextDisabled("No selection");
        }
        
        ImGui::SameLine(400);
        
        // Current tool
        const char* toolName = "Select";
        if (tileEditorEnabled_) toolName = "Terrain Sculpt";
        else if (texturePaintEnabled_) toolName = "Paint";
        else if (objectPlacementEnabled_) toolName = "Place Objects";
        ImGui::Text("Tool: %s", toolName);
        
        ImGui::SameLine(600);
        
        // FPS
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        
        ImGui::SameLine(viewport->WorkSize.x - 150);
        
        // Dirty indicator
        if (dirty_) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "* Modified");
        } else {
            ImGui::TextDisabled("Saved");
        }
    }
    ImGui::End();
    
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

// ============================================================================
// Tools Panel (combines Terrain, Paint, Objects)
// ============================================================================

void EditorUI::drawToolsPanel(World& world) {
    if (!ImGui::Begin("Tools")) {
        ImGui::End();
        return;
    }
    
    // Tab bar for different tool categories
    if (ImGui::BeginTabBar("ToolsTabs")) {
        if (ImGui::BeginTabItem("Terrain")) {
            drawTerrainTools(world);
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Paint")) {
            drawTexturePainting(world);
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Objects")) {
            drawObjectPlacement(world);
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Create")) {
            drawTerrain(world);
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void EditorUI::draw(World& world) {
    ensureSelectionValid(world);
    
    // Initialize gameplay controller and game mode if needed
    if (!gameplayController_) {
        gameplayController_ = std::make_unique<GameplayController>();
        gameplayController_->setWorld(&world);
    }
    if (!gameMode_) {
        gameMode_ = std::make_unique<GameMode>();
        gameMode_->setController(gameplayController_.get());
    }

    // Apply modern dark theme
    applyModernTheme();

    drawDockSpace();
    drawMainMenu(world);
    drawToolbar(world);
    drawUnsavedChangesPopup(world);
    
    // Draw game mode if active
    if (gameMode_ && gameMode_->isGameModeActive()) {
        gameMode_->draw(world);
    }
    
    drawViewport(world);
    drawGameView(world);
    drawHierarchy(world);
    drawInspector(world);
    drawToolsPanel(world);
    drawStats(world);
    drawMOBAGamePanel(world);
    drawPathVisualizationPanel(world);
    drawStatusBar(world);
}

void EditorUI::performNew(World& world) {
    world.clearEntities();
    selected_ = INVALID_ENTITY;
    undo_.clear();
    redo_.clear();
    activeEditOld_.clear();
    // New document is unsaved by definition.
    dirty_ = true;
}

void EditorUI::performOpen(World& world) {
    String err;
    if (!WorldEditor::MapIO::load(world, currentMapPath_, &err)) {
        LOG_ERROR("Map load failed: {}", err);
        return;
    }
    LOG_INFO("Map loaded: {}", currentMapPath_);
    selected_ = INVALID_ENTITY;
    undo_.clear();
    redo_.clear();
    activeEditOld_.clear();
    dirty_ = false;
}

void EditorUI::performSave(World& world) {
    String err;
    if (!WorldEditor::MapIO::save(world, currentMapPath_, &err)) {
        LOG_ERROR("Map save failed: {}", err);
        return;
    }
    LOG_INFO("Map saved: {}", currentMapPath_);
    dirty_ = false;
}

void EditorUI::drawUnsavedChangesPopup(World& world) {
    if (openUnsavedPopup_) {
        ImGui::OpenPopup("Unsaved changes");
        openUnsavedPopup_ = false;
    }

    bool open = true;
    if (ImGui::BeginPopupModal("Unsaved changes", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("You have unsaved changes.");
        ImGui::Text("File: %s", currentMapPath_.c_str());
        ImGui::Separator();

        if (ImGui::Button("Save")) {
            performSave(world);
            // If save succeeded, dirty_ is false; proceed.
            if (!dirty_) {
                const PendingAction act = pendingAction_;
                pendingAction_ = PendingAction::None;
                ImGui::CloseCurrentPopup();

                if (act == PendingAction::New) performNew(world);
                else if (act == PendingAction::Open) performOpen(world);
                else if (act == PendingAction::Exit) quitRequested_ = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            const PendingAction act = pendingAction_;
            pendingAction_ = PendingAction::None;
            ImGui::CloseCurrentPopup();

            if (act == PendingAction::New) performNew(world);
            else if (act == PendingAction::Open) performOpen(world);
            else if (act == PendingAction::Exit) quitRequested_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pendingAction_ = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else {
        if (!open) {
            pendingAction_ = PendingAction::None;
        }
    }
}

void EditorUI::drawDockSpace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Account for toolbar height
    const float toolbarHeight = 40.0f;
    const float statusBarHeight = 24.0f;
    
    ImVec2 dockPos = viewport->WorkPos;
    dockPos.y += toolbarHeight; // Start below toolbar
    
    ImVec2 dockSize = viewport->WorkSize;
    dockSize.y -= toolbarHeight + statusBarHeight; // Subtract toolbar and status bar
    
    ImGui::SetNextWindowPos(dockPos);
    ImGui::SetNextWindowSize(dockSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = 0;
    windowFlags |= ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar;
    windowFlags |= ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoResize;
    windowFlags |= ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    windowFlags |= ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    const bool open = true;
    ImGui::Begin("##DockSpaceRoot", const_cast<bool*>(&open), windowFlags);

    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("WorldEditorDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    // Build a sane default layout once (or on request).
    if (!dockLayoutBuilt_ || requestResetLayout_ || forceLayoutReset_) {
        buildDefaultDockLayout(dockspaceId);
        dockLayoutBuilt_ = true;
        requestResetLayout_ = false;
        forceLayoutReset_ = false;
    }

    ImGui::End();
}

void EditorUI::drawMainMenu(World& world) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
            if (dirty_) {
                pendingAction_ = PendingAction::New;
                openUnsavedPopup_ = true;
            } else {
                performNew(world);
            }
        }
        if (ImGui::MenuItem("Open")) {
            if (dirty_) {
                pendingAction_ = PendingAction::Open;
                openUnsavedPopup_ = true;
            } else {
                performOpen(world);
            }
        }
        if (ImGui::MenuItem("Save")) {
            performSave(world);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            if (dirty_) {
                pendingAction_ = PendingAction::Exit;
                openUnsavedPopup_ = true;
            } else {
                quitRequested_ = true;
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Game")) {
        if (ImGui::MenuItem("Start Game Mode", nullptr, gameMode_ && gameMode_->isGameModeActive())) {
            if (gameMode_) {
                gameMode_->setGameModeActive(true);
            }
        }
        if (ImGui::MenuItem("Stop Game Mode", nullptr, false, gameMode_ && gameMode_->isGameModeActive())) {
            if (gameMode_) {
                gameMode_->setGameModeActive(false);
            }
        }
        ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Edit")) {
        const bool canUndo = !undo_.empty();
        const bool canRedo = !redo_.empty();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
            // handled below
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
            // handled below
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Reset Layout")) {
            requestResetLayout_ = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Create")) {
        if (ImGui::MenuItem("Empty Entity")) {
            Entity e = world.createEntity("Entity");
            world.addComponent<TransformComponent>(e);
            selected_ = e;
            dirty_ = true;
        }
        if (ImGui::MenuItem("Material")) {
            Entity m = world.createEntity("Material");
            world.addComponent<MaterialComponent>(m, "Material");
            selected_ = m;
            dirty_ = true;
        }
        if (ImGui::MenuItem("Cube")) {
            Entity e = world.createEntity("Cube");
            world.addComponent<TransformComponent>(e);

            auto& mesh = world.addComponent<MeshComponent>(e);
            FillUnitCube(mesh);

            // Default red material (per cube). Later we'll switch to shared asset/material library.
            mesh.materialEntity = EnsureDefaultMaterial(world, "CubeMaterial", Vec3(1.0f, 0.0f, 0.0f));
            selected_ = e;
            dirty_ = true;
        }
        if (ImGui::MenuItem("Terrain")) {
            Entity e = world.createEntity("Terrain");
            world.addComponent<TransformComponent>(e);

            auto& t = world.addComponent<TerrainComponent>(e);
            // TerrainComponent already has default tile-based values
            WorldEditor::TerrainTools::syncHeightmapFromLevels(t);

            auto& mesh = world.addComponent<MeshComponent>(e);
            mesh.name = "Terrain";
            TerrainMesh::buildMesh(t, mesh);

            // Generate wireframe grid
            if (renderer_ && renderer_->GetWireframeGrid()) {
                renderer_->GetWireframeGrid()->generateGrid(t, mesh);
            }

            Entity matE = world.createEntity("TerrainMaterial");
            auto& mat = world.addComponent<MaterialComponent>(matE);
            mat.name = "TerrainMaterial";
            mat.baseColor = Vec3(0.25f, 0.6f, 0.25f);
            mat.gpuBufferCreated = false;
            mesh.materialEntity = matE;

            selected_ = e;
            dirty_ = true;
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();

    // Global shortcuts (avoid when typing into text fields).
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) && !undo_.empty()) {
            // Undo
            PropCommand cmd = undo_.back();
            undo_.pop_back();
            redo_.push_back(cmd);

            if (world.isValid(cmd.entity)) {
                if (cmd.component == ComponentSlot::Transform && world.hasComponent<TransformComponent>(cmd.entity)) {
                    auto& c = world.getComponent<TransformComponent>(cmd.entity);
                    if (cmd.kind == WorldEditor::Properties::Kind::Float) *WorldEditor::Properties::ptrFloat(&c, cmd.offset) = cmd.before.f;
                    else *WorldEditor::Properties::ptrVec3(&c, cmd.offset) = cmd.before.v;
                }
                if (cmd.component == ComponentSlot::Material && world.hasComponent<MaterialComponent>(cmd.entity)) {
                    auto& c = world.getComponent<MaterialComponent>(cmd.entity);
                    if (cmd.kind == WorldEditor::Properties::Kind::Float) *WorldEditor::Properties::ptrFloat(&c, cmd.offset) = cmd.before.f;
                    else *WorldEditor::Properties::ptrVec3(&c, cmd.offset) = cmd.before.v;
                    c.gpuBufferCreated = false;
                }
            }
            dirty_ = true;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false) && !redo_.empty()) {
            // Redo
            PropCommand cmd = redo_.back();
            redo_.pop_back();
            undo_.push_back(cmd);

            if (world.isValid(cmd.entity)) {
                if (cmd.component == ComponentSlot::Transform && world.hasComponent<TransformComponent>(cmd.entity)) {
                    auto& c = world.getComponent<TransformComponent>(cmd.entity);
                    if (cmd.kind == WorldEditor::Properties::Kind::Float) *WorldEditor::Properties::ptrFloat(&c, cmd.offset) = cmd.after.f;
                    else *WorldEditor::Properties::ptrVec3(&c, cmd.offset) = cmd.after.v;
                }
                if (cmd.component == ComponentSlot::Material && world.hasComponent<MaterialComponent>(cmd.entity)) {
                    auto& c = world.getComponent<MaterialComponent>(cmd.entity);
                    if (cmd.kind == WorldEditor::Properties::Kind::Float) *WorldEditor::Properties::ptrFloat(&c, cmd.offset) = cmd.after.f;
                    else *WorldEditor::Properties::ptrVec3(&c, cmd.offset) = cmd.after.v;
                    c.gpuBufferCreated = false;
                }
            }
            dirty_ = true;
        }
    }
}

void EditorUI::buildDefaultDockLayout(ImGuiID dockspaceId) {
    // DockBuilder API lives in imgui_internal.h (docking branch).
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

    ImGuiID dockMain = dockspaceId;
    
    // Left panel (Hierarchy) - 18%
    ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);
    
    // Right panel (Inspector + Tools) - 25%
    ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
    
    // Bottom panel (Stats, MOBA, Paths) - 25%
    ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, nullptr, &dockMain);
    
    // Split right into Inspector (top) and Tools (bottom)
    ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.50f, nullptr, &dockRight);

    // Central area is dockMain after splits - Viewport and Game View
    ImGui::DockBuilderDockWindow("Viewport", dockMain);
    ImGui::DockBuilderDockWindow("Game View", dockMain);
    
    // Left - Hierarchy
    ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
    
    // Right top - Inspector
    ImGui::DockBuilderDockWindow("Inspector", dockRight);
    
    // Right bottom - Tools and Camera
    ImGui::DockBuilderDockWindow("Tools", dockRightBottom);
    ImGui::DockBuilderDockWindow("Camera", dockRightBottom);
    
    // Bottom - Stats, MOBA, Paths (tabbed)
    ImGui::DockBuilderDockWindow("Stats", dockBottom);
    ImGui::DockBuilderDockWindow("MOBA Game Control", dockBottom);
    ImGui::DockBuilderDockWindow("Path Visualization", dockBottom);

    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorUI::drawTerrain(World& world) {
    if (!ImGui::Begin("Terrain")) {
        ImGui::End();
        return;
    }

    // Terrain Tools Section
    if (ImGui::CollapsingHeader("Terrain Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawTerrainTools(world);
    }

    // Texture Painting Section
    if (ImGui::CollapsingHeader("Texture Painting")) {
        drawTexturePainting(world);
    }

    // Object Placement Section
    if (ImGui::CollapsingHeader("Object Placement", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawObjectPlacement(world);
    }

    ImGui::Separator();

    // Create new terrain (always tile-based)
    ImGui::Text("Create New Terrain");
    ImGui::Text("Uses default tile settings from TerrainComponent");

    if (ImGui::Button("Create Terrain")) {
        Entity e = world.createEntity("Terrain");
        world.addComponent<TransformComponent>(e);

        auto& t = world.addComponent<TerrainComponent>(e);
        // Initialize tile terrain with default values to ensure heightLevels is properly set up
        WorldEditor::TerrainTools::initTileTerrain(t, t.tilesX, t.tilesZ, t.tileSize, t.heightStep);
        WorldEditor::TerrainTools::syncHeightmapFromLevels(t);

        auto& mesh = world.addComponent<MeshComponent>(e);
        mesh.name = "Terrain";
        TerrainMesh::buildMesh(t, mesh);

        // Generate wireframe grid
        if (renderer_ && renderer_->GetWireframeGrid()) {
            renderer_->GetWireframeGrid()->generateGrid(t, mesh);
        }

        // Add terrain material component
        auto& terrainMat = world.addComponent<TerrainMaterialComponent>(e);

        Entity matE = world.createEntity("TerrainMaterial");
        auto& mat = world.addComponent<MaterialComponent>(matE);
        mat.name = "TerrainMaterial";
        mat.baseColor = Vec3(0.25f, 0.6f, 0.25f);
        mat.gpuBufferCreated = false;
        mesh.materialEntity = matE;

        selected_ = e;
        dirty_ = true;
    }

    ImGui::Separator();
    if (ImGui::Button("Create Test Map", ImVec2(-1, 0))) {
        createTestMap(world);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Creates a test MOBA map with terrain, towers, spawns, and neutral camps");
    }

    // Selected terrain operations
    if (selected_ != INVALID_ENTITY && world.isValid(selected_) && world.hasComponent<TerrainComponent>(selected_)) {
        auto& t = world.getComponent<TerrainComponent>(selected_);
        auto& mesh = world.hasComponent<MeshComponent>(selected_) ? world.getComponent<MeshComponent>(selected_) : world.addComponent<MeshComponent>(selected_);
        
        ImGui::Separator();
        ImGui::Text("Selected Terrain Operations");
        ImGui::Text("Resolution: %d x %d", t.resolution.x, t.resolution.y);
        ImGui::Text("Size: %.2f", t.size);
        
        if (ImGui::Button("Rebuild Mesh")) {
            WorldEditor::TerrainTools::syncHeightmapFromLevels(t);
            TerrainMesh::buildMesh(t, mesh);
            
            // Generate wireframe grid
            if (renderer_ && renderer_->GetWireframeGrid()) {
                renderer_->GetWireframeGrid()->generateGrid(t, mesh);
            }
        }
    }

    ImGui::End();
}

void EditorUI::drawTerrainTools(World& world) {
    // Tile-based terrain editor (Dota 2 Workshop Tools style)
    ImGui::Text("Tile Editor (Dota 2 Workshop Tools style)");

    // Only meaningful when a terrain is selected.
    Entity terrainE = INVALID_ENTITY;
    if (selected_ != INVALID_ENTITY && world.isValid(selected_) && world.hasComponent<TerrainComponent>(selected_)) {
        terrainE = selected_;
    }

    ImGui::Checkbox("Enable Tile Editor", &tileEditorEnabled_);
    if (terrainE == INVALID_ENTITY) {
        ImGui::TextDisabled("Select a Terrain entity to use Tile Editor.");
        ImGui::BeginDisabled();
    }

    if (terrainE != INVALID_ENTITY) {
        auto& t = world.getComponent<TerrainComponent>(terrainE);
        ImGui::Text("Selected Terrain: Tile-based");

        ImGui::InputInt("Tiles X", &tileInitTilesX_);
        ImGui::InputInt("Tiles Z", &tileInitTilesZ_);
        tileInitTilesX_ = std::max(1, tileInitTilesX_);
        tileInitTilesZ_ = std::max(1, tileInitTilesZ_);
        ImGui::InputFloat("Tile Size", &tileInitTileSize_);
        ImGui::InputFloat("Height Step", &tileInitHeightStep_);
        tileInitTileSize_ = std::max(1.0f, tileInitTileSize_);
        tileInitHeightStep_ = std::max(1.0f, tileInitHeightStep_);

        if (ImGui::Button("Initialize Tile Terrain", ImVec2(-1, 0))) {
            // Convert selected terrain to tile grid and rebuild mesh immediately.
            WorldEditor::TerrainTools::initTileTerrain(t, tileInitTilesX_, tileInitTilesZ_, tileInitTileSize_, tileInitHeightStep_);
            WorldEditor::TerrainTools::syncHeightmapFromLevels(t);

            // Get or create mesh component (safe pattern)
            MeshComponent* meshPtr = nullptr;
            if (world.hasComponent<MeshComponent>(terrainE)) {
                meshPtr = &world.getComponent<MeshComponent>(terrainE);
            } else {
                meshPtr = &world.addComponent<MeshComponent>(terrainE);
            }
            auto& mesh = *meshPtr;

            // Wait for GPU to finish before invalidating buffers (prevents DeviceRemoved errors).
            if (renderer_) {
                renderer_->WaitForPreviousFrame();
            }

            // Clear old chunks before rebuilding (global chunks storage needs clearing when terrain size changes).
            auto& chunks = WorldEditor::TerrainChunks::getChunks(mesh);
            chunks.clear();

            // Invalidate all GPU buffers first (after GPU finished)
            TerrainMesh::invalidateGpu(mesh);
            
            // Clear CPU mesh data before rebuild
            mesh.vertices.clear();
            mesh.normals.clear();
            mesh.texCoords.clear();
            mesh.indices.clear();

            // Rebuild mesh from new tile terrain
            TerrainMesh::buildMesh(t, mesh);
            
            // For large terrains (like 128x128 tiles = 129x129 vertices), initialize chunk system immediately
            // to avoid creating huge single GPU buffers that can cause DeviceRemoved errors.
            if (renderer_ && renderer_->GetDevice()) {
                const int largeTerrainThreshold = 100; // Use chunks for terrains larger than 100x100
                if (t.resolution.x > largeTerrainThreshold || t.resolution.y > largeTerrainThreshold) {
                    // Initialize chunk system now to split terrain into manageable pieces
                    if (WorldEditor::TerrainChunks::initializeChunks(t, mesh)) {
                        // Immediately update all chunks so they have GPU buffers and can be rendered
                        WorldEditor::TerrainChunks::updateDirtyChunks(t, mesh, renderer_->GetDevice());
                        // Clear main mesh data since we're using chunks (prevents confusion)
                        mesh.vertices.clear();
                        mesh.normals.clear();
                        mesh.texCoords.clear();
                        mesh.indices.clear();
                    }
                }
                // For smaller terrains, keep main mesh data - it will be rendered normally
                // GPU buffers will be created automatically by RenderSystem::ensureMeshBuffers on next frame
            }
            
            // Skip wireframe grid for very large terrains (prevents GPU buffer overflow).
            // For 128x128 tiles (129x129 vertices), wireframe would create ~66k vertices which can crash.
            const int maxSafeResolution = 65; // Safe limit for wireframe grid
            if (renderer_ && renderer_->GetWireframeGrid() && 
                t.resolution.x <= maxSafeResolution && t.resolution.y <= maxSafeResolution) {
                renderer_->GetWireframeGrid()->generateGrid(t, mesh);
            }
            markDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Creates a Dota-like tile grid: resolution=(tiles+1), size=tilesX*tileSize.\nHeights become discrete levels (multiples of Height Step).");
        }

        ImGui::Separator();
        ImGui::Text("Hotkeys: Q=Height Brush, R=Ramp/Path, [ ]=Radius");

        const char* tileToolNames[] = { "Height Brush", "Ramp/Path" };
        int tool = static_cast<int>(tileTool_);
        if (ImGui::Combo("Tool", &tool, tileToolNames, IM_ARRAYSIZE(tileToolNames))) {
            tileTool_ = static_cast<TileTool>(tool);
        }

        ImGui::SliderInt("Tile Brush Radius (tiles)", &tileBrushRadiusTiles_, 1, 16);

        if (tileTool_ == TileTool::HeightBrush) {
            ImGui::InputInt("Flatten Level", &tileFlattenLevel_);
            ImGui::Text("Controls: LMB raise. Ctrl+LMB lower. Shift+LMB flatten to level.");
        } else {
            ImGui::SliderInt("Ramp Width (tiles)", &tileRampWidthTiles_, 1, 8);
            ImGui::Text("Controls: Drag LMB to draw a ramp path (start->end).");
        }
    }

    if (terrainE == INVALID_ENTITY) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    ImGui::Text("Visualization:");
    ImGui::Checkbox("Show Wireframe", &showWireframe_);
    ImGui::Checkbox("Unreal Viewport (Sky + Checker)", &unrealViewportStyle_);
    if (unrealViewportStyle_) {
        ImGui::SliderFloat("Checker Cell Size", &checkerCellSize_, 0.25f, 32.0f, "%.2f");
    }
}

void EditorUI::drawTexturePainting(World& world) {
    (void)world;
    
    ImGui::Checkbox("Texture Painting (T+LMB)", &texturePaintEnabled_);
    
    if (!texturePaintEnabled_) {
        ImGui::BeginDisabled();
    }
    
    // Texture Layer Selection
    ImGui::Text("Active Layer:");
    ImGui::SliderInt("##ActiveLayer", &activeTextureLayer_, 0, 3);
    
    // Brush Settings for Texture Painting
    ImGui::SliderFloat("Paint Radius", &textureBrushRadius_, 0.5f, 20.0f, "%.2f");
    ImGui::SliderFloat("Paint Strength", &textureBrushStrength_, 0.1f, 10.0f, "%.2f");
    
    // Show texture layers for selected terrain
    if (selected_ != INVALID_ENTITY && world.isValid(selected_) && 
        world.hasComponent<WorldEditor::TerrainMaterialComponent>(selected_)) {
        
        auto& terrainMat = world.getComponent<WorldEditor::TerrainMaterialComponent>(selected_);
        
        ImGui::Separator();
        ImGui::Text("Texture Layers:");
        
        for (size_t i = 0; i < terrainMat.layers.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            
            bool isActive = (activeTextureLayer_ == static_cast<int>(i));
            if (ImGui::Selectable(("Layer " + std::to_string(i)).c_str(), isActive)) {
                activeTextureLayer_ = static_cast<int>(i);
            }
            
            ImGui::SameLine();
            ImGui::Text("Tiling: %.1f", terrainMat.layers[i].tiling);
            
            ImGui::PopID();
        }
        
        if (ImGui::Button("Add Layer") && terrainMat.layers.size() < 4) {
            WorldEditor::TerrainMaterialComponent::TextureLayer newLayer;
            newLayer.diffuseTexture = "textures/rock_diffuse.png";
            newLayer.normalTexture = "textures/rock_normal.png";
            newLayer.tiling = 2.0f;
            terrainMat.layers.push_back(newLayer);
            markDirty();
        }
    }
    
    if (!texturePaintEnabled_) {
        ImGui::EndDisabled();
    }
}

void EditorUI::drawObjectPlacement(World& world) {
    (void)world;
    
    ImGui::Checkbox("Object Placement (4+LMB)", &objectPlacementEnabled_);
    
    if (!objectPlacementEnabled_) {
        ImGui::BeginDisabled();
    }
    
    // Object Type Selection
    ImGui::Text("Object Type:");
    const char* objectTypeNames[] = {
        "None", "Tower", "Creep Spawn", "Neutral Camp", "Tree", "Rock", "Building", "Waypoint", "Base", "Custom"
    };
    int currentType = static_cast<int>(selectedObjectType_);
    if (ImGui::Combo("##ObjectType", &currentType, objectTypeNames, IM_ARRAYSIZE(objectTypeNames))) {
        selectedObjectType_ = static_cast<ObjectType>(currentType);
    }
    
    // MOBA-specific properties (only for relevant types)
    if (selectedObjectType_ == ObjectType::Tower || 
        selectedObjectType_ == ObjectType::CreepSpawn ||
        selectedObjectType_ == ObjectType::NeutralCamp ||
        selectedObjectType_ == ObjectType::Base ||
        selectedObjectType_ == ObjectType::Waypoint) {
        ImGui::Separator();
        ImGui::Text("MOBA Properties:");
        ImGui::SliderInt("Team ID", &objectTeamId_, 0, 2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0 = Neutral, 1 = Team 1, 2 = Team 2");
        }
    }
    
    if (selectedObjectType_ == ObjectType::CreepSpawn || 
        selectedObjectType_ == ObjectType::NeutralCamp) {
        ImGui::SliderFloat("Spawn Radius", &objectSpawnRadius_, 1.0f, 20.0f, "%.1f");
        ImGui::SliderInt("Max Units", &objectMaxUnits_, 1, 10);
    }
    
    // Lane selection for CreepSpawn
    if (selectedObjectType_ == ObjectType::CreepSpawn) {
        const char* laneNames[] = { "All Lanes", "Top", "Middle", "Bottom" };
        int laneComboValue = objectSpawnLane_ + 1; // -1 becomes 0, 0-2 become 1-3
        if (ImGui::Combo("Spawn Lane", &laneComboValue, laneNames, IM_ARRAYSIZE(laneNames))) {
            objectSpawnLane_ = laneComboValue - 1; // Convert back: 0 becomes -1, 1-3 become 0-2
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select which lane(s) creeps should spawn for. 'All Lanes' spawns for Top, Middle, and Bottom.");
        }
    }
    
    // Waypoint-specific properties
    if (selectedObjectType_ == ObjectType::Waypoint) {
        ImGui::Separator();
        ImGui::Text("Waypoint Properties:");
        ImGui::DragInt("Order", &objectWaypointOrder_, 1.0f, 0, 100);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Order in the path (0 = first waypoint, higher = later)");
        }
        const char* waypointLaneNames[] = { "All Lanes", "Top", "Middle", "Bottom" };
        int waypointLaneComboValue = objectWaypointLane_ + 1;
        if (ImGui::Combo("Waypoint Lane", &waypointLaneComboValue, waypointLaneNames, IM_ARRAYSIZE(waypointLaneNames))) {
            objectWaypointLane_ = waypointLaneComboValue - 1;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Lane this waypoint belongs to. 'All Lanes' means it applies to all lanes.");
        }
    }
    
    ImGui::Separator();
    ImGui::Text("Instructions:");
    ImGui::BulletText("Press 4 to enable placement mode");
    ImGui::BulletText("LMB click on terrain to place object");
    ImGui::BulletText("Objects are placed at terrain height");
    
    if (!objectPlacementEnabled_) {
        ImGui::EndDisabled();
    }
}

void EditorUI::drawViewport(World& world) {
    (void)world;
    if (!ImGui::Begin("Viewport")) {
        ImGui::End();
        return;
    }

    viewportFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // Content rect in screen space (for picking)
    const ImVec2 contentMin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 contentMax = ImVec2(contentMin.x + (avail.x > 0 ? avail.x : 0.0f), contentMin.y + (avail.y > 0 ? avail.y : 0.0f));
    viewportRectMin_ = contentMin;
    viewportRectMax_ = contentMax;

    if (viewportTex_ != 0 && avail.x > 1.0f && avail.y > 1.0f) {
        // Note: ImGui DX12 backend expects ImTextureID to contain a D3D12_GPU_DESCRIPTOR_HANDLE (as ImU64).
        ImGui::Image(viewportTex_, avail);
        viewportHovered_ = ImGui::IsItemHovered();
    } else {
        ImGui::TextUnformatted("Viewport not ready.");
        viewportHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    }

    ImGui::End();
}

void EditorUI::drawGameView(World& world) {
    // Only show Game View while game mode is active (PIE-like).
    if (!gameMode_ || !gameMode_->isGameModeActive()) {
        gameViewRectMin_ = ImVec2(0, 0);
        gameViewRectMax_ = ImVec2(0, 0);
        gameViewHovered_ = false;
        gameViewFocused_ = false;
        return;
    }

    if (!ImGui::Begin("Game View")) {
        gameViewFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        gameViewHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::End();
        return;
    }

    gameViewFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    const ImVec2 contentMin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 contentMax = ImVec2(contentMin.x + (avail.x > 0 ? avail.x : 0.0f), contentMin.y + (avail.y > 0 ? avail.y : 0.0f));
    gameViewRectMin_ = contentMin;
    gameViewRectMax_ = contentMax;

    if (viewportTex_ != 0 && avail.x > 1.0f && avail.y > 1.0f) {
        ImGui::Image(viewportTex_, avail);
        gameViewHovered_ = ImGui::IsItemHovered();
        
        // Draw Hero HUD overlay on top of game view
        drawHeroHUD(world);
    } else {
        ImGui::TextUnformatted("Game View not ready.");
        gameViewHovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    }

    ImGui::End();
}

void EditorUI::drawHierarchy(World& world) {
    if (!ImGui::Begin("Hierarchy")) {
        ImGui::End();
        return;
    }

    auto& em = world.getEntityManager();
    auto& reg = em.getRegistry();

    // Quick create buttons
    if (ImGui::Button("Create Entity")) {
        Entity e = world.createEntity("Entity");
        world.addComponent<TransformComponent>(e);
        selected_ = e;
        dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Material")) {
        Entity m = world.createEntity("Material");
        world.addComponent<MaterialComponent>(m, "Material");
        selected_ = m;
        dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Cube")) {
        Entity e = world.createEntity("Cube");
        world.addComponent<TransformComponent>(e);
        auto& mesh = world.addComponent<MeshComponent>(e);
        FillUnitCube(mesh);
        mesh.materialEntity = EnsureDefaultMaterial(world, "CubeMaterial", Vec3(1.0f, 0.0f, 0.0f));
        selected_ = e;
        dirty_ = true;
    }

    ImGui::Separator();

    // Search filter
    ImGui::Text("Search:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##HierarchySearch", hierarchySearchBuffer_, sizeof(hierarchySearchBuffer_))) {
        // Filter updated
    }

    ImGui::Separator();

    // Filter checkboxes
    ImGui::Text("Filters:");
    ImGui::Checkbox("Terrain", &hierarchyShowTerrain_);
    ImGui::SameLine();
    ImGui::Checkbox("Objects", &hierarchyShowObjects_);
    ImGui::SameLine();
    ImGui::Checkbox("Creeps", &hierarchyShowCreeps_);
    ImGui::Checkbox("Materials", &hierarchyShowMaterials_);
    ImGui::SameLine();
    ImGui::Checkbox("Meshes", &hierarchyShowMeshes_);
    ImGui::SameLine();
    ImGui::Checkbox("Others", &hierarchyShowOthers_);

    ImGui::Separator();

    // Group entities by type
    Vector<Entity> terrainEntities;
    Vector<Entity> objectEntities;
    Vector<Entity> creepEntities;
    Vector<Entity> materialEntities;
    Vector<Entity> meshEntities;
    Vector<Entity> otherEntities;

    auto nameView = reg.view<NameComponent>();
    String searchLower = hierarchySearchBuffer_;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    for (auto entity : nameView) {
        const auto& name = nameView.get<NameComponent>(entity).name;
        
        // Apply search filter
        if (!searchLower.empty()) {
            String nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find(searchLower) == String::npos) {
                continue;
            }
        }

        // Categorize entity
        bool categorized = false;

        if (reg.all_of<TerrainComponent>(entity)) {
            if (hierarchyShowTerrain_) {
                terrainEntities.push_back(entity);
            }
            categorized = true;
        } else if (reg.all_of<ObjectComponent>(entity)) {
            if (hierarchyShowObjects_) {
                objectEntities.push_back(entity);
            }
            categorized = true;
        } else if (reg.all_of<CreepComponent>(entity)) {
            if (hierarchyShowCreeps_) {
                creepEntities.push_back(entity);
            }
            categorized = true;
        } else if (reg.all_of<MaterialComponent>(entity) && !reg.all_of<MeshComponent>(entity)) {
            if (hierarchyShowMaterials_) {
                materialEntities.push_back(entity);
            }
            categorized = true;
        } else if (reg.all_of<MeshComponent>(entity)) {
            if (hierarchyShowMeshes_) {
                meshEntities.push_back(entity);
            }
            categorized = true;
        } else {
            if (hierarchyShowOthers_) {
                otherEntities.push_back(entity);
            }
            categorized = true;
        }
    }

    // Helper lambda to draw entity list
    auto drawEntityList = [this, &reg, &world](const Vector<Entity>& entities, const char* categoryName, ImU32 color) {
        if (entities.empty()) {
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        bool open = ImGui::TreeNodeEx(categoryName, ImGuiTreeNodeFlags_DefaultOpen, "%s (%d)", categoryName, static_cast<int>(entities.size()));
        ImGui::PopStyleColor();

        if (open) {
            for (Entity entity : entities) {
                if (!reg.all_of<NameComponent>(entity)) {
                    continue;
                }

                const auto& name = reg.get<NameComponent>(entity).name;
                const bool isSelected = (entity == selected_);

                // Get entity type icon/prefix
                const char* prefix = "";
                if (reg.all_of<TerrainComponent>(entity)) {
                    prefix = "[T] ";
                } else if (reg.all_of<ObjectComponent>(entity)) {
                    auto& objComp = reg.get<ObjectComponent>(entity);
                    switch (objComp.type) {
                        case ObjectType::Tower: prefix = "[Tower] "; break;
                        case ObjectType::CreepSpawn: prefix = "[Spawn] "; break;
                        case ObjectType::NeutralCamp: prefix = "[Camp] "; break;
                        case ObjectType::Building: prefix = "[Bld] "; break;
                        default: prefix = "[Obj] "; break;
                    }
                } else if (reg.all_of<CreepComponent>(entity)) {
                    prefix = "[Creep] ";
                } else if (reg.all_of<MaterialComponent>(entity)) {
                    prefix = "[Mat] ";
                } else if (reg.all_of<MeshComponent>(entity)) {
                    prefix = "[Mesh] ";
                }

                char label[256];
                std::snprintf(label, sizeof(label), "%s%s##%u", prefix, name.c_str(), static_cast<u32>(entity));

                ImGui::PushID(static_cast<int>(static_cast<u32>(entity)));
                if (ImGui::Selectable(label, isSelected)) {
                    selected_ = entity;
                }

                // Right-click context menu
                if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
                    if (ImGui::MenuItem("Delete")) {
                        // Mark for deletion (will be handled in ensureSelectionValid)
                        if (selected_ == entity) {
                            selected_ = INVALID_ENTITY;
                        }
                        world.destroyEntity(entity);
                        dirty_ = true;
                    }
                    if (ImGui::MenuItem("Focus")) {
                        selected_ = entity;
                        // TODO: Focus camera on entity
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    };

    // Draw grouped entities
    drawEntityList(terrainEntities, "Terrain", IM_COL32(100, 200, 100, 255));
    drawEntityList(objectEntities, "Objects", IM_COL32(200, 150, 100, 255));
    drawEntityList(creepEntities, "Creeps", IM_COL32(100, 200, 200, 255));
    drawEntityList(materialEntities, "Materials", IM_COL32(200, 200, 100, 255));
    drawEntityList(meshEntities, "Meshes", IM_COL32(150, 150, 200, 255));
    drawEntityList(otherEntities, "Others", IM_COL32(150, 150, 150, 255));

    ImGui::End();
}

void EditorUI::drawInspector(World& world) {
    if (!ImGui::Begin("Inspector")) {
        ImGui::End();
        return;
    }

    if (selected_ == INVALID_ENTITY || !world.isValid(selected_)) {
        ImGui::TextUnformatted("No entity selected.");
        ImGui::End();
        return;
    }

    ImGui::Text("Entity: %u", static_cast<u32>(selected_));

    if (ImGui::Button("Delete Entity")) {
        world.destroyEntity(selected_);
        selected_ = INVALID_ENTITY;
        dirty_ = true;
        ImGui::End();
        return;
    }

    ImGui::Separator();

    drawName(world, selected_);
    drawTransform(world, selected_);
    drawMesh(world, selected_);
    drawMaterial(world, selected_);
    
    // Draw ObjectComponent if present
    if (world.hasComponent<ObjectComponent>(selected_)) {
        ImGui::Separator();
        ImGui::Text("Object Component");
        auto& objComp = world.getComponent<ObjectComponent>(selected_);
        
        const char* typeNames[] = { "None", "Tower", "Creep Spawn", "Neutral Camp", "Tree", "Rock", "Building", "Waypoint", "Base", "Custom" };
        int typeIdx = static_cast<int>(objComp.type);
        if (ImGui::Combo("Type", &typeIdx, typeNames, IM_ARRAYSIZE(typeNames))) {
            objComp.type = static_cast<ObjectType>(typeIdx);
            markDirty();
        }
        
        if (ImGui::DragInt("Team ID", &objComp.teamId, 0.1f, 0, 2)) {
            markDirty();
        }

        // Tower combat settings
        if (objComp.type == ObjectType::Tower) {
            if (ImGui::DragFloat("Attack Range", &objComp.attackRange, 1.0f, 0.0f, 5000.0f, "%.1f")) {
                markDirty();
            }
            if (ImGui::DragFloat("Attack Damage", &objComp.attackDamage, 1.0f, 0.0f, 10000.0f, "%.1f")) {
                markDirty();
            }
            if (ImGui::DragFloat("Attack Speed", &objComp.attackSpeed, 0.05f, 0.05f, 10.0f, "%.2f")) {
                markDirty();
            }
        }
        
        if (objComp.type == ObjectType::CreepSpawn || objComp.type == ObjectType::NeutralCamp) {
            if (ImGui::DragFloat("Spawn Radius", &objComp.spawnRadius, 0.1f, 1.0f, 20.0f)) {
                markDirty();
            }
            if (ImGui::DragInt("Max Units", &objComp.maxUnits, 0.1f, 1, 10)) {
                markDirty();
            }
        }
        
        // Lane selection for CreepSpawn
        if (objComp.type == ObjectType::CreepSpawn) {
            const char* laneNames[] = { "All Lanes", "Top", "Middle", "Bottom" };
            int laneComboValue = objComp.spawnLane + 1; // -1 becomes 0, 0-2 become 1-3
            if (ImGui::Combo("Spawn Lane", &laneComboValue, laneNames, IM_ARRAYSIZE(laneNames))) {
                objComp.spawnLane = laneComboValue - 1; // Convert back: 0 becomes -1, 1-3 become 0-2
                markDirty();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Select which lane(s) creeps should spawn for. 'All Lanes' spawns for Top, Middle, and Bottom.");
            }
        }
        
        if (objComp.type == ObjectType::Waypoint) {
            if (ImGui::DragInt("Waypoint Order", &objComp.waypointOrder, 1.0f, 0, 100)) {
                markDirty();
            }
            const char* waypointLaneNames[] = { "All Lanes", "Top", "Middle", "Bottom" };
            int waypointLaneComboValue = objComp.waypointLane + 1;
            if (ImGui::Combo("Waypoint Lane", &waypointLaneComboValue, waypointLaneNames, IM_ARRAYSIZE(waypointLaneNames))) {
                objComp.waypointLane = waypointLaneComboValue - 1;
                markDirty();
            }
        }
    }

    // Draw CreepComponent if present (runtime units)
    if (world.hasComponent<CreepComponent>(selected_)) {
        ImGui::Separator();
        ImGui::Text("Creep Component");
        auto& creep = world.getComponent<CreepComponent>(selected_);

        if (ImGui::DragFloat("Attack Range", &creep.attackRange, 0.1f, 0.0f, 5000.0f, "%.2f")) {
            // Not serialized; runtime tweak.
        }
        ImGui::DragFloat("Damage", &creep.damage, 0.1f, 0.0f, 10000.0f, "%.1f");
        ImGui::DragFloat("Attack Speed", &creep.attackSpeed, 0.05f, 0.05f, 10.0f, "%.2f");
        ImGui::DragFloat("Move Speed", &creep.moveSpeed, 0.1f, 0.0f, 200.0f, "%.1f");
        ImGui::DragFloat("Armor", &creep.armor, 0.1f, -50.0f, 50.0f, "%.1f");

        ImGui::Separator();
        ImGui::Text("HP: %.0f / %.0f", creep.currentHealth, creep.maxHealth);
        if (ImGui::Button("Kill Creep")) {
            creep.currentHealth = 0.0f;
            creep.state = CreepState::Dead;
            creep.deathTime = 0.0f;
            if (world.hasComponent<MeshComponent>(selected_)) {
                world.getComponent<MeshComponent>(selected_).visible = false;
            }
        }
    }

    ImGui::End();
}

void EditorUI::drawStats(World& world) {
    if (!ImGui::Begin("Stats")) {
        ImGui::End();
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f", io.Framerate);
    ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);
    ImGui::Text("Entities: %zu", world.getEntityCount());

    ImGui::Separator();
    ImGui::Text("Debug Visualization");
    ImGui::Checkbox("Show Unit Attack Ranges", &showUnitAttackRanges_);

    ImGui::End();
}

void EditorUI::drawPathVisualizationPanel(World& world) {
    if (!ImGui::Begin("Path Visualization")) {
        ImGui::End();
        return;
    }
    
    ImGui::Checkbox("Show Paths Info", &showPathVisualization_);
    ImGui::SameLine();
    ImGui::Checkbox("Show 3D Lines", &showPathLines_);
    
    if (showPathVisualization_) {
        auto& reg = world.getEntityManager().getRegistry();
        auto waypointView = reg.view<ObjectComponent, TransformComponent>();
        
        // Group waypoints by team and lane
        struct PathInfo {
            i32 teamId;
            i32 lane;
            Vector<std::pair<Vec3, i32>> waypoints; // position, order
        };
        Vector<PathInfo> paths;
        
        for (auto entity : waypointView) {
            const auto& objComp = waypointView.get<ObjectComponent>(entity);
            const auto& transform = waypointView.get<TransformComponent>(entity);
            
            if (objComp.type == ObjectType::Waypoint) {
                // Find or create path
                bool found = false;
                for (auto& path : paths) {
                    if (path.teamId == objComp.teamId && path.lane == objComp.waypointLane) {
                        path.waypoints.push_back({transform.position, objComp.waypointOrder});
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    PathInfo newPath;
                    newPath.teamId = objComp.teamId;
                    newPath.lane = objComp.waypointLane;
                    newPath.waypoints.push_back({transform.position, objComp.waypointOrder});
                    paths.push_back(newPath);
                }
            }
        }
        
        // Sort waypoints by order for each path
        for (auto& path : paths) {
            std::sort(path.waypoints.begin(), path.waypoints.end(),
                     [](const std::pair<Vec3, i32>& a, const std::pair<Vec3, i32>& b) {
                         return a.second < b.second;
                     });
        }
        
        ImGui::Separator();
        ImGui::Text("Paths: %d", static_cast<int>(paths.size()));
        
        for (const auto& path : paths) {
            const char* laneNames[] = { "All", "Top", "Middle", "Bottom" };
            const char* teamNames[] = { "Neutral", "Team 1", "Team 2" };
            const char* laneName = (path.lane >= 0 && path.lane <= 2) ? laneNames[path.lane + 1] : laneNames[0];
            const char* teamName = (path.teamId >= 0 && path.teamId <= 2) ? teamNames[path.teamId] : teamNames[0];
            
            // Debug: show teamId and lane values to help diagnose issues
            ImGui::Text("%s (ID:%d) - %s Lane (Lane:%d): %d waypoints", 
                       teamName, path.teamId, laneName, path.lane, static_cast<int>(path.waypoints.size()));
        }
        
        ImGui::Separator();
        ImGui::Text("Smart Path Guide");
        if (ImGui::Button("Auto-Create Paths for All Spawns")) {
            drawSmartPathGuide(world);
        }
        ImGui::SameLine();
        if (ImGui::Button("Auto-Number Waypoints")) {
            // Auto-number all waypoints
            for (i32 teamId = 1; teamId <= 2; ++teamId) {
                for (i32 lane = 0; lane <= 2; ++lane) {
                    autoNumberWaypoints(world, teamId, lane);
                }
            }
        }
    }
    
    ImGui::End();
}

void EditorUI::drawSmartPathGuide(World& world) {
    auto& reg = world.getEntityManager().getRegistry();
    
    // Find all CreepSpawn entities
    Vector<Entity> spawns;
    auto spawnView = reg.view<ObjectComponent, TransformComponent>();
    for (auto entity : spawnView) {
        const auto& objComp = spawnView.get<ObjectComponent>(entity);
        if (objComp.type == ObjectType::CreepSpawn) {
            spawns.push_back(entity);
        }
    }
    
    // Find all Base entities
    Vector<Entity> bases;
    for (auto entity : spawnView) {
        const auto& objComp = spawnView.get<ObjectComponent>(entity);
        if (objComp.type == ObjectType::Base) {
            bases.push_back(entity);
        }
    }
    
    // For each spawn, find matching base and create path
    for (auto spawnEntity : spawns) {
        const auto& spawnObj = world.getComponent<ObjectComponent>(spawnEntity);
        const auto& spawnTransform = world.getComponent<TransformComponent>(spawnEntity);
        
        // Skip spawns with teamId = 0 (Neutral) - they don't have a team
        if (spawnObj.teamId == 0) {
            continue;
        }
        
        // Find base for enemy team
        i32 enemyTeamId = (spawnObj.teamId == 1) ? 2 : 1;
        Entity targetBase = INVALID_ENTITY;
        for (auto baseEntity : bases) {
            const auto& baseObj = world.getComponent<ObjectComponent>(baseEntity);
            if (baseObj.teamId == enemyTeamId) {
                targetBase = baseEntity;
                break;
            }
        }
        
        if (targetBase != INVALID_ENTITY) {
            // Create paths for each lane
            if (spawnObj.spawnLane == -1) {
                // All lanes
                for (i32 lane = 0; lane <= 2; ++lane) {
                    createPathBetweenSpawnAndBase(world, spawnEntity, targetBase, lane);
                }
            } else {
                // Specific lane
                createPathBetweenSpawnAndBase(world, spawnEntity, targetBase, spawnObj.spawnLane);
            }
        }
    }
}

void EditorUI::createPathBetweenSpawnAndBase(World& world, Entity spawnEntity, Entity baseEntity, i32 lane, i32 numWaypoints) {
    auto& reg = world.getEntityManager().getRegistry();
    
    if (!world.hasComponent<TransformComponent>(spawnEntity) || !world.hasComponent<TransformComponent>(baseEntity)) {
        return;
    }
    
    const auto& spawnTransform = world.getComponent<TransformComponent>(spawnEntity);
    const auto& baseTransform = world.getComponent<TransformComponent>(baseEntity);
    const auto& spawnObj = world.getComponent<ObjectComponent>(spawnEntity);
    
    Vec3 startPos = spawnTransform.position;
    Vec3 endPos = baseTransform.position;
    
    // Create waypoints between spawn and base
    for (i32 i = 0; i < numWaypoints; ++i) {
        f32 t = static_cast<f32>(i + 1) / static_cast<f32>(numWaypoints + 1);
        Vec3 waypointPos = startPos + (endPos - startPos) * t;
        
        // Sample terrain height
        auto terrainView = reg.view<TerrainComponent, TransformComponent>();
        for (auto terrainEntity : terrainView) {
            const auto& terrain = terrainView.get<TerrainComponent>(terrainEntity);
            const auto& terrainTransform = terrainView.get<TransformComponent>(terrainEntity);
            
            Vec3 localPos = waypointPos - terrainTransform.position;
            f32 clampedX = std::clamp(localPos.x, 0.0f, terrain.size);
            f32 clampedZ = std::clamp(localPos.z, 0.0f, terrain.size);
            
            if (terrain.resolution.x > 1 && terrain.resolution.y > 1 && terrain.size > 0.0f) {
                f32 cellSize = terrain.size / static_cast<f32>(terrain.resolution.x - 1);
                if (cellSize > 0.0f) {
                    f32 gridX = clampedX / cellSize;
                    f32 gridZ = clampedZ / cellSize;
                    i32 x = std::clamp(static_cast<i32>(std::round(gridX)), 0, terrain.resolution.x - 1);
                    i32 z = std::clamp(static_cast<i32>(std::round(gridZ)), 0, terrain.resolution.y - 1);
                    f32 height = terrain.getHeightAt(x, z);
                    waypointPos = Vec3(terrainTransform.position.x + clampedX, height + 2.0f, terrainTransform.position.z + clampedZ);
                    break;
                }
            }
        }
        
        // Create waypoint entity
        const String waypointNameStr =
            "Waypoint_" + std::to_string(spawnObj.teamId) + "_" + std::to_string(lane) + "_" + std::to_string(i);
        Entity waypointEntity = world.createEntity(waypointNameStr);
        
        auto& waypointObj = world.getEntityManager().addComponent<ObjectComponent>(waypointEntity);
        waypointObj.type = ObjectType::Waypoint;
        waypointObj.teamId = spawnObj.teamId;
        waypointObj.waypointLane = lane;
        waypointObj.waypointOrder = i;
        
        auto& waypointTransform = world.getEntityManager().addComponent<TransformComponent>(waypointEntity);
        waypointTransform.position = waypointPos;
        waypointTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
        
        // NOTE: EntityManager::createEntity already adds NameComponent, so do not add it again.
        auto& waypointName = world.getEntityManager().getComponent<NameComponent>(waypointEntity);
        waypointName.name = waypointNameStr;
        
        // Create mesh for waypoint
        auto& waypointMesh = world.getEntityManager().addComponent<MeshComponent>(waypointEntity);
        MeshGenerators::GenerateSphere(waypointMesh, 1.5f, 16);
        waypointMesh.name = waypointName.name;
        waypointMesh.visible = true;
        
        // Create material for waypoint
        Entity waypointMatE = world.createEntity(waypointName.name + "_Material");
        auto& waypointMaterial = world.getEntityManager().addComponent<MaterialComponent>(waypointMatE);
        waypointMaterial.name = waypointName.name + "_Material";
        waypointMaterial.baseColor = Vec3(0.0f, 0.8f, 1.0f); // Cyan
        waypointMaterial.metallic = 0.0f;
        waypointMaterial.roughness = 0.5f;
        waypointMaterial.gpuBufferCreated = false;
        waypointMesh.materialEntity = waypointMatE;
    }
}

void EditorUI::autoNumberWaypoints(World& world, i32 teamId, i32 lane) {
    auto& reg = world.getEntityManager().getRegistry();
    
    // Collect all waypoints for this team and lane
    struct WaypointInfo {
        Entity entity;
        Vec3 position;
        i32 currentOrder;
    };
    Vector<WaypointInfo> waypoints;
    
    auto waypointView = reg.view<ObjectComponent, TransformComponent>();
    for (auto entity : waypointView) {
        const auto& objComp = waypointView.get<ObjectComponent>(entity);
        const auto& transform = waypointView.get<TransformComponent>(entity);
        
        if (objComp.type == ObjectType::Waypoint) {
            bool matchesTeam = (objComp.teamId == teamId || objComp.teamId == 0);
            bool matchesLane = (objComp.waypointLane == lane || objComp.waypointLane == -1);
            
            if (matchesTeam && matchesLane) {
                waypoints.push_back({entity, transform.position, objComp.waypointOrder});
            }
        }
    }
    
    // Find spawn point to determine direction
    Vec3 spawnPos(0.0f);
    bool foundSpawn = false;
    for (auto entity : waypointView) {
        const auto& objComp = waypointView.get<ObjectComponent>(entity);
        if (objComp.type == ObjectType::CreepSpawn && objComp.teamId == teamId) {
            const auto& transform = waypointView.get<TransformComponent>(entity);
            spawnPos = transform.position;
            foundSpawn = true;
            break;
        }
    }
    
    // Sort waypoints by distance from spawn (or by current order if no spawn)
    if (foundSpawn) {
        std::sort(waypoints.begin(), waypoints.end(),
                 [spawnPos](const WaypointInfo& a, const WaypointInfo& b) {
                     f32 distA = glm::length(a.position - spawnPos);
                     f32 distB = glm::length(b.position - spawnPos);
                     return distA < distB;
                 });
    } else {
        std::sort(waypoints.begin(), waypoints.end(),
                 [](const WaypointInfo& a, const WaypointInfo& b) {
                     return a.currentOrder < b.currentOrder;
                 });
    }
    
    // Renumber waypoints
    for (size_t i = 0; i < waypoints.size(); ++i) {
        auto& objComp = reg.get<ObjectComponent>(waypoints[i].entity);
        objComp.waypointOrder = static_cast<i32>(i);
    }
}

bool EditorUI::validatePath(World& world, Entity spawnEntity, i32 lane) {
    auto& reg = world.getEntityManager().getRegistry();
    
    if (!world.hasComponent<ObjectComponent>(spawnEntity)) {
        return false;
    }
    
    const auto& spawnObj = world.getComponent<ObjectComponent>(spawnEntity);
    
    // Find waypoints for this spawn's team and lane
    Vector<Entity> waypoints;
    auto waypointView = reg.view<ObjectComponent>();
    for (auto entity : waypointView) {
        const auto& objComp = waypointView.get<ObjectComponent>(entity);
        if (objComp.type == ObjectType::Waypoint) {
            bool matchesTeam = (objComp.teamId == spawnObj.teamId || objComp.teamId == 0);
            bool matchesLane = (objComp.waypointLane == lane || objComp.waypointLane == -1);
            
            if (matchesTeam && matchesLane) {
                waypoints.push_back(entity);
            }
        }
    }
    
    // Check if we have at least one waypoint or a base
    if (waypoints.empty()) {
        // Check if there's a base for enemy team
        i32 enemyTeamId = (spawnObj.teamId == 1) ? 2 : 1;
        for (auto entity : waypointView) {
            const auto& objComp = waypointView.get<ObjectComponent>(entity);
            if (objComp.type == ObjectType::Base && objComp.teamId == enemyTeamId) {
                return true; // Path exists (direct to base)
            }
        }
        return false;
    }
    
    return true; // Path exists
}

void EditorUI::drawCameraPanel(EditorCamera& camera) {
    if (!ImGui::Begin("Camera")) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Controls: RMB look, WASD move, Q/E down/up, Shift fast.");

    // Projection / map mode
    if (ImGui::Checkbox("Map View (ortho + top-down)", &camera.lockTopDown)) {
        if (camera.lockTopDown) {
            camera.orthographic = true;
        }
    }
    if (camera.orthographic) {
        ImGui::DragFloat("Ortho half-height (zoom)", &camera.orthoHalfHeight, 0.25f, 1.0f, 5000.0f, "%.2f");
        if (camera.lockTopDown) {
            ImGui::TextUnformatted("Map controls: RMB rotates (yaw), WASD pan, Q/E zoom.");
        } else {
            ImGui::TextUnformatted("Note: tilting in ortho will skew the map (parallelogram). Enable Map View.");
        }
    } else {
        ImGui::DragFloat("FOV (deg)", &camera.fovDeg, 0.1f, 10.0f, 120.0f);
    }

    float pos[3] = { camera.position.x, camera.position.y, camera.position.z };
    if (ImGui::DragFloat3("Position", pos, 0.05f)) {
        camera.position = Vec3(pos[0], pos[1], pos[2]);
    }
    ImGui::DragFloat("Yaw (deg)", &camera.yawDeg, 0.25f);
    ImGui::DragFloat("Pitch (deg)", &camera.pitchDeg, 0.25f, -89.0f, 89.0f);

    ImGui::Separator();
    ImGui::DragFloat("Move speed", &camera.moveSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Fast multiplier", &camera.fastMultiplier, 0.1f, 1.0f, 20.0f);
    ImGui::DragFloat("Mouse sensitivity", &camera.mouseSensitivity, 0.01f, 0.01f, 2.0f);

    if (ImGui::Button("Reset")) {
        camera.reset();
    }

    ImGui::End();
}

void EditorUI::ensureSelectionValid(World& world) {
    if (selected_ != INVALID_ENTITY && !world.isValid(selected_)) {
        selected_ = INVALID_ENTITY;
    }
}

void EditorUI::drawName(World& world, Entity e) {
    if (!world.hasComponent<NameComponent>(e)) {
        return;
    }

    if (!ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    auto& name = world.getComponent<NameComponent>(e).name;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", name.c_str());
    if (ImGui::InputText("##Name", buf, sizeof(buf))) {
        name = buf;
    }
}

void EditorUI::drawTransform(World& world, Entity e) {
    if (!world.hasComponent<TransformComponent>(e)) {
        return;
    }

    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    auto& tr = world.getComponent<TransformComponent>(e);
    drawComponentProperties(e, ComponentSlot::Transform, &tr);
    DragEulerDegrees("Rotation (deg)", tr.rotation);
}

void EditorUI::drawMesh(World& world, Entity e) {
    if (!world.hasComponent<MeshComponent>(e)) {
        return;
    }

    if (!ImGui::CollapsingHeader("Mesh")) {
        return;
    }

    auto& mesh = world.getComponent<MeshComponent>(e);
    ImGui::Checkbox("Visible", &mesh.visible);
    ImGui::Text("Vertices: %zu", mesh.getVertexCount());
    ImGui::Text("Indices: %zu", mesh.getIndexCount());
    ImGui::Text("Material entity: %s", mesh.materialEntity == INVALID_ENTITY ? "None" : "Set");
}

void EditorUI::drawMaterial(World& world, Entity e) {
    if (!world.hasComponent<MaterialComponent>(e)) {
        return;
    }

    if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    auto& mat = world.getComponent<MaterialComponent>(e);
    const bool changed = drawComponentProperties(e, ComponentSlot::Material, &mat);
    if (changed) {
        mat.gpuBufferCreated = false;
    }
}

void EditorUI::createTestMap(World& world) {
    // Clear existing entities
    world.clearEntities();
    selected_ = INVALID_ENTITY;
    undo_.clear();
    redo_.clear();
    activeEditOld_.clear();
    
    // Create terrain
    Entity terrainE = world.createEntity("Terrain");
    auto& terrainTransform = world.addComponent<TransformComponent>(terrainE);
    terrainTransform.position = Vec3(0.0f, 0.0f, 0.0f);
    
    auto& terrain = world.addComponent<TerrainComponent>(terrainE);
    // Initialize tile terrain with test map size matching object coordinates
    // Test map objects are placed for ~300 unit map
    // Use 3x3 tiles = 4x4 vertices, size = 3 * 128 = 384 units (close to original 300)
    // This matches the object placement coordinates in createTestMap
    WorldEditor::TerrainTools::initTileTerrain(terrain, 3, 3, 128.0f, 128.0f);
    WorldEditor::TerrainTools::syncHeightmapFromLevels(terrain);
    
    auto& terrainMesh = world.addComponent<MeshComponent>(terrainE);
    terrainMesh.name = "Terrain";
    TerrainMesh::buildMesh(terrain, terrainMesh);
    
    // Generate wireframe grid
    if (renderer_ && renderer_->GetWireframeGrid()) {
        renderer_->GetWireframeGrid()->generateGrid(terrain, terrainMesh);
    }
    
    // Add terrain material
    auto& terrainMat = world.addComponent<TerrainMaterialComponent>(terrainE);
    Entity terrainMatE = world.createEntity("TerrainMaterial");
    auto& terrainMaterial = world.addComponent<MaterialComponent>(terrainMatE);
    terrainMaterial.name = "TerrainMaterial";
    terrainMaterial.baseColor = Vec3(0.25f, 0.6f, 0.25f);
    terrainMaterial.gpuBufferCreated = false;
    terrainMesh.materialEntity = terrainMatE;
    
    // Helper function to create object at position
    u32 createObjectSerial = 0;
    u32 objectsCreated = 0;

    auto createObject = [&world, &terrain, &terrainTransform, &terrainE, this, &createObjectSerial, &objectsCreated](ObjectType type, const Vec3& worldPos, int teamId = 0, float spawnRadius = 5.0f, int maxUnits = 3, const Vec3& scale = Vec3(1.0f), int spawnLane = -1) -> Entity {
        const u32 serial = ++createObjectSerial;
        objectsCreated++;

        const String objTypeNames[] = {
            "None", "Tower", "CreepSpawn", "NeutralCamp", "Tree", "Rock", "Building", "Waypoint", "Base", "Custom"
        };
        const int typeIdx = static_cast<int>(type);
        const String objName = (typeIdx >= 0 && typeIdx < 10)
            ? objTypeNames[typeIdx] + "_" + std::to_string(serial)
            : "Object_" + std::to_string(serial);
        
        Entity objE = world.createEntity(objName);
        auto& objTransform = world.addComponent<TransformComponent>(objE);
        
        // Sample terrain height at position
        const Vec3 localPos = worldPos - terrainTransform.position;
        float height = 0.0f;
        // Terrain goes from (0,0,0) to (size, 0, size), so clamp to [0, size]
        const float clampedX = std::clamp(localPos.x, 0.0f, terrain.size);
        const float clampedZ = std::clamp(localPos.z, 0.0f, terrain.size);
        
        if (terrain.resolution.x > 1 && terrain.resolution.y > 1 && terrain.size > 0.0f) {
            const float cellSize = terrain.size / float(terrain.resolution.x - 1);
            if (cellSize > 0.0f) {
                // Convert to grid coordinates (0 to resolution-1)
                const float gridX = clampedX / cellSize;
                const float gridZ = clampedZ / cellSize;
                const int x = std::clamp(static_cast<int>(std::round(gridX)), 0, terrain.resolution.x - 1);
                const int z = std::clamp(static_cast<int>(std::round(gridZ)), 0, terrain.resolution.y - 1);
                height = terrain.getHeightAt(x, z);
            }
        }
        objTransform.position = Vec3(terrainTransform.position.x + clampedX, height, terrainTransform.position.z + clampedZ);
        objTransform.scale = scale;
        
        auto& objComp = world.addComponent<ObjectComponent>(objE);
        objComp.type = type;
        objComp.teamId = teamId;
        objComp.spawnRadius = spawnRadius;
        objComp.maxUnits = maxUnits;
        objComp.spawnLane = spawnLane; // -1 = all lanes, 0 = Top, 1 = Middle, 2 = Bottom
        
        // Add health component for towers and buildings
        if (type == ObjectType::Tower || type == ObjectType::Building || type == ObjectType::Base) {
            auto& health = world.addComponent<HealthComponent>(objE);
            if (type == ObjectType::Tower) {
                health.maxHealth = 1600.0f; // Tower health
                health.currentHealth = 1600.0f;
                health.armor = 10.0f;
            } else if (type == ObjectType::Base) {
                health.maxHealth = 5000.0f; // Base health (very high)
                health.currentHealth = 5000.0f;
                health.armor = 20.0f;
            } else {
                health.maxHealth = 2500.0f; // Building health
                health.currentHealth = 2500.0f;
                health.armor = 15.0f;
            }
        }
        
        // Create mesh
        auto& mesh = world.addComponent<MeshComponent>(objE);
        mesh.name = objName;
        mesh.visible = true;
        
        Vec3 objectColor(0.5f, 0.5f, 0.5f);
        Vec3 collisionSize(3.0f, 5.0f, 3.0f); // Default collision size
        
        // Generate mesh shape based on object type (larger sizes for better visibility)
        switch (type) {
            case ObjectType::Tower: {
                MeshGenerators::GenerateCylinder(mesh, 2.5f, 12.0f, 16); // Larger: radius 2.5, height 12
                objectColor = Vec3(1.0f, 0.1f, 0.1f); // Bright red
                collisionSize = Vec3(5.0f, 12.0f, 5.0f); // Match cylinder size
                break;
            }
            case ObjectType::CreepSpawn: {
                MeshGenerators::GenerateSphere(mesh, 3.5f, 16); // Larger: radius 3.5
                objectColor = Vec3(0.1f, 1.0f, 0.1f); // Bright green
                break;
            }
            case ObjectType::NeutralCamp: {
                MeshGenerators::GenerateCone(mesh, 3.5f, 6.0f, 8); // Larger: radius 3.5, height 6
                objectColor = Vec3(1.0f, 0.9f, 0.1f); // Bright yellow
                break;
            }
            case ObjectType::Building: {
                MeshGenerators::GenerateCube(mesh, Vec3(4.0f, 6.0f, 4.0f)); // Larger: 4x6x4
                // Green for Radiant (Team 1), brown for others
                if (teamId == 1) {
                    objectColor = Vec3(0.1f, 0.8f, 0.2f); // Bright green for Radiant
                } else {
                    objectColor = Vec3(0.7f, 0.6f, 0.5f); // Light brown for others
                }
                collisionSize = Vec3(4.0f, 6.0f, 4.0f); // Match cube size
                break;
            }
            case ObjectType::Waypoint: {
                MeshGenerators::GenerateSphere(mesh, 1.5f, 16); // Small sphere for waypoint
                objectColor = Vec3(0.0f, 0.8f, 1.0f); // Cyan
                break;
            }
            case ObjectType::Base: {
                MeshGenerators::GenerateCube(mesh, Vec3(8.0f, 10.0f, 8.0f)); // Large base
                // Green for Radiant (Team 1), red for Dire (Team 2)
                if (teamId == 1) {
                    objectColor = Vec3(0.0f, 1.0f, 0.0f); // Bright green for Radiant
                } else if (teamId == 2) {
                    objectColor = Vec3(1.0f, 0.0f, 0.0f); // Bright red for Dire
                } else {
                    objectColor = Vec3(0.5f, 0.5f, 0.5f); // Gray for neutral
                }
                collisionSize = Vec3(8.0f, 10.0f, 8.0f); // Match base size
                break;
            }
            default: {
                MeshGenerators::GenerateCube(mesh, Vec3(2.0f)); // Larger default cube
                objectColor = Vec3(0.6f, 0.6f, 0.6f); // Light gray
                break;
            }
        }
        
        // Create material with emissive for better visibility
        Entity matE = world.createEntity(objName + "_Material");
        auto& mat = world.addComponent<MaterialComponent>(matE);
        mat.name = objName + "_Material";
        mat.baseColor = objectColor;
        // Add slight emissive glow for better visibility
        mat.emissiveColor = objectColor * 0.3f; // 30% emissive glow
        mat.gpuBufferCreated = false;
        mesh.materialEntity = matE;
        
        return objE;
    };
    
    // Create minimal Dota 2-style map layout (gradual object placement)
    const float mapSize = terrain.size;
    const float centerX = mapSize * 0.5f;
    const float centerZ = mapSize * 0.5f;
    
    // ========== RADIANT BASE (Team 1) ==========
    // Используем точные координаты из спецификации
    
    // Base (Radiant) - use ObjectType::Base so creeps can find it as destination.
    Entity radiantBaseE = createObject(ObjectType::Base, Vec3(26.0f, 0.0f, 26.0f), 1, 5.0f, 3, Vec3(1.0f));
    // Ancient visual (optional building near base)
    Entity ancientE = createObject(ObjectType::Building, Vec3(26.0f, 0.0f, 26.0f), 1, 5.0f, 3, Vec3(5.0f, 1.0f, 5.0f));
    // Тавер (башня рядом с барраками) - радиант базовый
    createObject(ObjectType::Tower, Vec3(30.0f, 0.0f, 50.0f), 1);
    createObject(ObjectType::Tower, Vec3(50.0f, 0.0f, 30.0f), 1);
    
    
    // ========== MIDDLE LANE ==========
    // Барраки с тавером на мид лайне (идут по диагонали)
    // Барраки (казармы) - мид лайн
    createObject(ObjectType::Building, Vec3(65.0f, 0.0f, 50.0f), 1);
    createObject(ObjectType::Building, Vec3(50.0f, 0.0f, 65.0f), 1);
    // Creep spawn between mid barracks (Radiant). Lane: Middle (1)
    createObject(ObjectType::CreepSpawn, Vec3(57.5f, 0.0f, 57.5f), 1, 8.0f, 20, Vec3(1.0f), 1);
    // Тавер (башня рядом с барраками) - мид лайн
    createObject(ObjectType::Tower, Vec3(65.0f, 0.0f, 65.0f), 1);
    // Тавер (башня) - мид лайн (T2)
    createObject(ObjectType::Tower, Vec3(97.0f, 0.0f, 82.0f), 1);
    // Тавер (башня) - мид лайн (T1)
    createObject(ObjectType::Tower, Vec3(128.0f, 0.0f, 127.0f), 1);
    
    // ========== BOT LANE ==========
    // Барраки с тавером на бот лайне (идут по диагонали)
    // Барраки (казармы) - бот лайн
    createObject(ObjectType::Building, Vec3(80.0f, 0.0f, 32.0f), 1);
    createObject(ObjectType::Building, Vec3(80.0f, 0.0f, 16.0f), 1);
    // Тавер (башня рядом с барраками) - бот лайн (T3)
    createObject(ObjectType::Tower, Vec3(95.0f, 0.0f, 24.0f), 1);
    // Тавер (башня) - бот лайн (T2)
    createObject(ObjectType::Tower, Vec3(170.0f, 0.0f, 24.0f), 1);
    // Тавер (башня) - бот лайн (T1)
    createObject(ObjectType::Tower, Vec3(245.0f, 0.0f, 24.0f), 1);

    // ========== TOP LANE ==========
    // Дубликат баррак с тавером для топ лайна (аналогично бот лайну, но для верхней линии)
    // Барраки (казармы) - топ лайн (больше по Z, меньше по X)
    createObject(ObjectType::Building, Vec3(17.5f, 0.0f, 83.0f), 1);
    createObject(ObjectType::Building, Vec3(35.0f, 0.0f, 83.0f), 1);
    // Тавер (башня рядом с барраками) - топ лайн
    createObject(ObjectType::Tower, Vec3(26.25f, 0.0f, 98.0f), 1);
    // Тавер (башня) - топ лайн (T2)
    createObject(ObjectType::Tower, Vec3(26.25f, 0.0f, 153.0f), 1);
    // Тавер (башня) - топ лайн (T1)
    createObject(ObjectType::Tower, Vec3(26.25f, 0.0f, 208.0f), 1);
    
    // ========== DIRE BASE (Team 2) ==========
    // Mirror all Radiant objects for Dire side
    // Formula: x_dire = mapSize - x_radiant, z_dire = mapSize - z_radiant
    
    // Base (Dire) - mirrored from Radiant base.
    Entity direBaseE = createObject(ObjectType::Base, Vec3(mapSize - 26.0f, 0.0f, mapSize - 26.0f), 2, 5.0f, 3, Vec3(1.0f));
    // Ancient visual (optional building near base)
    Entity direAncientE = createObject(ObjectType::Building, Vec3(mapSize - 26.0f, 0.0f, mapSize - 26.0f), 2, 5.0f, 3, Vec3(5.0f, 1.0f, 5.0f));
    // Тавер (башня рядом с барраками) - dire базовый
    createObject(ObjectType::Tower, Vec3(mapSize - 30.0f, 0.0f, mapSize - 50.0f), 2);
    createObject(ObjectType::Tower, Vec3(mapSize - 50.0f, 0.0f, mapSize - 30.0f), 2);
    
    // ========== MIDDLE LANE (Dire) ==========
    // Барраки (казармы) - мид лайн (Dire)
    createObject(ObjectType::Building, Vec3(mapSize - 65.0f, 0.0f, mapSize - 50.0f), 2);
    createObject(ObjectType::Building, Vec3(mapSize - 50.0f, 0.0f, mapSize - 65.0f), 2);
    // Creep spawn between mid barracks (Dire). Lane: Middle (1)
    createObject(ObjectType::CreepSpawn, Vec3(mapSize - 57.5f, 0.0f, mapSize - 57.5f), 2, 8.0f, 20, Vec3(1.0f), 1);
    // Тавер (башня рядом с барраками) - мид лайн (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 65.0f, 0.0f, mapSize - 65.0f), 2);
    // Тавер (башня) - мид лайн (T2) (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 97.0f, 0.0f, mapSize - 82.0f), 2);
    // Тавер (башня) - мид лайн (T1) (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 128.0f, 0.0f, mapSize - 127.0f), 2);
    
    // ========== BOT LANE (Dire) ==========
    // Для Dire бот лайн - это зеркало топ лайна Radiant
    // Барраки (казармы) - бот лайн (Dire)
    createObject(ObjectType::Building, Vec3(mapSize - 17.5f, 0.0f, mapSize - 83.0f), 2);
    createObject(ObjectType::Building, Vec3(mapSize - 35.0f, 0.0f, mapSize - 83.0f), 2);
    // Тавер (башня рядом с барраками) - бот лайн (T3) (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 26.25f, 0.0f, mapSize - 98.0f), 2);
    // Тавер (башня) - бот лайн (T2) (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 26.25f, 0.0f, mapSize - 153.0f), 2);
    // Тавер (башня) - бот лайн (T1) (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 26.25f, 0.0f, mapSize - 208.0f), 2);
    
    // ========== TOP LANE (Dire) ==========
    // Для Dire топ лайн - это зеркало бот лайна Radiant
    // Барраки (казармы) - топ лайн (Dire)
    createObject(ObjectType::Building, Vec3(mapSize - 80.0f, 0.0f, mapSize - 32.0f), 2);
    createObject(ObjectType::Building, Vec3(mapSize - 80.0f, 0.0f, mapSize - 16.0f), 2);
    // Тавер (башня рядом с барраками) - топ лайн (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 95.0f, 0.0f, mapSize - 24.0f), 2);
    // Тавер (башня) - топ лайн (T2) (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 170.0f, 0.0f, mapSize - 24.0f), 2);
    // Тавер (башня) - топ лайн (T1) (Dire)
    createObject(ObjectType::Tower, Vec3(mapSize - 245.0f, 0.0f, mapSize - 24.0f), 2);
    
    selected_ = terrainE;
    dirty_ = true;
    
    LOG_INFO("Test map created with terrain and {} objects", 
             objectsCreated);
}

void EditorUI::drawMOBAGamePanel(World& world) {
    if (!ImGui::Begin("MOBA Game Control")) {
        ImGui::End();
        return;
    }

    ImGui::Text("MOBA Game Management");
    ImGui::Separator();

    // Game state info
    bool gameActive = world.isGameActive();
    f32 gameTime = world.getGameTime();
    i32 currentWave = world.getCurrentWave();
    f32 timeToNext = world.getTimeToNextWave();

    ImGui::Text("Game Status: %s", gameActive ? "ACTIVE" : "STOPPED");
    
    if (gameActive) {
        // Format game time as MM:SS
        i32 minutes = static_cast<i32>(gameTime) / 60;
        i32 seconds = static_cast<i32>(gameTime) % 60;
        ImGui::Text("Game Time: %02d:%02d", minutes, seconds);
        
        ImGui::Text("Current Wave: %d", currentWave + 1);
        
        if (timeToNext >= 0.0f) {
            ImGui::Text("Next Wave In: %.1fs", timeToNext);
        } else {
            ImGui::Text("No More Waves");
        }
    }

    ImGui::Separator();

    // Game control buttons
    if (ImGui::Button("Start Game", ImVec2(100, 30))) {
        world.startGame();
        setDirty(true);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Pause Game", ImVec2(100, 30))) {
        world.pauseGame();
        setDirty(true);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset Game", ImVec2(100, 30))) {
        world.resetGame();
        setDirty(true);
    }

    ImGui::Separator();

    // Entity counts for debugging
    auto& registry = world.getEntityManager().getRegistry();
    
    auto creepView = registry.view<CreepComponent>();
    auto projectileView = registry.view<ProjectileComponent>();
    auto towerView = registry.view<ObjectComponent>();
    
    i32 creepCount = 0;
    i32 radiantCreeps = 0;
    i32 direCreeps = 0;
    
    for (auto entity : creepView) {
        auto& creep = creepView.get<CreepComponent>(entity);
        if (creep.state != CreepState::Dead) {
            creepCount++;
            if (creep.teamId == 1) radiantCreeps++;
            else if (creep.teamId == 2) direCreeps++;
        }
    }
    
    i32 projectileCount = 0;
    for (auto entity : projectileView) {
        auto& proj = projectileView.get<ProjectileComponent>(entity);
        if (proj.active) projectileCount++;
    }
    
    i32 towerCount = 0;
    i32 radiantTowers = 0;
    i32 direTowers = 0;
    
    for (auto entity : towerView) {
        auto& obj = towerView.get<ObjectComponent>(entity);
        if (obj.type == ObjectType::Tower) {
            towerCount++;
            if (obj.teamId == 1) radiantTowers++;
            else if (obj.teamId == 2) direTowers++;
        }
    }

    ImGui::Text("Live Entities:");
    ImGui::Text("  Total Creeps: %d", creepCount);
    ImGui::Text("    Radiant: %d", radiantCreeps);
    ImGui::Text("    Dire: %d", direCreeps);
    ImGui::Text("  Active Projectiles: %d", projectileCount);
    ImGui::Text("  Towers: %d", towerCount);
    ImGui::Text("    Radiant: %d", radiantTowers);
    ImGui::Text("    Dire: %d", direTowers);

    ImGui::Separator();

    // Quick spawn buttons for testing
    if (ImGui::CollapsingHeader("Debug Spawning")) {
        if (ImGui::Button("Spawn Test Creep Wave")) {
            // Find a creep spawn point
            auto spawnView = registry.view<ObjectComponent, TransformComponent>();
            Entity spawnPoint = INVALID_ENTITY;
            
            for (auto entity : spawnView) {
                auto& obj = spawnView.get<ObjectComponent>(entity);
                if (obj.type == ObjectType::CreepSpawn && obj.teamId == 1) {
                    spawnPoint = entity;
                    break;
                }
            }
            
            if (spawnPoint != INVALID_ENTITY) {
                // Get CreepSystem and spawn test creeps
                if (auto* creepSystem = dynamic_cast<CreepSystem*>(world.getSystem("CreepSystem"))) {
                    creepSystem->spawnCreep(spawnPoint, CreepType::Melee, 1, CreepLane::Middle);
                    creepSystem->spawnCreep(spawnPoint, CreepType::Ranged, 1, CreepLane::Middle);
                    setDirty(true);
                }
            }
        }
        
        if (ImGui::Button("Clear All Creeps")) {
            Vector<Entity> toDestroy;
            for (auto entity : creepView) {
                toDestroy.push_back(entity);
            }
            for (Entity entity : toDestroy) {
                world.destroyEntity(entity);
            }
            setDirty(true);
        }
    }

    ImGui::End();
}

// ============================================================================
// Hero HUD (Dota-like interface)
// ============================================================================

void EditorUI::drawHeroHUD(World& world) {
    // Get HeroSystem and player hero
    auto* heroSystem = dynamic_cast<HeroSystem*>(world.getSystem("HeroSystem"));
    if (!heroSystem) return;
    
    Entity playerHero = heroSystem->getPlayerHero();
    if (playerHero == INVALID_ENTITY) return;
    
    auto& registry = world.getEntityManager().getRegistry();
    if (!registry.valid(playerHero)) return;
    
    auto* heroComp = registry.try_get<HeroComponent>(playerHero);
    if (!heroComp) return;
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 viewMin = gameViewRectMin_;
    const ImVec2 viewMax = gameViewRectMax_;
    const float viewWidth = viewMax.x - viewMin.x;
    const float viewHeight = viewMax.y - viewMin.y;
    
    if (viewWidth < 100 || viewHeight < 100) return;
    
    // ========== Bottom Center HUD Panel ==========
    const float hudHeight = 120.0f;
    const float hudWidth = 600.0f;
    const float hudX = viewMin.x + (viewWidth - hudWidth) * 0.5f;
    const float hudY = viewMax.y - hudHeight - 10.0f;
    
    // HUD Background
    drawList->AddRectFilled(
        ImVec2(hudX, hudY),
        ImVec2(hudX + hudWidth, hudY + hudHeight),
        IM_COL32(20, 20, 25, 220),
        8.0f
    );
    drawList->AddRect(
        ImVec2(hudX, hudY),
        ImVec2(hudX + hudWidth, hudY + hudHeight),
        IM_COL32(60, 60, 70, 255),
        8.0f, 0, 2.0f
    );
    
    // ========== Hero Portrait (Left) ==========
    const float portraitSize = 80.0f;
    const float portraitX = hudX + 15.0f;
    const float portraitY = hudY + 20.0f;
    
    // Portrait background
    ImU32 portraitColor = heroComp->teamId == 1 ? IM_COL32(40, 80, 150, 255) : IM_COL32(150, 50, 50, 255);
    drawList->AddRectFilled(
        ImVec2(portraitX, portraitY),
        ImVec2(portraitX + portraitSize, portraitY + portraitSize),
        portraitColor,
        4.0f
    );
    
    // Hero name
    const char* heroName = heroComp->heroName.c_str();
    drawList->AddText(ImVec2(portraitX + 5, portraitY + 5), IM_COL32(255, 255, 255, 255), heroName);
    
    // Level
    char levelText[16];
    snprintf(levelText, sizeof(levelText), "Lv %d", heroComp->level);
    drawList->AddText(ImVec2(portraitX + 5, portraitY + portraitSize - 20), IM_COL32(255, 215, 0, 255), levelText);
    
    // ========== HP/Mana Bars ==========
    const float barWidth = 180.0f;
    const float barHeight = 20.0f;
    const float barX = portraitX + portraitSize + 15.0f;
    const float hpBarY = portraitY + 10.0f;
    const float manaBarY = hpBarY + barHeight + 8.0f;
    
    // HP Bar background
    drawList->AddRectFilled(
        ImVec2(barX, hpBarY),
        ImVec2(barX + barWidth, hpBarY + barHeight),
        IM_COL32(40, 20, 20, 255),
        4.0f
    );
    
    // HP Bar fill
    float hpPercent = heroComp->maxHealth > 0 ? heroComp->currentHealth / heroComp->maxHealth : 0.0f;
    hpPercent = std::clamp(hpPercent, 0.0f, 1.0f);
    drawList->AddRectFilled(
        ImVec2(barX, hpBarY),
        ImVec2(barX + barWidth * hpPercent, hpBarY + barHeight),
        IM_COL32(50, 180, 50, 255),
        4.0f
    );
    
    // HP Text
    char hpText[32];
    snprintf(hpText, sizeof(hpText), "%.0f / %.0f", heroComp->currentHealth, heroComp->maxHealth);
    ImVec2 hpTextSize = ImGui::CalcTextSize(hpText);
    drawList->AddText(
        ImVec2(barX + (barWidth - hpTextSize.x) * 0.5f, hpBarY + 2),
        IM_COL32(255, 255, 255, 255),
        hpText
    );
    
    // Mana Bar background
    drawList->AddRectFilled(
        ImVec2(barX, manaBarY),
        ImVec2(barX + barWidth, manaBarY + barHeight),
        IM_COL32(20, 20, 50, 255),
        4.0f
    );
    
    // Mana Bar fill
    float manaPercent = heroComp->maxMana > 0 ? heroComp->currentMana / heroComp->maxMana : 0.0f;
    manaPercent = std::clamp(manaPercent, 0.0f, 1.0f);
    drawList->AddRectFilled(
        ImVec2(barX, manaBarY),
        ImVec2(barX + barWidth * manaPercent, manaBarY + barHeight),
        IM_COL32(50, 100, 200, 255),
        4.0f
    );
    
    // Mana Text
    char manaText[32];
    snprintf(manaText, sizeof(manaText), "%.0f / %.0f", heroComp->currentMana, heroComp->maxMana);
    ImVec2 manaTextSize = ImGui::CalcTextSize(manaText);
    drawList->AddText(
        ImVec2(barX + (barWidth - manaTextSize.x) * 0.5f, manaBarY + 2),
        IM_COL32(255, 255, 255, 255),
        manaText
    );
    
    // ========== Ability Bar ==========
    drawHeroAbilityBar(*heroComp, ImVec2(barX, manaBarY + barHeight + 15.0f));
    
    // ========== Inventory (Right of abilities) ==========
    const float invX = barX + 220.0f; // After 4 ability slots
    const float invY = manaBarY + barHeight + 15.0f;
    drawHeroInventory(*heroComp, ImVec2(invX, invY));
    
    // ========== Stats Panel (Right side) ==========
    const float statsX = hudX + hudWidth - 150.0f;
    const float statsY = hudY + 15.0f;
    
    char statsText[256];
    snprintf(statsText, sizeof(statsText),
        "DMG: %.0f\n"
        "ARM: %.1f\n"
        "SPD: %.0f\n"
        "AS: %.0f",
        heroComp->damage,
        heroComp->armor,
        heroComp->moveSpeed,
        heroComp->attackSpeed
    );
    drawList->AddText(ImVec2(statsX, statsY), IM_COL32(200, 200, 200, 255), statsText);
    
    // ========== Gold Display ==========
    char goldText[32];
    snprintf(goldText, sizeof(goldText), "Gold: %d", heroComp->gold);
    drawList->AddText(
        ImVec2(statsX, statsY + 70),
        IM_COL32(255, 215, 0, 255),
        goldText
    );
    
    // ========== KDA Display (Top) ==========
    const float kdaX = viewMin.x + 10.0f;
    const float kdaY = viewMin.y + 10.0f;
    
    char kdaText[64];
    snprintf(kdaText, sizeof(kdaText), "K/D/A: %d/%d/%d  LH: %d",
        heroComp->kills, heroComp->deaths, heroComp->assists, heroComp->lastHits);
    
    drawList->AddRectFilled(
        ImVec2(kdaX - 5, kdaY - 3),
        ImVec2(kdaX + 180, kdaY + 20),
        IM_COL32(0, 0, 0, 180),
        4.0f
    );
    drawList->AddText(ImVec2(kdaX, kdaY), IM_COL32(255, 255, 255, 255), kdaText);
    
    // ========== Experience Bar ==========
    const float expBarWidth = 180.0f;
    const float expBarHeight = 6.0f;
    const float expBarX = barX;
    const float expBarY = hpBarY - 10.0f;
    
    // Exp bar background
    drawList->AddRectFilled(
        ImVec2(expBarX, expBarY),
        ImVec2(expBarX + expBarWidth, expBarY + expBarHeight),
        IM_COL32(30, 30, 30, 255),
        2.0f
    );
    
    // Exp bar fill
    float expPercent = heroComp->experienceToNextLevel > 0 ? 
        heroComp->experience / heroComp->experienceToNextLevel : 0.0f;
    expPercent = std::clamp(expPercent, 0.0f, 1.0f);
    drawList->AddRectFilled(
        ImVec2(expBarX, expBarY),
        ImVec2(expBarX + expBarWidth * expPercent, expBarY + expBarHeight),
        IM_COL32(180, 120, 255, 255),
        2.0f
    );
}

void EditorUI::drawHeroAbilityBar(const HeroComponent& hero, const ImVec2& screenPos) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    const float abilitySize = 45.0f;
    const float spacing = 5.0f;
    float x = screenPos.x;
    float y = screenPos.y;
    
    // Show ability points indicator if available
    if (hero.abilityPoints > 0) {
        char apText[32];
        snprintf(apText, sizeof(apText), "Skill Points: %d", hero.abilityPoints);
        drawList->AddText(ImVec2(x, y - 18), IM_COL32(255, 215, 0, 255), apText);
    }
    
    for (int i = 0; i < 4; ++i) {
        const auto& ability = hero.abilities[i];
        
        // Create invisible button for hover detection and tooltip (must be before drawing)
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        char buttonId[32];
        snprintf(buttonId, sizeof(buttonId), "ability_%d", i);
        ImGui::InvisibleButton(buttonId, ImVec2(abilitySize, abilitySize));
        
        // Ability background
        ImU32 bgColor = IM_COL32(40, 40, 50, 255);
        if (ability.level == 0) {
            bgColor = IM_COL32(30, 30, 30, 200); // Not learned
        } else if (ability.currentCooldown > 0.0f) {
            bgColor = IM_COL32(60, 30, 30, 255); // On cooldown
        }
        
        // Golden border if can level up
        bool canLevelUp = hero.abilityPoints > 0 && ability.level < ability.data.maxLevel;
        // Ultimate requires level 6/12/18
        if (i == 3 && canLevelUp) {
            i32 requiredLevel = 6 + (ability.level * 6);
            if (hero.level < requiredLevel) {
                canLevelUp = false;
            }
        }
        
        drawList->AddRectFilled(
            ImVec2(x, y),
            ImVec2(x + abilitySize, y + abilitySize),
            bgColor,
            4.0f
        );
        
        // Border - golden if can level up
        ImU32 borderColor;
        if (canLevelUp) {
            borderColor = IM_COL32(255, 215, 0, 255); // Gold
        } else if (ability.level > 0) {
            borderColor = IM_COL32(100, 100, 120, 255);
        } else {
            borderColor = IM_COL32(50, 50, 60, 255);
        }
        drawList->AddRect(
            ImVec2(x, y),
            ImVec2(x + abilitySize, y + abilitySize),
            borderColor,
            4.0f, 0, canLevelUp ? 3.0f : 2.0f
        );
        
        // "+" indicator for level up (Ctrl+hotkey)
        if (canLevelUp) {
            drawList->AddCircleFilled(
                ImVec2(x + abilitySize - 8, y + 8),
                8.0f,
                IM_COL32(255, 215, 0, 255)
            );
            drawList->AddText(
                ImVec2(x + abilitySize - 12, y + 1),
                IM_COL32(0, 0, 0, 255),
                "+"
            );
        }
        
        // Hotkey
        char hotkeyText[2] = { ability.data.hotkey, '\0' };
        drawList->AddText(ImVec2(x + 3, y + 2), IM_COL32(255, 255, 255, 200), hotkeyText);
        
        // Level indicator
        if (ability.level > 0) {
            char levelText[8];
            snprintf(levelText, sizeof(levelText), "%d", ability.level);
            drawList->AddText(
                ImVec2(x + abilitySize - 12, y + abilitySize - 15),
                IM_COL32(255, 215, 0, 255),
                levelText
            );
        }
        
        // Cooldown overlay
        if (ability.currentCooldown > 0.0f && ability.level > 0) {
            float cdPercent = ability.currentCooldown / ability.data.cooldown;
            cdPercent = std::clamp(cdPercent, 0.0f, 1.0f);
            
            drawList->AddRectFilled(
                ImVec2(x, y + abilitySize * (1.0f - cdPercent)),
                ImVec2(x + abilitySize, y + abilitySize),
                IM_COL32(0, 0, 0, 150),
                4.0f
            );
            
            // Cooldown text
            char cdText[8];
            snprintf(cdText, sizeof(cdText), "%.1f", ability.currentCooldown);
            ImVec2 cdTextSize = ImGui::CalcTextSize(cdText);
            drawList->AddText(
                ImVec2(x + (abilitySize - cdTextSize.x) * 0.5f, y + (abilitySize - cdTextSize.y) * 0.5f),
                IM_COL32(255, 255, 255, 255),
                cdText
            );
        }
        
        // Mana cost display (only if ability is learned and has mana cost) - LEFT SIDE
        if (ability.level > 0 && ability.data.manaCost > 0.0f) {
            char manaText[16];
            snprintf(manaText, sizeof(manaText), "%.0f", ability.data.manaCost);
            
            // Check if hero has enough mana
            bool hasEnoughMana = hero.currentMana >= ability.data.manaCost;
            ImU32 manaColor = hasEnoughMana ? IM_COL32(100, 150, 255, 255) : IM_COL32(255, 100, 100, 255);
            
            // Draw small background for mana text (left side)
            ImVec2 manaTextSize = ImGui::CalcTextSize(manaText);
            float manaTextX = x + 2.0f;
            float manaTextY = y + abilitySize - 12.0f;
            
            // Small background rectangle for better readability
            drawList->AddRectFilled(
                ImVec2(x + 1.0f, manaTextY - 1.0f),
                ImVec2(x + manaTextSize.x + 3.0f, y + abilitySize - 1.0f),
                IM_COL32(0, 0, 0, 180),
                2.0f
            );
            
            // Draw mana cost text
            drawList->AddText(
                ImVec2(manaTextX, manaTextY),
                manaColor,
                manaText
            );
        }
        
        // Check hover and show tooltip (after drawing so tooltip appears on top)
        if (ImGui::IsItemHovered()) {
            drawAbilityTooltip(ability, hero);
        }
        
        x += abilitySize + spacing;
    }
}

void EditorUI::drawAbilityTooltip(const HeroAbility& ability, const HeroComponent& hero) {
    ImGui::BeginTooltip();
    
    // Ability name (golden, bold)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.84f, 0.0f, 1.0f)); // Gold color
    ImGui::PushFont(nullptr); // Use default font for now, could use bold font
    ImGui::TextUnformatted(ability.data.name.c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    
    // Level indicator
    if (ability.level > 0) {
        ImGui::Text("Level: %d / %d", ability.level, ability.data.maxLevel);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::Text("Not learned");
        ImGui::PopStyleColor();
    }
    
    // Description
    if (!ability.data.description.empty()) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f); // Wrap at reasonable width
        ImGui::TextWrapped("%s", ability.data.description.c_str());
        ImGui::PopTextWrapPos();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Stats section
    
    // Mana cost
    if (ability.data.manaCost > 0.0f) {
        bool hasEnoughMana = hero.currentMana >= ability.data.manaCost;
        ImVec4 manaColor = hasEnoughMana ? ImVec4(0.4f, 0.6f, 1.0f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, manaColor);
        ImGui::Text("Mana Cost: %.0f", ability.data.manaCost);
        ImGui::PopStyleColor();
    }
    
    // Cooldown
    if (ability.data.cooldown > 0.0f) {
        ImGui::Text("Cooldown: %.1fs", ability.data.cooldown);
        if (ability.currentCooldown > 0.0f) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
            ImGui::Text("(%.1fs remaining)", ability.currentCooldown);
            ImGui::PopStyleColor();
        }
    }
    
    // Cast range
    if (ability.data.castRange > 0.0f && ability.data.targetType != AbilityTargetType::Passive) {
        ImGui::Text("Cast Range: %.0f", ability.data.castRange);
    }
    
    // Damage
    if (ability.data.damage > 0.0f) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("Damage: %.0f", ability.data.damage);
        ImGui::PopStyleColor();
    }
    
    // Radius (for AoE abilities)
    if (ability.data.radius > 0.0f) {
        ImGui::Text("Radius: %.0f", ability.data.radius);
    }
    
    // Duration
    if (ability.data.duration > 0.0f) {
        ImGui::Text("Duration: %.1fs", ability.data.duration);
    }
    
    // Target type
    if (ability.data.targetType != AbilityTargetType::Passive) {
        ImGui::Spacing();
        const char* targetTypeStr = "";
        switch (ability.data.targetType) {
            case AbilityTargetType::NoTarget:
                targetTypeStr = "No Target";
                break;
            case AbilityTargetType::PointTarget:
                targetTypeStr = "Point Target";
                break;
            case AbilityTargetType::UnitTarget:
                targetTypeStr = "Unit Target";
                break;
            case AbilityTargetType::VectorTarget:
                targetTypeStr = "Vector Target";
                break;
            default:
                break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
        ImGui::Text("Target: %s", targetTypeStr);
        ImGui::PopStyleColor();
    }
    
    // Hotkey hint
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::Text("Hotkey: [%c]", ability.data.hotkey);
    if (ability.level < ability.data.maxLevel && hero.abilityPoints > 0) {
        ImGui::SameLine();
        ImGui::Text(" | Level up: Ctrl+%c", ability.data.hotkey);
    }
    ImGui::PopStyleColor();
    
    ImGui::EndTooltip();
}

void EditorUI::drawHeroInventory(const HeroComponent& hero, const ImVec2& screenPos) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    const float slotSize = 40.0f;
    const float spacing = 3.0f;
    float x = screenPos.x;
    float y = screenPos.y;
    
    // Item hotkeys: Z, X, C, V, B, N
    const char itemHotkeys[6] = { 'Z', 'X', 'C', 'V', 'B', 'N' };
    
    // Draw 6 inventory slots (2 rows x 3 columns)
    for (int i = 0; i < 6; ++i) {
        float slotX = x + (i % 3) * (slotSize + spacing);
        float slotY = y + (i / 3) * (slotSize + spacing);
        
        const auto& item = hero.inventory[i];
        
        // Slot background
        ImU32 bgColor = item.data.name.empty() ? IM_COL32(30, 30, 35, 255) : IM_COL32(50, 50, 60, 255);
        drawList->AddRectFilled(
            ImVec2(slotX, slotY),
            ImVec2(slotX + slotSize, slotY + slotSize),
            bgColor,
            3.0f
        );
        
        // Border
        drawList->AddRect(
            ImVec2(slotX, slotY),
            ImVec2(slotX + slotSize, slotY + slotSize),
            IM_COL32(70, 70, 80, 255),
            3.0f
        );
        
        // Hotkey in corner
        char hotkeyText[2] = { itemHotkeys[i], '\0' };
        drawList->AddText(
            ImVec2(slotX + 2, slotY + 2),
            IM_COL32(180, 180, 180, 200),
            hotkeyText
        );
        
        // Item name (abbreviated)
        if (!item.data.name.empty()) {
            String shortName = item.data.name.substr(0, 3);
            drawList->AddText(
                ImVec2(slotX + 3, slotY + slotSize - 15),
                IM_COL32(255, 255, 255, 200),
                shortName.c_str()
            );
        }
    }
}

} // namespace WorldEditor


