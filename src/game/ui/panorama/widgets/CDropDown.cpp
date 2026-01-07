#include "CDropDown.h"
#include "../rendering/CUIRenderer.h"
#include <algorithm>

namespace Panorama {

CDropDown::CDropDown() {
    m_panelType = PanelType::DropDown;
    m_acceptsInput = true;
    m_inlineStyle.backgroundColor = Color(0.15f, 0.15f, 0.2f, 0.95f);
    m_inlineStyle.borderRadius = 4.0f;
    m_inlineStyle.borderWidth = 1.0f;
    m_inlineStyle.borderColor = Color(0.3f, 0.3f, 0.35f, 0.8f);
}

CDropDown::CDropDown(const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::DropDown;
    m_acceptsInput = true;
}

void CDropDown::AddOption(const std::string& id, const std::string& text) {
    m_options.push_back({id, text});
    if (m_selectedId.empty()) m_selectedId = id;
}

void CDropDown::RemoveOption(const std::string& id) {
    m_options.erase(std::remove_if(m_options.begin(), m_options.end(),
        [&](const Option& o) { return o.id == id; }), m_options.end());
}

void CDropDown::ClearOptions() { 
    m_options.clear(); 
    m_selectedId.clear(); 
}

void CDropDown::SetSelected(const std::string& id) {
    for (const auto& opt : m_options) {
        if (opt.id == id) {
            std::string oldId = m_selectedId;
            m_selectedId = id;
            if (oldId != id && m_onSelectionChanged) m_onSelectionChanged(id);
            return;
        }
    }
}

bool CDropDown::OnMouseUp(f32 x, f32 y, i32 button) {
    if (button == 0 && IsPointInPanel(x, y)) { 
        m_isOpen = !m_isOpen; 
        return true; 
    }
    if (m_isOpen && button == 0) m_isOpen = false;
    return CPanel2D::OnMouseUp(x, y, button);
}

void CDropDown::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (!renderer) return;
    
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    
    // Find selected text
    std::string selectedText;
    for (const auto& opt : m_options) {
        if (opt.id == m_selectedId) { 
            selectedText = opt.text; 
            break; 
        }
    }
    
    FontInfo font; 
    font.size = m_computedStyle.fontSize.value_or(16.0f);
    Color textColor = m_computedStyle.color.value_or(Color::White()); 
    textColor.a *= opacity;
    renderer->DrawText(selectedText, m_contentBounds, textColor, font);
    
    // Draw dropdown options if open
    if (m_isOpen) {
        f32 optionY = m_actualBounds.y + m_actualBounds.height;
        for (const auto& opt : m_options) {
            Rect2D optRect = {m_actualBounds.x, optionY, m_actualBounds.width, 30};
            Color bgColor(0.2f, 0.2f, 0.25f, 0.95f); 
            bgColor.a *= opacity;
            renderer->DrawRect(optRect, bgColor);
            renderer->DrawText(opt.text, optRect, textColor, font);
            optionY += 30;
        }
    }
}

} // namespace Panorama