#include "LoginHeader.h"
#include "../panorama/core/CUIEngine.h"
#include "../panorama/widgets/CLabel.h"

namespace Game {

void LoginHeader::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight) {
    if (!parent) return;
    
    // Container for header elements (styled by #LoginHeader in CSS)
    // NOTE: Position managed by CSS Flexbox on parent (#LoginRoot)
    m_container = std::make_shared<Panorama::CPanel2D>("LoginHeader");
    // Width/height set via CSS, no inline styles needed for flexbox layout
    parent->AddChild(m_container);
    
    // Game logo "WORLD EDITOR" (styled by #GameLogo in CSS)
    // NOTE: Position managed by CSS Flexbox (#LoginHeader with justify-content: center, align-items: center)
    m_logoLabel = std::make_shared<Panorama::CLabel>("WORLD EDITOR", "GameLogo");
    m_container->AddChild(m_logoLabel);
    
    // Decorative accent line (styled by #AccentLine in CSS)
    // NOTE: Position managed by CSS Flexbox
    m_accentLine = std::make_shared<Panorama::CPanel2D>("AccentLine");
    m_container->AddChild(m_accentLine);
}

void LoginHeader::Destroy() {
    if (m_container) {
        if (auto* parent = m_container->GetParent()) {
            parent->RemoveChild(m_container.get());
        }
        m_accentLine.reset();
        m_logoLabel.reset();
        m_container.reset();
    }
}

} // namespace Game
