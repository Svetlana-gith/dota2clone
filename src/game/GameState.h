#pragma once
/**
 * Game State System - Valve-style state management
 * Handles transitions between MainMenu, Loading, InGame states
 */

#include "../core/Types.h"
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

// Full includes needed for unique_ptr
#include "../client/ClientWorld.h"
#include "../server/ServerWorld.h"
#include "../world/World.h"
#include "../network/MatchmakingTypes.h"
#include "../gameplay/GameplayController.h"

namespace WorldEditor {
namespace Matchmaking {
class MatchmakingClient;
} // namespace Matchmaking
namespace Network {
class NetworkClient;
} // namespace Network
} // namespace WorldEditor

namespace auth {
class AuthClient;
} // namespace auth

namespace Game {

// Forward declarations
class GameStateManager;

// ============ Game State Types ============

enum class EGameState {
    None,
    Login,          // Login/Registration screen
    MainMenu,       // Dashboard, profile, matchmaking
    Heroes,         // Hero selection/browsing screen
    Loading,        // Loading screen during map/asset load
    HeroPick,       // Hero pick phase (Dota 2 style)
    InGame,         // Active gameplay with HUD
    PostGame        // End game scoreboard
};

// ============ Base Game State ============

class IGameState {
public:
    virtual ~IGameState() = default;
    
    virtual EGameState GetType() const = 0;
    virtual const char* GetName() const = 0;
    
    // Lifecycle
    virtual void OnEnter() = 0;
    virtual void OnExit() = 0;
    virtual void OnPause() {}   // When another state pushes on top
    virtual void OnResume() {}  // When returning from pushed state
    
    // Frame update
    virtual void Update(f32 deltaTime) = 0;
    virtual void Render() = 0;
    
    // Input (return true if handled)
    virtual bool OnKeyDown(i32 key) { return false; }
    virtual bool OnKeyUp(i32 key) { return false; }
    virtual bool OnMouseMove(f32 x, f32 y) { return false; }
    virtual bool OnMouseDown(f32 x, f32 y, i32 button) { return false; }
    virtual bool OnMouseUp(f32 x, f32 y, i32 button) { return false; }
    virtual bool OnMouseWheel(f32 delta) { return false; }
    
protected:
    GameStateManager* m_manager = nullptr;
    friend class GameStateManager;
};

// ============ Login State ============

class LoginState : public IGameState {
public:
    LoginState();
    ~LoginState() override;
    
    EGameState GetType() const override { return EGameState::Login; }
    const char* GetName() const override { return "Login"; }
    
    void OnEnter() override;
    void OnExit() override;
    void Update(f32 deltaTime) override;
    void Render() override;
    
    bool OnKeyDown(i32 key) override;
    bool OnMouseMove(f32 x, f32 y) override;
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    
    // Actions
    void OnLoginClicked();
    void OnRegisterClicked();
    void OnGuestClicked();
    
private:
    void CreateUI();
    void DestroyUI();
    void SetupAuthCallbacks();
    void ShowError(const std::string& message);
    void ClearError();
    
    struct LoginUI;
    std::unique_ptr<LoginUI> m_ui;
    std::unique_ptr<auth::AuthClient> m_authClient;
    
    // Input state
    std::string m_username;
    std::string m_password;
    bool m_isRegistering = false;
    std::string m_confirmPassword;
    
    // Deferred UI rebuild (to avoid destroying UI from within callback)
    bool m_needsUIRebuild = false;
};

// ============ Main Menu State ============

class MainMenuState : public IGameState {
public:
    MainMenuState();
    ~MainMenuState() override;
    
    EGameState GetType() const override { return EGameState::MainMenu; }
    const char* GetName() const override { return "MainMenu"; }
    
    void OnEnter() override;
    void OnExit() override;
    void Update(f32 deltaTime) override;
    void Render() override;
    
    bool OnKeyDown(i32 key) override;
    bool OnMouseMove(f32 x, f32 y) override;
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    
    // Menu actions
    void OnPlayClicked();
    void OnSettingsClicked();
    void OnExitClicked();
    
private:
    void CreateUI();
    void DestroyUI();
    
