/**
 * HeroPickState - Dota 2 style hero pick phase
 * 
 * Displays hero grid with timer, team picks, and confirm button.
 * After all players pick or timer expires, transitions to InGame.
 * 
 * Uses shared NetworkClient from GameStateManager for persistent connection.
 */

#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/core/CUIEngine.h"
#include "../ui/panorama/core/CPanel2D.h"
#include "../ui/panorama/widgets/CLabel.h"
#include "../ui/panorama/widgets/CButton.h"
#include "../ui/panorama/layout/CStyleSheet.h"
#include "../../client/ClientWorld.h"
#include "../../server/ServerWorld.h"
#include "../../network/NetworkClient.h"
#include "../../auth/AuthClient.h"

namespace Game {

struct HeroPickState::HeroPickUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    
    // Header
    std::shared_ptr<Panorama::CLabel> timerLabel;
    std::shared_ptr<Panorama::CLabel> phaseLabel;
    
    // Team panels
    std::shared_ptr<Panorama::CPanel2D> radiantPanel;
    std::shared_ptr<Panorama::CPanel2D> direPanel;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> radiantSlots;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> direSlots;
    std::vector<std::shared_ptr<Panorama::CLabel>> radiantLabels;
    std::vector<std::shared_ptr<Panorama::CLabel>> direLabels;
    
    // Hero grid
    std::shared_ptr<Panorama::CPanel2D> heroGrid;
    std::vector<std::shared_ptr<Panorama::CPanel2D>> heroCards;
    
    // Selected hero preview
    std::shared_ptr<Panorama::CPanel2D> previewPanel;
    std::shared_ptr<Panorama::CLabel> heroNameLabel;
    std::shared_ptr<Panorama::CLabel> heroAttrLabel;
    std::shared_ptr<Panorama::CButton> confirmButton;
    std::shared_ptr<Panorama::CButton> randomButton;
};

HeroPickState::HeroPickState() 
    : m_ui(std::make_unique<HeroPickUI>()) {}

HeroPickState::~HeroPickState() = default;

// Helper to get shared NetworkClient from GameStateManager
WorldEditor::Network::NetworkClient* HeroPickState::GetNetworkClient() {
    return m_manager ? m_manager->GetNetworkClient() : nullptr;
}

void HeroPickState::OnEnter() {
    LOG_INFO("HeroPickState::OnEnter()");
    m_pickTimer = 30.0f;
    m_hasPicked = false;
    m_allPicked = false;
    m_gameStartDelay = 0.0f;
    m_selectedHero.clear();
    m_confirmedHero.clear();
    m_myTeamSlot = 0;
    m_playerPicks.clear();
    
    CreateUI();
    
    // Connect to game server using shared NetworkClient from GameStateManager
    if (m_manager) {
        std::string serverIp = m_manager->GetGameServerIp();
        u16 serverPort = m_manager->GetGameServerPort();
        
        if (!serverIp.empty() && serverPort > 0) {
            // Get username from AuthClient
            std::string username = "Player";
            if (auto* authClient = m_manager->GetAuthClient()) {
                if (authClient->IsAuthenticated()) {
                    username = authClient->GetUsername();
                }
            }
            
            // Check if already connected (connection persists across states)
            if (!m_manager->IsConnectedToGameServer()) {
                LOG_INFO("HeroPickState connecting to server {}:{} as {}", serverIp, serverPort, username);
                if (m_manager->ConnectToGameServer(serverIp, serverPort, username)) {
                    ConsoleLog("Connecting to game server as " + username + "...");
                } else {
                    ConsoleLog("Failed to connect to server!");
                }
            } else {
                LOG_INFO("HeroPickState: Already connected to game server");
                ConsoleLog("Already connected to game server");
            }
            
            // Setup callbacks after connection
            SetupNetworkCallbacks();
        } else {
            LOG_WARN("No game server target set!");
        }
    }
    
    ConsoleLog("=== HERO PICK PHASE ===");
    ConsoleLog("Select your hero! 30 seconds remaining.");
}

