#include "MatchmakingPanel.h"
#include "../panorama/CUIEngine.h"
#include <cstdio>

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

MatchmakingPanel::MatchmakingPanel() = default;

void MatchmakingPanel::Create(Panorama::CPanel2D* parent, Panorama::CPanel2D* bottomBar,
                               f32 screenWidth, f32 screenHeight, f32 contentWidth, f32 playButtonX) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    const float playX = playButtonX;
    const float playY = 12.0f;

    m_findingPanel = P("MM_FindingPanel", 180, 45, Panorama::Color(0.12f, 0.16f, 0.22f, 1.0f));
    m_findingPanel->GetStyle().borderRadius = S(3);
    m_findingPanel->GetStyle().marginLeft = Panorama::Length::Px(playX);
    m_findingPanel->GetStyle().marginTop = Panorama::Length::Px(playY);
    m_findingPanel->SetVisible(false);
    bottomBar->AddChild(m_findingPanel);

    m_findingLabel = L("FINDING MATCH", "body", Panorama::Color(0.85f, 0.9f, 0.95f, 1.0f));
    m_findingLabel->GetStyle().marginLeft = Panorama::Length::Px(S(14));
    m_findingLabel->GetStyle().marginTop = Panorama::Length::Px(S(14));
    m_findingPanel->AddChild(m_findingLabel);

    m_findingCancelButton = std::make_shared<Panorama::CButton>("X", "MM_FindCancel");
    m_findingCancelButton->GetStyle().width = Panorama::Length::Px(S(30));
    m_findingCancelButton->GetStyle().height = Panorama::Length::Px(S(30));
    m_findingCancelButton->GetStyle().backgroundColor = Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
    m_findingCancelButton->GetStyle().borderRadius = S(3);
    m_findingCancelButton->AddClass("subheading");
    m_findingCancelButton->GetStyle().color = Panorama::Color::White();
    m_findingCancelButton->GetStyle().marginLeft = Panorama::Length::Px(S(180 - 36));
    m_findingCancelButton->GetStyle().marginTop = Panorama::Length::Px(S(7));
    m_findingCancelButton->SetOnActivate([this]() {
        if (m_onCancelClicked) m_onCancelClicked();
    });
    m_findingPanel->AddChild(m_findingCancelButton);

    m_findingTimeLabel = L("00:00", "body", Panorama::Color(0.65f, 0.75f, 0.85f, 1.0f));
    m_findingTimeLabel->GetStyle().marginLeft = Panorama::Length::Px(playX + S(2));
    m_findingTimeLabel->GetStyle().marginTop = Panorama::Length::Px(S(2));
    m_findingTimeLabel->SetVisible(false);
    bottomBar->AddChild(m_findingTimeLabel);

    m_searchingOverlay = P("MM_SearchingOverlay", 0, 0, Panorama::Color(0.0f, 0.0f, 0.0f, 0.55f));
    m_searchingOverlay->SetVisible(false);
    parent->AddChild(m_searchingOverlay);

    auto searchingBox = P("MM_SearchingBox", 420, 170, Panorama::Color(0.08f, 0.09f, 0.11f, 0.96f));
    searchingBox->GetStyle().borderRadius = S(4);
    searchingBox->GetStyle().marginLeft = Panorama::Length::Px((screenWidth - S(420)) * 0.5f);
    searchingBox->GetStyle().marginTop = Panorama::Length::Px((screenHeight - S(170)) * 0.5f);
    m_searchingOverlay->AddChild(searchingBox);

    m_searchingLabel = L("SEARCHING FOR MATCH...", "subheading", Panorama::Color(0.85f, 0.85f, 0.85f, 1.0f));
    m_searchingLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_searchingLabel->GetStyle().marginTop = Panorama::Length::Px(S(22));
    searchingBox->AddChild(m_searchingLabel);

    m_searchTimeLabel = L("00:00", "body", Panorama::Color(0.65f, 0.65f, 0.65f, 1.0f));
    m_searchTimeLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_searchTimeLabel->GetStyle().marginTop = Panorama::Length::Px(S(55));
    searchingBox->AddChild(m_searchTimeLabel);

    m_cancelSearchButton = std::make_shared<Panorama::CButton>("CANCEL", "MM_Cancel");
    m_cancelSearchButton->GetStyle().width = Panorama::Length::Px(S(140));
    m_cancelSearchButton->GetStyle().height = Panorama::Length::Px(S(40));
    m_cancelSearchButton->GetStyle().backgroundColor = Panorama::Color(0.25f, 0.25f, 0.3f, 0.95f);
    m_cancelSearchButton->GetStyle().borderRadius = S(3);
    m_cancelSearchButton->AddClass("body");
    m_cancelSearchButton->GetStyle().color = Panorama::Color::White();
    m_cancelSearchButton->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_cancelSearchButton->GetStyle().marginTop = Panorama::Length::Px(S(95));
    m_cancelSearchButton->SetOnActivate([this]() {
        if (m_onCancelClicked) m_onCancelClicked();
    });
    searchingBox->AddChild(m_cancelSearchButton);

    m_acceptOverlay = P("MM_AcceptOverlay", 0, 0, Panorama::Color(0.0f, 0.0f, 0.0f, 0.65f));
    m_acceptOverlay->SetVisible(false);
    parent->AddChild(m_acceptOverlay);

    auto acceptBox = P("MM_AcceptBox", 460, 210, Panorama::Color(0.08f, 0.09f, 0.11f, 0.98f));
    acceptBox->GetStyle().borderRadius = S(4);
    acceptBox->GetStyle().marginLeft = Panorama::Length::Px((screenWidth - S(460)) * 0.5f);
    acceptBox->GetStyle().marginTop = Panorama::Length::Px((screenHeight - S(210)) * 0.5f);
    m_acceptOverlay->AddChild(acceptBox);

    m_acceptLabel = L("MATCH FOUND", "heading", Panorama::Color(0.92f, 0.92f, 0.92f, 1.0f));
    m_acceptLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_acceptLabel->GetStyle().marginTop = Panorama::Length::Px(S(25));
    acceptBox->AddChild(m_acceptLabel);

    m_acceptCountdownLabel = L("00:20", "body", Panorama::Color(0.65f, 0.75f, 0.85f, 1.0f));
    m_acceptCountdownLabel->GetStyle().marginLeft = Panorama::Length::Px(S(360));
    m_acceptCountdownLabel->GetStyle().marginTop = Panorama::Length::Px(S(28));
    acceptBox->AddChild(m_acceptCountdownLabel);

    m_acceptStatusLabel = L("0/0 ACCEPTED", "body", Panorama::Color(0.75f, 0.75f, 0.75f, 1.0f));
    m_acceptStatusLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_acceptStatusLabel->GetStyle().marginTop = Panorama::Length::Px(S(60));
    m_acceptStatusLabel->SetVisible(false);
    acceptBox->AddChild(m_acceptStatusLabel);

    m_acceptStatusPanel = P("MM_AcceptStatusPanel", 420, 28, Panorama::Color(0, 0, 0, 0));
    m_acceptStatusPanel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_acceptStatusPanel->GetStyle().marginTop = Panorama::Length::Px(S(85));
    m_acceptStatusPanel->SetVisible(false);
    acceptBox->AddChild(m_acceptStatusPanel);

    m_acceptButton = std::make_shared<Panorama::CButton>("ACCEPT", "MM_Accept");
    m_acceptButton->GetStyle().width = Panorama::Length::Px(S(160));
    m_acceptButton->GetStyle().height = Panorama::Length::Px(S(46));
    m_acceptButton->GetStyle().backgroundColor = Panorama::Color(0.18f, 0.45f, 0.18f, 1.0f);
    m_acceptButton->GetStyle().borderRadius = S(3);
    m_acceptButton->AddClass("subheading");
    m_acceptButton->GetStyle().color = Panorama::Color::White();
    m_acceptButton->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_acceptButton->GetStyle().marginTop = Panorama::Length::Px(S(115));
    m_acceptButton->SetOnActivate([this]() {
        if (m_onAcceptClicked) m_onAcceptClicked();
    });
    acceptBox->AddChild(m_acceptButton);

    m_declineButton = std::make_shared<Panorama::CButton>("DECLINE", "MM_Decline");
    m_declineButton->GetStyle().width = Panorama::Length::Px(S(160));
    m_declineButton->GetStyle().height = Panorama::Length::Px(S(46));
    m_declineButton->GetStyle().backgroundColor = Panorama::Color(0.45f, 0.18f, 0.18f, 1.0f);
    m_declineButton->GetStyle().borderRadius = S(3);
    m_declineButton->AddClass("subheading");
    m_declineButton->GetStyle().color = Panorama::Color::White();
    m_declineButton->GetStyle().marginLeft = Panorama::Length::Px(S(210));
    m_declineButton->GetStyle().marginTop = Panorama::Length::Px(S(115));
    m_declineButton->SetOnActivate([this]() {
        if (m_onDeclineClicked) m_onDeclineClicked();
    });
    acceptBox->AddChild(m_declineButton);
}

