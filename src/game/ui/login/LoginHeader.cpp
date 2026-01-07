#include "LoginHeader.h"
#include "../panorama/core/CUIEngine.h"
#include "../panorama/widgets/CLabel.h"

namespace Game {

void LoginHeader::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight) {
    if (!parent) return;
    
    // Calculate centered block position using percentages
    const f32 headerHeightPct = 15.0f;
    const f32 formHeightPct = 47.0f;
    const f32 gapPct = 3.0f;
    const f32 totalBlockPct = headerHeightPct + gapPct + formHeightPct;  // 65%
    const f32 blockStartY = (100.0f - totalBlockPct) / 2.0f;  // 17.5%
    
    // Container for header elements (styled by #LoginHeader in CSS)
    m_container = std::make_shared<Panorama::CPanel2D>("LoginHeader");
    m_container->GetStyle().width = Panorama::Length::Pct(100.0f);
    m_container->GetStyle().height = Panorama::Length::Pct(headerHeightPct);
    m_container->GetStyle().x = Panorama::Length::Pct(0.0f);
    m_container->GetStyle().y = Panorama::Length::Pct(blockStartY);
    parent->AddChild(m_container);
    
    // Game logo "WORLD EDITOR" (styled by #GameLogo in CSS)
    // Center horizontally: (100% - ~23%) / 2 = ~38%
    m_logoLabel = std::make_shared<Panorama::CLabel>("WORLD EDITOR", "GameLogo");
    m_logoLabel->GetStyle().x = Panorama::Length::Pct(38.0f);
    m_logoLabel->GetStyle().y = Panorama::Length::Pct(27.0f);
    m_container->AddChild(m_logoLabel);
    
    // Decorative accent line (styled by #AccentLine in CSS)
    // Center horizontally: (100% - 15%) / 2 = 42.5%
    m_accentLine = std::make_shared<Panorama::CPanel2D>("AccentLine");
    m_accentLine->GetStyle().width = Panorama::Length::Pct(15.0f);
    m_accentLine->GetStyle().height = Panorama::Length::Px(3.0f);
    m_accentLine->GetStyle().x = Panorama::Length::Pct(42.5f);
    m_accentLine->GetStyle().y = Panorama::Length::Pct(75.0f);
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