void HeroPickState::SetupNetworkCallbacks() {
    auto* client = GetNetworkClient();
    if (!client) return;
    
    // Team assignment from server (includes our username)
    client->setOnTeamAssignment([this](u8 teamSlot, u8 teamId, const std::string& username) {
        LOG_INFO("Received team assignment: slot={}, team={}, username={}", 
                 teamSlot, teamId == 0 ? "Radiant" : "Dire", username);
        m_myTeamSlot = teamSlot;
        
        // Save team to GameStateManager for InGameState to use
        if (m_manager) {
            m_manager->SetPlayerTeam(teamSlot);
        }
        
        std::string teamName = teamSlot < 5 ? "RADIANT" : "DIRE";
        ConsoleLog("You (" + username + ") are on team " + teamName);
        UpdatePlayerSlot(teamSlot, username + " (You)", false);
    });
    
    // Player info broadcast (other players in the game)
    client->setOnPlayerInfo([this](u64 playerId, u8 teamSlot, const std::string& username) {
        LOG_INFO("Player info: id={}, slot={}, username={}", playerId, teamSlot, username);
        
        if (teamSlot != m_myTeamSlot) {
            UpdatePlayerSlot(teamSlot, username, false);
        }
        ConsoleLog("Player joined: " + username + " (slot " + std::to_string(teamSlot) + ")");
    });
    
    // Hero pick broadcast from other players
    client->setOnHeroPick([this](u64 playerId, const std::string& heroName, u8 teamSlot, bool confirmed) {
        LOG_INFO("Received hero pick: player {} -> {} (slot {}) - CALLBACK DISABLED FOR DEBUG", playerId, heroName, teamSlot);
        // Temporarily disabled to debug crash
        // m_playerPicks[playerId] = {heroName, teamSlot, confirmed};
        // if (teamSlot != m_myTeamSlot) {
        //     if (m_manager && m_manager->GetCurrentStateType() == EGameState::HeroPick && m_ui && m_ui->root) {
        //         UpdatePlayerSlot(teamSlot, heroName, confirmed);
        //     }
        //     ConsoleLog("Player " + std::to_string(playerId) + " picked " + heroName);
        // }
    });
    
    // All players picked - start game countdown
    client->setOnAllPicked([this](u8 playerCount, f32 startDelay) {
        LOG_INFO("All {} players picked! Game starts in {} seconds", playerCount, startDelay);
        m_allPicked = true;
        m_gameStartDelay = startDelay;
        
        // Safety: only update UI if we're still in HeroPick state
        if (m_manager && m_manager->GetCurrentStateType() == EGameState::HeroPick && m_ui && m_ui->root) {
            if (m_ui->phaseLabel) {
                m_ui->phaseLabel->SetText("ALL PICKED! STARTING GAME...");
            }
            if (m_ui->timerLabel) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f", startDelay);
                m_ui->timerLabel->SetText(buf);
            }
        }
        ConsoleLog("All heroes picked! Game starting in " + std::to_string((int)startDelay) + " seconds");
    });
    
    // Timer sync from server
    client->setOnPickTimer([this](f32 timeRemaining, u8 phase) {
        m_pickTimer = timeRemaining;
        UpdateTimer();
    });
}

