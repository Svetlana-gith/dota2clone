#include "MainMenuTopBar.h"
#include "../panorama/CUIEngine.h"

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

MainMenuTopBar::MainMenuTopBar() = default;

void MainMenuTopBar::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight,
                            f32 contentWidth, f32 contentOffsetX) {
    Panorama::Color header(0.01f, 0.02f, 0.04f, 0.95f);
    Panorama::Color gray(0.45f, 0.45f, 0.45f, 1.0f);
    Panorama::Color none(0, 0, 0, 0);
    
    float topBarHeight = 55.0f;
    
    m_topBar = P("Top", 0, 55, header);
    m_topBar->GetStyle().marginTop = Panorama::Length::Px(0);
    parent->AddChild(m_topBar);
    
    m_settingsButton = std::make_shared<Panorama::CButton>("⚙", "SettingsBtn");
    m_settingsButton->GetStyle().width = Panorama::Length::Px(40);
    m_settingsButton->GetStyle().height = Panorama::Length::Px(40);
    m_settingsButton->GetStyle().marginLeft = Panorama::Length::Px(10);
    m_settingsButton->GetStyle().marginTop = Panorama::Length::Px(7);
    m_settingsButton->GetStyle().backgroundColor = Panorama::Color(0.12f, 0.12f, 0.15f, 0.9f);
    m_settingsButton->GetStyle().borderRadius = 4.0f;
    m_settingsButton->GetStyle().fontSize = 20.0f;
    m_settingsButton->SetOnActivate([this]() {
        if (m_onSettings) m_onSettings();
    });
    m_topBar->AddChild(m_settingsButton);
    
    m_returnToGameButton = std::make_shared<Panorama::CButton>("←", "ReturnBtn");
    m_returnToGameButton->GetStyle().width = Panorama::Length::Px(160);
    m_returnToGameButton->GetStyle().height = Panorama::Length::Px(40);
    m_returnToGameButton->GetStyle().marginLeft = Panorama::Length::Px(60);
    m_returnToGameButton->GetStyle().marginTop = Panorama::Length::Px(7);
    m_returnToGameButton->GetStyle().backgroundColor = Panorama::Color(0.18f, 0.45f, 0.18f, 1.0f);
    m_returnToGameButton->GetStyle().borderRadius = 4.0f;
    m_returnToGameButton->GetStyle().fontSize = 14.0f;
    m_returnToGameButton->GetStyle().color = Panorama::Color::White();
    m_returnToGameButton->SetOnActivate([this]() {
        if (m_onReturnToGame) m_onReturnToGame();
    });
    m_returnToGameButton->SetVisible(false);
    m_topBar->AddChild(m_returnToGameButton);
    
    auto topContent = P("TopContent", contentWidth, 55, none);
    topContent->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    m_topBar->AddChild(topContent);
    
    float logoHeight = S(22);
    float logoOffsetY = (topBarHeight - logoHeight) / 2.0f;
    
    auto logo = P("Logo", 32, 22, Panorama::Color(0.75f, 0.12f, 0.12f, 1.0f));
    logo->GetStyle().borderRadius = S(2);
    logo->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    logo->GetStyle().marginTop = Panorama::Length::Px(logoOffsetY);
    topContent->AddChild(logo);
    
    const char* nav[] = {"HEROES", "STORE", "WATCH", "LEARN", "ARCADE"};
    m_navButtons.clear();
    for (int i = 0; i < 5; i++) {
        auto nb = std::make_shared<Panorama::CButton>(nav[i], std::string("Nav") + std::to_string(i));
        nb->AddClass("MainMenuNavButton");
        
        nb->GetStyle().backgroundColor.reset();
        nb->GetStyle().borderWidth.reset();
        nb->GetStyle().borderRadius.reset();
        nb->GetStyle().borderColor.reset();
        
        nb->GetStyle().width = Panorama::Length::Px(S(55));
        nb->GetStyle().height = Panorama::Length::Px(topBarHeight);
        nb->GetStyle().marginLeft = Panorama::Length::Px(S(48 + i * 61));
        nb->GetStyle().marginTop = Panorama::Length::Px(0);
        
        int navIndex = i;
        nb->SetOnActivate([this, navIndex]() {
            if (m_onNavClicked) m_onNavClicked(navIndex);
        });
        
        topContent->AddChild(nb);
        m_navButtons.push_back(nb);
    }
    
    auto pc = L("824,156 PLAYING", "caption", gray);
    pc->GetStyle().marginLeft = Panorama::Length::Px(S(contentWidth - 180));
    pc->GetStyle().marginTop = Panorama::Length::Px((topBarHeight - S(9)) / 2.0f);
    topContent->AddChild(pc);
    
    auto tm = L("7:48 PM", "caption", gray);
    tm->GetStyle().marginLeft = Panorama::Length::Px(S(contentWidth - 60));
    tm->GetStyle().marginTop = Panorama::Length::Px((topBarHeight - S(9)) / 2.0f);
    topContent->AddChild(tm);
}

void MainMenuTopBar::Destroy() {
    m_navButtons.clear();
    m_settingsButton.reset();
    m_returnToGameButton.reset();
    m_usernameLabel.reset();
    if (m_topBar) {
        if (auto* parent = m_topBar->GetParent()) {
            parent->RemoveChild(m_topBar.get());
        }
        m_topBar.reset();
    }
}

void MainMenuTopBar::SetReturnToGameVisible(bool visible) {
    if (m_returnToGameButton) {
        m_returnToGameButton->SetVisible(visible);
    }
}

void MainMenuTopBar::SetUsername(const std::string& username) {
    if (m_usernameLabel) {
        m_usernameLabel->SetText(username);
    }
}

} // namespace Game
