#include "../GameState.h"
#include "../ui/panorama/CUIEngine.h"
#include "../ui/panorama/CPanel2D.h"
#include "../ui/panorama/CStyleSheet.h"
#include "../ui/panorama/GameEvents.h"

namespace Game {

struct HeroesState::HeroesUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    std::shared_ptr<Panorama::CButton> backButton;
};

HeroesState::HeroesState() : m_ui(std::make_unique<HeroesUI>()) {}
HeroesState::~HeroesState() = default;

void HeroesState::OnEnter() { CreateUI(); }
void HeroesState::OnExit() { DestroyUI(); }

// Scaled helpers
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

static std::shared_ptr<Panorama::CLabel> L(const std::string& text, float size, Panorama::Color col) {
    auto l = std::make_shared<Panorama::CLabel>(text, text);
    l->GetStyle().fontSize = S(size);
    l->GetStyle().color = col;
    return l;
}

void HeroesState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    float sw = engine.GetScreenWidth();
    float sh = engine.GetScreenHeight();
    
    // Colors
    Panorama::Color bg(0.02f, 0.04f, 0.08f, 1.0f);
    Panorama::Color header(0.01f, 0.02f, 0.04f, 0.95f);
    Panorama::Color panel(0.08f, 0.09f, 0.11f, 0.92f);
    Panorama::Color light(0.85f, 0.85f, 0.85f, 1.0f);
    Panorama::Color gray(0.55f, 0.55f, 0.55f, 1.0f);
    Panorama::Color accent(0.35f, 0.65f, 0.85f, 1.0f);
    Panorama::Color none(0,0,0,0);

    float contentWidth = sw * 0.8f;
    float contentOffsetX = (sw - contentWidth) / 2.0f;

    // ROOT
    m_ui->root = P("HeroesRoot", 0, 0, bg);
    uiRoot->AddChild(m_ui->root);

    // ===== TOP BAR =====
    auto top = P("Top", 0, 55, header);
    m_ui->root->AddChild(top);
    
    auto topContent = P("TopContent", contentWidth, 55, none);
    topContent->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    top->AddChild(topContent);

    float topBarHeight = 55.0f;
    float textOffsetY = (topBarHeight - S(14)) / 2.0f;

    auto title = L("HEROES", 18, light);
    title->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    title->GetStyle().marginTop = Panorama::Length::Px(textOffsetY);
    topContent->AddChild(title);

    // Back button
    m_ui->backButton = std::make_shared<Panorama::CButton>("← BACK", "BackBtn");
    m_ui->backButton->GetStyle().width = Panorama::Length::Px(S(100));
    m_ui->backButton->GetStyle().height = Panorama::Length::Px(S(35));
    m_ui->backButton->GetStyle().backgroundColor = Panorama::Color(0.15f, 0.15f, 0.18f, 1.0f);
    m_ui->backButton->GetStyle().borderRadius = S(3);
    m_ui->backButton->GetStyle().fontSize = S(12);
    m_ui->backButton->GetStyle().color = Panorama::Color::White();
    m_ui->backButton->GetStyle().marginLeft = Panorama::Length::Px(S(contentWidth - 120));
    m_ui->backButton->GetStyle().marginTop = Panorama::Length::Px((topBarHeight - S(35)) / 2.0f);
    m_ui->backButton->SetOnActivate([this]() { OnBackClicked(); });
    topContent->AddChild(m_ui->backButton);

    // ===== MAIN CONTENT =====
    auto mainWrapper = P("MainWrapper", 0, sh - 55, none);
    mainWrapper->GetStyle().marginTop = Panorama::Length::Px(55);
    m_ui->root->AddChild(mainWrapper);
    
    auto main = P("Main", contentWidth, sh - 55, none);
    main->GetStyle().marginLeft = Panorama::Length::Px(contentOffsetX);
    mainWrapper->AddChild(main);

    // Filter bar
    auto filterBar = P("FilterBar", 0, 50, panel);
    filterBar->GetStyle().borderRadius = S(3);
    filterBar->GetStyle().marginTop = Panorama::Length::Px(S(20));
    main->AddChild(filterBar);

    const char* filters[] = {"ALL", "STRENGTH", "AGILITY", "INTELLIGENCE"};
    for (int i = 0; i < 4; i++) {
        auto fb = std::make_shared<Panorama::CButton>(filters[i], std::string("Filter") + std::to_string(i));
        fb->GetStyle().width = Panorama::Length::Px(S(120));
        fb->GetStyle().height = Panorama::Length::Px(S(30));
        fb->GetStyle().backgroundColor = i == 0 ? accent : Panorama::Color(0.12f, 0.12f, 0.15f, 1.0f);
        fb->GetStyle().borderRadius = S(2);
        fb->GetStyle().fontSize = S(10);
        fb->GetStyle().color = Panorama::Color::White();
        fb->GetStyle().marginLeft = Panorama::Length::Px(S(15 + i * 130));
        fb->GetStyle().marginTop = Panorama::Length::Px(S(10));
        filterBar->AddChild(fb);
    }

    // Hero grid
    auto heroGrid = P("HeroGrid", 0, sh - 200, none);
    heroGrid->GetStyle().marginTop = Panorama::Length::Px(S(80));
    main->AddChild(heroGrid);

    // Sample heroes (6x4 grid)
    const char* heroNames[] = {
        "Axe", "Juggernaut", "Sven", "Pudge", "Invoker", "Crystal Maiden",
        "Lina", "Lion", "Shadow Fiend", "Anti-Mage", "Phantom Assassin", "Drow Ranger",
        "Sniper", "Mirana", "Zeus", "Earthshaker", "Tidehunter", "Tiny",
        "Witch Doctor", "Rubick", "Enigma", "Nature's Prophet", "Furion", "Windranger"
    };

    Panorama::Color heroColors[] = {
        {0.75f, 0.25f, 0.25f, 1.0f}, {0.85f, 0.45f, 0.25f, 1.0f}, {0.55f, 0.65f, 0.85f, 1.0f},
        {0.45f, 0.75f, 0.35f, 1.0f}, {0.85f, 0.75f, 0.25f, 1.0f}, {0.65f, 0.85f, 0.95f, 1.0f},
        {0.95f, 0.45f, 0.35f, 1.0f}, {0.55f, 0.35f, 0.75f, 1.0f}, {0.25f, 0.25f, 0.35f, 1.0f},
        {0.75f, 0.55f, 0.85f, 1.0f}, {0.35f, 0.45f, 0.65f, 1.0f}, {0.45f, 0.65f, 0.85f, 1.0f},
        {0.65f, 0.55f, 0.35f, 1.0f}, {0.55f, 0.75f, 0.95f, 1.0f}, {0.85f, 0.75f, 0.45f, 1.0f},
        {0.65f, 0.45f, 0.25f, 1.0f}, {0.35f, 0.75f, 0.65f, 1.0f}, {0.55f, 0.55f, 0.55f, 1.0f},
        {0.75f, 0.45f, 0.75f, 1.0f}, {0.45f, 0.85f, 0.45f, 1.0f}, {0.35f, 0.25f, 0.55f, 1.0f},
        {0.55f, 0.75f, 0.35f, 1.0f}, {0.65f, 0.75f, 0.45f, 1.0f}, {0.85f, 0.65f, 0.35f, 1.0f}
    };

    float heroCardWidth = 90;
    float heroCardHeight = 120;
    float gridSpacing = 12;
    int cols = 6;

    for (int i = 0; i < 24; i++) {
        int row = i / cols;
        int col = i % cols;
        
        auto heroCard = P(std::string("Hero") + std::to_string(i), heroCardWidth, heroCardHeight, heroColors[i]);
        heroCard->GetStyle().borderRadius = S(4);
        heroCard->GetStyle().marginLeft = Panorama::Length::Px(S(20 + col * (heroCardWidth + gridSpacing)));
        heroCard->GetStyle().marginTop = Panorama::Length::Px(S(row * (heroCardHeight + gridSpacing)));
        heroGrid->AddChild(heroCard);

        // Hero portrait placeholder
        auto portrait = P("Portrait" + std::to_string(i), heroCardWidth - 10, 80, Panorama::Color(0.1f, 0.1f, 0.12f, 0.5f));
        portrait->GetStyle().borderRadius = S(3);
        portrait->GetStyle().marginLeft = Panorama::Length::Px(S(5));
        portrait->GetStyle().marginTop = Panorama::Length::Px(S(5));
        heroCard->AddChild(portrait);

        // Hero name
        auto heroName = L(heroNames[i], 9, light);
        heroName->GetStyle().marginLeft = Panorama::Length::Px(S(8));
        heroName->GetStyle().marginTop = Panorama::Length::Px(S(92));
        heroCard->AddChild(heroName);
    }
}