void HeroPickState::UpdatePlayerSlot(u8 teamSlot, const std::string& playerName, bool confirmed) {
    // Safety check - UI may not be initialized or may be destroyed
    if (!m_ui || !m_ui->root) {
        LOG_WARN("UpdatePlayerSlot called but UI is not valid");
        return;
    }
    
    Panorama::Color pickedColor(0.2f, 0.5f, 0.3f, 1.0f);
    Panorama::Color selectedColor(0.3f, 0.4f, 0.5f, 0.9f);
    Panorama::Color emptyColor(0.1f, 0.1f, 0.12f, 0.8f);
    
    if (teamSlot < 5) {
        if (teamSlot < m_ui->radiantSlots.size() && m_ui->radiantSlots[teamSlot]) {
            auto& slot = m_ui->radiantSlots[teamSlot];
            slot->GetStyle().backgroundColor = confirmed ? pickedColor : (playerName.empty() ? emptyColor : selectedColor);
            if (teamSlot < m_ui->radiantLabels.size() && m_ui->radiantLabels[teamSlot]) {
                m_ui->radiantLabels[teamSlot]->SetText(playerName.empty() ? "---" : playerName);
                m_ui->radiantLabels[teamSlot]->GetStyle().color = Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }
    } else {
        u8 direSlot = teamSlot - 5;
        if (direSlot < m_ui->direSlots.size() && m_ui->direSlots[direSlot]) {
            auto& slot = m_ui->direSlots[direSlot];
            slot->GetStyle().backgroundColor = confirmed ? pickedColor : (playerName.empty() ? emptyColor : selectedColor);
            if (direSlot < m_ui->direLabels.size() && m_ui->direLabels[direSlot]) {
                m_ui->direLabels[direSlot]->SetText(playerName.empty() ? "---" : playerName);
                m_ui->direLabels[direSlot]->GetStyle().color = Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }
    }
}

void HeroPickState::OnExit() {
    // Don't disconnect - connection persists to InGameState
    LOG_INFO("HeroPickState::OnExit()");
    DestroyUI();

    // Post-condition: HeroPick UI must not remain attached to Panorama root.
    auto& engine = Panorama::CUIEngine::Instance();
    if (auto* leaked = engine.FindPanelByID("HeroPickRoot")) {
        LOG_ERROR("HeroPickState::OnExit - HeroPickRoot still found after DestroyUI (ptr={}, parent={})",
                  static_cast<void*>(leaked),
                  leaked->GetParent() ? leaked->GetParent()->GetID().c_str() : "<null>");
    } else {
        LOG_INFO("HeroPickState::OnExit - HeroPickRoot cleaned up");
    }
}

// Scaled helper
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


void HeroPickState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    float sw = engine.GetScreenWidth();
    float sh = engine.GetScreenHeight();
    
    // Colors
    Panorama::Color bg(0.02f, 0.03f, 0.05f, 1.0f);
    Panorama::Color headerBg(0.01f, 0.02f, 0.03f, 0.95f);
    Panorama::Color radiantColor(0.15f, 0.45f, 0.25f, 0.9f);
    Panorama::Color direColor(0.55f, 0.15f, 0.15f, 0.9f);
    Panorama::Color slotEmpty(0.1f, 0.1f, 0.12f, 0.8f);
    Panorama::Color gridBg(0.06f, 0.07f, 0.09f, 0.95f);
    Panorama::Color white(1.0f, 1.0f, 1.0f, 1.0f);
    Panorama::Color gold(0.95f, 0.75f, 0.25f, 1.0f);
    Panorama::Color gray(0.5f, 0.5f, 0.5f, 1.0f);
    Panorama::Color greenBtn(0.2f, 0.55f, 0.2f, 1.0f);
    Panorama::Color blueBtn(0.2f, 0.4f, 0.6f, 1.0f);
    Panorama::Color none(0, 0, 0, 0);
    
    m_ui->root = P("HeroPickRoot", 0, 0, bg);
    uiRoot->AddChild(m_ui->root);
    
    // Header
    auto header = P("Header", 0, 80, headerBg);
    m_ui->root->AddChild(header);
    
    m_ui->timerLabel = L("0:30", "display", gold);
    m_ui->timerLabel->GetStyle().marginLeft = Panorama::Length::Px((sw - S(80)) / 2);
    m_ui->timerLabel->GetStyle().marginTop = Panorama::Length::Px(S(15));
    header->AddChild(m_ui->timerLabel);
    
    m_ui->phaseLabel = L("PICK YOUR HERO", "heading", white);
    m_ui->phaseLabel->GetStyle().marginLeft = Panorama::Length::Px((sw - S(150)) / 2);
    m_ui->phaseLabel->GetStyle().marginTop = Panorama::Length::Px(S(50));
    header->AddChild(m_ui->phaseLabel);
    
    // Team panels
    float teamPanelWidth = 180;
    float teamPanelHeight = sh - 180;
    
    // Radiant (left)
    m_ui->radiantPanel = P("RadiantPanel", teamPanelWidth, teamPanelHeight, radiantColor);
    m_ui->radiantPanel->GetStyle().borderRadius = S(4);
    m_ui->radiantPanel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->radiantPanel->GetStyle().marginTop = Panorama::Length::Px(S(100));
    m_ui->root->AddChild(m_ui->radiantPanel);
    
    auto radiantTitle = L("RADIANT", "subheading", white);
    radiantTitle->GetStyle().marginLeft = Panorama::Length::Px(S(50));
    radiantTitle->GetStyle().marginTop = Panorama::Length::Px(S(10));
    m_ui->radiantPanel->AddChild(radiantTitle);
    
    for (int i = 0; i < 5; i++) {
        auto slot = P("RadiantSlot" + std::to_string(i), 160, 50, slotEmpty);
        slot->GetStyle().borderRadius = S(3);
        slot->GetStyle().marginLeft = Panorama::Length::Px(S(10));
        slot->GetStyle().marginTop = Panorama::Length::Px(S(45 + i * 60));
        m_ui->radiantPanel->AddChild(slot);
        m_ui->radiantSlots.push_back(slot);
        
        auto playerLabel = L("---", "caption", gray);
        playerLabel->GetStyle().marginLeft = Panorama::Length::Px(S(10));
        playerLabel->GetStyle().marginTop = Panorama::Length::Px(S(15));
        slot->AddChild(playerLabel);
        m_ui->radiantLabels.push_back(playerLabel);
    }
    
    // Dire (right)
    m_ui->direPanel = P("DirePanel", teamPanelWidth, teamPanelHeight, direColor);
    m_ui->direPanel->GetStyle().borderRadius = S(4);
    m_ui->direPanel->GetStyle().marginLeft = Panorama::Length::Px(sw - S(teamPanelWidth + 20));
    m_ui->direPanel->GetStyle().marginTop = Panorama::Length::Px(S(100));
    m_ui->root->AddChild(m_ui->direPanel);
    
    auto direTitle = L("DIRE", "subheading", white);
    direTitle->GetStyle().marginLeft = Panorama::Length::Px(S(70));
    direTitle->GetStyle().marginTop = Panorama::Length::Px(S(10));
    m_ui->direPanel->AddChild(direTitle);
    
    for (int i = 0; i < 5; i++) {
        auto slot = P("DireSlot" + std::to_string(i), 160, 50, slotEmpty);
        slot->GetStyle().borderRadius = S(3);
        slot->GetStyle().marginLeft = Panorama::Length::Px(S(10));
        slot->GetStyle().marginTop = Panorama::Length::Px(S(45 + i * 60));
        m_ui->direPanel->AddChild(slot);
        m_ui->direSlots.push_back(slot);
        
        auto playerLabel = L("---", "caption", gray);
        playerLabel->GetStyle().marginLeft = Panorama::Length::Px(S(10));
        playerLabel->GetStyle().marginTop = Panorama::Length::Px(S(15));
        slot->AddChild(playerLabel);
        m_ui->direLabels.push_back(playerLabel);
    }
    
    // Hero grid (center)
    float gridWidth = sw - S(teamPanelWidth * 2 + 80);
    float gridHeight = sh - 280;
    
    m_ui->heroGrid = P("HeroGrid", gridWidth / 1.35f, gridHeight / 1.35f, gridBg);
    m_ui->heroGrid->GetStyle().borderRadius = S(4);
    m_ui->heroGrid->GetStyle().marginLeft = Panorama::Length::Px(S(teamPanelWidth + 40));
    m_ui->heroGrid->GetStyle().marginTop = Panorama::Length::Px(S(100));
    m_ui->root->AddChild(m_ui->heroGrid);
    
    struct HeroData { const char* name; const char* attr; Panorama::Color color; };
    HeroData heroes[] = {
        {"Axe", "STR", {0.75f, 0.25f, 0.25f, 1.0f}},
        {"Sven", "STR", {0.55f, 0.65f, 0.85f, 1.0f}},
        {"Pudge", "STR", {0.45f, 0.55f, 0.35f, 1.0f}},
        {"Tidehunter", "STR", {0.35f, 0.65f, 0.55f, 1.0f}},
        {"Earthshaker", "STR", {0.65f, 0.45f, 0.25f, 1.0f}},
        {"Tiny", "STR", {0.55f, 0.55f, 0.55f, 1.0f}},
        {"Juggernaut", "AGI", {0.85f, 0.45f, 0.25f, 1.0f}},
        {"Anti-Mage", "AGI", {0.75f, 0.55f, 0.85f, 1.0f}},
        {"Phantom Assassin", "AGI", {0.35f, 0.45f, 0.65f, 1.0f}},
        {"Drow Ranger", "AGI", {0.45f, 0.65f, 0.85f, 1.0f}},
        {"Sniper", "AGI", {0.65f, 0.55f, 0.35f, 1.0f}},
        {"Mirana", "AGI", {0.55f, 0.75f, 0.95f, 1.0f}},
        {"Invoker", "INT", {0.85f, 0.75f, 0.25f, 1.0f}},
        {"Crystal Maiden", "INT", {0.65f, 0.85f, 0.95f, 1.0f}},
        {"Lina", "INT", {0.95f, 0.45f, 0.35f, 1.0f}},
        {"Lion", "INT", {0.55f, 0.35f, 0.75f, 1.0f}},
        {"Shadow Fiend", "INT", {0.25f, 0.25f, 0.35f, 1.0f}},
        {"Zeus", "INT", {0.85f, 0.75f, 0.45f, 1.0f}},
    };
    
    float cardW = 70, cardH = 90, spacing = 8;
    int cols = 6;
    
    for (int i = 0; i < 18; i++) {
        int row = i / cols, col = i % cols;
        
        auto card = P("HeroCard" + std::to_string(i), cardW, cardH, heroes[i].color);
        card->GetStyle().borderRadius = S(3);
        card->GetStyle().marginLeft = Panorama::Length::Px(S(15 + col * (cardW + spacing)));
        card->GetStyle().marginTop = Panorama::Length::Px(S(15 + row * (cardH + spacing)));
        m_ui->heroGrid->AddChild(card);
        m_ui->heroCards.push_back(card);
        
        auto portrait = P("Portrait" + std::to_string(i), cardW - 8, 55, Panorama::Color(0.1f, 0.1f, 0.12f, 0.5f));
        portrait->GetStyle().borderRadius = S(2);
        portrait->GetStyle().marginLeft = Panorama::Length::Px(S(4));
        portrait->GetStyle().marginTop = Panorama::Length::Px(S(4));
        card->AddChild(portrait);
        
        auto nameLabel = L(heroes[i].name, "small", white);
        nameLabel->GetStyle().marginLeft = Panorama::Length::Px(S(4));
        nameLabel->GetStyle().marginTop = Panorama::Length::Px(S(62));
        card->AddChild(nameLabel);
        
        Panorama::Color attrColor = 
            strcmp(heroes[i].attr, "STR") == 0 ? Panorama::Color(0.85f, 0.35f, 0.35f, 1.0f) :
            strcmp(heroes[i].attr, "AGI") == 0 ? Panorama::Color(0.35f, 0.85f, 0.35f, 1.0f) :
            Panorama::Color(0.35f, 0.55f, 0.95f, 1.0f);
        
        auto attrBadge = P("Attr" + std::to_string(i), 25, 12, attrColor);
        attrBadge->GetStyle().borderRadius = S(2);
        attrBadge->GetStyle().marginLeft = Panorama::Length::Px(S(cardW - 29));
        attrBadge->GetStyle().marginTop = Panorama::Length::Px(S(4));
        card->AddChild(attrBadge);
        
        std::string heroName = heroes[i].name;
        auto clickBtn = std::make_shared<Panorama::CButton>("", "HeroBtn" + std::to_string(i));
        clickBtn->GetStyle().width = Panorama::Length::Px(S(cardW));
        clickBtn->GetStyle().height = Panorama::Length::Px(S(cardH));
        clickBtn->GetStyle().backgroundColor = Panorama::Color(0, 0, 0, 0);
        // Position button at (0,0) relative to card to cover entire card
        clickBtn->GetStyle().marginLeft = Panorama::Length::Px(0);
        clickBtn->GetStyle().marginTop = Panorama::Length::Px(0);
        
        // Simple callback - just log and call OnHeroClicked
        clickBtn->SetOnActivate([this, heroName]() { 
            OnHeroClicked(heroName); 
        });
        card->AddChild(clickBtn);
    }
    
    // Bottom panel
    auto bottomPanel = P("BottomPanel", gridWidth / 1.35f, 80, headerBg);
    bottomPanel->GetStyle().borderRadius = S(4);
    bottomPanel->GetStyle().marginLeft = Panorama::Length::Px(S(teamPanelWidth + 40));
    bottomPanel->GetStyle().marginTop = Panorama::Length::Px(sh - S(100));
    m_ui->root->AddChild(bottomPanel);
    
    m_ui->heroNameLabel = L("Select a hero", "heading", gray);
    m_ui->heroNameLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->heroNameLabel->GetStyle().marginTop = Panorama::Length::Px(S(25));
    bottomPanel->AddChild(m_ui->heroNameLabel);
    
    m_ui->confirmButton = std::make_shared<Panorama::CButton>("PICK HERO", "ConfirmBtn");
    m_ui->confirmButton->GetStyle().width = Panorama::Length::Px(S(140));
    m_ui->confirmButton->GetStyle().height = Panorama::Length::Px(S(45));
    m_ui->confirmButton->GetStyle().backgroundColor = greenBtn;
    m_ui->confirmButton->GetStyle().borderRadius = S(4);
    m_ui->confirmButton->AddClass("body");
    m_ui->confirmButton->GetStyle().color = white;
    m_ui->confirmButton->GetStyle().marginLeft = Panorama::Length::Px(S(gridWidth / 1.35f - 320));
    m_ui->confirmButton->GetStyle().marginTop = Panorama::Length::Px(S(18));
    m_ui->confirmButton->SetOnActivate([this]() { OnConfirmPick(); });
    m_ui->confirmButton->SetVisible(false);
    bottomPanel->AddChild(m_ui->confirmButton);
    
    m_ui->randomButton = std::make_shared<Panorama::CButton>("RANDOM", "RandomBtn");
    m_ui->randomButton->GetStyle().width = Panorama::Length::Px(S(100));
    m_ui->randomButton->GetStyle().height = Panorama::Length::Px(S(45));
    m_ui->randomButton->GetStyle().backgroundColor = blueBtn;
    m_ui->randomButton->GetStyle().borderRadius = S(4);
    m_ui->randomButton->AddClass("body");
    m_ui->randomButton->GetStyle().color = white;
    m_ui->randomButton->GetStyle().marginLeft = Panorama::Length::Px(S(gridWidth / 1.35f - 160));
    m_ui->randomButton->GetStyle().marginTop = Panorama::Length::Px(S(18));
    m_ui->randomButton->SetOnActivate([this]() { OnRandomHero(); });
    bottomPanel->AddChild(m_ui->randomButton);
}

void HeroPickState::DestroyUI() {
    auto& engine = Panorama::CUIEngine::Instance();

    // Be robust: sometimes the subtree we want to destroy may not be the exact pointer stored in m_ui->root
    // (e.g. if something re-created UI or the pointer got swapped). Always try to remove by ID too.
    Panorama::CPanel2D* rootById = engine.FindPanelByID("HeroPickRoot");

    if (m_ui->root) {
        engine.ClearInputStateForSubtree(m_ui->root.get());
        if (auto* uiRoot = engine.GetRoot()) {
            uiRoot->RemoveChild(m_ui->root.get());
        }
        // If it still exists in the tree, detach it from its current parent.
        if (rootById) {
            LOG_WARN("HeroPickState::DestroyUI - HeroPickRoot still in tree after RemoveChild(m_ui->root). Detaching by ID.");
            rootById->SetVisible(false);
            rootById->SetParent(nullptr);
        }
    } else if (rootById) {
        // m_ui->root is null but the UI subtree still exists. Detach it to avoid leaking UI across states.
        LOG_WARN("HeroPickState::DestroyUI - m_ui->root is null but HeroPickRoot exists. Detaching by ID.");
        engine.ClearInputStateForSubtree(rootById);
        rootById->SetVisible(false);
        rootById->SetParent(nullptr);
    }

    // Last resort: if multiple HeroPick panels were accidentally created, remove them all.
    // This guarantees the pick UI cannot leak into InGame even if IDs are duplicated.
    int removed = 0;
    for (;;) {
        auto* p = engine.FindPanelByID("HeroPickRoot");
        if (!p) break;
        LOG_WARN("HeroPickState::DestroyUI - removing extra HeroPickRoot (ptr={})", static_cast<void*>(p));
        engine.ClearInputStateForSubtree(p);
        p->SetVisible(false);
        p->SetParent(nullptr);
        removed++;
        if (removed > 16) { // sanity guard
            LOG_ERROR("HeroPickState::DestroyUI - too many HeroPickRoot panels, aborting cleanup loop");
            break;
        }
    }

    // Extra safety: detach key panels if they exist outside the expected subtree.
    const char* extraIds[] = {"HeroGrid", "RadiantPanel", "DirePanel", "Header", "BottomPanel"};
    for (const char* id : extraIds) {
        int n = 0;
        for (;;) {
            auto* p = engine.FindPanelByID(id);
            if (!p) break;
            // If these are still present, they are part of leaked UI. Detach them too.
            LOG_WARN("HeroPickState::DestroyUI - detaching leaked panel id='{}' ptr={}", id, static_cast<void*>(p));
            engine.ClearInputStateForSubtree(p);
            p->SetVisible(false);
            p->SetParent(nullptr);
            if (++n > 32) break;
        }
    }

    m_ui->root.reset();
    m_ui->heroCards.clear();
    m_ui->radiantSlots.clear();
    m_ui->direSlots.clear();
    m_ui->radiantLabels.clear();
    m_ui->direLabels.clear();
}


void HeroPickState::Update(f32 deltaTime) {
    // Debug: log every N frames to see if Update is running
    static int updateCount = 0;
    if (++updateCount % 60 == 0) {
        LOG_INFO("HeroPickState::Update frame {}", updateCount);
        spdlog::default_logger()->flush();
    }
    
    // Safety check - ensure we're still in HeroPick state
    if (!m_manager || m_manager->GetCurrentStateType() != EGameState::HeroPick) {
        return;
    }
    
    Panorama::CUIEngine::Instance().Update(deltaTime);
    
    // Update shared network client
    if (auto* client = GetNetworkClient()) {
        client->update(deltaTime);
    }
    
    // Safety check again after network update (callbacks may change state)
    if (!m_manager || m_manager->GetCurrentStateType() != EGameState::HeroPick) {
        return;
    }
    
    // Handle game start countdown after all picked
    if (m_allPicked) {
        m_gameStartDelay -= deltaTime;
        
        if (m_ui && m_ui->timerLabel) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f", std::max(0.0f, m_gameStartDelay));
            m_ui->timerLabel->SetText(buf);
        }
        
        if (m_gameStartDelay <= 0.0f) {
            TransitionToGame();
        }
        return;
    }
    
    if (!m_hasPicked) {
        m_pickTimer -= deltaTime;
        UpdateTimer();
        
        if (m_pickTimer <= 0.0f) {
            OnRandomHero();
            OnConfirmPick();
        }
    }
}

