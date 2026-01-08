/**
 * LoginState - Authentication screen (Refactored)
 * 
 * Uses modular components: LoginHeader, LoginForm, LoginFooter
 * Requirements: 1.1, 1.3, 5.1, 5.3, 6.1, 6.3, 6.4, 6.7, 7.1-7.3
 * 
 * Keyboard shortcuts:
 *   Tab       - Next input field
 *   Shift+Tab - Previous input field  
 *   Enter     - Submit form (login)
 *   Escape    - Clear error
 */

#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/core/CUIEngine.h"
#include "../ui/panorama/core/CPanel2D.h"
#include "../ui/panorama/widgets/CLabel.h"
#include "../ui/panorama/layout/CStyleSheet.h"
#include "../ui/login/LoginHeader.h"
#include "../ui/login/LoginForm.h"
#include "../ui/login/LoginFooter.h"
#include "../../auth/AuthClient.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Game {

struct LoginState::LoginUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    
    // Modular components
    std::unique_ptr<LoginHeader> header;
    std::unique_ptr<LoginForm> form;
    std::unique_ptr<LoginFooter> footer;
    
    // Loading overlay
    std::shared_ptr<Panorama::CPanel2D> loadingOverlay;
    std::shared_ptr<Panorama::CLabel> loadingLabel;
};

LoginState::LoginState() 
    : m_ui(std::make_unique<LoginUI>())
{
}

LoginState::~LoginState() = default;

void LoginState::OnEnter() {
    LOG_INFO("LoginState::OnEnter()");
    
    // Load login stylesheet (Flexbox + Tailwind utilities)
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/login-modern.css");
    
    // Enable hot reload for rapid UI iteration (Debug only)
    #ifdef _DEBUG
    Panorama::CUIEngine::Instance().EnableHotReload(true);
    Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/login-modern.css");
    Panorama::CUIEngine::Instance().WatchStyleSheet("resources/styles/base.css");
    LOG_INFO("LoginState: Hot reload enabled for CSS files (login-modern.css)");
    #endif
    
    CreateUI();
    SetupAuthCallbacks();
    
    LOG_INFO("LoginState UI created");
    ConsoleLog("Login screen loaded");
}

void LoginState::OnExit() {
    DestroyUI();
}

void LoginState::Update(f32 deltaTime) {
    auto* authClient = m_manager->GetAuthClient();
    if (authClient) {
        authClient->Update();
    }
    
    Panorama::CUIEngine::Instance().Update(deltaTime);
}

void LoginState::Render() {
    Panorama::CUIEngine::Instance().Render();
}

void LoginState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    float sw = engine.GetScreenWidth();
    float sh = engine.GetScreenHeight();
    LOG_INFO("LoginState::CreateUI() - screen size from engine: {}x{}", sw, sh);
    
    // ROOT - full screen background (styled by #LoginRoot in CSS)
    m_ui->root = std::make_shared<Panorama::CPanel2D>("LoginRoot");
    m_ui->root->GetStyle().width = Panorama::Length::Fill();
    m_ui->root->GetStyle().height = Panorama::Length::Fill();
    // NOTE: Layout managed by CSS Flexbox (display: flex, flex-direction: column)
    uiRoot->AddChild(m_ui->root);
    
    // Create modular components
    m_ui->header = std::make_unique<LoginHeader>();
    m_ui->header->Create(m_ui->root.get(), sw, sh);
    
    m_ui->form = std::make_unique<LoginForm>();
    m_ui->form->Create(m_ui->root.get(), sw, sh);
    
    // Setup form callbacks
    m_ui->form->SetOnSubmit([this]() {
        OnLoginClicked();
    });
    
    m_ui->form->SetOnCreateAccount([this]() {
        // Transition to Register state
        m_manager->ChangeState(EGameState::Register);
    });
    
    m_ui->footer = std::make_unique<LoginFooter>();
    m_ui->footer->Create(m_ui->root.get(), sw, sh);

    // Loading overlay (styled by #LoadingOverlay in CSS)
    m_ui->loadingOverlay = std::make_shared<Panorama::CPanel2D>("LoadingOverlay");
    m_ui->loadingOverlay->GetStyle().x = Panorama::Length::Px(0);
    m_ui->loadingOverlay->GetStyle().y = Panorama::Length::Px(0);
    m_ui->loadingOverlay->GetStyle().width = Panorama::Length::Pct(100);
    m_ui->loadingOverlay->GetStyle().height = Panorama::Length::Pct(100);
    m_ui->loadingOverlay->SetVisible(false);
    m_ui->root->AddChild(m_ui->loadingOverlay);
    
    // Loading label (styled by #LoadingLabel in CSS)
    m_ui->loadingLabel = std::make_shared<Panorama::CLabel>("Connecting...", "LoadingLabel");
    m_ui->loadingLabel->GetStyle().x = Panorama::Length::Pct(42);
    m_ui->loadingLabel->GetStyle().y = Panorama::Length::Pct(48);
    m_ui->loadingOverlay->AddChild(m_ui->loadingLabel);
    
    // Set initial focus to username field
    m_ui->form->FocusUsername();
}

