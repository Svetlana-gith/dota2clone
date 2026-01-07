#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/core/CUIEngine.h"
#include "../ui/panorama/core/CPanel2D.h"
#include "../ui/panorama/layout/CStyleSheet.h"
#include "../ui/panorama/core/GameEvents.h"
#include "../ui/panels/SettingsPanel.h"
#include "../ui/panels/MatchmakingPanel.h"
#include "../ui/panels/ReconnectPanel.h"
#include "../ui/mainmenu/MainMenuTopBar.h"
#include "../ui/mainmenu/MainMenuBottomBar.h"
#include "../ui/mainmenu/MainMenuContent.h"
#include "network/MatchmakingClient.h"
#include "auth/AuthClient.h"
#include <cstdio>

namespace Game {

struct MainMenuState::MenuUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    
    // Modular components
    std::unique_ptr<MainMenuTopBar> topBar;
    std::unique_ptr<MainMenuBottomBar> bottomBar;
    std::unique_ptr<MainMenuContent> content;
    std::unique_ptr<SettingsPanel> settingsPanel;
    std::unique_ptr<MatchmakingPanel> matchmakingPanel;
    std::unique_ptr<ReconnectPanel> reconnectPanel;
};

MainMenuState::MainMenuState() : m_ui(std::make_unique<MenuUI>()) {}
MainMenuState::~MainMenuState() = default;

void MainMenuState::OnEnter() { 
    LOG_INFO("MainMenuState::OnEnter()");
    m_matchReadyHandled = false;  // Reset for new matchmaking session
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/main_menu.css");
    CreateUI(); 
    LOG_INFO("MainMenuState UI created");
    ConsoleLog("Main Menu loaded");
    CheckForActiveGame();
}

void MainMenuState::OnExit() {
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/base.css");
    DestroyUI();
}

