#include "GameState.h"
#include "DebugConsole.h"
#include "ui/panorama/CUIEngine.h"
#include "ui/panorama/CPanel2D.h"
#include "auth/AuthClient.h"
#include "network/NetworkClient.h"
#include <algorithm>

namespace Game {

// ============ GameStateManager Implementation ============

GameStateManager& GameStateManager::Instance() {
    static GameStateManager instance;
    return instance;
}

void GameStateManager::Initialize() {
    LOG_INFO("GameStateManager::Initialize()");
    
    // Create global AuthClient
    m_authClient = std::make_unique<auth::AuthClient>();
    LOG_INFO("AuthClient created");
    
    // Pre-create all states
    m_loginState = std::make_unique<LoginState>();
    m_loginState->m_manager = this;
    LOG_INFO("LoginState created");
    
    m_mainMenuState = std::make_unique<MainMenuState>();
    m_mainMenuState->m_manager = this;
    LOG_INFO("MainMenuState created");
    
    m_heroesState = std::make_unique<HeroesState>();
    m_heroesState->m_manager = this;
    LOG_INFO("HeroesState created");
    
    m_heroPickState = std::make_unique<HeroPickState>();
    m_heroPickState->m_manager = this;
    LOG_INFO("HeroPickState created");
    
    m_loadingState = std::make_unique<LoadingState>();
    m_loadingState->m_manager = this;
    LOG_INFO("LoadingState created");
    
    m_inGameState = std::make_unique<InGameState>();
    m_inGameState->m_manager = this;
    LOG_INFO("InGameState created");
    
    // Start with login screen
    LOG_INFO("Changing to Login state");
    ChangeState(EGameState::Login);
    LOG_INFO("GameStateManager initialized");
}

void GameStateManager::Shutdown() {
    // Exit all states
    while (!m_stateStack.empty()) {
        m_stateStack.back()->OnExit();
        m_stateStack.pop_back();
    }
    
    // Delete pre-created states
    m_loginState.reset();
    m_mainMenuState.reset();
    m_heroesState.reset();
    m_heroPickState.reset();
    m_loadingState.reset();
    m_inGameState.reset();
}

std::unique_ptr<IGameState> GameStateManager::CreateState(EGameState type) {
    // Return reference to pre-created state wrapped in unique_ptr
    // Note: In real implementation, we'd manage ownership differently
    switch (type) {
        case EGameState::Login: {
            auto state = std::make_unique<LoginState>();
            state->m_manager = this;
            return state;
        }
        case EGameState::MainMenu: {
            auto state = std::make_unique<MainMenuState>();
            state->m_manager = this;
            return state;
        }
        case EGameState::Heroes: {
            auto state = std::make_unique<HeroesState>();
            state->m_manager = this;
            return state;
        }
        case EGameState::HeroPick: {
            auto state = std::make_unique<HeroPickState>();
            state->m_manager = this;
            return state;
        }
        case EGameState::Loading: {
            auto state = std::make_unique<LoadingState>();
            state->m_manager = this;
            return state;
        }
        case EGameState::InGame: {
            auto state = std::make_unique<InGameState>();
            state->m_manager = this;
            return state;
        }
        default:
            return nullptr;
    }
}

void GameStateManager::ChangeState(EGameState newState) {
    EGameState oldState = GetCurrentStateType();
    
    ConsoleLog("ChangeState: " + std::string(GetCurrentState() ? GetCurrentState()->GetName() : "None") + 
               " -> " + std::to_string((int)newState));
    
    // Get new state first
    IGameState* state = nullptr;
    switch (newState) {
        case EGameState::Login:
            state = m_loginState.get();
            break;
        case EGameState::MainMenu:
            state = m_mainMenuState.get();
            break;
        case EGameState::Heroes:
            state = m_heroesState.get();
            break;
        case EGameState::HeroPick:
            state = m_heroPickState.get();
            break;
        case EGameState::Loading:
            state = m_loadingState.get();
            break;
        case EGameState::InGame:
            state = m_inGameState.get();
            break;
        default:
            ConsoleLog("ERROR: Unknown state!");
            return;
    }
    
    if (!state) {
        ConsoleLog("ERROR: Failed to get state!");
        return;
    }
    
    // Exit current state (if different from new state)
    if (!m_stateStack.empty() && m_stateStack.back() != state) {
        m_stateStack.back()->OnExit();
    }
    m_stateStack.clear();
    
    // Enter new state
    state->OnEnter();
    m_stateStack.push_back(state);
    ConsoleLog("State changed successfully");
    
    NotifyStateChange(oldState, newState);
}

void GameStateManager::PushState(EGameState state) {
    EGameState oldState = GetCurrentStateType();
    
    // Pause current state
    if (!m_stateStack.empty()) {
        m_stateStack.back()->OnPause();
    }
    
    // Get pre-created state
    IGameState* newState = nullptr;
    switch (state) {
        case EGameState::Login: newState = m_loginState.get(); break;
        case EGameState::MainMenu: newState = m_mainMenuState.get(); break;
        case EGameState::Heroes: newState = m_heroesState.get(); break;
        case EGameState::HeroPick: newState = m_heroPickState.get(); break;
        case EGameState::Loading: newState = m_loadingState.get(); break;
        case EGameState::InGame: newState = m_inGameState.get(); break;
        default: return;
    }
    
    if (newState) {
        newState->OnEnter();
        m_stateStack.push_back(newState);
    }
    
    NotifyStateChange(oldState, state);
}

void GameStateManager::PopState() {
    if (m_stateStack.empty()) return;
    
    EGameState oldState = GetCurrentStateType();
    
    // Exit current state
    m_stateStack.back()->OnExit();
    m_stateStack.pop_back();
    
    // Resume previous state
    if (!m_stateStack.empty()) {
        m_stateStack.back()->OnResume();
    }
    
    NotifyStateChange(oldState, GetCurrentStateType());
}

IGameState* GameStateManager::GetCurrentState() const {
    return m_stateStack.empty() ? nullptr : m_stateStack.back();
}

EGameState GameStateManager::GetCurrentStateType() const {
    return m_stateStack.empty() ? EGameState::None : m_stateStack.back()->GetType();
}

bool GameStateManager::IsInState(EGameState state) const {
    return GetCurrentStateType() == state;
}

void GameStateManager::Update(f32 deltaTime) {
    if (!m_stateStack.empty()) {
        m_stateStack.back()->Update(deltaTime);
    }
}

void GameStateManager::Render() {
    // Render all states from bottom to top (for overlay effects)
    for (auto& state : m_stateStack) {
        state->Render();
    }
}

void GameStateManager::OnKeyDown(i32 key) {
    // Console gets priority for key events
    if (DebugConsole::Instance().IsVisible()) {
        if (DebugConsole::Instance().OnKeyDown(key)) {
            return; // Console handled it
        }
    }
    
    if (!m_stateStack.empty()) {
        m_stateStack.back()->OnKeyDown(key);
    }
}

void GameStateManager::OnKeyUp(i32 key) {
    if (!m_stateStack.empty()) {
        m_stateStack.back()->OnKeyUp(key);
    }
}

void GameStateManager::OnMouseMove(f32 x, f32 y) {
    // Console gets priority for mouse events when visible
    if (DebugConsole::Instance().IsVisible()) {
        if (DebugConsole::Instance().OnMouseMove(x, y)) {
            return; // Console handled it
        }
    }
    
    if (!m_stateStack.empty()) {
        m_stateStack.back()->OnMouseMove(x, y);
    }
}

void GameStateManager::OnMouseDown(f32 x, f32 y, i32 button) {
    // Console gets priority for mouse events when visible
    if (DebugConsole::Instance().IsVisible()) {
        if (DebugConsole::Instance().OnMouseDown(x, y)) {
            return; // Console handled it
        }
    }
    
    if (!m_stateStack.empty()) {
        m_stateStack.back()->OnMouseDown(x, y, button);
    }
}

void GameStateManager::OnMouseUp(f32 x, f32 y, i32 button) {
    // Console gets priority for mouse events when visible
    if (DebugConsole::Instance().IsVisible()) {
        if (DebugConsole::Instance().OnMouseUp(x, y)) {
            return; // Console handled it
        }
    }
    
    if (!m_stateStack.empty()) {
        m_stateStack.back()->OnMouseUp(x, y, button);
    }
}

void GameStateManager::AddStateChangeListener(StateChangeCallback callback) {
    m_stateChangeListeners.push_back(callback);
}

void GameStateManager::NotifyStateChange(EGameState oldState, EGameState newState) {
    for (auto& listener : m_stateChangeListeners) {
        listener(oldState, newState);
    }
}

MainMenuState* GameStateManager::GetMainMenuState() {
    return m_mainMenuState.get();
}

LoginState* GameStateManager::GetLoginState() {
    return m_loginState.get();
}

HeroesState* GameStateManager::GetHeroesState() {
    return m_heroesState.get();
}

HeroPickState* GameStateManager::GetHeroPickState() {
    return m_heroPickState.get();
}

LoadingState* GameStateManager::GetLoadingState() {
    return m_loadingState.get();
}

InGameState* GameStateManager::GetInGameState() {
    return m_inGameState.get();
}

// ============ Network Client Management ============

bool GameStateManager::ConnectToGameServer(const std::string& ip, u16 port, const std::string& username) {
    LOG_INFO("GameStateManager::ConnectToGameServer({}:{}, {})", ip, port, username);
    
    // Create NetworkClient if not exists
    if (!m_networkClient) {
        m_networkClient = std::make_unique<WorldEditor::Network::NetworkClient>();
    }
    
    // Disconnect if already connected to different server
    if (m_networkClient->isConnected()) {
        LOG_INFO("Already connected, disconnecting first...");
        m_networkClient->disconnect();
    }
    
    // Store server info
    m_gameServerIp = ip;
    m_gameServerPort = port;
    
    // Set username before connecting
    m_networkClient->setUsername(username);
    
    // Set accountId from AuthClient for reconnect support
    if (m_authClient && m_authClient->IsAuthenticated()) {
        m_networkClient->setAccountId(m_authClient->GetAccountId());
        LOG_INFO("Set accountId {} for network connection", m_authClient->GetAccountId());
    }
    
    // Connect
    if (m_networkClient->connect(ip.c_str(), port)) {
        LOG_INFO("Connection initiated to {}:{}", ip, port);
        return true;
    }
    
    LOG_ERROR("Failed to connect to {}:{}", ip, port);
    return false;
}

void GameStateManager::DisconnectFromGameServer() {
    if (m_networkClient && m_networkClient->isConnected()) {
        LOG_INFO("Disconnecting from game server");
        m_networkClient->disconnect();
    }
}

bool GameStateManager::IsConnectedToGameServer() const {
    return m_networkClient && m_networkClient->isConnected();
}

} // namespace Game
