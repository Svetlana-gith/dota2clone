#include "ReconnectPanel.h"
#include <cstdio>

namespace Game {

static const f32 PANEL_WIDTH = 500.0f;
static const f32 PANEL_HEIGHT = 250.0f;

static const Panorama::Color COL_OVERLAY(0.0f, 0.0f, 0.0f, 0.85f);
static const Panorama::Color COL_PANEL(0.1f, 0.12f, 0.15f, 0.98f);
static const Panorama::Color COL_TITLE(0.95f, 0.75f, 0.25f, 1.0f);
static const Panorama::Color COL_INFO(0.7f, 0.7f, 0.7f, 1.0f);
static const Panorama::Color COL_GREEN(0.2f, 0.55f, 0.2f, 1.0f);
static const Panorama::Color COL_RED(0.55f, 0.2f, 0.2f, 1.0f);

ReconnectPanel::ReconnectPanel() = default;

void ReconnectPanel::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    
    m_overlay = std::make_shared<Panorama::CPanel2D>("ReconnectOverlay");
    m_overlay->GetStyle().width = Panorama::Length::Fill();
    m_overlay->GetStyle().height = Panorama::Length::Fill();
    m_overlay->GetStyle().backgroundColor = COL_OVERLAY;
    m_overlay->SetVisible(false);
    parent->AddChild(m_overlay);
    
    auto panel = std::make_shared<Panorama::CPanel2D>("ReconnectPanel");
    panel->GetStyle().width = Panorama::Length::Px(PANEL_WIDTH);
    panel->GetStyle().height = Panorama::Length::Px(PANEL_HEIGHT);
    panel->GetStyle().backgroundColor = COL_PANEL;
    panel->GetStyle().borderRadius = 8.0f;
    panel->GetStyle().marginLeft = Panorama::Length::Px((screenWidth - PANEL_WIDTH) / 2);
    panel->GetStyle().marginTop = Panorama::Length::Px((screenHeight - PANEL_HEIGHT) / 2);
    m_overlay->AddChild(panel);
    
    m_titleLabel = std::make_shared<Panorama::CLabel>("GAME IN PROGRESS", "ReconnectTitle");
    m_titleLabel->GetStyle().fontSize = 28.0f;
    m_titleLabel->GetStyle().color = COL_TITLE;
    m_titleLabel->GetStyle().marginLeft = Panorama::Length::Px(120);
    m_titleLabel->GetStyle().marginTop = Panorama::Length::Px(30);
    panel->AddChild(m_titleLabel);
    
    m_infoLabel = std::make_shared<Panorama::CLabel>("", "ReconnectInfo");
    m_infoLabel->GetStyle().fontSize = 16.0f;
    m_infoLabel->GetStyle().color = COL_INFO;
    m_infoLabel->GetStyle().marginLeft = Panorama::Length::Px(50);
    m_infoLabel->GetStyle().marginTop = Panorama::Length::Px(80);
    panel->AddChild(m_infoLabel);
    
    m_reconnectButton = std::make_shared<Panorama::CButton>("RECONNECT", "ReconnectBtn");
    m_reconnectButton->GetStyle().width = Panorama::Length::Px(180);
    m_reconnectButton->GetStyle().height = Panorama::Length::Px(50);
    m_reconnectButton->GetStyle().backgroundColor = COL_GREEN;
    m_reconnectButton->GetStyle().borderRadius = 4.0f;
    m_reconnectButton->GetStyle().color = Panorama::Color::White();
    m_reconnectButton->GetStyle().marginLeft = Panorama::Length::Px(50);
    m_reconnectButton->GetStyle().marginTop = Panorama::Length::Px(160);
    m_reconnectButton->SetOnActivate([this]() {
        if (m_onReconnect) m_onReconnect();
    });
    panel->AddChild(m_reconnectButton);
    
    m_abandonButton = std::make_shared<Panorama::CButton>("ABANDON", "AbandonBtn");
    m_abandonButton->GetStyle().width = Panorama::Length::Px(180);
    m_abandonButton->GetStyle().height = Panorama::Length::Px(50);
    m_abandonButton->GetStyle().backgroundColor = COL_RED;
    m_abandonButton->GetStyle().borderRadius = 4.0f;
    m_abandonButton->GetStyle().color = Panorama::Color::White();
    m_abandonButton->GetStyle().marginLeft = Panorama::Length::Px(270);
    m_abandonButton->GetStyle().marginTop = Panorama::Length::Px(160);
    m_abandonButton->SetOnActivate([this]() {
        if (m_onAbandon) m_onAbandon();
    });
    panel->AddChild(m_abandonButton);
}

void ReconnectPanel::Destroy() {
    m_titleLabel.reset();
    m_infoLabel.reset();
    m_reconnectButton.reset();
    m_abandonButton.reset();
    m_overlay.reset();
}

void ReconnectPanel::Show(const WorldEditor::Matchmaking::ActiveGameInfo& gameInfo) {
    m_activeGameInfo = gameInfo;
    
    if (m_infoLabel) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Hero: %s\nGame Time: %.0f seconds\nDisconnected: %.0f seconds ago",
                 gameInfo.heroName.c_str(), gameInfo.gameTime, gameInfo.disconnectTime);
        m_infoLabel->SetText(buf);
    }
    
    if (m_overlay) m_overlay->SetVisible(true);
}

void ReconnectPanel::Hide() {
    if (m_overlay) m_overlay->SetVisible(false);
}

bool ReconnectPanel::IsVisible() const {
    return m_overlay && m_overlay->IsVisible();
}

} // namespace Game