void HeroesState::DestroyUI() {
    if (m_ui->root) {
        auto& engine = Panorama::CUIEngine::Instance();
        if (auto* uiRoot = engine.GetRoot()) {
            uiRoot->RemoveChild(m_ui->root.get());
        }
    }
    m_ui->root.reset();
}

void HeroesState::Update(f32 dt) { 
    Panorama::CUIEngine::Instance().Update(dt); 
}

void HeroesState::Render() { 
    Panorama::CUIEngine::Instance().Render(); 
}

bool HeroesState::OnKeyDown(i32 key) {
    if (key == 27) { OnBackClicked(); return true; }
    return false;
}

bool HeroesState::OnMouseMove(f32 x, f32 y) { 
    Panorama::CUIEngine::Instance().OnMouseMove(x, y); 
    return true; 
}

bool HeroesState::OnMouseDown(f32 x, f32 y, i32 b) { 
    Panorama::CUIEngine::Instance().OnMouseDown(x, y, b); 
    return true; 
}

bool HeroesState::OnMouseUp(f32 x, f32 y, i32 b) { 
    Panorama::CUIEngine::Instance().OnMouseUp(x, y, b); 
    return true; 
}

void HeroesState::OnHeroSelected(const std::string& heroId) {
    m_selectedHero = heroId;
    // TODO: Show hero details panel
}

void HeroesState::OnBackClicked() {
    m_manager->ChangeState(EGameState::MainMenu);
}

} // namespace Game
