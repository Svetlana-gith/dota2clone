#pragma once
/**
 * CUIEngine - Main Panorama UI Engine
 * Central manager for the entire UI system (like Valve's $.GetContextPanel())
 */

#include "PanoramaTypes.h"
#include "CPanel2D.h"
#include "../rendering/CUIRenderer.h"
#include "../layout/CStyleSheet.h"
#include "../layout/CLayoutFile.h"
#include "GameEvents.h"
#include <memory>
#include <functional>

// Forward declarations
class DirectXRenderer;

namespace Panorama {

// ============ UI Engine Configuration ============

struct UIEngineConfig {
    f32 screenWidth = 1920.0f;
    f32 screenHeight = 1080.0f;
    f32 uiScale = 1.0f;
    std::string defaultFont = "Roboto Condensed";
    std::string resourcePath = "panorama/";
    bool debugMode = false;
};

// ============ Main UI Engine ============

class CUIEngine {
public:
    static CUIEngine& Instance();
    
    // ============ Initialization ============
    bool Initialize(ID3D12Device* device, DirectXRenderer* renderer, const UIEngineConfig& config = {});
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }
    
    // ============ Root Panel Access ============
    CPanel2D* GetRoot() { return m_root.get(); }
    const CPanel2D* GetRoot() const { return m_root.get(); }
    
    // ============ Panel Creation (Valve-style API) ============
    // Like $.CreatePanel() in Panorama JavaScript
    template<typename T = CPanel2D>
    std::shared_ptr<T> CreatePanel(const std::string& panelType, CPanel2D* parent, const std::string& id = "") {
        auto panel = std::make_shared<T>();
        panel->SetID(id);
        if (parent) {
            parent->AddChild(panel);
        }
        return panel;
    }
    
    std::shared_ptr<CPanel2D> CreatePanelByType(const std::string& type, CPanel2D* parent, const std::string& id = "");
    
    // ============ Layout Loading ============
    std::shared_ptr<CPanel2D> LoadLayout(const std::string& path, CPanel2D* parent = nullptr);
    void LoadLayoutAsync(const std::string& path, CPanel2D* parent, std::function<void(std::shared_ptr<CPanel2D>)> callback);
    
    // ============ Stylesheet Loading ============
    void LoadStyleSheet(const std::string& path);
    void ApplyGlobalStyles();
    
    // ============ Panel Lookup ============
    CPanel2D* FindPanelByID(const std::string& id);
    std::vector<CPanel2D*> FindPanelsByClass(const std::string& className);
    CPanel2D* GetFocusedPanel() const { return m_focusedPanel; }
    CPanel2D* GetHoveredPanel() const { return m_hoveredPanel; }
    
    // ============ Focus Management ============
    void SetFocus(CPanel2D* panel);
    void ClearFocus();
    void ClearAllInputState();  // Clear focus, hover, pressed - call before destroying UI
    void ClearInputStateForSubtree(CPanel2D* subtreeRoot); // Clear only if pointers are within subtreeRoot
    
    // ============ Update & Render ============
    void Update(f32 deltaTime);
    void Render();
    
    // ============ Input Handling ============
    void OnMouseMove(f32 x, f32 y);
    void OnMouseDown(f32 x, f32 y, i32 button);
    void OnMouseUp(f32 x, f32 y, i32 button);
    void OnMouseWheel(f32 x, f32 y, i32 delta);
    void OnKeyDown(i32 key);
    void OnKeyUp(i32 key);
    void OnTextInput(const std::string& text);
    
    // ============ Screen Info ============
    f32 GetScreenWidth() const { return m_config.screenWidth; }
    f32 GetScreenHeight() const { return m_config.screenHeight; }
    f32 GetUIScale() const { return m_config.uiScale; }
    void SetScreenSize(f32 width, f32 height);
    void SetUIScale(f32 scale);
    
    // ============ Renderer Access ============
    CUIRenderer* GetRenderer() { return m_renderer.get(); }
    
    // ============ Debug ============
    void SetDebugMode(bool enabled) { m_config.debugMode = enabled; }
    bool IsDebugMode() const { return m_config.debugMode; }
    void DrawDebugInfo();

    // ============ Custom Rendering Hook ============
    // Optional hook to allow game code to inject additional immediate-mode UI rendering
    // into the Panorama UI pass (after layout, with render target already bound).
    void SetCustomRenderCallback(std::function<void(CUIRenderer*)> callback) { m_customRenderCallback = std::move(callback); }
    
    // ============ Localization ============
    void SetLanguage(const std::string& language);
    std::string Localize(const std::string& token) const;
    void LoadLocalizationFile(const std::string& path);
    
    // ============ Sound ============
    void PlaySound(const std::string& soundName);
    void SetSoundEnabled(bool enabled) { m_soundEnabled = enabled; }
    
private:
    CUIEngine() = default;
    ~CUIEngine() = default;
    
    bool m_initialized = false;
    UIEngineConfig m_config;
    
    // Core components
    std::unique_ptr<CUIRenderer> m_renderer;
    std::shared_ptr<CPanel2D> m_root;
    std::shared_ptr<CStyleSheet> m_globalStylesheet;
    DirectXRenderer* m_dxRenderer = nullptr;  // Non-owning pointer
    
    // State
    CPanel2D* m_focusedPanel = nullptr;
    CPanel2D* m_hoveredPanel = nullptr;
    CPanel2D* m_pressedPanel = nullptr;
    f32 m_mouseX = 0, m_mouseY = 0;
    
    // Localization
    std::string m_currentLanguage = "english";
    std::unordered_map<std::string, std::string> m_localizationStrings;
    
    // Sound
    bool m_soundEnabled = true;
    
    // Internal helpers
    void UpdateHoverState();
    CPanel2D* FindPanelAtPoint(CPanel2D* root, f32 x, f32 y);
    void UpdatePanelRecursive(CPanel2D* panel, f32 deltaTime);
    void RenderPanelRecursive(CPanel2D* panel);

    // Custom UI render callback (executed during Render()).
    std::function<void(CUIRenderer*)> m_customRenderCallback;
};

// ============ Convenience Functions (Valve-style $ API) ============

// Get context panel (root)
inline CPanel2D* GetContextPanel() {
    return CUIEngine::Instance().GetRoot();
}

// Create panel
inline std::shared_ptr<CPanel2D> CreatePanel(const std::string& type, CPanel2D* parent, const std::string& id = "") {
    return CUIEngine::Instance().CreatePanelByType(type, parent, id);
}

// Find panel by ID
inline CPanel2D* FindPanel(const std::string& id) {
    return CUIEngine::Instance().FindPanelByID(id);
}

// Localize string
inline std::string Localize(const std::string& token) {
    return CUIEngine::Instance().Localize(token);
}

// Dispatch event
inline void DispatchEvent(const std::string& eventName, CPanel2D* panel = nullptr) {
    CGameEventData data;
    CGameEvents::Instance().DispatchEvent(eventName, data);
}

// Play UI sound
inline void PlayUISoundScript(const std::string& sound) {
    CUIEngine::Instance().PlaySound(sound);
}

} // namespace Panorama