void MainMenuState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    f32 sw = engine.GetScreenWidth();
    f32 sh = engine.GetScreenHeight();
    f32 contentWidth = sw * 0.8f;
    f32 contentHeight = (sh - 70) * 0.9f;
    f32 contentOffsetX = (sw - contentWidth) / 2.0f;
    f32 contentOffsetY = ((sh - 70) - contentHeight) / 2.0f;

    // Root panel
    m_ui->root = std::make_shared<Panorama::CPanel2D>("Root");
    m_ui->root->GetStyle().width = Panorama::Length::Fill();
    m_ui->root->GetStyle().height = Panorama::Length::Fill();
    m_ui->root->GetStyle().backgroundColor = Panorama::Color(0.02f, 0.04f, 0.08f, 1.0f);
    uiRoot->AddChild(m_ui->root);

    // Create modular components
    
    // Top Bar
    m_ui->topBar = std::make_unique<MainMenuTopBar>();
    m_ui->topBar->Create(m_ui->root.get(), sw, sh, contentWidth, contentOffsetX);
    m_ui->topBar->SetReturnToGameVisible(m_manager && m_manager->IsGameInProgress());
    m_ui->topBar->SetOnSettingsClicked([this]() { OnSettingsClicked(); });
    m_ui->topBar->SetOnReturnToGameClicked([this]() {
        if (m_manager) m_manager->PopState();
    });
    m_ui->topBar->SetOnNavClicked([this](int idx) {
        const char* nav[] = {"HEROES", "STORE", "WATCH", "LEARN", "ARCADE"};
        if (idx == 0 && m_manager) {
            m_manager->ChangeState(EGameState::Heroes);
        } else {
            ConsoleLog(std::string(nav[idx]) + " clicked (not implemented)");
        }
    });
    
    // Set username from auth
    if (m_manager) {
        if (auto* auth = m_manager->GetAuthClient()) {
            if (auth->IsAuthenticated()) {
                m_ui->topBar->SetUsername(auth->GetUsername());
            }
        }
    }

    // Content Area
    m_ui->content = std::make_unique<MainMenuContent>();
    m_ui->content->Create(m_ui->root.get(), contentWidth, contentHeight, contentOffsetX, contentOffsetY);
    
    // Set username in profile from auth
    if (m_manager) {
        if (auto* auth = m_manager->GetAuthClient()) {
            if (auth->IsAuthenticated()) {
                m_ui->content->SetUsername(auth->GetUsername());
            }
        }
    }

    // Bottom Bar (created after Content so it's on top for z-order)
    m_ui->bottomBar = std::make_unique<MainMenuBottomBar>();
    m_ui->bottomBar->Create(m_ui->root.get(), sw, sh, contentWidth, contentOffsetX);
    m_ui->bottomBar->SetOnPlayClicked([this]() { OnPlayClicked(); });

    // Matchmaking Panel (finding UI, accept overlay - Play button is in BottomBar)
    m_ui->matchmakingPanel = std::make_unique<MatchmakingPanel>();
    m_ui->matchmakingPanel->Create(m_ui->root.get(), m_ui->bottomBar->GetBottomBar().get(),
                                    sw, sh, contentWidth, contentWidth - 200);
    m_ui->matchmakingPanel->SetOnCancelClicked([this]() {
        if (m_mmClient) m_mmClient->cancelQueue();
        m_ui->matchmakingPanel->HideFindingUI();
        m_ui->bottomBar->SetPlayButtonVisible(true);
        ConsoleLog("Matchmaking cancelled");
    });
    m_ui->matchmakingPanel->SetOnAcceptClicked([this]() {
        if (m_mmClient) m_mmClient->acceptMatch();
        ConsoleLog("Match accepted");
        m_ui->matchmakingPanel->OnLocalPlayerAccepted(
            m_mmClient ? m_mmClient->getPlayerInfo().steamId : 0,
            m_mmClient ? m_mmClient->getAcceptPlayerIds() : std::vector<u64>{}
        );
    });
    m_ui->matchmakingPanel->SetOnDeclineClicked([this]() {
        if (m_mmClient) m_mmClient->declineMatch();
        m_ui->matchmakingPanel->HideAcceptOverlay();
        ConsoleLog("Match declined");
    });

    // Settings Panel
    m_ui->settingsPanel = std::make_unique<SettingsPanel>();
    m_ui->settingsPanel->Create(m_ui->root.get(), sw, sh);

    // Reconnect Panel
    m_ui->reconnectPanel = std::make_unique<ReconnectPanel>();
    m_ui->reconnectPanel->Create(m_ui->root.get(), sw, sh);
    m_ui->reconnectPanel->SetOnReconnect([this]() { OnReconnectClicked(); });
    m_ui->reconnectPanel->SetOnAbandon([this]() { OnAbandonClicked(); });
}

void MainMenuState::DestroyUI() {
    if (m_ui->root) {
        auto& engine = Panorama::CUIEngine::Instance();
        engine.ClearInputStateForSubtree(m_ui->root.get());
        if (auto* uiRoot = engine.GetRoot()) {
            uiRoot->RemoveChild(m_ui->root.get());
        }
    }
    
    m_ui->topBar.reset();
    m_ui->bottomBar.reset();
    m_ui->content.reset();
    m_ui->settingsPanel.reset();
    m_ui->matchmakingPanel.reset();
    m_ui->reconnectPanel.reset();
    m_ui->root.reset();
}

void MainMenuState::Update(f32 dt) {
    if (m_mmClient) {
        m_mmClient->update(dt);
        
        // Update matchmaking panel timers
        if (m_ui->matchmakingPanel) {
            m_ui->matchmakingPanel->Update(dt);
            
            // Update finding match timer
            if (m_ui->matchmakingPanel->IsSearching()) {
                const auto& qs = m_mmClient->getQueueStatus();
                // Timer is updated internally in MatchmakingPanel::Update
            }
            
            // Update accept countdown
            f32 remaining = m_mmClient->getAcceptTimeRemainingSeconds();
            m_ui->matchmakingPanel->UpdateAcceptCountdown(remaining);
        }
    }
    
    Panorama::CUIEngine::Instance().Update(dt);
}

void MainMenuState::Render() { 
    Panorama::CUIEngine::Instance().Render(); 
}

bool MainMenuState::OnKeyDown(i32 key) {
    if (key == 27) { OnExitClicked(); return true; }
    return false;
}