void MatchmakingPanel::Destroy() {
    m_findingPanel.reset();
    m_findingLabel.reset();
    m_findingTimeLabel.reset();
    m_findingCancelButton.reset();
    m_searchingOverlay.reset();
    m_searchingLabel.reset();
    m_searchTimeLabel.reset();
    m_cancelSearchButton.reset();
    m_acceptOverlay.reset();
    m_acceptLabel.reset();
    m_acceptCountdownLabel.reset();
    m_acceptButton.reset();
    m_declineButton.reset();
    m_acceptStatusLabel.reset();
    m_acceptStatusPanel.reset();
    m_acceptCubes.clear();
}

void MatchmakingPanel::Update(f32 dt) {
}

void MatchmakingPanel::ShowFindingUI() {
    if (m_findingPanel) m_findingPanel->SetVisible(true);
    if (m_findingTimeLabel) {
        m_findingTimeLabel->SetText("00:00");
        m_findingTimeLabel->SetVisible(true);
    }
}

void MatchmakingPanel::HideFindingUI() {
    if (m_findingPanel) m_findingPanel->SetVisible(false);
    if (m_findingTimeLabel) m_findingTimeLabel->SetVisible(false);
}

void MatchmakingPanel::ShowAcceptOverlay(const WorldEditor::Matchmaking::LobbyInfo& lobby) {
    if (m_acceptOverlay) m_acceptOverlay->SetVisible(true);

    if (m_acceptButton) m_acceptButton->SetVisible(true);
    if (m_declineButton) m_declineButton->SetVisible(true);
    if (m_acceptButton) m_acceptButton->SetEnabled(true);
    if (m_declineButton) m_declineButton->SetEnabled(true);
    if (m_acceptStatusLabel) m_acceptStatusLabel->SetVisible(false);
    if (m_acceptStatusPanel) m_acceptStatusPanel->SetVisible(false);

    if (m_acceptStatusPanel) {
        m_acceptStatusPanel->RemoveAndDeleteChildren();
        m_acceptCubes.clear();
        const int count = (int)lobby.players.size();
        const float cube = S(18);
        const float gap = S(8);
        for (int i = 0; i < count; ++i) {
            auto c = P("MM_Cube_" + std::to_string(i), 18, 18, Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f));
            c->GetStyle().borderRadius = S(2);
            c->GetStyle().marginLeft = Panorama::Length::Px(i * (cube + gap));
            c->GetStyle().marginTop = Panorama::Length::Px(0);
            m_acceptStatusPanel->AddChild(c);
            m_acceptCubes.push_back(c);
        }
    }
}

