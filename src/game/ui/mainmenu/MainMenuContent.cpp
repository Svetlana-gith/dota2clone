#include "MainMenuContent.h"
#include "../panorama/CPanel2D.h"

namespace Game {

// Scale helper (1.35x for high DPI)
static inline float S(float v) { return v * 1.35f; }

// Panel helper
static std::shared_ptr<Panorama::CPanel2D> P(const std::string& id, float w, float h, Panorama::Color bg) {
    auto p = std::make_shared<Panorama::CPanel2D>(id);
    if (w > 0) p->GetStyle().width = Panorama::Length::Px(S(w));
    else p->GetStyle().width = Panorama::Length::Fill();
    if (h > 0) p->GetStyle().height = Panorama::Length::Px(S(h));
    else p->GetStyle().height = Panorama::Length::Fill();
    p->GetStyle().backgroundColor = bg;
    return p;
}

// Label helper using CSS classes
static std::shared_ptr<Panorama::CLabel> L(const std::string& text, const std::string& cssClass, Panorama::Color col) {
    auto l = std::make_shared<Panorama::CLabel>(text, text);
    l->AddClass(cssClass);
    l->GetStyle().color = col;
    return l;
}

MainMenuContent::MainMenuContent() = default;

void MainMenuContent::Create(Panorama::CPanel2D* parent, f32 contentWidth, f32 contentHeight, f32 offsetX, f32 offsetY) {
    if (!parent) return;
    
    // Colors
    Panorama::Color panel(0.08f, 0.09f, 0.11f, 0.92f);
    Panorama::Color gray(0.45f, 0.45f, 0.45f, 1.0f);
    Panorama::Color none(0, 0, 0, 0);

    // Main content container
    m_mainContainer = P("ContentMain", contentWidth / S(1), contentHeight, none);
    m_mainContainer->GetStyle().marginLeft = Panorama::Length::Px(offsetX);
    m_mainContainer->GetStyle().marginTop = Panorama::Length::Px(offsetY);
    parent->AddChild(m_mainContainer);

    CreateProfileColumn(m_mainContainer.get(), contentWidth, contentHeight);
    CreateCenterColumn(m_mainContainer.get(), contentWidth, contentHeight);
    CreateRightColumn(m_mainContainer.get(), contentWidth, contentHeight);
}

void MainMenuContent::CreateProfileColumn(Panorama::CPanel2D* main, f32 contentWidth, f32 contentHeight) {
    Panorama::Color panel(0.08f, 0.09f, 0.11f, 0.92f);
    Panorama::Color gray(0.45f, 0.45f, 0.45f, 1.0f);
    Panorama::Color none(0, 0, 0, 0);
    
    // Left column container (Profile + Friends)
    auto leftCol = P("LeftCol", 250, contentHeight, none);
    leftCol->GetStyle().marginLeft = Panorama::Length::Px(S(0));
    leftCol->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(leftCol);
    
    // ===== PROFILE PANEL =====
    m_profilePanel = P("Profile", 0, 140, panel);
    m_profilePanel->GetStyle().borderRadius = S(3);
    leftCol->AddChild(m_profilePanel);
    
    // Avatar
    m_avatarPanel = P("Avatar", 80, 80, Panorama::Color(0.25f, 0.35f, 0.45f, 1.0f));
    m_avatarPanel->GetStyle().borderRadius = S(4);
    m_avatarPanel->GetStyle().marginLeft = Panorama::Length::Px(S(12));
    m_avatarPanel->GetStyle().marginTop = Panorama::Length::Px(S(12));
    m_profilePanel->AddChild(m_avatarPanel);
    
    // Username
    m_usernameLabel = L("Player", "subheading", Panorama::Color::White());
    m_usernameLabel->GetStyle().marginLeft = Panorama::Length::Px(S(105));
    m_usernameLabel->GetStyle().marginTop = Panorama::Length::Px(S(18));
    m_profilePanel->AddChild(m_usernameLabel);
    
    // Level badge
    auto levelBadge = P("LvlBadge", 24, 24, Panorama::Color(0.6f, 0.5f, 0.2f, 1.0f));
    levelBadge->GetStyle().borderRadius = S(12);
    levelBadge->GetStyle().marginLeft = Panorama::Length::Px(S(105));
    levelBadge->GetStyle().marginTop = Panorama::Length::Px(S(45));
    m_profilePanel->AddChild(levelBadge);
    
    m_levelLabel = L("42", "caption", Panorama::Color::White());
    m_levelLabel->GetStyle().marginLeft = Panorama::Length::Px(S(135));
    m_levelLabel->GetStyle().marginTop = Panorama::Length::Px(S(48));
    m_profilePanel->AddChild(m_levelLabel);
    
    // Stats row
    auto statsLabel = L("1,247 MATCHES  |  52% WIN", "small", gray);
    statsLabel->GetStyle().marginLeft = Panorama::Length::Px(S(12));
    statsLabel->GetStyle().marginTop = Panorama::Length::Px(S(105));
    m_profilePanel->AddChild(statsLabel);
    
    // ===== FRIENDS PANEL =====
    // Fixed height instead of filling to bottom
    m_friendsPanel = P("Friends", 0, 280, panel);
    m_friendsPanel->GetStyle().borderRadius = S(3);
    m_friendsPanel->GetStyle().marginTop = Panorama::Length::Px(S(146));
    leftCol->AddChild(m_friendsPanel);
    
    // Friends header
    auto friendsHeader = P("FriendsHdr", 0, 28, Panorama::Color(0.06f, 0.07f, 0.09f, 1.0f));
    m_friendsPanel->AddChild(friendsHeader);
    
    auto friendsTitle = L("FRIENDS", "caption", gray);
    friendsTitle->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    friendsTitle->GetStyle().marginTop = Panorama::Length::Px(S(7));
    friendsHeader->AddChild(friendsTitle);
    
    auto onlineCount = L("3 ONLINE", "small", Panorama::Color(0.4f, 0.7f, 0.4f, 1.0f));
    onlineCount->GetStyle().marginLeft = Panorama::Length::Px(S(170));
    onlineCount->GetStyle().marginTop = Panorama::Length::Px(S(9));
    friendsHeader->AddChild(onlineCount);
    
    // Sample friends
    const char* friends[] = {"Serenity", "Choice", "HHr", "Dota2wage", "Iphone"};
    const bool online[] = {true, true, true, false, false};
    const char* status[] = {"In Game - Ranked", "Online", "In Game - Turbo", "Last seen 2h ago", "Last seen 1d ago"};
    
    for (int i = 0; i < 5; i++) {
        auto fe = P("FE" + std::to_string(i), 235, 45, Panorama::Color(0.07f, 0.08f, 0.1f, 0.6f));
        fe->GetStyle().borderRadius = S(2);
        fe->GetStyle().marginLeft = Panorama::Length::Px(S(8));
        fe->GetStyle().marginTop = Panorama::Length::Px(S(34 + i * 50));
        m_friendsPanel->AddChild(fe);
        
        // Friend avatar
        Panorama::Color avatarColor = online[i] ? 
            Panorama::Color(0.3f, 0.5f, 0.3f, 1.0f) : 
            Panorama::Color(0.3f, 0.3f, 0.35f, 1.0f);
        auto fa = P("FA" + std::to_string(i), 32, 32, avatarColor);
        fa->GetStyle().borderRadius = S(2);
        fa->GetStyle().marginLeft = Panorama::Length::Px(S(6));
        fa->GetStyle().marginTop = Panorama::Length::Px(S(6));
        fe->AddChild(fa);
        
        // Online indicator
        if (online[i]) {
            auto indicator = P("Ind" + std::to_string(i), 8, 8, Panorama::Color(0.3f, 0.8f, 0.3f, 1.0f));
            indicator->GetStyle().borderRadius = S(4);
            indicator->GetStyle().marginLeft = Panorama::Length::Px(S(30));
            indicator->GetStyle().marginTop = Panorama::Length::Px(S(30));
            fe->AddChild(indicator);
        }
        
        // Friend name
        Panorama::Color nameColor = online[i] ? Panorama::Color::White() : gray;
        auto fn = L(friends[i], "caption", nameColor);
        fn->GetStyle().marginLeft = Panorama::Length::Px(S(45));
        fn->GetStyle().marginTop = Panorama::Length::Px(S(6));
        fe->AddChild(fn);
        
        // Status
        Panorama::Color statusColor = online[i] ? 
            Panorama::Color(0.4f, 0.65f, 0.4f, 1.0f) : 
            Panorama::Color(0.4f, 0.4f, 0.4f, 1.0f);
        auto fs = L(status[i], "small", statusColor);
        fs->GetStyle().marginLeft = Panorama::Length::Px(S(45));
        fs->GetStyle().marginTop = Panorama::Length::Px(S(24));
        fe->AddChild(fs);
        
        m_friendEntries.push_back(fe);
    }
}

void MainMenuContent::CreateCenterColumn(Panorama::CPanel2D* main, f32 contentWidth, f32 contentHeight) {
    Panorama::Color gray(0.45f, 0.45f, 0.45f, 1.0f);
    Panorama::Color none(0, 0, 0, 0);

    // Center column (main content area)
    float cw = contentWidth - S(250) - S(270) - S(50);
    auto ctr = P("Ctr", cw / S(1), contentHeight, none);
    ctr->GetStyle().marginLeft = Panorama::Length::Px(S(270));
    ctr->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(ctr);

    float bw = cw / S(1) / 2 - S(4); // Block width for 2x2 grid

    // ===== ROW 1: NEWS BLOCKS =====
    // News 1 - Top Left
    m_newsBlock1 = P("N1", bw, 115, Panorama::Color(0.12f, 0.1f, 0.15f, 1.0f));
    m_newsBlock1->GetStyle().borderRadius = S(3);
    ctr->AddChild(m_newsBlock1);

    auto n1h = L("NEW IN DOTA 2", "small", gray);
    n1h->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1h->GetStyle().marginTop = Panorama::Length::Px(S(6));
    m_newsBlock1->AddChild(n1h);
    
    auto n1i = P("N1I", bw - S(20), 70, Panorama::Color(0.35f, 0.22f, 0.42f, 1.0f));
    n1i->GetStyle().borderRadius = S(3);
    n1i->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1i->GetStyle().marginTop = Panorama::Length::Px(S(20));
    m_newsBlock1->AddChild(n1i);
    
    auto n1t = L("FALL 2024 TREASURE II", "caption", Panorama::Color::White());
    n1t->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1t->GetStyle().marginTop = Panorama::Length::Px(S(95));
    m_newsBlock1->AddChild(n1t);

    // News 2 - Top Right
    m_newsBlock2 = P("N2", bw, 115, Panorama::Color(0.1f, 0.12f, 0.15f, 1.0f));
    m_newsBlock2->GetStyle().borderRadius = S(3);
    m_newsBlock2->GetStyle().marginLeft = Panorama::Length::Px(S(bw + 8));
    m_newsBlock2->GetStyle().marginTop = Panorama::Length::Px(S(0));
    ctr->AddChild(m_newsBlock2);

    auto n2h = L("PRO PLAYING LIVE", "small", gray);
    n2h->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2h->GetStyle().marginTop = Panorama::Length::Px(S(6));
    m_newsBlock2->AddChild(n2h);
    
    auto n2i = P("N2I", bw - S(20), 70, Panorama::Color(0.18f, 0.25f, 0.32f, 1.0f));
    n2i->GetStyle().borderRadius = S(3);
    n2i->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2i->GetStyle().marginTop = Panorama::Length::Px(S(20));
    m_newsBlock2->AddChild(n2i);
    
    auto n2t = L("FlipSid3.RodjER", "caption", Panorama::Color::White());
    n2t->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2t->GetStyle().marginTop = Panorama::Length::Px(S(95));
    m_newsBlock2->AddChild(n2t);

    // ===== ROW 2: FEATURED BLOCKS =====
    // Featured 1 - Bottom Left (Battle Pass)
    m_featuredBlock1 = P("FT1", bw, 80, Panorama::Color(0.15f, 0.12f, 0.08f, 1.0f));
    m_featuredBlock1->GetStyle().borderRadius = S(3);
    m_featuredBlock1->GetStyle().marginTop = Panorama::Length::Px(S(121));
    ctr->AddChild(m_featuredBlock1);

    auto bpi = P("BPI", 45, 50, Panorama::Color(0.5f, 0.4f, 0.18f, 1.0f));
    bpi->GetStyle().borderRadius = S(3);
    bpi->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    bpi->GetStyle().marginTop = Panorama::Length::Px(S(15));
    m_featuredBlock1->AddChild(bpi);
    
    auto bpl1 = L("FALL 2024", "small", Panorama::Color(0.75f, 0.65f, 0.35f, 1.0f));
    bpl1->GetStyle().marginLeft = Panorama::Length::Px(S(59));
    bpl1->GetStyle().marginTop = Panorama::Length::Px(S(20));
    m_featuredBlock1->AddChild(bpl1);
    
    auto bpl2 = L("BATTLE PASS", "small", Panorama::Color(0.75f, 0.65f, 0.35f, 1.0f));
    bpl2->GetStyle().marginLeft = Panorama::Length::Px(S(59));
    bpl2->GetStyle().marginTop = Panorama::Length::Px(S(32));
    m_featuredBlock1->AddChild(bpl2);

    // Featured 2 - Bottom Right (Game of the Day)
    m_featuredBlock2 = P("FT2", bw, 80, Panorama::Color(0.08f, 0.1f, 0.12f, 1.0f));
    m_featuredBlock2->GetStyle().borderRadius = S(3);
    m_featuredBlock2->GetStyle().marginLeft = Panorama::Length::Px(S(bw + 8));
    m_featuredBlock2->GetStyle().marginTop = Panorama::Length::Px(S(121));
    ctr->AddChild(m_featuredBlock2);

    auto gd = L("GAME OF THE DAY", "small", gray);
    gd->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    gd->GetStyle().marginTop = Panorama::Length::Px(S(6));
    m_featuredBlock2->AddChild(gd);
    
    auto hm = P("HM", bw - S(20), 40, Panorama::Color(0.12f, 0.15f, 0.18f, 1.0f));
    hm->GetStyle().borderRadius = S(3);
    hm->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    hm->GetStyle().marginTop = Panorama::Length::Px(S(26));
    m_featuredBlock2->AddChild(hm);
    
    auto hml = L("HORDE MODE", "body", Panorama::Color::White());
    hml->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    hml->GetStyle().marginTop = Panorama::Length::Px(S(12));
    hm->AddChild(hml);

    // ===== CHAT (Below 2x2 grid) =====
    CreateChatPanel(ctr.get());
}

void MainMenuContent::CreateChatPanel(Panorama::CPanel2D* container) {
    Panorama::Color gray(0.45f, 0.45f, 0.45f, 1.0f);

    m_chatPanel = P("Ch", 0, 110, Panorama::Color(0.08f, 0.08f, 0.1f, 1.0f));
    m_chatPanel->GetStyle().borderRadius = S(3);
    m_chatPanel->GetStyle().marginTop = Panorama::Length::Px(S(207));
    container->AddChild(m_chatPanel);

    // Chat header
    auto chh = P("CHH", 0, 22, Panorama::Color(0.06f, 0.06f, 0.08f, 1.0f));
    m_chatPanel->AddChild(chh);

    auto pty = L("Party", "caption", Panorama::Color(0.55f, 0.55f, 0.55f, 1.0f));
    pty->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    pty->GetStyle().marginTop = Panorama::Length::Px(S(5));
    chh->AddChild(pty);
    
    auto chl = L("CHANNELS +", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    chl->GetStyle().marginLeft = Panorama::Length::Px(S(150));
    chl->GetStyle().marginTop = Panorama::Length::Px(S(7));
    chh->AddChild(chl);

    // Chat messages
    auto m1 = L("Serenity: noob riki", "caption", Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    m1->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    m1->GetStyle().marginTop = Panorama::Length::Px(S(28));
    m_chatPanel->AddChild(m1);
    
    auto m2 = L("Choice: poor pudge", "caption", Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    m2->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    m2->GetStyle().marginTop = Panorama::Length::Px(S(44));
    m_chatPanel->AddChild(m2);

    // Chat input
    auto chi = P("CHI", 240, 18, Panorama::Color(0.12f, 0.12f, 0.14f, 1.0f));
    chi->GetStyle().borderRadius = S(2);
    chi->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    chi->GetStyle().marginTop = Panorama::Length::Px(S(78));
    m_chatPanel->AddChild(chi);
}

void MainMenuContent::CreateRightColumn(Panorama::CPanel2D* main, f32 contentWidth, f32 contentHeight) {
    Panorama::Color panel(0.08f, 0.09f, 0.11f, 0.92f);
    Panorama::Color gray(0.45f, 0.45f, 0.45f, 1.0f);
    Panorama::Color none(0, 0, 0, 0);

    // ===== RIGHT COLUMN =====
    auto rgt = P("Rgt", 270, contentHeight, none);
    rgt->GetStyle().marginLeft = Panorama::Length::Px(contentWidth - S(270) - S(10));
    rgt->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(rgt);

    // Last Match
    m_lastMatchPanel = P("LM", 0, 120, panel);
    m_lastMatchPanel->GetStyle().borderRadius = S(3);
    m_lastMatchPanel->GetStyle().marginTop = Panorama::Length::Px(S(0));
    rgt->AddChild(m_lastMatchPanel);

    auto lmh = L("YOUR LAST MATCH", "small", gray);
    lmh->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    lmh->GetStyle().marginTop = Panorama::Length::Px(S(6));
    m_lastMatchPanel->AddChild(lmh);
    
    auto lmt = L("10/31/2024  7:03 PM", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    lmt->GetStyle().marginLeft = Panorama::Length::Px(S(135));
    lmt->GetStyle().marginTop = Panorama::Length::Px(S(6));
    m_lastMatchPanel->AddChild(lmt);

    m_heroNameLabel = L("JUGGERNAUT", "subheading", Panorama::Color::White());
    m_heroNameLabel->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    m_heroNameLabel->GetStyle().marginTop = Panorama::Length::Px(S(24));
    m_lastMatchPanel->AddChild(m_heroNameLabel);

    m_matchResultLabel = L("WON - ALL PICK", "caption", Panorama::Color(0.28f, 0.7f, 0.28f, 1.0f));
    m_matchResultLabel->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    m_matchResultLabel->GetStyle().marginTop = Panorama::Length::Px(S(44));
    m_lastMatchPanel->AddChild(m_matchResultLabel);

    auto kdl = L("K/D/A", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    kdl->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    kdl->GetStyle().marginTop = Panorama::Length::Px(S(62));
    m_lastMatchPanel->AddChild(kdl);
    
    m_kdaLabel = L("9 / 2 / 4", "caption", Panorama::Color::White());
    m_kdaLabel->GetStyle().marginLeft = Panorama::Length::Px(S(50));
    m_kdaLabel->GetStyle().marginTop = Panorama::Length::Px(S(61));
    m_lastMatchPanel->AddChild(m_kdaLabel);
    
    auto drl = L("DURATION", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    drl->GetStyle().marginLeft = Panorama::Length::Px(S(135));
    drl->GetStyle().marginTop = Panorama::Length::Px(S(62));
    m_lastMatchPanel->AddChild(drl);
    
    m_durationLabel = L("37:14", "caption", Panorama::Color::White());
    m_durationLabel->GetStyle().marginLeft = Panorama::Length::Px(S(200));
    m_durationLabel->GetStyle().marginTop = Panorama::Length::Px(S(61));
    m_lastMatchPanel->AddChild(m_durationLabel);

    auto itl = L("ITEMS", "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    itl->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    itl->GetStyle().marginTop = Panorama::Length::Px(S(80));
    m_lastMatchPanel->AddChild(itl);

    Panorama::Color ic[] = {{0.45f,0.32f,0.18f,1}, {0.28f,0.45f,0.28f,1}, {0.55f,0.28f,0.28f,1}, {0.35f,0.35f,0.45f,1}, {0.45f,0.35f,0.22f,1}, {0.32f,0.32f,0.35f,1}};
    for (int i = 0; i < 6; i++) {
        auto it = P("IT" + std::to_string(i), 24, 18, ic[i]);
        it->GetStyle().borderRadius = S(1);
        it->GetStyle().marginLeft = Panorama::Length::Px(S(8 + i * 32));
        it->GetStyle().marginTop = Panorama::Length::Px(S(95));
        m_lastMatchPanel->AddChild(it);
    }

    // Activity
    m_activityPanel = P("Act", 0, 220, panel);
    m_activityPanel->GetStyle().borderRadius = S(3);
    m_activityPanel->GetStyle().marginTop = Panorama::Length::Px(S(126));
    rgt->AddChild(m_activityPanel);

    auto acth = L("Say something on your feed...", "caption", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    acth->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    acth->GetStyle().marginTop = Panorama::Length::Px(S(6));
    m_activityPanel->AddChild(acth);

    const char* at[] = {"HHr got a RAMPAGE as Sven", "Dota2wage advanced to Semi", "Iphone posted: random games", "Choice is now playing"};
    const char* atm[] = {"Yesterday", "Saturday", "Friday", "Just now"};
    
    for (int i = 0; i < 4; i++) {
        // Activity entry container
        auto ae = P("AE" + std::to_string(i), 250, 40, Panorama::Color(0.08f, 0.08f, 0.1f, 0.5f));
        ae->GetStyle().borderRadius = S(2);
        ae->GetStyle().marginLeft = Panorama::Length::Px(S(8));
        ae->GetStyle().marginTop = Panorama::Length::Px(S(26 + i * 45));
        m_activityPanel->AddChild(ae);

        // Avatar
        auto aa = P("AA" + std::to_string(i), 26, 26, Panorama::Color(0.28f, 0.22f + i*0.03f, 0.18f, 1.0f));
        aa->GetStyle().borderRadius = S(2);
        aa->GetStyle().marginLeft = Panorama::Length::Px(S(5));
        aa->GetStyle().marginTop = Panorama::Length::Px(S(7));
        ae->AddChild(aa);

        // Activity text
        auto atx = L(at[i], "small", Panorama::Color(0.55f, 0.55f, 0.55f, 1.0f));
        atx->GetStyle().marginLeft = Panorama::Length::Px(S(36));
        atx->GetStyle().marginTop = Panorama::Length::Px(S(8));
        ae->AddChild(atx);
        
        // Time
        auto atmx = L(atm[i], "small", Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
        atmx->GetStyle().marginLeft = Panorama::Length::Px(S(36));
        atmx->GetStyle().marginTop = Panorama::Length::Px(S(22));
        ae->AddChild(atmx);
    }
}

void MainMenuContent::Destroy() {
    if (m_mainContainer) {
        if (auto* parent = m_mainContainer->GetParent()) {
            parent->RemoveChild(m_mainContainer.get());
        }
        m_mainContainer.reset();
    }
    
    // Profile column
    m_profilePanel.reset();
    m_avatarPanel.reset();
    m_usernameLabel.reset();
    m_levelLabel.reset();
    m_friendsPanel.reset();
    m_friendEntries.clear();
    
    // Center column
    m_newsBlock1.reset();
    m_newsBlock2.reset();
    m_featuredBlock1.reset();
    m_featuredBlock2.reset();
    m_chatPanel.reset();
    
    // Right column
    m_lastMatchPanel.reset();
    m_activityPanel.reset();
    m_heroNameLabel.reset();
    m_matchResultLabel.reset();
    m_kdaLabel.reset();
    m_durationLabel.reset();
}

void MainMenuContent::SetUsername(const std::string& username) {
    if (m_usernameLabel) {
        m_usernameLabel->SetText(username);
    }
}

void MainMenuContent::SetLevel(int level) {
    if (m_levelLabel) {
        m_levelLabel->SetText(std::to_string(level));
    }
}

void MainMenuContent::AddFriend(const std::string& name, bool online, const std::string& status) {
    // TODO: Implement dynamic friend list management
}

void MainMenuContent::UpdateLastMatch(const std::string& heroName, const std::string& result, 
                                       int kills, int deaths, int assists, const std::string& duration) {
    if (m_heroNameLabel) {
        m_heroNameLabel->SetText(heroName);
    }
    if (m_matchResultLabel) {
        m_matchResultLabel->SetText(result);
        // Update color based on win/loss
        if (result.find("WON") != std::string::npos) {
            m_matchResultLabel->GetStyle().color = Panorama::Color(0.28f, 0.7f, 0.28f, 1.0f);
        } else {
            m_matchResultLabel->GetStyle().color = Panorama::Color(0.7f, 0.28f, 0.28f, 1.0f);
        }
    }
    if (m_kdaLabel) {
        m_kdaLabel->SetText(std::to_string(kills) + " / " + std::to_string(deaths) + " / " + std::to_string(assists));
    }
    if (m_durationLabel) {
        m_durationLabel->SetText(duration);
    }
}

void MainMenuContent::AddChatMessage(const std::string& username, const std::string& message) {
    // TODO: Implement scrolling chat with message history
    // For now this is a placeholder
}

} // namespace Game