void LoginState::DestroyUI() {
    if (m_ui->root) {
        auto& engine = Panorama::CUIEngine::Instance();
        
        // Clear input state
        engine.ClearInputStateForSubtree(m_ui->root.get());
        
        // Destroy components
        if (m_ui->footer) m_ui->footer->Destroy();
        if (m_ui->form) m_ui->form->Destroy();
        if (m_ui->header) m_ui->header->Destroy();
        
        auto* uiRoot = engine.GetRoot();
        if (uiRoot) {
            uiRoot->RemoveChild(m_ui->root.get());
        }
        
        m_ui->loadingOverlay.reset();
        m_ui->loadingLabel.reset();
        m_ui->footer.reset();
        m_ui->form.reset();
        m_ui->header.reset();
        m_ui->root.reset();
    }
}

void LoginState::SetupAuthCallbacks() {
    auto* authClient = m_manager->GetAuthClient();
    if (!authClient) return;
    
    authClient->SetOnLoginSuccess([this](u64 accountId, const std::string& token) {
        LOG_INFO("Login successful! Account ID: {}", accountId);
        if (m_ui->loadingOverlay) m_ui->loadingOverlay->SetVisible(false);
        ConsoleLog("Login successful!");
        
        m_manager->ChangeState(EGameState::MainMenu);
    });
    
    authClient->SetOnLoginFailed([this](const std::string& error) {
        LOG_WARN("Login failed: {}", error);
        if (m_ui->loadingOverlay) m_ui->loadingOverlay->SetVisible(false);
        ShowError(error);
    });
    
    authClient->SetOnRegisterSuccess([this](u64 accountId, const std::string& token) {
        LOG_INFO("Registration successful! Account ID: {}", accountId);
        if (m_ui->loadingOverlay) m_ui->loadingOverlay->SetVisible(false);
        ConsoleLog("Account created successfully!");
        
        m_manager->ChangeState(EGameState::MainMenu);
    });
    
    authClient->SetOnRegisterFailed([this](const std::string& error) {
        LOG_WARN("Registration failed: {}", error);
        if (m_ui->loadingOverlay) m_ui->loadingOverlay->SetVisible(false);
        ShowError(error);
    });
    
    authClient->SetOnTokenValid([this](u64 accountId) {
        LOG_INFO("Stored token valid! Account ID: {}", accountId);
        ConsoleLog("Session restored!");
        
        m_manager->ChangeState(EGameState::MainMenu);
    });
    
    authClient->SetOnTokenInvalid([this]() {
        LOG_INFO("Stored token invalid or expired");
    });
}

void LoginState::ShowError(const std::string& message) {
    if (m_ui->form) {
        m_ui->form->ShowError(message);
    }
}

void LoginState::ClearError() {
    if (m_ui->form) {
        m_ui->form->ClearError();
    }
}

void LoginState::OnLoginClicked() {
    if (!m_ui->form) return;
    
    // Validate inputs using form's validation
    if (!m_ui->form->ValidateInputs()) {
        return;
    }
    
    auto* authClient = m_manager->GetAuthClient();
    if (!authClient) return;
    
    std::string username = m_ui->form->GetUsername();
    std::string password = m_ui->form->GetPassword();
    
    // Try to connect if not connected
    if (!authClient->IsConnected()) {
        if (!authClient->Connect("127.0.0.1", 27016)) {
            ShowError("Cannot connect to auth server");
            return;
        }
    }
    
    ClearError();
    if (m_ui->loadingOverlay) {
        m_ui->loadingLabel->SetText("Logging in...");
        m_ui->loadingOverlay->SetVisible(true);
    }
    
    authClient->Login(username, password);
}

void LoginState::OnRegisterClicked() {
    // Not used in refactored version - registration is in RegisterState
}

void LoginState::OnGuestClicked() {
    // Guest functionality removed per requirements
}

bool LoginState::OnKeyDown(i32 key) {
    // Tab - cycle through focusable elements
    if (key == VK_TAB) {
        if (!m_ui->form) return false;
        
        bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shiftHeld) {
            m_ui->form->FocusPrevious();
        } else {
            m_ui->form->FocusNext();
        }
        return true;
    }
    
    // Enter - submit form
    if (key == VK_RETURN) {
        OnLoginClicked();
        return true;
    }
    
    // Escape - clear error
    if (key == VK_ESCAPE) {
        ClearError();
        return true;
    }
    
    // Forward all other keys to UI engine
    Panorama::CUIEngine::Instance().OnKeyDown(key);
    return false;
}

bool LoginState::OnMouseMove(f32 x, f32 y) {
    Panorama::CUIEngine::Instance().OnMouseMove(x, y);
    return true;
}

bool LoginState::OnMouseDown(f32 x, f32 y, i32 button) {
    Panorama::CUIEngine::Instance().OnMouseDown(x, y, button);
    return true;
}

bool LoginState::OnMouseUp(f32 x, f32 y, i32 button) {
    Panorama::CUIEngine::Instance().OnMouseUp(x, y, button);
    return true;
}

void LoginState::OnResize(f32 width, f32 height) {
    LOG_INFO("LoginState::OnResize({}x{})", width, height);
    
    // Recreate UI with new screen dimensions
    LOG_INFO("LoginState::OnResize - DestroyUI...");
    DestroyUI();
    
    LOG_INFO("LoginState::OnResize - CreateUI...");
    CreateUI();
    
    LOG_INFO("LoginState::OnResize - Complete");
}

} // namespace Game
