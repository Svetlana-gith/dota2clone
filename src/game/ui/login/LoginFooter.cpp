#include "LoginFooter.h"
#include "../panorama/core/CUIEngine.h"
#include "../panorama/widgets/CLabel.h"

namespace Game {

void LoginFooter::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight) {
    if (!parent) return;
    
    // Container for footer elements (styled by #LoginFooter in CSS)
    m_container = std::make_shared<Panorama::CPanel2D>("LoginFooter");
    m_container->GetStyle().marginTop = Panorama::Length::Px(20.0f);
    parent->AddChild(m_container);
    
    // Keyboard hints label (styled by #HintLabel in CSS)
    m_hintLabel = std::make_shared<Panorama::CLabel>("Tab: next | Enter: submit | Esc: back", "HintLabel");
    m_container->AddChild(m_hintLabel);
}

void LoginFooter::Destroy() {
    if (m_container) {
        if (auto* parent = m_container->GetParent()) {
            parent->RemoveChild(m_container.get());
        }
        m_hintLabel.reset();
        m_container.reset();
    }
}

} // namespace Game