    // Reconnect functionality
    void CheckForActiveGame();
    void SetupMatchmakingCallbacks();
    void SetupReconnectCallbacks();
    void OnReconnectClicked();
    void OnAbandonClicked();
    
    // UI panels (modular components)
    struct MenuUI;
    std::unique_ptr<MenuUI> m_ui;

    // Matchmaking (Dota-like; no manual server selection)
    std::unique_ptr<WorldEditor::Matchmaking::MatchmakingClient> m_mmClient;
};

// ============ Loading State ============

class HeroesState : public IGameState {
public:
    HeroesState();
    ~HeroesState() override;
    
    EGameState GetType() const override { return EGameState::Heroes; }
    const char* GetName() const override { return "Heroes"; }
    
    void OnEnter() override;
    void OnExit() override;
    void Update(f32 deltaTime) override;
    void Render() override;
    
    bool OnKeyDown(i32 key) override;
    bool OnMouseMove(f32 x, f32 y) override;
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    
    // Hero actions
    void OnHeroSelected(const std::string& heroId);
    void OnBackClicked();
    
private:
    void CreateUI();
    void DestroyUI();
    
    struct HeroesUI;
    std::unique_ptr<HeroesUI> m_ui;
    std::string m_selectedHero;
};

// ============ Hero Pick State (In-Match) ============

class HeroPickState : public IGameState {
public:
    HeroPickState();
    ~HeroPickState() override;
    
    EGameState GetType() const override { return EGameState::HeroPick; }
    const char* GetName() const override { return "HeroPick"; }
    
    void OnEnter() override;
    void OnExit() override;
    void Update(f32 deltaTime) override;
    void Render() override;
    
    bool OnKeyDown(i32 key) override;
    bool OnMouseMove(f32 x, f32 y) override;
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    
    // Hero pick actions
    void OnHeroClicked(const std::string& heroId);
    void OnConfirmPick();
    void OnRandomHero();
    
    // Set worlds from LoadingState
    void SetWorlds(std::unique_ptr<WorldEditor::ClientWorld> client,
                   std::unique_ptr<WorldEditor::ServerWorld> server);
    
private:
    // Helper to get shared NetworkClient from GameStateManager
    WorldEditor::Network::NetworkClient* GetNetworkClient();
    
    void CreateUI();
    void DestroyUI();
    void UpdateTimer();
    void SetupNetworkCallbacks();
    void UpdatePlayerSlot(u8 teamSlot, const std::string& heroName, bool confirmed);
    void TransitionToGame();
    
    struct HeroPickUI;
    std::unique_ptr<HeroPickUI> m_ui;
    
    std::string m_selectedHero;
    std::string m_confirmedHero;
    f32 m_pickTimer = 30.0f;
    bool m_hasPicked = false;
    bool m_allPicked = false;
    f32 m_gameStartDelay = 0.0f;
    u8 m_myTeamSlot = 0;
    
    // Other players' picks
    struct PlayerPick {
        std::string heroName;
        u8 teamSlot;
        bool confirmed;
    };
    std::unordered_map<u64, PlayerPick> m_playerPicks;
    
    // Game worlds (passed from LoadingState)
    std::unique_ptr<WorldEditor::ClientWorld> m_clientWorld;
    std::unique_ptr<WorldEditor::ServerWorld> m_serverWorld;
};

// ============ Loading State ============

class LoadingState : public IGameState {
public:
    LoadingState();
    ~LoadingState() override;
    
    EGameState GetType() const override { return EGameState::Loading; }
    const char* GetName() const override { return "Loading"; }
    
    void OnEnter() override;
    void OnExit() override;
    void Update(f32 deltaTime) override;
    void Render() override;
    
    // Loading control
    void SetLoadingTarget(const std::string& mapName);
    void SetServerTarget(const std::string& serverIp, u16 serverPort);
    void SetProgress(f32 progress);
    void SetStatusText(const std::string& text);
    void SetReconnect(bool isReconnect) { m_isReconnect = isReconnect; }
    bool IsLoadingComplete() const { return m_progress >= 1.0f; }
    