void HeroPickState::TransitionToGame() {
    LOG_INFO("Transitioning to InGame state with hero '{}' (connection persists)", m_confirmedHero);
    
    if (m_manager) {
        if (auto* inGame = m_manager->GetInGameState()) {
            inGame->SetWorlds(std::move(m_clientWorld), std::move(m_serverWorld));
            inGame->SetSelectedHero(m_confirmedHero);
        }
        // Connection persists - no need to pass server info, it's in GameStateManager
        m_manager->ChangeState(EGameState::InGame);
    }
}

void HeroPickState::UpdateTimer() {
    if (!m_ui || !m_ui->timerLabel) return;
    
    int seconds = static_cast<int>(std::max(0.0f, m_pickTimer));
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
    m_ui->timerLabel->SetText(buf);
    
    if (m_pickTimer < 10.0f) {
        m_ui->timerLabel->GetStyle().color = Panorama::Color(0.95f, 0.25f, 0.25f, 1.0f);
    }
}

void HeroPickState::Render() {
    Panorama::CUIEngine::Instance().Render();
}

void HeroPickState::OnHeroClicked(const std::string& heroId) {
    // Flush logs immediately to capture crash location
    LOG_INFO("OnHeroClicked START heroId='{}'", heroId);
    spdlog::default_logger()->flush();
    
    if (!m_ui) {
        LOG_ERROR("OnHeroClicked: m_ui is nullptr!");
        spdlog::default_logger()->flush();
        return;
    }
    
    if (m_hasPicked) return;
    
    m_selectedHero = heroId;
    LOG_INFO("Hero selected: {}", heroId);
    ConsoleLog("Selected: " + heroId);
    
    if (m_ui->heroNameLabel) {
        m_ui->heroNameLabel->SetText(heroId);
        m_ui->heroNameLabel->GetStyle().color = Panorama::Color(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    if (m_ui->confirmButton) {
        m_ui->confirmButton->SetVisible(true);
    }
}

void HeroPickState::OnConfirmPick() {
    if (m_selectedHero.empty() || m_hasPicked) return;
    
    m_confirmedHero = m_selectedHero;
    m_hasPicked = true;
    
    LOG_INFO("Hero confirmed: {}", m_confirmedHero);
    ConsoleLog("Picked: " + m_confirmedHero);
    
    // Send pick to server using shared client
    if (auto* client = GetNetworkClient()) {
        if (client->isConnected()) {
            client->sendHeroPick(m_confirmedHero, m_myTeamSlot, true);
        }
    }
    
    // Update UI
    if (m_ui->phaseLabel) {
        m_ui->phaseLabel->SetText("WAITING FOR OTHER PLAYERS...");
    }
    if (m_ui->confirmButton) {
        m_ui->confirmButton->SetVisible(false);
    }
    if (m_ui->randomButton) {
        m_ui->randomButton->SetVisible(false);
    }
    
    UpdatePlayerSlot(m_myTeamSlot, m_confirmedHero + " (You)", true);
    
    // In single player / local mode, transition immediately
    if (auto* client = GetNetworkClient()) {
        if (!client->isConnected()) {
            if (m_manager) {
                if (auto* inGame = m_manager->GetInGameState()) {
                    inGame->SetWorlds(std::move(m_clientWorld), std::move(m_serverWorld));
                    inGame->SetSelectedHero(m_confirmedHero);
                }
                m_manager->ChangeState(EGameState::InGame);
            }
        }
    }
}

void HeroPickState::OnRandomHero() {
    if (m_hasPicked) return;
    
    const char* heroes[] = {
        "Axe", "Sven", "Pudge", "Tidehunter", "Earthshaker", "Tiny",
        "Juggernaut", "Anti-Mage", "Phantom Assassin", "Drow Ranger", "Sniper", "Mirana",
        "Invoker", "Crystal Maiden", "Lina", "Lion", "Shadow Fiend", "Zeus"
    };
    
    int idx = rand() % 18;
    OnHeroClicked(heroes[idx]);
    ConsoleLog("Random hero: " + std::string(heroes[idx]));
}

void HeroPickState::SetWorlds(std::unique_ptr<WorldEditor::ClientWorld> client,
                              std::unique_ptr<WorldEditor::ServerWorld> server) {
    m_clientWorld = std::move(client);
    m_serverWorld = std::move(server);
}

bool HeroPickState::OnKeyDown(i32 key) {
    if (key == 13 && !m_selectedHero.empty()) {
        OnConfirmPick();
        return true;
    }
    if (key == 'R' || key == 'r') {
        OnRandomHero();
        return true;
    }
    return false;
}

bool HeroPickState::OnMouseMove(f32 x, f32 y) {
    Panorama::CUIEngine::Instance().OnMouseMove(x, y);
    return true;
}

bool HeroPickState::OnMouseDown(f32 x, f32 y, i32 b) {
    LOG_INFO("HeroPickState::OnMouseDown pos=({:.0f},{:.0f}) button={}", x, y, b);
    spdlog::default_logger()->flush();
    Panorama::CUIEngine::Instance().OnMouseDown(x, y, b);
    return true;
}

bool HeroPickState::OnMouseUp(f32 x, f32 y, i32 b) {
    LOG_INFO("HeroPickState::OnMouseUp pos=({:.0f},{:.0f}) button={}", x, y, b);
    spdlog::default_logger()->flush();
    Panorama::CUIEngine::Instance().OnMouseUp(x, y, b);
    return true;
}

} // namespace Game
