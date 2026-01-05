#include "CUIEngine.h"
#include "CStyleSheet.h"
#include "CLayoutFile.h"
#include "CUITextSystem.h"
#include "DirectXRenderer.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Panorama {

CUIEngine& CUIEngine::Instance() {
    static CUIEngine instance;
    return instance;
}

bool CUIEngine::Initialize(ID3D12Device* device, DirectXRenderer* renderer, const UIEngineConfig& config) {
    m_config = config;
    m_dxRenderer = renderer;
    
    m_renderer = std::make_unique<CUIRenderer>();
    if (renderer) {
        m_renderer->Initialize(device, renderer->GetCommandQueue(), renderer->GetCommandList(),
                              renderer->GetSrvHeap(), config.screenWidth, config.screenHeight);
    }
    
    m_root = std::make_shared<CPanel2D>("__root__");
    m_root->GetStyle().width = Length::Px(config.screenWidth);
    m_root->GetStyle().height = Length::Px(config.screenHeight);
    
    m_globalStylesheet = std::make_shared<CStyleSheet>();

    // Auto-load base stylesheet if present (HTML/CSS-like workflow).
    // Users can override by calling LoadStyleSheet() with their own file.
    {
        std::filesystem::path base = std::filesystem::path("resources") / "styles" / "base.css";
        std::error_code ec;
        if (std::filesystem::exists(base, ec) && !ec) {
            LoadStyleSheet(base.u8string());
        }
    }
    
    m_initialized = true;
    LOG_INFO("CUIEngine initialized (DX12 mode)");
    return true;
}

void CUIEngine::Shutdown() {
    m_root.reset();
    m_globalStylesheet.reset();
    
    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }
    
    m_initialized = false;
}

std::shared_ptr<CPanel2D> CUIEngine::CreatePanelByType(const std::string& type, CPanel2D* parent, const std::string& id) {
    auto panel = CLayoutManager::Instance().CreatePanel(type);
    if (panel) {
        panel->SetID(id);
        if (parent) {
            parent->AddChild(panel);
        }
    }
    return panel;
}

std::shared_ptr<CPanel2D> CUIEngine::LoadLayout(const std::string& path, CPanel2D* parent) {
    auto panel = CLayoutManager::Instance().CreatePanelFromLayout(path);
    if (panel) {
        // Apply any stored text attributes AFTER the element hierarchy is created.
        // This keeps element creation and text creation independent.
        CUITextSystem::Instance().ApplyTextRecursive(panel.get());
        if (parent) {
            parent->AddChild(panel);
        }
    }
    return panel;
}

void CUIEngine::LoadLayoutAsync(const std::string& path, CPanel2D* parent, 
                                std::function<void(std::shared_ptr<CPanel2D>)> callback) {
    // For now, just load synchronously
    auto panel = LoadLayout(path, parent);
    if (callback) callback(panel);
}

void CUIEngine::LoadStyleSheet(const std::string& path) {
    // Route all global styling through CStyleManager so panels compute styles consistently.
    LOG_INFO("CUIEngine::LoadStyleSheet('{}') cwd='{}'",
        path, std::filesystem::current_path().u8string());
    CStyleManager::Instance().LoadGlobalStyles(path);
    if (m_root) m_root->InvalidateStyle();
}

void CUIEngine::ApplyGlobalStyles() {
    // Styles are applied lazily during layout; force invalidation if caller wants an update now.
    if (m_root) m_root->InvalidateStyle();
}

CPanel2D* CUIEngine::FindPanelByID(const std::string& id) {
    return m_root ? m_root->FindChildTraverse(id) : nullptr;
}

std::vector<CPanel2D*> CUIEngine::FindPanelsByClass(const std::string& className) {
    return m_root ? m_root->FindChildrenWithClass(className) : std::vector<CPanel2D*>{};
}

void CUIEngine::SetFocus(CPanel2D* panel) {
    if (m_focusedPanel != panel) {
        if (m_focusedPanel) {
            m_focusedPanel->RemoveFocus();
        }
        m_focusedPanel = panel;
        if (m_focusedPanel) {
            m_focusedPanel->SetFocus();
        }
    }
}

void CUIEngine::ClearFocus() {
    SetFocus(nullptr);
}

void CUIEngine::ClearAllInputState() {
    if (m_focusedPanel) {
        m_focusedPanel->RemoveFocus();
        m_focusedPanel = nullptr;
    }
    m_hoveredPanel = nullptr;
    m_pressedPanel = nullptr;
}

void CUIEngine::Update(f32 deltaTime) {
    if (m_root) {
        UpdatePanelRecursive(m_root.get(), deltaTime);
    }
}

void CUIEngine::UpdatePanelRecursive(CPanel2D* panel, f32 deltaTime) {
    if (!panel) return;
    
    panel->Update(deltaTime);
    
    for (const auto& child : panel->GetChildren()) {
        UpdatePanelRecursive(child.get(), deltaTime);
    }
}

