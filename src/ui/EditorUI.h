#pragma once

#include "core/Types.h"
#include "world/TerrainTools.h"
#include "world/Components.h"

#include <imgui.h>
#include "properties/Properties.h"

// Forward declarations
class DirectXRenderer;

namespace WorldEditor {

class World;

// Editor UI front-end built on ImGui.
// This module is intentionally editor-only and should not be used for in-game UI.
class EditorUI {
public:
    EditorUI() = default;

    void draw(World& world);
    void drawCameraPanel(class EditorCamera& camera);
    
    // Set renderer for wireframe grid updates
    void setRenderer(class DirectXRenderer* renderer) { renderer_ = renderer; }

    void setSelected(Entity e) { selected_ = e; }
    Entity getSelected() const { return selected_; }

    // Tool mode helpers (Unreal-like workflow).
    void setTerrainEditEnabled(bool enabled) { terrainEditEnabled_ = enabled; }
    void setTexturePaintEnabled(bool enabled) { texturePaintEnabled_ = enabled; }
    bool isObjectPlacementEnabled() const { return objectPlacementEnabled_; }
    ObjectType getSelectedObjectType() const { return selectedObjectType_; }
    int getObjectTeamId() const { return objectTeamId_; }
    float getObjectSpawnRadius() const { return objectSpawnRadius_; }
    int getObjectMaxUnits() const { return objectMaxUnits_; }
    int getObjectSpawnLane() const { return objectSpawnLane_; }
    int getObjectWaypointOrder() const { return objectWaypointOrder_; }
    int getObjectWaypointLane() const { return objectWaypointLane_; }
    bool getShowPathLines() const { return showPathLines_; }
    bool getShowUnitAttackRanges() const { return showUnitAttackRanges_; }

    void setViewportTexture(ImTextureID tex) { viewportTex_ = tex; }
    ImVec2 getViewportRectMin() const { return viewportRectMin_; }
    ImVec2 getViewportRectMax() const { return viewportRectMax_; }
    ImVec2 getViewportSize() const { return ImVec2(viewportRectMax_.x - viewportRectMin_.x, viewportRectMax_.y - viewportRectMin_.y); }
    bool isViewportHovered() const { return viewportHovered_; }
    bool isViewportFocused() const { return viewportFocused_; }

    // Game View (PIE-like)
    ImVec2 getGameViewRectMin() const { return gameViewRectMin_; }
    ImVec2 getGameViewRectMax() const { return gameViewRectMax_; }
    ImVec2 getGameViewSize() const { return ImVec2(gameViewRectMax_.x - gameViewRectMin_.x, gameViewRectMax_.y - gameViewRectMin_.y); }
    bool isGameViewHovered() const { return gameViewHovered_; }
    bool isGameViewFocused() const { return gameViewFocused_; }

    // Legacy terrain sculpting (deprecated)
    bool isTerrainEditEnabled() const { return false; } // Always disabled - use tile editor
    bool isTerrainSculptRequireCtrl() const { return terrainSculptRequireCtrl_; }
    float getTerrainBrushRadius() const { return terrainBrushRadius_; }
    float getTerrainBrushStrength() const { return terrainBrushStrength_; }
    
    // Tile terrain editor (Dota-like)
    enum class TileTool : u8 { HeightBrush = 0, RampPath = 1 };
    bool isTileEditorEnabled() const { return tileEditorEnabled_; }
    TileTool getTileTool() const { return tileTool_; }
    int getTileBrushRadiusTiles() const { return tileBrushRadiusTiles_; }
    int getTileFlattenLevel() const { return tileFlattenLevel_; }
    int getTileRampWidthTiles() const { return tileRampWidthTiles_; }
    int getTileInitTilesX() const { return tileInitTilesX_; }
    int getTileInitTilesZ() const { return tileInitTilesZ_; }
    float getTileInitTileSize() const { return tileInitTileSize_; }
    float getTileInitHeightStep() const { return tileInitHeightStep_; }