    // Get loaded worlds (transfers ownership to InGameState)
    std::unique_ptr<WorldEditor::ClientWorld> TakeClientWorld() { return std::move(m_clientWorld); }
    std::unique_ptr<WorldEditor::ServerWorld> TakeServerWorld() { return std::move(m_serverWorld); }
    std::unique_ptr<WorldEditor::World> TakeGameWorld() { return std::move(m_gameWorld); }
    
private:
    void CreateUI();
    void DestroyUI();
    void LoadGameWorld();
    void CreateTower(const Vec3& pos, int team, const std::string& name);
    void CreateCreep(const Vec3& pos, int team, const std::string& name);
    
    std::string m_mapName;
    std::string m_statusText;
    f32 m_progress = 0.0f;

    // Network target for this match (set by matchmaking)
    std::string m_serverIp = "127.0.0.1";
    u16 m_serverPort = 27015;
    
    // Game worlds
    std::unique_ptr<WorldEditor::ClientWorld> m_clientWorld;
    std::unique_ptr<WorldEditor::ServerWorld> m_serverWorld;
    std::unique_ptr<WorldEditor::World> m_gameWorld;  // Static map data for rendering
    bool m_worldsLoaded = false;
    bool m_isReconnect = false;  // True if reconnecting to existing game
    
    struct LoadingUI;
    std::unique_ptr<LoadingUI> m_ui;
};

// ============ In-Game State ============

class InGameState : public IGameState {
public:
    InGameState();
    ~InGameState() override;
    
    EGameState GetType() const override { return EGameState::InGame; }
    const char* GetName() const override { return "InGame"; }
    
    void OnEnter() override;
    void OnExit() override;
    void OnPause() override;
    void OnResume() override;
    void Update(f32 deltaTime) override;
    void Render() override;
    
    bool OnKeyDown(i32 key) override;
    bool OnKeyUp(i32 key) override;
    bool OnMouseMove(f32 x, f32 y) override;
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    bool OnMouseWheel(f32 delta) override;
    
    // Game actions
    void OnEscapePressed();
    void OnDisconnect();
    
    // Set game worlds from LoadingState
    void SetWorlds(std::unique_ptr<::WorldEditor::ClientWorld> client,
                   std::unique_ptr<::WorldEditor::ServerWorld> server);
    void SetWorlds(std::unique_ptr<::WorldEditor::ClientWorld> client,
                   std::unique_ptr<::WorldEditor::ServerWorld> server,
                   std::unique_ptr<::WorldEditor::World> gameWorld);
    
    // Access to gameplay controller (for UI components)
    ::WorldEditor::GameplayController* GetGameplayController() { return m_gameplayController.get(); }
    
private:
    // Helper to get shared NetworkClient from GameStateManager
    ::WorldEditor::Network::NetworkClient* GetNetworkClient();
    
    void CreateHUD();
    void DestroyHUD();
    void RenderWorld();
    void RenderHUD();
    void RenderHealthBars();
    void RenderTopBar();
    void UpdateHUDFromGameState();
    void SetupNetworkCallbacks();
    void UpdateInputState();
    
    // Network
    void UpdateNetwork(f32 deltaTime);
    void SendInputToServer();
    void ProcessServerSnapshot();
    
    bool m_isPaused = false;
    
    // Game worlds
    std::unique_ptr<::WorldEditor::ClientWorld> m_clientWorld;
    std::unique_ptr<::WorldEditor::ServerWorld> m_serverWorld;
    std::unique_ptr<::WorldEditor::World> m_gameWorld;  // Static map for rendering
    
    // Gameplay controller (shared logic with editor)
    std::unique_ptr<::WorldEditor::GameplayController> m_gameplayController;
    
    // Input state for GameplayController
    ::WorldEditor::GameplayInput m_currentInput;
    
    // Network input state
    f32 m_lastInputSendTime = 0.0f;
    u32 m_inputSequence = 0;
    
    struct GameHUD;
    std::unique_ptr<GameHUD> m_hud;
};

// ============ Game State Manager ============

class GameStateManager {
public:
    static GameStateManager& Instance();
    
    // Initialization
    void Initialize();
    void Shutdown();
    
    // State transitions
    void ChangeState(EGameState newState);
    void PushState(EGameState state);
    void PopState();
    
