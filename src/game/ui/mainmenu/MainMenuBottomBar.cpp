#include "MainMenuBottomBar.h"
#include "../panorama/core/CPanel2D.h"
#include "../../DebugConsole.h"

namespace Game {

static float S(float v) { return v * 1.35f; }

static std::shared_ptr<Panorama::CPanel2D> P(const std::string& id, float w, float h, Panorama::Color bg) {
    auto p = std::make_shared<Panorama::CPanel2D>(id);
    if (w > 0) p->GetStyle().width = Panorama::Length::Px(S(w));
    else p->GetStyle().width = Panorama::Length::Fill();
    if (h > 0) p->GetStyle().height = Panorama::Length::Px(S(h));
    else p->GetStyle().height = Panorama::Length::Fill();
    p->GetStyle().backgroundColor = bg;
    return p;
}

static std::shared_ptr<Panorama::CLabel> L(const std::string& text, const std::string& cssClass, Panorama::Color col) {
    auto l = std::make_shared<Panorama::CLabel>(text, text);
    l->AddClass(cssClass);
    l->GetStyle().color = col;
    return l;
}

MainMenuBottomBar::MainMenuBottomBar() = default;

void MainMenuBottomBar::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight,
                                f32 contentWidth, f32 contentOffsetX) {
    Panorama::Color none(0, 0, 0, 0);
    
    m_bottomBarBg = P("BotBg", 0, 70, Panorama::Color(0.03f, 0.04f, 0.06f, 0.95f));
    m_bottomBarBg->GetStyle().marginTop = Panorama::Length::Px(screenHeight - S(70));
    parent->AddChild(m_bottomBarBg);
    
    m_bottomBar = P("Bot", contentWidth / S(1), 70, none);
    m_bottomBar->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    m_bottomBarBg->AddChild(m_bottomBar);

    m_gameModeIcon = P("GI", 40, 40, Panorama::Color(0.12f, 0.12f, 0.15f, 1.0f));
    m_gameModeIcon->GetStyle().borderRadius = S(3);
    m_gameModeIcon->GetStyle().marginLeft = Panorama::Length::Px(S(15));
    m_gameModeIcon->GetStyle().marginTop = Panorama::Length::Px(S(15));
    m_bottomBar->AddChild(m_gameModeIcon);

    Panorama::Color partyColors[] = {
        {0.45f, 0.28f, 0.22f, 1},
        {0.28f, 0.32f, 0.45f, 1},
        {0.35f, 0.45f, 0.32f, 1}
    };
    m_partySlots.clear();
    for (int i = 0; i < 3; i++) {
        auto pt = P("PT" + std::to_string(i), 35, 40, partyColors[i]);
        pt->GetStyle().borderRadius = S(2);
        pt->GetStyle().marginLeft = Panorama::Length::Px(S(65 + i * 40));
        pt->GetStyle().marginTop = Panorama::Length::Px(S(15));
        m_bottomBar->AddChild(pt);
        m_partySlots.push_back(pt);
    }
    
    m_addPartyButton = P("AddParty", 35, 40, Panorama::Color(0.15f, 0.15f, 0.18f, 1.0f));
    m_addPartyButton->GetStyle().borderRadius = S(2);
    m_addPartyButton->GetStyle().marginLeft = Panorama::Length::Px(S(185));
    m_addPartyButton->GetStyle().marginTop = Panorama::Length::Px(S(15));
    m_bottomBar->AddChild(m_addPartyButton);
    
    auto plusLabel = L("+", "title", Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    plusLabel->GetStyle().marginLeft = Panorama::Length::Px(S(11));
    plusLabel->GetStyle().marginTop = Panorama::Length::Px(S(5));
    m_addPartyButton->AddChild(plusLabel);

    m_gameModeLabel = L("ALL PICK", "body", Panorama::Color(0.6f, 0.6f, 0.6f, 1.0f));
    m_gameModeLabel->GetStyle().marginLeft = Panorama::Length::Px(S(250));
    m_gameModeLabel->GetStyle().marginTop = Panorama::Length::Px(S(28));
    m_bottomBar->AddChild(m_gameModeLabel);
    
    // Play button (right side)
    m_playButton = std::make_shared<Panorama::CButton>("PLAY DOTA", "PlayBtn");
    m_playButton->GetStyle().width = Panorama::Length::Px(S(140));
    m_playButton->GetStyle().height = Panorama::Length::Px(S(45));
    m_playButton->GetStyle().backgroundColor = Panorama::Color(0.18f, 0.45f, 0.18f, 1.0f);
    m_playButton->GetStyle().borderRadius = S(3);
    m_playButton->GetStyle().fontSize = 16.0f;
    m_playButton->GetStyle().color = Panorama::Color::White();
    m_playButton->GetStyle().marginLeft = Panorama::Length::Px(contentWidth / S(1) - S(160));
    m_playButton->GetStyle().marginTop = Panorama::Length::Px(S(12));
    m_playButton->SetOnActivate([this]() {
        if (m_onPlayClicked) {
            m_onPlayClicked();
        }
    });
    m_bottomBar->AddChild(m_playButton);
}

void MainMenuBottomBar::Destroy() {
    if (m_bottomBarBg) {
        m_bottomBarBg->SetParent(nullptr);
        m_bottomBarBg.reset();
    }
    m_bottomBar.reset();
    m_gameModeIcon.reset();
    m_gameModeLabel.reset();
    m_partySlots.clear();
    m_addPartyButton.reset();
    m_playButton.reset();
}

void MainMenuBottomBar::SetGameMode(const std::string& mode) {
    if (m_gameModeLabel) {
        m_gameModeLabel->SetText(mode);
    }
}

void MainMenuBottomBar::SetPartyMembers(int count) {
    for (size_t i = 0; i < m_partySlots.size(); i++) {
        if (m_partySlots[i]) {
            m_partySlots[i]->SetVisible(static_cast<int>(i) < count);
        }
    }
}

void MainMenuBottomBar::SetPlayButtonVisible(bool visible) {
    if (m_playButton) {
        m_playButton->SetVisible(visible);
    }
}

void MainMenuBottomBar::SetPlayButtonText(const std::string& text) {
    if (m_playButton) {
        m_playButton->SetText(text);
    }
}

} // namespace Game