    // Texture painting getters
    bool isTexturePaintEnabled() const { return texturePaintEnabled_; }
    int getActiveTextureLayer() const { return activeTextureLayer_; }
    float getTextureBrushRadius() const { return textureBrushRadius_; }
    float getTextureBrushStrength() const { return textureBrushStrength_; }
    
    // Visualization getters
    bool isWireframeEnabled() const { return showWireframe_; }
    bool isUnrealViewportEnabled() const { return unrealViewportStyle_; }
    float getCheckerCellSize() const { return checkerCellSize_; }

    // Document state
    bool isDirty() const { return dirty_; }
    void markDirty() { dirty_ = true; }
    void setDirty(bool dirty) { dirty_ = dirty; }
    void clearDirty() { dirty_ = false; }
    const String& getCurrentMapPath() const { return currentMapPath_; }
    bool consumeQuitRequested() {
        const bool v = quitRequested_;
        quitRequested_ = false;
        return v;
    }
    void requestExit() { pendingAction_ = PendingAction::Exit; openUnsavedPopup_ = true; }
    
    // Game mode access
    class GameMode* getGameMode() { return gameMode_.get(); }

    // Tile editor controls (for hotkeys)
    void setTileTool(TileTool tool) { tileTool_ = tool; }
    void adjustTileBrushRadiusTiles(int delta) {
        tileBrushRadiusTiles_ = std::clamp(tileBrushRadiusTiles_ + delta, 1, 64);
    }

private:
    // Game mode for testing gameplay
    UniquePtr<class GameMode> gameMode_;
    Entity selected_ = INVALID_ENTITY;
    bool dockLayoutBuilt_ = false;
    bool requestResetLayout_ = false;
    ImTextureID viewportTex_ = 0;
    ImVec2 viewportRectMin_ = ImVec2(0, 0);
    ImVec2 viewportRectMax_ = ImVec2(0, 0);
    bool viewportHovered_ = false;
    bool viewportFocused_ = false;
    ImVec2 gameViewRectMin_ = ImVec2(0, 0);
    ImVec2 gameViewRectMax_ = ImVec2(0, 0);
    bool gameViewHovered_ = false;
    bool gameViewFocused_ = false;

    // Property undo/redo (only for supported kinds: float/vec3/color3).
    struct PropValue {
        WorldEditor::Properties::Kind kind = WorldEditor::Properties::Kind::Float;
        float f = 0.0f;
        Vec3 v = Vec3(0.0f);
    };
    enum class ComponentSlot : u8 { Transform, Material };
    struct PropCommand {
        Entity entity = INVALID_ENTITY;
        ComponentSlot component = ComponentSlot::Transform;
        WorldEditor::Properties::Kind kind = WorldEditor::Properties::Kind::Float;
        size_t offset = 0;
        PropValue before;
        PropValue after;
    };
    Vector<PropCommand> undo_;
    Vector<PropCommand> redo_;
    Map<u64, PropValue> activeEditOld_; // key -> old value captured on activation

    // Legacy terrain sculpting (deprecated - use tile editor instead)
    bool terrainEditEnabled_ = false;
    bool terrainSculptRequireCtrl_ = false;
    float terrainBrushRadius_ = 8.0f;
    float terrainBrushStrength_ = 15.0f;
    
    // Texture painting
    bool texturePaintEnabled_ = false;
    int activeTextureLayer_ = 0;
    float textureBrushRadius_ = 3.0f;
    float textureBrushStrength_ = 2.0f;

    // Tile terrain editing (Dota 2 Workshop Tools style)
    bool tileEditorEnabled_ = false;
    TileTool tileTool_ = TileTool::HeightBrush;
    int tileBrushRadiusTiles_ = 2;
    int tileFlattenLevel_ = 0;
    int tileRampWidthTiles_ = 2;
    int tileInitTilesX_ = 128;
    int tileInitTilesZ_ = 128;
    float tileInitTileSize_ = 128.0f;
    float tileInitHeightStep_ = 128.0f;
    
