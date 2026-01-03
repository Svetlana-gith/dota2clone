#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/CUIEngine.h"
#include "../ui/panorama/CPanel2D.h"
#include "../ui/panorama/CStyleSheet.h"
#include "../ui/panorama/GameEvents.h"
#include "network/MatchmakingClient.h"
#include <cstdio>

namespace Game {

struct MainMenuState::MenuUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    std::shared_ptr<Panorama::CPanel2D> bottomBar;
    std::shared_ptr<Panorama::CButton> playButton;
    std::shared_ptr<Panorama::CLabel> serverIPLabel;
    std::string serverIP = "127.0.0.1";  // Default to localhost
    
    // Matchmaking UI
    std::shared_ptr<Panorama::CPanel2D> searchingOverlay;
    std::shared_ptr<Panorama::CLabel> searchingLabel;
    std::shared_ptr<Panorama::CLabel> searchTimeLabel;
    std::shared_ptr<Panorama::CButton> cancelSearchButton;

    // Dota-like "Finding Match" button swap (bottom bar)
    std::shared_ptr<Panorama::CPanel2D> findingPanel;
    std::shared_ptr<Panorama::CLabel> findingTimeLabel;
    std::shared_ptr<Panorama::CLabel> findingLabel;
    std::shared_ptr<Panorama::CButton> findingCancelButton;

    std::shared_ptr<Panorama::CPanel2D> acceptOverlay;
    std::shared_ptr<Panorama::CLabel> acceptLabel;
    std::shared_ptr<Panorama::CLabel> acceptCountdownLabel;
    std::shared_ptr<Panorama::CButton> acceptButton;
    std::shared_ptr<Panorama::CButton> declineButton;
    std::shared_ptr<Panorama::CLabel> acceptStatusLabel;
    std::shared_ptr<Panorama::CPanel2D> acceptStatusPanel;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> acceptCubes;
};

MainMenuState::MainMenuState() : m_ui(std::make_unique<MenuUI>()) {}
MainMenuState::~MainMenuState() = default;

void MainMenuState::OnEnter() { 
    LOG_INFO("MainMenuState::OnEnter()");
    CreateUI(); 
    LOG_INFO("MainMenuState UI created");
    ConsoleLog("Main Menu loaded");
}
void MainMenuState::OnExit() { DestroyUI(); }

// Scaled helpers
static float S(float v) { return v * 1.35f; } // Scale factor (35% increase)

static std::shared_ptr<Panorama::CPanel2D> P(const std::string& id, float w, float h, Panorama::Color bg) {
    auto p = std::make_shared<Panorama::CPanel2D>(id);
    if (w > 0) p->GetStyle().width = Panorama::Length::Px(S(w));
    else p->GetStyle().width = Panorama::Length::Fill();
    if (h > 0) p->GetStyle().height = Panorama::Length::Px(S(h));
    else p->GetStyle().height = Panorama::Length::Fill();
    p->GetStyle().backgroundColor = bg;
    return p;
}

static std::shared_ptr<Panorama::CLabel> L(const std::string& text, float size, Panorama::Color col) {
    auto l = std::make_shared<Panorama::CLabel>(text, text);
    l->GetStyle().fontSize = S(size);
    l->GetStyle().color = col;
    return l;
}