void CUIEngine::Render() {
    if (!m_renderer || !m_root) return;
    
    // Layout pass
    Rect2D screenBounds = {0, 0, m_config.screenWidth, m_config.screenHeight};
    m_root->PerformLayout(screenBounds);
    
    // Render pass
    m_renderer->BeginFrame();
    RenderPanelRecursive(m_root.get());
    m_renderer->EndFrame();
}

void CUIEngine::RenderPanelRecursive(CPanel2D* panel) {
    if (!panel || !panel->IsVisible()) return;
    
    // Render this panel (which also renders its children internally)
    panel->Render(m_renderer.get());
    
    // Note: CPanel2D::Render() already handles children recursively,
    // so we don't need to iterate children here
}

void CUIEngine::UpdateHoverState() {
    m_hoveredPanel = FindPanelAtPoint(m_root.get(), m_mouseX, m_mouseY);
}

CPanel2D* CUIEngine::FindPanelAtPoint(CPanel2D* root, f32 x, f32 y) {
    if (!root || !root->IsVisible() || !root->IsEnabled()) return nullptr;
    
    // Check children in reverse order (top to bottom in z-order)
    const auto& children = root->GetChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (auto* found = FindPanelAtPoint(it->get(), x, y)) {
            return found;
        }
    }
    
    // Check this panel
    if (root->IsPointInPanel(x, y) && root->IsAcceptingInput()) {
        return root;
    }
    
    return nullptr;
}

void CUIEngine::OnMouseMove(f32 x, f32 y) {
    m_mouseX = x;
    m_mouseY = y;
    
    UpdateHoverState();
    
    if (m_root) {
        m_root->OnMouseMove(x, y);
    }
}

void CUIEngine::OnMouseDown(f32 x, f32 y, i32 button) {
    m_mouseX = x;
    m_mouseY = y;
    
    if (m_root) {
        m_root->OnMouseDown(x, y, button);
    }
    
    // Update focus
    auto* clicked = FindPanelAtPoint(m_root.get(), x, y);
    if (clicked != m_focusedPanel) {
        SetFocus(clicked);
    }
}

void CUIEngine::OnMouseUp(f32 x, f32 y, i32 button) {
    m_mouseX = x;
    m_mouseY = y;
    
    if (m_root) {
        m_root->OnMouseUp(x, y, button);
    }
}

void CUIEngine::OnMouseWheel(f32 x, f32 y, i32 delta) {
    if (m_root) {
        m_root->OnMouseWheel(x, y, delta);
    }
}

void CUIEngine::OnKeyDown(i32 key) {
    if (m_focusedPanel) {
        m_focusedPanel->OnKeyDown(key);
    }
}

void CUIEngine::OnKeyUp(i32 key) {
    if (m_focusedPanel) {
        m_focusedPanel->OnKeyUp(key);
    }
}

void CUIEngine::OnTextInput(const std::string& text) {
    if (m_focusedPanel) {
        m_focusedPanel->OnTextInput(text);
    }
}

void CUIEngine::SetScreenSize(f32 width, f32 height) {
    m_config.screenWidth = width;
    m_config.screenHeight = height;
    
    if (m_renderer) {
        m_renderer->SetScreenSize(width, height);
    }
    
    if (m_root) {
        m_root->GetStyle().width = Length::Px(width);
        m_root->GetStyle().height = Length::Px(height);
        m_root->InvalidateLayout();
    }
}

void CUIEngine::SetUIScale(f32 scale) {
    m_config.uiScale = scale;
}

void CUIEngine::DrawDebugInfo() {
    if (!m_config.debugMode || !m_renderer) return;
    
    // Draw panel bounds, hover state, etc.
    // Would iterate through panels and draw debug overlays
}

void CUIEngine::SetLanguage(const std::string& language) {
    m_currentLanguage = language;
    // Would reload localization strings
}

std::string CUIEngine::Localize(const std::string& token) const {
    // Check if token starts with #
    std::string key = token;
    if (!key.empty() && key[0] == '#') {
        key = key.substr(1);
    }
    
    auto it = m_localizationStrings.find(key);
    if (it != m_localizationStrings.end()) {
        return it->second;
    }
    
    // Return token as-is if not found
    return token;
}

void CUIEngine::LoadLocalizationFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    
    std::string line;
    while (std::getline(file, line)) {
        // Simple key=value format
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            
            // Trim whitespace
            auto trim = [](std::string& s) {
                size_t start = s.find_first_not_of(" \t\r\n");
                size_t end = s.find_last_not_of(" \t\r\n");
                if (start != std::string::npos) {
                    s = s.substr(start, end - start + 1);
                }
            };
            
            trim(key);
            trim(value);
            
            m_localizationStrings[key] = value;
        }
    }
}

void CUIEngine::PlaySound(const std::string& soundName) {
    if (!m_soundEnabled) return;
    
    // Would play sound through audio system
    // For now, just a placeholder
}

} // namespace Panorama