bool MainMenuState::OnMouseMove(f32 x, f32 y) { 
    Panorama::CUIEngine::Instance().OnMouseMove(x, y); 
    return true; 
}

bool MainMenuState::OnMouseDown(f32 x, f32 y, i32 b) { 
    Panorama::CUIEngine::Instance().OnMouseDown(x, y, b); 
    return true; 
}

bool MainMenuState::OnMouseUp(f32 x, f32 y, i32 b) { 
    Panorama::CUIEngine::Instance().OnMouseUp(x, y, b); 
    return true; 
}

void MainMenuState::OnPlayClicked() {
    LOG_INFO("=== PLAY BUTTON CLICKED ===");
    ConsoleLog("Searching for match...");

    if (!m_mmClient) {
        m_mmClient = std::make_unique<WorldEditor::Matchmaking::MatchmakingClient>();
    }
    
    SetupMatchmakingCallbacks();

    if (!m_mmClient->isConnected()) {
        auto* authClient = m_manager->GetAuthClient();
        if (!authClient || !authClient->IsAuthenticated()) {
            ConsoleLog("Not authenticated - please login first");
            return;
        }
        
        std::string sessionToken = authClient->GetSessionToken();
        if (sessionToken.empty()) {
            ConsoleLog("No session token - please login first");
            return;
        }
        
        m_mmClient->setSessionToken(sessionToken);
        
        if (!m_mmClient->connect("127.0.0.1", 27017)) {
            ConsoleLog("Failed to connect to matchmaking coordinator");
            return;
        }
        
        LOG_INFO("Connected to matchmaking coordinator");
    }

    WorldEditor::Matchmaking::MatchPreferences prefs;
    prefs.mode = WorldEditor::Matchmaking::MatchMode::AllPick;
    prefs.region = "auto";

    if (m_mmClient->queueForMatch(prefs)) {
        m_ui->bottomBar->SetPlayButtonVisible(false);
        m_ui->matchmakingPanel->ShowFindingUI();
    } else {
        ConsoleLog("Failed to queue for match");
    }
}

void MainMenuState::OnSettingsClicked() {
    if (m_ui && m_ui->settingsPanel) {
        if (m_ui->settingsPanel->IsVisible()) {
            m_ui->settingsPanel->Hide();
        } else {
            m_ui->settingsPanel->Show();
        }
    }
}

void MainMenuState::OnExitClicked() {
    Panorama::CGameEventData d;
    Panorama::GameEvents_Fire("Game_RequestExit", d);
}

void MainMenuState::CheckForActiveGame() {
    auto* authClient = m_manager->GetAuthClient();
    if (!authClient || !authClient->IsAuthenticated()) return;
    
    u64 accountId = authClient->GetAccountId();
    if (accountId == 0) return;
    
    if (!m_mmClient) {
        m_mmClient = std::make_unique<WorldEditor::Matchmaking::MatchmakingClient>();
        SetupReconnectCallbacks();
    }
    
    if (!m_mmClient->isConnected()) {
        std::string sessionToken = authClient->GetSessionToken();
        m_mmClient->setSessionToken(sessionToken);
        
        if (!m_mmClient->connect("127.0.0.1", 27017)) {
            LOG_WARN("Failed to connect to matchmaking coordinator for active game check");
            return;
        }
    }
    
    m_mmClient->checkForActiveGame(accountId);
    LOG_INFO("Checking for active game for account {}", accountId);
}

