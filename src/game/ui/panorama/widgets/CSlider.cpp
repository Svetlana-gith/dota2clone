#include "CSlider.h"
#include "../rendering/CUIRenderer.h"
#include <cmath>

namespace Panorama {

CSlider::CSlider() {
    m_panelType = PanelType::Slider;
    m_acceptsInput = true;
    m_inlineStyle.backgroundColor = Color(0.15f, 0.15f, 0.2f, 0.9f);
    m_inlineStyle.borderRadius = 4.0f;
}

CSlider::CSlider(const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::Slider;
    m_acceptsInput = true;
}

void CSlider::SetValue(f32 value) {
    f32 oldValue = m_value;
    if (value < m_min) value = m_min;
    if (value > m_max) value = m_max;
    m_value = value;
    if (m_step > 0) m_value = std::round((m_value - m_min) / m_step) * m_step + m_min;
    if (m_value != oldValue && m_onValueChanged) m_onValueChanged(m_value);
}

void CSlider::SetRange(f32 min, f32 max) { 
    m_min = min; 
    m_max = max; 
    SetValue(m_value); 
}

bool CSlider::OnMouseDown(f32 x, f32 y, i32 button) {
    if (button == 0 && IsPointInPanel(x, y)) { 
        m_dragging = true; 
        OnMouseMove(x, y); 
        return true; 
    }
    return CPanel2D::OnMouseDown(x, y, button);
}

bool CSlider::OnMouseMove(f32 x, f32 y) {
    CPanel2D::OnMouseMove(x, y);
    if (m_dragging) {
        f32 ratio;
        if (m_vertical) 
            ratio = 1.0f - (y - m_actualBounds.y) / m_actualBounds.height;
        else 
            ratio = (x - m_actualBounds.x) / m_actualBounds.width;
        
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        SetValue(m_min + ratio * (m_max - m_min));
        return true;
    }
    return false;
}

bool CSlider::OnMouseUp(f32 x, f32 y, i32 button) {
    if (button == 0) m_dragging = false;
    return CPanel2D::OnMouseUp(x, y, button);
}

void CSlider::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (!renderer) return;
    
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    
    // Draw track
    Rect2D trackRect = m_actualBounds;
    if (m_vertical) { 
        trackRect.x += m_actualBounds.width / 2 - 3; 
        trackRect.width = 6; 
    } else { 
        trackRect.y += m_actualBounds.height / 2 - 3; 
        trackRect.height = 6; 
    }
    Color trackColor(0.2f, 0.2f, 0.25f, 0.9f); 
    trackColor.a *= opacity;
    renderer->DrawRoundedRect(trackRect, trackColor, 3);
    
    // Draw thumb
    f32 ratio = (m_value - m_min) / (m_max - m_min);
    f32 thumbX, thumbY;
    if (m_vertical) { 
        thumbX = m_actualBounds.x + m_actualBounds.width / 2; 
        thumbY = m_actualBounds.y + m_actualBounds.height * (1.0f - ratio); 
    } else { 
        thumbX = m_actualBounds.x + m_actualBounds.width * ratio; 
        thumbY = m_actualBounds.y + m_actualBounds.height / 2; 
    }
    Color thumbColor = m_hovered || m_dragging ? Color(0.5f, 0.5f, 0.55f, 1.0f) : Color(0.4f, 0.4f, 0.45f, 1.0f);
    thumbColor.a *= opacity;
    renderer->DrawCircle(thumbX, thumbY, 8, thumbColor, true);
}

} // namespace Panorama