void MainMenuState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    float sw = engine.GetScreenWidth();
    float sh = engine.GetScreenHeight();
    
    // Colors - Dark blue-black gradient background like Dota 2
    Panorama::Color bg(0.02f, 0.04f, 0.08f, 1.0f);  // Very dark blue-black
    Panorama::Color panel(0.08f, 0.09f, 0.11f, 0.92f);  // Slightly transparent dark panels
    Panorama::Color header(0.01f, 0.02f, 0.04f, 0.95f);  // Almost black with blue tint
    Panorama::Color gray(0.45f, 0.45f, 0.45f, 1.0f);
    Panorama::Color light(0.65f, 0.65f, 0.65f, 1.0f);
    Panorama::Color green(0.35f, 0.65f, 0.35f, 1.0f);
    Panorama::Color greenBtn(0.18f, 0.45f, 0.18f, 1.0f);
    Panorama::Color none(0,0,0,0);

    // Calculate content dimensions (80% of screen width, centered)
    float contentWidth = sw * 0.8f;
    float contentHeight = (sh - 70) * 0.9f;
    float contentOffsetX = (sw - contentWidth) / 2.0f;
    float contentOffsetY = ((sh - 70) - contentHeight) / 2.0f;

    // ROOT
    m_ui->root = P("Root", 0, 0, bg);
    uiRoot->AddChild(m_ui->root);

    // ===== TOP BAR =====
    // Full width background
    auto top = P("Top", 0, 55, header);
    top->GetStyle().marginTop = Panorama::Length::Px(0);
    m_ui->root->AddChild(top);
    
    // Content container (centered, 80% width)
    auto topContent = P("TopContent", contentWidth, 55, none);
    topContent->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    top->AddChild(topContent);

    // Calculate vertical center offset for elements
    float topBarHeight = 55.0f;
    float logoHeight = S(22);
    float logoOffsetY = (topBarHeight - logoHeight) / 2.0f;
    float textOffsetY = (topBarHeight - S(11)) / 2.0f; // 11 is font size

    auto logo = P("Logo", 32, 22, Panorama::Color(0.75f, 0.12f, 0.12f, 1.0f));
    logo->GetStyle().borderRadius = S(2);
    logo->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    logo->GetStyle().marginTop = Panorama::Length::Px(logoOffsetY);
    topContent->AddChild(logo);

    const char* nav[] = {"HEROES", "STORE", "WATCH", "LEARN", "ARCADE"};
    for (int i = 0; i < 5; i++) {
        auto nb = std::make_shared<Panorama::CButton>(nav[i], std::string("Nav") + std::to_string(i));
        nb->GetStyle().width = Panorama::Length::Px(S(55));
        nb->GetStyle().height = Panorama::Length::Px(topBarHeight); // Full height
        nb->GetStyle().backgroundColor = Panorama::Color(0.15f, 0.15f, 0.18f, 0.8f);
        nb->GetStyle().borderWidth = 0.0f;
        nb->GetStyle().borderRadius = 0.0f;
        nb->GetStyle().fontSize = S(11);
        nb->GetStyle().color = light;
        nb->GetStyle().marginLeft = Panorama::Length::Px(S(48 + i * 61));
        nb->GetStyle().marginTop = Panorama::Length::Px(0); // No top margin
        
        if (i == 0) { // HEROES button
            nb->SetOnActivate([this]() { 
                m_manager->ChangeState(EGameState::Heroes); 
            });
        }
        
        topContent->AddChild(nb);
    }
    
    auto pc = L("824,156 PLAYING", 9, gray);
    pc->GetStyle().marginLeft = Panorama::Length::Px(S(contentWidth - 180));
    pc->GetStyle().marginTop = Panorama::Length::Px((topBarHeight - S(9)) / 2.0f);
    topContent->AddChild(pc);
    
    auto tm = L("7:48 PM", 9, gray);
    tm->GetStyle().marginLeft = Panorama::Length::Px(S(contentWidth - 60));
    tm->GetStyle().marginTop = Panorama::Length::Px((topBarHeight - S(9)) / 2.0f);
    topContent->AddChild(tm);


    // ===== MAIN CONTENT =====
    // Create wrapper for centering (without flow layout)
    auto mainWrapper = P("MainWrapper", 0, sh - 70, none);
    mainWrapper->GetStyle().marginTop = Panorama::Length::Px(55);
    m_ui->root->AddChild(mainWrapper);
    
    auto main = P("Main", contentWidth, contentHeight, none);
    // NO FLOW LAYOUT - pure absolute positioning
    main->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    main->GetStyle().marginTop = Panorama::Length::Px(contentOffsetY);
    mainWrapper->AddChild(main);

    // ===== LEFT COLUMN =====
    auto left = P("Left", 250, contentHeight, none);
    // NO FLOW LAYOUT - children positioned absolutely
    left->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    left->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(left);

    // Profile
    auto prof = P("Prof", 0, 130, panel);
    prof->GetStyle().borderRadius = S(3);
    prof->GetStyle().marginTop = Panorama::Length::Px(S(0));
    left->AddChild(prof);

    auto ph = L("MY PROFILE", 8, gray);
    ph->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    ph->GetStyle().marginTop = Panorama::Length::Px(S(6));
    prof->AddChild(ph);

    // Avatar
    auto av = P("Av", 40, 40, Panorama::Color(0.28f, 0.45f, 0.28f, 1.0f));
    av->GetStyle().borderRadius = S(3);
    av->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    av->GetStyle().marginTop = Panorama::Length::Px(S(24));
    prof->AddChild(av);

    // Username
    auto un = L("Choice", 12, green);
    un->GetStyle().marginLeft = Panorama::Length::Px(S(54));
    un->GetStyle().marginTop = Panorama::Length::Px(S(26));
    prof->AddChild(un);
    
    // Status
    auto us = L("Main Menu", 9, gray);
    us->GetStyle().marginLeft = Panorama::Length::Px(S(54));
    us->GetStyle().marginTop = Panorama::Length::Px(S(42));
    prof->AddChild(us);

    // Level badge
    auto lvl = P("Lvl", 26, 26, Panorama::Color(0.55f, 0.45f, 0.18f, 1.0f));
    lvl->GetStyle().borderRadius = S(13);
    lvl->GetStyle().marginLeft = Panorama::Length::Px(S(210));
    lvl->GetStyle().marginTop = Panorama::Length::Px(S(30));
    prof->AddChild(lvl);

    // Hero trophies
    Panorama::Color hc[] = {{0.55f,0.28f,0.18f,1}, {0.28f,0.35f,0.55f,1}, {0.45f,0.28f,0.45f,1}, {0.35f,0.45f,0.28f,1}};
    for (int i = 0; i < 4; i++) {
        auto ht = P("HT" + std::to_string(i), 20, 20, hc[i]);
        ht->GetStyle().borderRadius = S(2);
        ht->GetStyle().marginLeft = Panorama::Length::Px(S(8 + i * 28));
        ht->GetStyle().marginTop = Panorama::Length::Px(S(76));
        prof->AddChild(ht);
    }

    // Stats
    auto ms = L("MATCHES\n776", 8, gray);
    ms->GetStyle().marginLeft = Panorama::Length::Px(S(30));
    ms->GetStyle().marginTop = Panorama::Length::Px(S(102));
    prof->AddChild(ms);
    
    auto cs = L("COMMENDS\n167", 8, gray);
    cs->GetStyle().marginLeft = Panorama::Length::Px(S(140));
    cs->GetStyle().marginTop = Panorama::Length::Px(S(102));
    prof->AddChild(cs);

    // Battle Pass
    auto bp = P("BP", 0, 50, Panorama::Color(0.1f, 0.12f, 0.08f, 1.0f));
    bp->GetStyle().borderRadius = S(3);
    bp->GetStyle().marginTop = Panorama::Length::Px(S(136));
    left->AddChild(bp);

    auto bpt = L("BATTLE PASS", 8, gray);
    bpt->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    bpt->GetStyle().marginTop = Panorama::Length::Px(S(5));
    bp->AddChild(bpt);
    
    auto bpx = L("The Fall 2024 Battle Pass is Here", 9, green);
    bpx->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    bpx->GetStyle().marginTop = Panorama::Length::Px(S(18));
    bp->AddChild(bpx);

    auto lkb = std::make_shared<Panorama::CButton>("LOOK INSIDE", "LkB");
    lkb->GetStyle().width = Panorama::Length::Px(S(230));
    lkb->GetStyle().height = Panorama::Length::Px(S(18));
    lkb->GetStyle().backgroundColor = Panorama::Color(0.28f, 0.5f, 0.18f, 1.0f);
    lkb->GetStyle().borderRadius = S(2);
    lkb->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    lkb->GetStyle().marginTop = Panorama::Length::Px(S(30));
    lkb->GetStyle().fontSize = S(9);
    lkb->GetStyle().color = Panorama::Color::White();
    bp->AddChild(lkb);


    // Friends
    auto fr = P("Fr", 0, 200, panel);
    fr->GetStyle().borderRadius = S(3);
    fr->GetStyle().marginTop = Panorama::Length::Px(S(192));
    left->AddChild(fr);

    auto fh = L("FRIENDS", 8, gray);
    fh->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    fh->GetStyle().marginTop = Panorama::Length::Px(S(6));
    fr->AddChild(fh);
    
    auto id = L("IN DOTA (4)", 7, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    id->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    id->GetStyle().marginTop = Panorama::Length::Px(S(22));
    fr->AddChild(id);

    const char* fn[] = {"Choice", "Ivy R.", "Mythe", "standi", "Iphone"};
    const char* fhr[] = {"Serenity", "Pudge Lvl 18", "Crystal Maiden", "INDO...", "Pudge Lvl 18"};
    
    for (int i = 0; i < 5; i++) {
        // Friend row container
        auto frw = P("FW" + std::to_string(i), 230, 28, none);
        frw->GetStyle().marginLeft = Panorama::Length::Px(S(8));
        frw->GetStyle().marginTop = Panorama::Length::Px(S(40 + i * 32));
        fr->AddChild(frw);

        // Avatar
        auto fa = P("FA" + std::to_string(i), 22, 22, Panorama::Color(0.22f + i*0.03f, 0.28f, 0.32f, 1.0f));
        fa->GetStyle().borderRadius = S(2);
        fa->GetStyle().marginTop = Panorama::Length::Px(S(3));
        frw->AddChild(fa);

        // Friend name
        auto fnl = L(fn[i], 10, Panorama::Color(0.45f, 0.65f, 0.45f, 1.0f));
        fnl->GetStyle().marginLeft = Panorama::Length::Px(S(28));
        fnl->GetStyle().marginTop = Panorama::Length::Px(S(2));
        frw->AddChild(fnl);
        
        // Friend status
        auto fhl = L(fhr[i], 7, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
        fhl->GetStyle().marginLeft = Panorama::Length::Px(S(28));
        fhl->GetStyle().marginTop = Panorama::Length::Px(S(14));
        frw->AddChild(fhl);
    }

    // ===== CENTER COLUMN =====
    float cw = contentWidth - S(250) - S(270) - S(50);
    auto ctr = P("Ctr", cw / S(1), contentHeight, none);
    ctr->GetStyle().marginLeft = Panorama::Length::Px(S(270));
    ctr->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(ctr);

    float bw = cw / S(1) / 2 - S(4); // Block width for 2x2 grid

    // ===== ROW 1: NEWS BLOCKS =====
    // News 1 - Top Left
    auto n1 = P("N1", bw, 115, Panorama::Color(0.12f, 0.1f, 0.15f, 1.0f));
    n1->GetStyle().borderRadius = S(3);
    ctr->AddChild(n1);

    auto n1h = L("NEW IN DOTA 2", 7, gray);
    n1h->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1h->GetStyle().marginTop = Panorama::Length::Px(S(6));
    n1->AddChild(n1h);
    
    auto n1i = P("N1I", bw - S(20), 70, Panorama::Color(0.35f, 0.22f, 0.42f, 1.0f));
    n1i->GetStyle().borderRadius = S(3);
    n1i->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1i->GetStyle().marginTop = Panorama::Length::Px(S(20));
    n1->AddChild(n1i);
    
    auto n1t = L("FALL 2024 TREASURE II", 10, Panorama::Color::White());
    n1t->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n1t->GetStyle().marginTop = Panorama::Length::Px(S(95));
    n1->AddChild(n1t);

    // News 2 - Top Right
    auto n2 = P("N2", bw, 115, Panorama::Color(0.1f, 0.12f, 0.15f, 1.0f));
    n2->GetStyle().borderRadius = S(3);
    n2->GetStyle().marginLeft = Panorama::Length::Px(S(bw + 8));
    n2->GetStyle().marginTop = Panorama::Length::Px(S(0));
    ctr->AddChild(n2);

    auto n2h = L("PRO PLAYING LIVE", 7, gray);
    n2h->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2h->GetStyle().marginTop = Panorama::Length::Px(S(6));
    n2->AddChild(n2h);
    
    auto n2i = P("N2I", bw - S(20), 70, Panorama::Color(0.18f, 0.25f, 0.32f, 1.0f));
    n2i->GetStyle().borderRadius = S(3);
    n2i->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2i->GetStyle().marginTop = Panorama::Length::Px(S(20));
    n2->AddChild(n2i);
    
    auto n2t = L("FlipSid3.RodjER", 10, Panorama::Color::White());
    n2t->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    n2t->GetStyle().marginTop = Panorama::Length::Px(S(95));
    n2->AddChild(n2t);

    // ===== ROW 2: FEATURED BLOCKS =====
    // Featured 1 - Bottom Left (Battle Pass)
    auto ft1 = P("FT1", bw, 80, Panorama::Color(0.15f, 0.12f, 0.08f, 1.0f));
    ft1->GetStyle().borderRadius = S(3);
    ft1->GetStyle().marginTop = Panorama::Length::Px(S(121));
    ctr->AddChild(ft1);

    auto bpi = P("BPI", 45, 50, Panorama::Color(0.5f, 0.4f, 0.18f, 1.0f));
    bpi->GetStyle().borderRadius = S(3);
    bpi->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    bpi->GetStyle().marginTop = Panorama::Length::Px(S(15));
    ft1->AddChild(bpi);
    
    auto bpl1 = L("FALL 2024", 8, Panorama::Color(0.75f, 0.65f, 0.35f, 1.0f));
    bpl1->GetStyle().marginLeft = Panorama::Length::Px(S(59));
    bpl1->GetStyle().marginTop = Panorama::Length::Px(S(20));
    ft1->AddChild(bpl1);
    
    auto bpl2 = L("BATTLE PASS", 8, Panorama::Color(0.75f, 0.65f, 0.35f, 1.0f));
    bpl2->GetStyle().marginLeft = Panorama::Length::Px(S(59));
    bpl2->GetStyle().marginTop = Panorama::Length::Px(S(32));
    ft1->AddChild(bpl2);

    // Featured 2 - Bottom Right (Game of the Day)
    auto ft2 = P("FT2", bw, 80, Panorama::Color(0.08f, 0.1f, 0.12f, 1.0f));
    ft2->GetStyle().borderRadius = S(3);
    ft2->GetStyle().marginLeft = Panorama::Length::Px(S(bw + 8));
    ft2->GetStyle().marginTop = Panorama::Length::Px(S(121));
    ctr->AddChild(ft2);

    auto gd = L("GAME OF THE DAY", 7, gray);
    gd->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    gd->GetStyle().marginTop = Panorama::Length::Px(S(6));
    ft2->AddChild(gd);
    
    auto hm = P("HM", bw - S(20), 40, Panorama::Color(0.12f, 0.15f, 0.18f, 1.0f));
    hm->GetStyle().borderRadius = S(3);
    hm->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    hm->GetStyle().marginTop = Panorama::Length::Px(S(26));
    ft2->AddChild(hm);
    
    auto hml = L("HORDE MODE", 11, Panorama::Color::White());
    hml->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    hml->GetStyle().marginTop = Panorama::Length::Px(S(12));
    hm->AddChild(hml);

    // ===== CHAT (Below 2x2 grid) =====
    auto ch = P("Ch", 0, 110, Panorama::Color(0.08f, 0.08f, 0.1f, 1.0f));
    ch->GetStyle().borderRadius = S(3);
    ch->GetStyle().marginTop = Panorama::Length::Px(S(207));
    ctr->AddChild(ch);

    // Chat header
    auto chh = P("CHH", 0, 22, Panorama::Color(0.06f, 0.06f, 0.08f, 1.0f));
    ch->AddChild(chh);

    auto pty = L("Party", 10, Panorama::Color(0.55f, 0.55f, 0.55f, 1.0f));
    pty->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    pty->GetStyle().marginTop = Panorama::Length::Px(S(5));
    chh->AddChild(pty);
    
    auto chl = L("CHANNELS +", 8, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    chl->GetStyle().marginLeft = Panorama::Length::Px(S(150));
    chl->GetStyle().marginTop = Panorama::Length::Px(S(7));
    chh->AddChild(chl);

    // Chat messages
    auto m1 = L("Serenity: noob riki", 10, Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    m1->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    m1->GetStyle().marginTop = Panorama::Length::Px(S(28));
    ch->AddChild(m1);
    
    auto m2 = L("Choice: poor pudge", 10, Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    m2->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    m2->GetStyle().marginTop = Panorama::Length::Px(S(44));
    ch->AddChild(m2);

    // Chat input
    auto chi = P("CHI", 240, 18, Panorama::Color(0.12f, 0.12f, 0.14f, 1.0f));
    chi->GetStyle().borderRadius = S(2);
    chi->GetStyle().marginLeft = Panorama::Length::Px(S(10));
    chi->GetStyle().marginTop = Panorama::Length::Px(S(78));
    ch->AddChild(chi);


    // ===== RIGHT COLUMN =====
    auto rgt = P("Rgt", 270, contentHeight, none);
    // NO FLOW LAYOUT - children positioned absolutely
    rgt->GetStyle().marginLeft = Panorama::Length::Px(contentWidth - S(270) - S(10));
    rgt->GetStyle().marginTop = Panorama::Length::Px(S(10));
    main->AddChild(rgt);

    // Last Match
    auto lm = P("LM", 0, 120, panel);
    lm->GetStyle().borderRadius = S(3);
    lm->GetStyle().marginTop = Panorama::Length::Px(S(0));
    rgt->AddChild(lm);

    auto lmh = L("YOUR LAST MATCH", 7, gray);
    lmh->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    lmh->GetStyle().marginTop = Panorama::Length::Px(S(6));
    lm->AddChild(lmh);
    
    auto lmt = L("10/31/2024  7:03 PM", 7, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    lmt->GetStyle().marginLeft = Panorama::Length::Px(S(135));
    lmt->GetStyle().marginTop = Panorama::Length::Px(S(6));
    lm->AddChild(lmt);

    auto hn = L("JUGGERNAUT", 14, Panorama::Color::White());
    hn->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    hn->GetStyle().marginTop = Panorama::Length::Px(S(24));
    lm->AddChild(hn);

    auto rs = L("WON - ALL PICK", 10, Panorama::Color(0.28f, 0.7f, 0.28f, 1.0f));
    rs->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    rs->GetStyle().marginTop = Panorama::Length::Px(S(44));
    lm->AddChild(rs);

    auto kdl = L("K/D/A", 7, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    kdl->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    kdl->GetStyle().marginTop = Panorama::Length::Px(S(62));
    lm->AddChild(kdl);
    
    auto kdv = L("9 / 2 / 4", 9, Panorama::Color::White());
    kdv->GetStyle().marginLeft = Panorama::Length::Px(S(50));
    kdv->GetStyle().marginTop = Panorama::Length::Px(S(61));
    lm->AddChild(kdv);
    
    auto drl = L("DURATION", 7, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    drl->GetStyle().marginLeft = Panorama::Length::Px(S(135));
    drl->GetStyle().marginTop = Panorama::Length::Px(S(62));
    lm->AddChild(drl);
    
    auto drv = L("37:14", 9, Panorama::Color::White());
    drv->GetStyle().marginLeft = Panorama::Length::Px(S(200));
    drv->GetStyle().marginTop = Panorama::Length::Px(S(61));
    lm->AddChild(drv);

    auto itl = L("ITEMS", 7, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    itl->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    itl->GetStyle().marginTop = Panorama::Length::Px(S(80));
    lm->AddChild(itl);

    Panorama::Color ic[] = {{0.45f,0.32f,0.18f,1}, {0.28f,0.45f,0.28f,1}, {0.55f,0.28f,0.28f,1}, {0.35f,0.35f,0.45f,1}, {0.45f,0.35f,0.22f,1}, {0.32f,0.32f,0.35f,1}};
    for (int i = 0; i < 6; i++) {
        auto it = P("IT" + std::to_string(i), 24, 18, ic[i]);
        it->GetStyle().borderRadius = S(1);
        it->GetStyle().marginLeft = Panorama::Length::Px(S(8 + i * 32));
        it->GetStyle().marginTop = Panorama::Length::Px(S(95));
        lm->AddChild(it);
    }

    // Activity
    auto act = P("Act", 0, 220, panel);
    act->GetStyle().borderRadius = S(3);
    act->GetStyle().marginTop = Panorama::Length::Px(S(126));
    rgt->AddChild(act);

    auto acth = L("Say something on your feed...", 9, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
    acth->GetStyle().marginLeft = Panorama::Length::Px(S(8));
    acth->GetStyle().marginTop = Panorama::Length::Px(S(6));
    act->AddChild(acth);

    const char* at[] = {"HHr got a RAMPAGE as Sven", "Dota2wage advanced to Semi", "Iphone posted: random games", "Choice is now playing"};
    const char* atm[] = {"Yesterday", "Saturday", "Friday", "Just now"};
    
    for (int i = 0; i < 4; i++) {
        // Activity entry container
        auto ae = P("AE" + std::to_string(i), 250, 40, Panorama::Color(0.08f, 0.08f, 0.1f, 0.5f));
        ae->GetStyle().borderRadius = S(2);
        ae->GetStyle().marginLeft = Panorama::Length::Px(S(8));
        ae->GetStyle().marginTop = Panorama::Length::Px(S(26 + i * 45));
        act->AddChild(ae);

        // Avatar
        auto aa = P("AA" + std::to_string(i), 26, 26, Panorama::Color(0.28f, 0.22f + i*0.03f, 0.18f, 1.0f));
        aa->GetStyle().borderRadius = S(2);
        aa->GetStyle().marginLeft = Panorama::Length::Px(S(5));
        aa->GetStyle().marginTop = Panorama::Length::Px(S(7));
        ae->AddChild(aa);

        // Activity text
        auto atx = L(at[i], 8, Panorama::Color(0.55f, 0.55f, 0.55f, 1.0f));
        atx->GetStyle().marginLeft = Panorama::Length::Px(S(36));
        atx->GetStyle().marginTop = Panorama::Length::Px(S(8));
        ae->AddChild(atx);
        
        // Time
        auto atmx = L(atm[i], 7, Panorama::Color(0.35f, 0.35f, 0.35f, 1.0f));
        atmx->GetStyle().marginLeft = Panorama::Length::Px(S(36));
        atmx->GetStyle().marginTop = Panorama::Length::Px(S(22));
        ae->AddChild(atmx);
    }


    // ===== BOTTOM BAR =====
    // Full width background
    auto bottomBarBg = P("BotBg", 0, 70, Panorama::Color(0.03f, 0.04f, 0.06f, 0.95f));
    bottomBarBg->GetStyle().marginTop = Panorama::Length::Px(sh - 70);
    m_ui->root->AddChild(bottomBarBg);
    
    // Content container (centered, 80% width)
    m_ui->bottomBar = P("Bot", contentWidth, 70, none);
    m_ui->bottomBar->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    bottomBarBg->AddChild(m_ui->bottomBar);

    // Game mode icon
    auto gi = P("GI", 40, 40, Panorama::Color(0.12f, 0.12f, 0.15f, 1.0f));
    gi->GetStyle().borderRadius = S(3);
    gi->GetStyle().marginLeft = Panorama::Length::Px(15);
    gi->GetStyle().marginTop = Panorama::Length::Px(15);
    m_ui->bottomBar->AddChild(gi);

    // Party members
    Panorama::Color pc2[] = {{0.45f,0.28f,0.22f,1}, {0.28f,0.32f,0.45f,1}, {0.35f,0.45f,0.32f,1}};
    for (int i = 0; i < 3; i++) {
        auto pt = P("PT" + std::to_string(i), 35, 40, pc2[i]);
        pt->GetStyle().borderRadius = S(2);
        pt->GetStyle().marginLeft = Panorama::Length::Px(65 + i * 40);
        pt->GetStyle().marginTop = Panorama::Length::Px(15);
        m_ui->bottomBar->AddChild(pt);
    }
    
    // Add party button
    auto addParty = P("AddParty", 35, 40, Panorama::Color(0.15f, 0.15f, 0.18f, 1.0f));
    addParty->GetStyle().borderRadius = S(2);
    addParty->GetStyle().marginLeft = Panorama::Length::Px(185);
    addParty->GetStyle().marginTop = Panorama::Length::Px(15);
    m_ui->bottomBar->AddChild(addParty);
    
    auto plusLabel = L("+", 20, Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f));
    plusLabel->GetStyle().marginLeft = Panorama::Length::Px(11);
    plusLabel->GetStyle().marginTop = Panorama::Length::Px(5);
    addParty->AddChild(plusLabel);

    // Game mode label
    auto modeLabel = L("ALL PICK", 12, Panorama::Color(0.6f, 0.6f, 0.6f, 1.0f));
    modeLabel->GetStyle().marginLeft = Panorama::Length::Px(250);
    modeLabel->GetStyle().marginTop = Panorama::Length::Px(28);
    m_ui->bottomBar->AddChild(modeLabel);

    // Play button
    m_ui->playButton = std::make_shared<Panorama::CButton>("PLAY DOTA", "PB");
    m_ui->playButton->GetStyle().width = Panorama::Length::Px(180);
    m_ui->playButton->GetStyle().height = Panorama::Length::Px(45);
    m_ui->playButton->GetStyle().backgroundColor = greenBtn;
    m_ui->playButton->GetStyle().borderRadius = S(3);
    m_ui->playButton->GetStyle().fontSize = S(14);
    m_ui->playButton->GetStyle().color = Panorama::Color::White();
    m_ui->playButton->GetStyle().marginLeft = Panorama::Length::Px(contentWidth - 200);
    m_ui->playButton->GetStyle().marginTop = Panorama::Length::Px(12);
    m_ui->playButton->SetOnActivate([this]() { OnPlayClicked(); });
    m_ui->bottomBar->AddChild(m_ui->playButton);

    // ===== Finding match button (swap while searching) =====
    // Same placement as play button, but with [X] cancel and a timer above.
    const float playX = contentWidth - 200;
    const float playY = 12.0f;

    m_ui->findingPanel = P("MM_FindingPanel", 180, 45, Panorama::Color(0.12f, 0.16f, 0.22f, 1.0f));
    m_ui->findingPanel->GetStyle().borderRadius = S(3);
    m_ui->findingPanel->GetStyle().marginLeft = Panorama::Length::Px(playX);
    m_ui->findingPanel->GetStyle().marginTop = Panorama::Length::Px(playY);
    m_ui->findingPanel->SetVisible(false);
    m_ui->bottomBar->AddChild(m_ui->findingPanel);

    m_ui->findingLabel = L("FINDING MATCH", 12, Panorama::Color(0.85f, 0.9f, 0.95f, 1.0f));
    m_ui->findingLabel->GetStyle().marginLeft = Panorama::Length::Px(S(14));
    m_ui->findingLabel->GetStyle().marginTop = Panorama::Length::Px(S(14));
    m_ui->findingPanel->AddChild(m_ui->findingLabel);

    m_ui->findingCancelButton = std::make_shared<Panorama::CButton>("X", "MM_FindCancel");
    m_ui->findingCancelButton->GetStyle().width = Panorama::Length::Px(S(30));
    m_ui->findingCancelButton->GetStyle().height = Panorama::Length::Px(S(30));
    m_ui->findingCancelButton->GetStyle().backgroundColor = Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
    m_ui->findingCancelButton->GetStyle().borderRadius = S(3);
    m_ui->findingCancelButton->GetStyle().fontSize = S(14);
    m_ui->findingCancelButton->GetStyle().color = Panorama::Color::White();
    // Position inside 180px button (right aligned)
    m_ui->findingCancelButton->GetStyle().marginLeft = Panorama::Length::Px(S(180 - 36));
    m_ui->findingCancelButton->GetStyle().marginTop = Panorama::Length::Px(S(7));
    m_ui->findingCancelButton->SetOnActivate([this]() {
        if (m_mmClient) m_mmClient->cancelQueue();
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
        if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
        ConsoleLog("Matchmaking cancelled");
    });
    m_ui->findingPanel->AddChild(m_ui->findingCancelButton);

    m_ui->findingTimeLabel = L("00:00", 11, Panorama::Color(0.65f, 0.75f, 0.85f, 1.0f));
    m_ui->findingTimeLabel->GetStyle().marginLeft = Panorama::Length::Px(playX + S(2));
    m_ui->findingTimeLabel->GetStyle().marginTop = Panorama::Length::Px(S(2));
    m_ui->findingTimeLabel->SetVisible(false);
    m_ui->bottomBar->AddChild(m_ui->findingTimeLabel);

    // ===== Matchmaking overlays =====
    // Searching overlay
    m_ui->searchingOverlay = P("MM_SearchingOverlay", 0, 0, Panorama::Color(0.0f, 0.0f, 0.0f, 0.55f));
    m_ui->searchingOverlay->SetVisible(false);
    m_ui->root->AddChild(m_ui->searchingOverlay);

    auto searchingBox = P("MM_SearchingBox", 420, 170, Panorama::Color(0.08f, 0.09f, 0.11f, 0.96f));
    searchingBox->GetStyle().borderRadius = S(4);
    searchingBox->GetStyle().marginLeft = Panorama::Length::Px((sw - S(420)) * 0.5f);
    searchingBox->GetStyle().marginTop = Panorama::Length::Px((sh - S(170)) * 0.5f);
    m_ui->searchingOverlay->AddChild(searchingBox);

    m_ui->searchingLabel = L("SEARCHING FOR MATCH...", 14, Panorama::Color(0.85f, 0.85f, 0.85f, 1.0f));
    m_ui->searchingLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->searchingLabel->GetStyle().marginTop = Panorama::Length::Px(S(22));
    searchingBox->AddChild(m_ui->searchingLabel);

    m_ui->searchTimeLabel = L("00:00", 12, Panorama::Color(0.65f, 0.65f, 0.65f, 1.0f));
    m_ui->searchTimeLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->searchTimeLabel->GetStyle().marginTop = Panorama::Length::Px(S(55));
    searchingBox->AddChild(m_ui->searchTimeLabel);

    m_ui->cancelSearchButton = std::make_shared<Panorama::CButton>("CANCEL", "MM_Cancel");
    m_ui->cancelSearchButton->GetStyle().width = Panorama::Length::Px(S(140));
    m_ui->cancelSearchButton->GetStyle().height = Panorama::Length::Px(S(40));
    m_ui->cancelSearchButton->GetStyle().backgroundColor = Panorama::Color(0.25f, 0.25f, 0.3f, 0.95f);
    m_ui->cancelSearchButton->GetStyle().borderRadius = S(3);
    m_ui->cancelSearchButton->GetStyle().fontSize = S(13);
    m_ui->cancelSearchButton->GetStyle().color = Panorama::Color::White();
    m_ui->cancelSearchButton->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->cancelSearchButton->GetStyle().marginTop = Panorama::Length::Px(S(95));
    m_ui->cancelSearchButton->SetOnActivate([this]() {
        if (m_mmClient) m_mmClient->cancelQueue();
        if (m_ui->searchingOverlay) m_ui->searchingOverlay->SetVisible(false);
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
        if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
        if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
        ConsoleLog("Matchmaking cancelled");
    });
    searchingBox->AddChild(m_ui->cancelSearchButton);

    // Accept overlay
    m_ui->acceptOverlay = P("MM_AcceptOverlay", 0, 0, Panorama::Color(0.0f, 0.0f, 0.0f, 0.65f));
    m_ui->acceptOverlay->SetVisible(false);
    m_ui->root->AddChild(m_ui->acceptOverlay);

    auto acceptBox = P("MM_AcceptBox", 460, 210, Panorama::Color(0.08f, 0.09f, 0.11f, 0.98f));
    acceptBox->GetStyle().borderRadius = S(4);
    acceptBox->GetStyle().marginLeft = Panorama::Length::Px((sw - S(460)) * 0.5f);
    acceptBox->GetStyle().marginTop = Panorama::Length::Px((sh - S(210)) * 0.5f);
    m_ui->acceptOverlay->AddChild(acceptBox);

    m_ui->acceptLabel = L("MATCH FOUND", 16, Panorama::Color(0.92f, 0.92f, 0.92f, 1.0f));
    m_ui->acceptLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->acceptLabel->GetStyle().marginTop = Panorama::Length::Px(S(25));
    acceptBox->AddChild(m_ui->acceptLabel);

    // Countdown (Dota-like accept timer)
    m_ui->acceptCountdownLabel = L("00:20", 12, Panorama::Color(0.65f, 0.75f, 0.85f, 1.0f));
    m_ui->acceptCountdownLabel->GetStyle().marginLeft = Panorama::Length::Px(S(360));
    m_ui->acceptCountdownLabel->GetStyle().marginTop = Panorama::Length::Px(S(28));
    acceptBox->AddChild(m_ui->acceptCountdownLabel);

    // Status row (Dota-like cubes). Hidden until local player accepts.
    m_ui->acceptStatusLabel = L("0/0 ACCEPTED", 12, Panorama::Color(0.75f, 0.75f, 0.75f, 1.0f));
    m_ui->acceptStatusLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->acceptStatusLabel->GetStyle().marginTop = Panorama::Length::Px(S(60));
    m_ui->acceptStatusLabel->SetVisible(false);
    acceptBox->AddChild(m_ui->acceptStatusLabel);

    m_ui->acceptStatusPanel = P("MM_AcceptStatusPanel", 420, 28, Panorama::Color(0, 0, 0, 0));
    m_ui->acceptStatusPanel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->acceptStatusPanel->GetStyle().marginTop = Panorama::Length::Px(S(85));
    m_ui->acceptStatusPanel->SetVisible(false);
    acceptBox->AddChild(m_ui->acceptStatusPanel);

    m_ui->acceptButton = std::make_shared<Panorama::CButton>("ACCEPT", "MM_Accept");
    m_ui->acceptButton->GetStyle().width = Panorama::Length::Px(S(160));
    m_ui->acceptButton->GetStyle().height = Panorama::Length::Px(S(46));
    m_ui->acceptButton->GetStyle().backgroundColor = Panorama::Color(0.18f, 0.45f, 0.18f, 1.0f);
    m_ui->acceptButton->GetStyle().borderRadius = S(3);
    m_ui->acceptButton->GetStyle().fontSize = S(14);
    m_ui->acceptButton->GetStyle().color = Panorama::Color::White();
    m_ui->acceptButton->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->acceptButton->GetStyle().marginTop = Panorama::Length::Px(S(115));
    m_ui->acceptButton->SetOnActivate([this]() {
        if (m_mmClient) m_mmClient->acceptMatch();
        ConsoleLog("Match accepted");
        // Swap UI: buttons -> status row
        if (m_ui && m_ui->acceptButton) m_ui->acceptButton->SetVisible(false);
        if (m_ui && m_ui->declineButton) m_ui->declineButton->SetVisible(false);
        if (m_ui && m_ui->acceptStatusLabel) m_ui->acceptStatusLabel->SetVisible(true);
        if (m_ui && m_ui->acceptStatusPanel) m_ui->acceptStatusPanel->SetVisible(true);

        // Optimistic local UI: immediately show 1/N accepted for this client.
        // Coordinator will send MatchAcceptStatus shortly after; that will correct any mismatch.
        if (m_ui && m_ui->acceptStatusLabel) {
            const int total = (int)m_ui->acceptCubes.size();
            if (total > 0) {
                m_ui->acceptStatusLabel->SetText("1/" + std::to_string(total) + " ACCEPTED");
            }
        }
        if (m_ui) {
            // Color a cube green for local acceptance if we can identify our slot.
            int selfIndex = -1;
            if (m_mmClient) {
                const u64 selfId = m_mmClient->getPlayerInfo().steamId;
                const auto& ids = m_mmClient->getAcceptPlayerIds();
                for (size_t i = 0; i < ids.size(); ++i) {
                    if (ids[i] == selfId) { selfIndex = (int)i; break; }
                }
            }
            if (selfIndex < 0) selfIndex = 0; // fallback if status packet not received yet
            for (int i = 0; i < (int)m_ui->acceptCubes.size(); ++i) {
                if (!m_ui->acceptCubes[i]) continue;
                m_ui->acceptCubes[i]->GetStyle().backgroundColor = (i == selfIndex)
                    ? Panorama::Color(0.18f, 0.55f, 0.18f, 1.0f)
                    : Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
            }
        }
    });
    acceptBox->AddChild(m_ui->acceptButton);

    m_ui->declineButton = std::make_shared<Panorama::CButton>("DECLINE", "MM_Decline");
    m_ui->declineButton->GetStyle().width = Panorama::Length::Px(S(160));
    m_ui->declineButton->GetStyle().height = Panorama::Length::Px(S(46));
    m_ui->declineButton->GetStyle().backgroundColor = Panorama::Color(0.45f, 0.18f, 0.18f, 1.0f);
    m_ui->declineButton->GetStyle().borderRadius = S(3);
    m_ui->declineButton->GetStyle().fontSize = S(14);
    m_ui->declineButton->GetStyle().color = Panorama::Color::White();
    m_ui->declineButton->GetStyle().marginLeft = Panorama::Length::Px(S(210));
    m_ui->declineButton->GetStyle().marginTop = Panorama::Length::Px(S(115));
    m_ui->declineButton->SetOnActivate([this]() {
        if (m_mmClient) m_mmClient->declineMatch();
        if (m_ui->acceptOverlay) m_ui->acceptOverlay->SetVisible(false);
        if (m_ui->searchingOverlay) m_ui->searchingOverlay->SetVisible(false);
        ConsoleLog("Match declined");
    });
    acceptBox->AddChild(m_ui->declineButton);
}

void MainMenuState::DestroyUI() {
    if (m_ui->root) {
        auto& engine = Panorama::CUIEngine::Instance();
        if (auto* uiRoot = engine.GetRoot()) {
            uiRoot->RemoveChild(m_ui->root.get());
        }
    }
    m_ui->root.reset();
}

void MainMenuState::Update(f32 dt) {
    if (m_mmClient) {
        m_mmClient->update(dt);

        // Update finding match timer (shown above play button).
        if (m_ui && m_ui->findingPanel && m_ui->findingPanel->IsVisible() && m_ui->findingTimeLabel) {
            const auto& qs = m_mmClient->getQueueStatus();
            const int t = (int)qs.searchTime;
            const int mm = t / 60;
            const int ss = t % 60;
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
            m_ui->findingTimeLabel->SetText(buf);
        }

        // Update accept countdown if accept overlay visible.
        if (m_ui && m_ui->acceptOverlay && m_ui->acceptOverlay->IsVisible() && m_ui->acceptCountdownLabel) {
            const float remaining = m_mmClient->getAcceptTimeRemainingSeconds();
            const int sec = std::max(0, (int)std::ceil(remaining));
            const int mm = sec / 60;
            const int ss = sec % 60;
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
            m_ui->acceptCountdownLabel->SetText(buf);

            // Disable buttons at 0 to match Dota feel; coordinator will cancel shortly after.
            const bool expired = (sec <= 0);
            if (m_ui->acceptButton) m_ui->acceptButton->SetEnabled(!expired);
            if (m_ui->declineButton) m_ui->declineButton->SetEnabled(!expired);
        }
    }

    Panorama::CUIEngine::Instance().Update(dt);
}
void MainMenuState::Render() { Panorama::CUIEngine::Instance().Render(); }

bool MainMenuState::OnKeyDown(i32 key) {
    if (key == 27) { OnExitClicked(); return true; }
    // Removed Enter key handler - use Play button instead
    return false;
}

bool MainMenuState::OnMouseMove(f32 x, f32 y) { Panorama::CUIEngine::Instance().OnMouseMove(x, y); return true; }
bool MainMenuState::OnMouseDown(f32 x, f32 y, i32 b) { Panorama::CUIEngine::Instance().OnMouseDown(x, y, b); return true; }
bool MainMenuState::OnMouseUp(f32 x, f32 y, i32 b) { Panorama::CUIEngine::Instance().OnMouseUp(x, y, b); return true; }

void MainMenuState::OnPlayClicked() {
    LOG_INFO("=== PLAY BUTTON CLICKED ===");
    ConsoleLog("=== PLAY BUTTON CLICKED ===");
    ConsoleLog("Searching for match...");

    if (!m_mmClient) {
        m_mmClient = std::make_unique<WorldEditor::Matchmaking::MatchmakingClient>();

        // Wire callbacks once.
        m_mmClient->setOnQueueConfirmed([this]() {
            ConsoleLog("Queue confirmed");
        });
        m_mmClient->setOnMatchFound([this](const WorldEditor::Matchmaking::LobbyInfo& lobby) {
            ConsoleLog("Match found!");
            // Hide finding UI and show accept overlay
            if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
            if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
            if (m_ui && m_ui->acceptOverlay) m_ui->acceptOverlay->SetVisible(true);

            // Reset accept UI to initial state (buttons visible, status hidden)
            if (m_ui && m_ui->acceptButton) m_ui->acceptButton->SetVisible(true);
            if (m_ui && m_ui->declineButton) m_ui->declineButton->SetVisible(true);
            if (m_ui && m_ui->acceptButton) m_ui->acceptButton->SetEnabled(true);
            if (m_ui && m_ui->declineButton) m_ui->declineButton->SetEnabled(true);
            if (m_ui && m_ui->acceptStatusLabel) m_ui->acceptStatusLabel->SetVisible(false);
            if (m_ui && m_ui->acceptStatusPanel) m_ui->acceptStatusPanel->SetVisible(false);

            // Prime countdown text immediately.
            if (m_ui && m_ui->acceptCountdownLabel && m_mmClient) {
                const int sec = std::max(0, (int)std::ceil(m_mmClient->getAcceptTimeRemainingSeconds()));
                char buf[32]{};
                std::snprintf(buf, sizeof(buf), "%02d:%02d", sec / 60, sec % 60);
                m_ui->acceptCountdownLabel->SetText(buf);
            }

            // Build cubes (dev: lobby.players.size() == requiredPlayers).
            if (m_ui && m_ui->acceptStatusPanel) {
                m_ui->acceptStatusPanel->RemoveAndDeleteChildren();
                m_ui->acceptCubes.clear();
                const int count = (int)lobby.players.size();
                const float cube = S(18);
                const float gap = S(8);
                for (int i = 0; i < count; ++i) {
                    auto c = P("MM_Cube_" + std::to_string(i), 18, 18, Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f));
                    c->GetStyle().borderRadius = S(2);
                    c->GetStyle().marginLeft = Panorama::Length::Px(i * (cube + gap));
                    c->GetStyle().marginTop = Panorama::Length::Px(0);
                    m_ui->acceptStatusPanel->AddChild(c);
                    m_ui->acceptCubes.push_back(c);
                }
            }
        });

        m_mmClient->setOnMatchAcceptStatus([this](u16 requiredPlayers, const std::vector<u64>& playerIds, const std::vector<bool>& accepted) {
            if (!m_ui) return;
            const int count = (int)accepted.size();

            int acceptedCount = 0;
            for (bool a : accepted) if (a) acceptedCount++;

            if (m_ui->acceptStatusLabel) {
                m_ui->acceptStatusLabel->SetText(std::to_string(acceptedCount) + "/" + std::to_string(requiredPlayers) + " ACCEPTED");
            }

            // Update cube colors: green = accepted, red = not accepted
            for (int i = 0; i < count && i < (int)m_ui->acceptCubes.size(); ++i) {
                if (!m_ui->acceptCubes[i]) continue;
                m_ui->acceptCubes[i]->GetStyle().backgroundColor = accepted[i]
                    ? Panorama::Color(0.18f, 0.55f, 0.18f, 1.0f)
                    : Panorama::Color(0.55f, 0.16f, 0.16f, 1.0f);
            }

            // If local player accepted, show status row (Dota-like).
            if (m_mmClient) {
                const u64 selfId = m_mmClient->getPlayerInfo().steamId;
                bool selfAccepted = false;
                for (size_t i = 0; i < playerIds.size() && i < accepted.size(); ++i) {
                    if (playerIds[i] == selfId) {
                        selfAccepted = accepted[i];
                        break;
                    }
                }
                if (selfAccepted) {
                    if (m_ui->acceptButton) m_ui->acceptButton->SetVisible(false);
                    if (m_ui->declineButton) m_ui->declineButton->SetVisible(false);
                    if (m_ui->acceptStatusLabel) m_ui->acceptStatusLabel->SetVisible(true);
                    if (m_ui->acceptStatusPanel) m_ui->acceptStatusPanel->SetVisible(true);
                }
            }
        });
        m_mmClient->setOnMatchReady([this](const std::string& serverIP, u16 port) {
            ConsoleLog("Match ready! Connecting...");
            if (m_ui && m_ui->acceptOverlay) m_ui->acceptOverlay->SetVisible(false);
            if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
            if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);

            // Pass server endpoint to LoadingState (no manual selection).
            if (m_manager) {
                if (auto* loading = m_manager->GetLoadingState()) {
                    loading->SetServerTarget(serverIP, port);
                }
            }

            LOG_INFO("Transitioning to Loading state (server {}:{})", serverIP, port);
            m_manager->ChangeState(EGameState::Loading);
        });
        m_mmClient->setOnMatchCancelled([this](const std::string& reason, bool shouldRequeue) {
            ConsoleLog(std::string("Match cancelled: ") + reason + (shouldRequeue ? " (returning to queue)" : ""));
            if (m_ui && m_ui->acceptOverlay) m_ui->acceptOverlay->SetVisible(false);
            
            if (shouldRequeue) {
                // Player accepted but someone else didn't - return to search UI
                if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(true);
                if (m_ui && m_ui->findingTimeLabel) {
                    m_ui->findingTimeLabel->SetText("00:00");
                    m_ui->findingTimeLabel->SetVisible(true);
                }
                if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(false);
            } else {
                // This player didn't accept - show normal Play button (no timer)
                if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
                if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
                if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
            }
        });
        m_mmClient->setOnError([this](const std::string& error) {
            ConsoleLog(std::string("MM error: ") + error);
            if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(false);
            if (m_ui && m_ui->findingTimeLabel) m_ui->findingTimeLabel->SetVisible(false);
            if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(true);
        });
    }

    if (!m_mmClient->isConnected()) {
        // Local dev defaults: coordinator on localhost:27016
        if (!m_mmClient->connect("127.0.0.1", 27016)) {
            ConsoleLog("Failed to connect to matchmaking coordinator");
            return;
        }
    }

    WorldEditor::Matchmaking::MatchPreferences prefs;
    prefs.mode = WorldEditor::Matchmaking::MatchMode::AllPick;
    prefs.region = "auto";

    if (m_mmClient->queueForMatch(prefs)) {
        // Swap play button -> finding match UI
        if (m_ui && m_ui->playButton) m_ui->playButton->SetVisible(false);
        if (m_ui && m_ui->findingPanel) m_ui->findingPanel->SetVisible(true);
        if (m_ui && m_ui->findingTimeLabel) {
            m_ui->findingTimeLabel->SetText("00:00");
            m_ui->findingTimeLabel->SetVisible(true);
        }
    } else {
        ConsoleLog("Failed to queue for match");
    }
}

void MainMenuState::OnSettingsClicked() {
    Panorama::CGameEventData d;
    Panorama::GameEvents_Fire("UI_OpenSettings", d);
}

void MainMenuState::OnExitClicked() {
    Panorama::CGameEventData d;
    Panorama::GameEvents_Fire("Game_RequestExit", d);
}

} // namespace Game
