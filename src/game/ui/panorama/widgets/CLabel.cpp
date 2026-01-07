#include "CLabel.h"
#include "../rendering/CUIRenderer.h"

namespace Panorama {

CLabel::CLabel() {
    m_panelType = PanelType::Label;
}

CLabel::CLabel(const std::string& text, const std::string& id) 
    : CPanel2D(id), m_text(text) {
    m_panelType = PanelType::Label;
}

void CLabel::SetText(const std::string& text) {
    m_text = text;
}

void CLabel::SetLocString(const std::string& token) {
    m_locToken = token;
}

void CLabel::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    
    CPanel2D::Render(renderer);
    if (m_text.empty() || !renderer) return;
    
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    if (opacity <= 0) return;
    
    FontInfo font;
    font.size = m_computedStyle.fontSize.value_or(16.0f);
    font.bold = m_computedStyle.fontWeight.has_value() && m_computedStyle.fontWeight.value() == "bold";
    font.family = m_computedStyle.fontFamily.value_or(font.family);
    
    Color textColor = m_computedStyle.color.value_or(Color::White());
    textColor.a *= opacity;
    
    HorizontalAlign hAlign = m_computedStyle.textAlign.value_or(HorizontalAlign::Left);
    VerticalAlign vAlign = m_computedStyle.verticalTextAlign.value_or(VerticalAlign::Top);
    
    renderer->DrawText(m_text, m_contentBounds, textColor, font, hAlign, vAlign);
}

} // namespace Panorama