    // Current state
    IGameState* GetCurrentState() const;
    EGameState GetCurrentStateType() const;
    bool IsInState(EGameState state) const;
    
    // Frame update
    void Update(f32 deltaTime);
    void Render();
    
    // Input forwarding
    void OnKeyDown(i32 key);
    void OnKeyUp(i32 key);
    void OnMouseMove(f32 x, f32 y);
    void OnMouseDown(f32 x, f32 y, i32 button);
    void OnMouseUp(f32 x, f32 y, i32 button);
    
    // Transition callbacks
    using StateChangeCallback = std::function<void(EGameState oldState, EGameState newState)>;
    void AddStateChangeListener(StateChangeCallback callback);
    
    // Quick access to states
    LoginState* GetLoginState();
    MainMenuState* GetMainMenuState();
    HeroesState* GetHeroesState();
    HeroPickState* GetHeroPickState();
    LoadingState* GetLoadingState();
    InGameState* GetInGameState();
    
    // Global AuthClient (shared across states)
    auth::AuthClient* GetAuthClient() { return m_authClient.get(); }
    
    // Global NetworkClient (shared across states - single connection to game server)
    ::WorldEditor::Network::NetworkClient* GetNetworkClient() { return m_networkClient.get(); }
    bool ConnectToGameServer(const std::string& ip, u16 port, const std::string& username);
    void DisconnectFromGameServer();
    bool IsConnectedToGameServer() const;
    
    // Game server info (set by matchmaking, used by states)
    void SetGameServerTarget(const std::string& ip, u16 port) { m_gameServerIp = ip; m_gameServerPort = port; }
    void GetGameServerTarget(std::string& ip, u16& port) const { ip = m_gameServerIp; port = m_gameServerPort; }
    const std::string& GetGameServerIp() const { return m_gameServerIp; }
    u16 GetGameServerPort() const { return m_gameServerPort; }
    
    // Player team info (set by HeroPickState, used by InGameState)
    void SetPlayerTeam(u8 teamSlot) { m_playerTeamSlot = teamSlot; }
    u8 GetPlayerTeamSlot() const { return m_playerTeamSlot; }
    bool IsPlayerRadiant() const { return m_playerTeamSlot < 5; }  // Slots 0-4 = Radiant, 5-9 = Dire
    
    // Game in progress flag (for "Return to Game" button in MainMenu)
    void SetGameInProgress(bool inProgress) { m_gameInProgress = inProgress; }
    bool IsGameInProgress() const { return m_gameInProgress; }
    
private:
    GameStateManager() = default;
    
    std::unique_ptr<IGameState> CreateState(EGameState type);
    void NotifyStateChange(EGameState oldState, EGameState newState);
    
    std::vector<IGameState*> m_stateStack;  // Non-owning pointers to pre-created states
    std::vector<StateChangeCallback> m_stateChangeListeners;
    
    // Pre-created states for quick switching
    std::unique_ptr<LoginState> m_loginState;
    std::unique_ptr<MainMenuState> m_mainMenuState;
    std::unique_ptr<HeroesState> m_heroesState;
    std::unique_ptr<HeroPickState> m_heroPickState;
    std::unique_ptr<LoadingState> m_loadingState;
    std::unique_ptr<InGameState> m_inGameState;
    
    // Global AuthClient (shared across all states)
    std::unique_ptr<auth::AuthClient> m_authClient;
    
    // Global NetworkClient (shared across all states - single connection to game server)
    std::unique_ptr<::WorldEditor::Network::NetworkClient> m_networkClient;
    std::string m_gameServerIp;
    u16 m_gameServerPort = 0;
    u8 m_playerTeamSlot = 0;  // 0-4 = Radiant, 5-9 = Dire
    bool m_gameInProgress = false;  // True when in active game (for "Return to Game" button)
};

// ============ Convenience Functions ============

inline GameStateManager& GetGameStateManager() {
    return GameStateManager::Instance();
}

inline void ChangeGameState(EGameState state) {
    GameStateManager::Instance().ChangeState(state);
}

inline EGameState GetCurrentGameState() {
    return GameStateManager::Instance().GetCurrentStateType();
}

} // namespace Game
