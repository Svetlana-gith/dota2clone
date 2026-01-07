#include "CTooltip.h"
#include "../widgets/CPanelWidgets.h"
#include "../widgets/CImage.h"
#include "../widgets/CLabel.h"

namespace Panorama {

CTooltip::CTooltip() {
    // Create tooltip components
    m_background = CPanelWidgets::CreateImage("", 0, 0, 200, 100);
    m_titleLabel = CPanelWidgets::CreateLabel("", 10, 10);
    m_descriptionLabel = CPanelWidgets::CreateLabel("", 10, 30);
    
    AddChild(m_background);
    AddChild(m_titleLabel);
    AddChild(m_descriptionLabel);
    
    SetVisible(false);
}

void CTooltip::ShowTooltip(const std::string& title, const std::string& description, f32 x, f32 y) {
    // TODO: Set title and description text when CLabel text setting is implemented
    GetStyle().x = Length::Px(x);
    GetStyle().y = Length::Px(y);
    SetVisible(true);
}

void CTooltip::HideTooltip() {
    SetVisible(false);
}

void CTooltip::UpdatePosition(f32 x, f32 y) {
    GetStyle().x = Length::Px(x);
    GetStyle().y = Length::Px(y);
}

} // namespace Panorama