void MatchmakingPanel::HideAcceptOverlay() {
    if (m_acceptOverlay) m_acceptOverlay->SetVisible(false);
}

bool MatchmakingPanel::IsSearching() const {
    return m_findingPanel && m_findingPanel->IsVisible();
}

void MatchmakingPanel::UpdateAcceptCountdown(f32 remainingSeconds) {
    if (m_acceptCountdownLabel) {
        const int sec = std::max(0, (int)std::ceil(remainingSeconds));
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%02d:%02d", sec / 60, sec % 60);
        m_acceptCountdownLabel->SetText(buf);
    }
}

void MatchmakingPanel::UpdateAcceptStatus(u16 requiredPlayers, const std::vector<u64>& playerIds,
                                          const std::vector<bool>& accepted, u64 selfId) {
    const int count = (int)accepted.size();

    int acceptedCount = 0;
    for (bool a : accepted) if (a) acceptedCount++;

    if (m_acceptStatusLabel) {
        m_acceptStatusLabel->SetText(std::to_string(acceptedCount) + "/" + std::to_string(requiredPlayers) + " ACCEPTED");
    }

    for (int i = 0; i < count && i < (int)m_acceptCubes.size(); ++i) {
        if (!m_acceptCubes[i]) continue;
        m_acceptCubes[i]->GetStyle().backgroundColor = accepted[i]
            ? Panorama::Color(0.18f, 0.55f, 0.18f, 1.0f)
            : Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
    }

    bool selfAccepted = false;
    for (size_t i = 0; i < playerIds.size() && i < accepted.size(); ++i) {
        if (playerIds[i] == selfId) {
            selfAccepted = accepted[i];
            break;
        }
    }
    if (selfAccepted) {
        if (m_acceptButton) m_acceptButton->SetVisible(false);
        if (m_declineButton) m_declineButton->SetVisible(false);
        if (m_acceptStatusLabel) m_acceptStatusLabel->SetVisible(true);
        if (m_acceptStatusPanel) m_acceptStatusPanel->SetVisible(true);
    }
}

void MatchmakingPanel::OnLocalPlayerAccepted(u64 selfId, const std::vector<u64>& playerIds) {
    if (m_acceptButton) m_acceptButton->SetVisible(false);
    if (m_declineButton) m_declineButton->SetVisible(false);
    if (m_acceptStatusLabel) m_acceptStatusLabel->SetVisible(true);
    if (m_acceptStatusPanel) m_acceptStatusPanel->SetVisible(true);

    if (m_acceptStatusLabel) {
        const int total = (int)m_acceptCubes.size();
        if (total > 0) {
            m_acceptStatusLabel->SetText("1/" + std::to_string(total) + " ACCEPTED");
        }
    }

    int selfIndex = -1;
    for (size_t i = 0; i < playerIds.size(); ++i) {
        if (playerIds[i] == selfId) { selfIndex = (int)i; break; }
    }
    if (selfIndex < 0) selfIndex = 0;

    for (int i = 0; i < (int)m_acceptCubes.size(); ++i) {
        if (!m_acceptCubes[i]) continue;
        m_acceptCubes[i]->GetStyle().backgroundColor = (i == selfIndex)
            ? Panorama::Color(0.18f, 0.55f, 0.18f, 1.0f)
            : Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
    }
}

} // namespace Game
