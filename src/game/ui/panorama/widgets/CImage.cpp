#include "CImage.h"
#include "../rendering/CUIRenderer.h"

namespace Panorama {

CImage::CImage() { 
    m_panelType = PanelType::Image; 
}

CImage::CImage(const std::string& src, const std::string& id)
    : CPanel2D(id), m_imagePath(src) { 
    m_panelType = PanelType::Image; 
}

void CImage::SetImage(const std::string& path) { 
    m_imagePath = path; 
}

void CImage::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (m_imagePath.empty() || !renderer) return;
    
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    if (opacity <= 0) return;
    
    renderer->DrawImage(m_imagePath, m_actualBounds, opacity);
}

} // namespace Panorama