    // Visualization options
    bool showWireframe_ = false;
    bool unrealViewportStyle_ = true;
    float checkerCellSize_ = 1.0f; // world units
    
    // Object placement
    bool objectPlacementEnabled_ = false;
    ObjectType selectedObjectType_ = ObjectType::Tower;
    int objectTeamId_ = 0;
    float objectSpawnRadius_ = 5.0f;
    int objectMaxUnits_ = 3;
    int objectSpawnLane_ = -1; // -1 = all lanes, 0 = Top, 1 = Middle, 2 = Bottom
    int objectWaypointOrder_ = 0; // Order for waypoint (0 = first)
    int objectWaypointLane_ = -1; // Lane for waypoint (-1 = all lanes)
    
    // Renderer reference for wireframe grid updates
    class DirectXRenderer* renderer_ = nullptr;
    
    // Hierarchy UI state
    char hierarchySearchBuffer_[256] = {};
    bool hierarchyShowTerrain_ = true;
    bool hierarchyShowObjects_ = true;
    bool hierarchyShowCreeps_ = true;
    bool hierarchyShowMaterials_ = true;
    bool hierarchyShowMeshes_ = true;
    bool hierarchyShowOthers_ = true;
    
    // Force layout reset on first run with new GUI
    bool forceLayoutReset_ = true;
    
    // Path visualization
    bool showPathVisualization_ = true;
    bool showPathLines_ = true; // Show 3D lines between waypoints

    // Gameplay visualization
    bool showUnitAttackRanges_ = true;
    
    // Smart path guide
    void drawSmartPathGuide(World& world);
    void createPathBetweenSpawnAndBase(World& world, Entity spawnEntity, Entity baseEntity, i32 lane, i32 numWaypoints = 5);
    void autoNumberWaypoints(World& world, i32 teamId, i32 lane);
    bool validatePath(World& world, Entity spawnEntity, i32 lane);

    // Document / file workflow
    enum class PendingAction : u8 { None, New, Open, Exit };
    PendingAction pendingAction_ = PendingAction::None;
    bool openUnsavedPopup_ = false;
    bool dirty_ = false;
    bool quitRequested_ = false;
    String currentMapPath_ = "maps/scene.json";
    void drawUnsavedChangesPopup(World& world);
    void performNew(World& world);
    void performOpen(World& world);
    void performSave(World& world);
    void createTestMap(World& world);

    void drawEditMenu();
    void applyCommand(const PropCommand& cmd, bool useAfter);
    bool drawComponentProperties(Entity e, ComponentSlot slot, void* componentPtr);
    static u64 makePropKey(Entity e, ComponentSlot slot, size_t offset);

    void drawMainMenu(World& world);
    void drawDockSpace();
    void buildDefaultDockLayout(ImGuiID dockspaceId);
    void drawToolbar(World& world);
    void drawStatusBar(World& world);
    void drawToolsPanel(World& world);
    void applyModernTheme();
    void drawViewport(World& world);
    void drawGameView(World& world);
    void drawHierarchy(World& world);
    void drawInspector(World& world);
    void drawTerrain(World& world);
    void drawTerrainTools(World& world);
    void drawTexturePainting(World& world);
    void drawObjectPlacement(World& world);
    void drawStats(World& world);
    void drawMOBAGamePanel(World& world);
    void drawPathVisualizationPanel(World& world);
    void drawHeroHUD(World& world);
    void drawHeroAbilityBar(const struct HeroComponent& hero, const ImVec2& screenPos);
    void drawHeroInventory(const struct HeroComponent& hero, const ImVec2& screenPos);
    void drawAbilityTooltip(const struct HeroAbility& ability, const struct HeroComponent& hero);

    void ensureSelectionValid(World& world);

    // Component drawers
    void drawName(World& world, Entity e);
    void drawTransform(World& world, Entity e);
    void drawMesh(World& world, Entity e);
    void drawMaterial(World& world, Entity e);
};

} // namespace WorldEditor