void MainMenuState::SetupMatchmakingCallbacks() {
    if (!m_mmClient) return;
    
    m_mmClient->setOnQueueConfirmed([this]() {
        ConsoleLog("Queue confirmed");
    });
    
    m_mmClient->setOnMatchFound([this](const WorldEditor::Matchmaking::LobbyInfo& lobby) {
        LOG_INFO("=== onMatchFound callback triggered! ===");
        ConsoleLog("Match found!");
        m_ui->matchmakingPanel->HideFindingUI();
        m_ui->matchmakingPanel->ShowAcceptOverlay(lobby);
    });

    m_mmClient->setOnMatchAcceptStatus([this](u16 requiredPlayers, const std::vector<u64>& playerIds, const std::vector<bool>& accepted) {
        u64 selfId = m_mmClient ? m_mmClient->getPlayerInfo().steamId : 0;
        m_ui->matchmakingPanel->UpdateAcceptStatus(requiredPlayers, playerIds, accepted, selfId);
    });
    
    m_mmClient->setOnMatchReady([this](const std::string& serverIP, u16 port) {
        // Prevent duplicate handling of MatchReady
        if (m_matchReadyHandled) {
            LOG_WARN("MatchReady already handled, ignoring duplicate");
            return;
        }
        m_matchReadyHandled = true;
        
        ConsoleLog("Match ready! Connecting...");
        m_ui->matchmakingPanel->HideAcceptOverlay();
        m_ui->matchmakingPanel->HideFindingUI();

        if (m_manager) {
            if (auto* loading = m_manager->GetLoadingState()) {
                loading->SetServerTarget(serverIP, port);
                loading->SetReconnect(false);
            }
        }

        LOG_INFO("Transitioning to Loading state (server {}:{})", serverIP, port);
        m_manager->ChangeState(EGameState::Loading);
    });
    
    m_mmClient->setOnMatchCancelled([this](const std::string& reason, bool shouldRequeue) {
        ConsoleLog(std::string("Match cancelled: ") + reason);
        m_ui->matchmakingPanel->HideAcceptOverlay();
        
        if (shouldRequeue) {
            m_ui->matchmakingPanel->ShowFindingUI();
            m_ui->bottomBar->SetPlayButtonVisible(false);
        } else {
            m_ui->matchmakingPanel->HideFindingUI();
            m_ui->bottomBar->SetPlayButtonVisible(true);
        }
    });
    
    m_mmClient->setOnError([this](const std::string& error) {
        ConsoleLog(std::string("MM error: ") + error);
        m_ui->matchmakingPanel->HideFindingUI();
        m_ui->bottomBar->SetPlayButtonVisible(true);
    });
    
    m_mmClient->setOnQueueRejected([this](const std::string& reason, bool authFailed, bool isBanned) {
        ConsoleLog(std::string("Queue rejected: ") + reason);
        m_ui->matchmakingPanel->HideFindingUI();
        m_ui->bottomBar->SetPlayButtonVisible(true);
    });
}

void MainMenuState::SetupReconnectCallbacks() {
    if (!m_mmClient) return;
    
    m_mmClient->setOnActiveGameFound([this](const WorldEditor::Matchmaking::ActiveGameInfo& gameInfo) {
        LOG_INFO("Active game found! Hero: {}, Server: {}:{}", 
                 gameInfo.heroName, gameInfo.serverIP, gameInfo.serverPort);
        ConsoleLog("You have an active game! Click RECONNECT to rejoin.");
        m_ui->reconnectPanel->Show(gameInfo);
    });
    
    m_mmClient->setOnNoActiveGame([this]() {
        LOG_INFO("No active game found");
        m_ui->reconnectPanel->Hide();
    });
    
    m_mmClient->setOnReconnectApproved([this](const std::string& serverIP, u16 port, u8 teamSlot, const std::string& heroName) {
        LOG_INFO("Reconnect approved! Connecting to {}:{}", serverIP, port);
        ConsoleLog("Reconnecting to game...");
        
        m_ui->reconnectPanel->Hide();
        
        if (m_manager) {
            if (auto* loading = m_manager->GetLoadingState()) {
                loading->SetServerTarget(serverIP, port);
                loading->SetReconnect(true);
            }
            m_manager->ChangeState(EGameState::Loading);
        }
    });
}

void MainMenuState::OnReconnectClicked() {
    if (!m_mmClient || !m_ui->reconnectPanel) return;
    
    const auto& gameInfo = m_ui->reconnectPanel->GetActiveGameInfo();
    LOG_INFO("Reconnect clicked, requesting reconnect to lobby {}", gameInfo.lobbyId);
    m_mmClient->requestReconnect(gameInfo.lobbyId);
}

void MainMenuState::OnAbandonClicked() {
    LOG_INFO("Abandon clicked");
    ConsoleLog("Game abandoned. You may receive a penalty.");
    m_ui->reconnectPanel->Hide();
}

} // namespace Game
