#include "CProgressBar.h"
#include "../rendering/CUIRenderer.h"

namespace Panorama {

CProgressBar::CProgressBar() {
    m_panelType = PanelType::ProgressBar;
    m_inlineStyle.backgroundColor = Color(0.15f, 0.15f, 0.2f, 0.9f);
    m_inlineStyle.borderRadius = 4.0f;
}

CProgressBar::CProgressBar(const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::ProgressBar;
    m_inlineStyle.backgroundColor = Color(0.15f, 0.15f, 0.2f, 0.9f);
    m_inlineStyle.borderRadius = 4.0f;
}

void CProgressBar::SetValue(f32 value) {
    if (value < m_min) value = m_min;
    if (value > m_max) value = m_max;
    m_value = value;
}

void CProgressBar::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (!renderer) return;
    
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    if (opacity <= 0) return;
    
    f32 normalizedValue = (m_value - m_min) / (m_max - m_min);
    f32 fillWidth = m_actualBounds.width * normalizedValue;
    
    if (fillWidth > 0) {
        Rect2D fillRect = m_actualBounds;
        fillRect.width = fillWidth;
        fillRect.x += 2; fillRect.y += 2;
        fillRect.width -= 4; fillRect.height -= 4;
        
        Color fillColor = Color(0.2f, 0.7f, 0.2f, 0.9f);
        fillColor.a *= opacity;
        
        f32 radius = m_computedStyle.borderRadius.value_or(4.0f) - 2;
        if (radius > 0) 
            renderer->DrawRoundedRect(fillRect, fillColor, radius);
        else 
            renderer->DrawRect(fillRect, fillColor);
    }
}

} // namespace Panorama