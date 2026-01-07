#include "CButton.h"
#include "CLabel.h"
#include "../rendering/CUIRenderer.h"
#include <spdlog/spdlog.h>

namespace Panorama {

CButton::CButton() {
    m_panelType = PanelType::Button;
    m_acceptsInput = true;
    m_label = std::make_shared<CLabel>();
    
    // Center the label inside the button
    m_label->GetStyle().horizontalAlign = HorizontalAlign::Center;
    m_label->GetStyle().verticalAlign = VerticalAlign::Center;
    m_label->GetStyle().textAlign = HorizontalAlign::Center;
    m_label->GetStyle().verticalTextAlign = VerticalAlign::Center;
    m_label->GetStyle().width = Length::Fill();
    m_label->GetStyle().height = Length::Fill();
    m_label->SetAcceptsInput(false);
    AddChild(m_label);
    
    // Default button styling
    m_inlineStyle.backgroundColor = Color(0.25f, 0.25f, 0.3f, 0.9f);
    m_inlineStyle.borderRadius = 6.0f;
    m_inlineStyle.borderWidth = 1.0f;
    m_inlineStyle.borderColor = Color(0.4f, 0.4f, 0.45f, 0.8f);
}

CButton::CButton(const std::string& text, const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::Button;
    m_acceptsInput = true;
    m_label = std::make_shared<CLabel>(text);
    
    // Center the label inside the button
    m_label->GetStyle().horizontalAlign = HorizontalAlign::Center;
    m_label->GetStyle().verticalAlign = VerticalAlign::Center;
    m_label->GetStyle().textAlign = HorizontalAlign::Center;
    m_label->GetStyle().verticalTextAlign = VerticalAlign::Center;
    m_label->GetStyle().width = Length::Fill();
    m_label->GetStyle().height = Length::Fill();
    m_label->SetAcceptsInput(false);
    AddChild(m_label);
    
    // Default button styling
    m_inlineStyle.backgroundColor = Color(0.25f, 0.25f, 0.3f, 0.9f);
    m_inlineStyle.borderRadius = 6.0f;
    m_inlineStyle.borderWidth = 1.0f;
    m_inlineStyle.borderColor = Color(0.4f, 0.4f, 0.45f, 0.8f);
}

void CButton::SetText(const std::string& text) { 
    if (m_label) m_label->SetText(text); 
}

const std::string& CButton::GetText() const { 
    static std::string empty; 
    return m_label ? m_label->GetText() : empty; 
}

void CButton::SetOnActivate(std::function<void()> handler) { 
    m_onActivate = handler; 
}

bool CButton::OnMouseUp(f32 x, f32 y, i32 button) {
    // LOG_INFO("CButton::OnMouseUp START id='{}'", m_id);
    
    bool wasPressed = m_pressed;
    bool inPanel = IsPointInPanel(x, y);
    bool result = CPanel2D::OnMouseUp(x, y, button);
    
    // LOG_INFO("CButton::OnMouseUp id='{}' wasPressed={} inPanel={} hasCallback={}", 
    //          m_id, wasPressed, inPanel, m_onActivate ? true : false);
    
    if (wasPressed && inPanel && button == 0 && m_onActivate) {
        // LOG_INFO("CButton::OnMouseUp CALLING CALLBACK for '{}'", m_id);
        auto callback = m_onActivate;
        callback();
        return true;
    }
    return result;
}

void CButton::Render(CUIRenderer* renderer) {
    if (!m_visible || !renderer) return;
    CPanel2D::Render(renderer);
}

bool CButton::OnKeyDown(i32 key) {
    if (m_focused && (key == 13 || key == 32)) { 
        if (m_onActivate) m_onActivate(); 
        return true; 
    }
    return false;
}

} // namespace Panorama