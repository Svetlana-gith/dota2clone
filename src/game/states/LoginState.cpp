/**
 * LoginState - Authentication screen
 * 
 * Displays login/registration UI and handles authentication flow.
 * Requirements: 7.1, 7.2, 7.3, 7.4, 7.5
 * 
 * Keyboard shortcuts:
 *   Tab       - Next input field
 *   Shift+Tab - Previous input field  
 *   Enter     - Submit form (login/register)
 *   Escape    - Clear error / switch to login mode
 */

#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/core/CUIEngine.h"
#include "../ui/panorama/core/CPanel2D.h"
#include "../ui/panorama/widgets/CLabel.h"
#include "../ui/panorama/widgets/CButton.h"
#include "../ui/panorama/widgets/CTextEntry.h"
#include "../ui/panorama/layout/CStyleSheet.h"
#include "../../auth/AuthClient.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Game {

struct LoginState::LoginUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    std::shared_ptr<Panorama::CPanel2D> centerContainer;  // For vertical centering
    std::shared_ptr<Panorama::CPanel2D> loginBox;
    std::shared_ptr<Panorama::CPanel2D> formContainer;    // Flow layout for form fields
    
    // Login form
    std::shared_ptr<Panorama::CLabel> titleLabel;
    std::shared_ptr<Panorama::CTextEntry> usernameInput;
    std::shared_ptr<Panorama::CTextEntry> passwordInput;
    std::shared_ptr<Panorama::CTextEntry> confirmPasswordInput;  // For registration
    std::shared_ptr<Panorama::CButton> loginButton;
    std::shared_ptr<Panorama::CButton> guestButton;
    std::shared_ptr<Panorama::CButton> switchModeButton;
    
    // Button row container
    std::shared_ptr<Panorama::CPanel2D> buttonRow;
    
    // Status
    std::shared_ptr<Panorama::CLabel> errorLabel;
    
    // Loading indicator
    std::shared_ptr<Panorama::CPanel2D> loadingOverlay;
    std::shared_ptr<Panorama::CLabel> loadingLabel;
    
    // Focusable elements for Tab navigation
    std::vector<std::shared_ptr<Panorama::CPanel2D>> focusOrder;
};

LoginState::LoginState() 
    : m_ui(std::make_unique<LoginUI>())
{
    // AuthClient теперь глобальный в GameStateManager
}

LoginState::~LoginState() = default;

void LoginState::OnEnter() {
    LOG_INFO("LoginState::OnEnter()");
    
    // Load login stylesheet (includes base.css)
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/login.css");
    
    CreateUI();
    SetupAuthCallbacks();
    
    // Try to connect to auth server in background (non-blocking)
    // Connection will be attempted when user tries to login
    
    LOG_INFO("LoginState UI created");
    ConsoleLog("Login screen loaded");
}

void LoginState::OnExit() {
    DestroyUI();
}

void LoginState::Update(f32 deltaTime) {
    // Handle deferred UI rebuild (from switch mode button)
    if (m_needsUIRebuild) {
        m_needsUIRebuild = false;
        DestroyUI();
        CreateUI();
    }
    
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
    
    // Clear focus order
    m_ui->focusOrder.clear();
    
    // Box dimensions (from CSS: 513px width, 32px padding each side = 449px input width)
    float boxW = 513.0f;
    float inputW = 449.0f;
    
    // ROOT - full screen background (styled by #LoginRoot in CSS)
    m_ui->root = std::make_shared<Panorama::CPanel2D>("LoginRoot");
    uiRoot->AddChild(m_ui->root);
    
    // Title at top center
    auto title = std::make_shared<Panorama::CLabel>("WORLD EDITOR", "GameTitle");
    title->GetStyle().marginLeft = Panorama::Length::Px(std::round((sw - 200.0f) / 2));
    title->GetStyle().marginTop = Panorama::Length::Px(std::round(sh * 0.10f));
    m_ui->root->AddChild(title);
    
    // Login box - centered (styled by #LoginBox in CSS)
    m_ui->loginBox = std::make_shared<Panorama::CPanel2D>("LoginBox");
    m_ui->loginBox->GetStyle().marginLeft = Panorama::Length::Px(std::round((sw - boxW) / 2));
    m_ui->loginBox->GetStyle().marginTop = Panorama::Length::Px(std::round(sh * 0.22f));
    m_ui->root->AddChild(m_ui->loginBox);
    
    // Title label inside box
    std::string titleText = m_isRegistering ? "CREATE ACCOUNT" : "LOGIN";
    m_ui->titleLabel = std::make_shared<Panorama::CLabel>(titleText, "FormTitle");
    m_ui->loginBox->AddChild(m_ui->titleLabel);
    
    // Username label
    auto usernameLabel = std::make_shared<Panorama::CLabel>("Username", "UsernameLabel");
    usernameLabel->AddClass("FieldLabel");
    m_ui->loginBox->AddChild(usernameLabel);
    
    // Username input
    m_ui->usernameInput = std::make_shared<Panorama::CTextEntry>("UsernameInput");
    m_ui->usernameInput->AddClass("LoginInput");
    m_ui->usernameInput->SetText(m_username);
    m_ui->usernameInput->SetPlaceholder("Enter username");
    m_ui->usernameInput->SetOnTextChanged([this](const std::string& text) {
        m_username = text;
        ClearError();
    });
    m_ui->loginBox->AddChild(m_ui->usernameInput);
    m_ui->focusOrder.push_back(m_ui->usernameInput);
    
    // Password label
    auto passwordLabel = std::make_shared<Panorama::CLabel>("Password", "PasswordLabel");
    passwordLabel->AddClass("FieldLabelSpaced");
    m_ui->loginBox->AddChild(passwordLabel);
    
    // Password input
    m_ui->passwordInput = std::make_shared<Panorama::CTextEntry>("PasswordInput");
    m_ui->passwordInput->AddClass("LoginInput");
    m_ui->passwordInput->SetPassword(true);
    m_ui->passwordInput->SetText(m_password);
    m_ui->passwordInput->SetPlaceholder("Enter password");
    m_ui->passwordInput->SetOnTextChanged([this](const std::string& text) {
        m_password = text;
        ClearError();
    });
    m_ui->loginBox->AddChild(m_ui->passwordInput);
    m_ui->focusOrder.push_back(m_ui->passwordInput);
    
    // Confirm password (registration only)
    if (m_isRegistering) {
        auto confirmLabel = std::make_shared<Panorama::CLabel>("Confirm Password", "ConfirmLabel");
        confirmLabel->AddClass("FieldLabelSpaced");
        m_ui->loginBox->AddChild(confirmLabel);
        
        m_ui->confirmPasswordInput = std::make_shared<Panorama::CTextEntry>("ConfirmPasswordInput");
        m_ui->confirmPasswordInput->AddClass("LoginInput");
        m_ui->confirmPasswordInput->SetPassword(true);
        m_ui->confirmPasswordInput->SetText(m_confirmPassword);
        m_ui->confirmPasswordInput->SetPlaceholder("Confirm password");
        m_ui->confirmPasswordInput->SetOnTextChanged([this](const std::string& text) {
            m_confirmPassword = text;
            ClearError();
        });
        m_ui->loginBox->AddChild(m_ui->confirmPasswordInput);
        m_ui->focusOrder.push_back(m_ui->confirmPasswordInput);
    }
    
    // Error label (styled by #ErrorLabel in CSS)
    m_ui->errorLabel = std::make_shared<Panorama::CLabel>("", "ErrorLabel");
    m_ui->errorLabel->SetVisible(false);
    m_ui->loginBox->AddChild(m_ui->errorLabel);
    
    // Primary action button (styled by #LoginBtn in CSS)
    std::string btnText = m_isRegistering ? "CREATE ACCOUNT" : "LOGIN";
    m_ui->loginButton = std::make_shared<Panorama::CButton>(btnText, "LoginBtn");
    m_ui->loginButton->SetOnActivate([this]() {
        if (m_isRegistering) {
            OnRegisterClicked();
        } else {
            OnLoginClicked();
        }
    });
    m_ui->loginBox->AddChild(m_ui->loginButton);
    m_ui->focusOrder.push_back(m_ui->loginButton);
    
    // Button row (styled by #ButtonRow in CSS)
    m_ui->buttonRow = std::make_shared<Panorama::CPanel2D>("ButtonRow");
    m_ui->loginBox->AddChild(m_ui->buttonRow);
    
    // Switch mode button (styled by #SwitchBtn in CSS)
    std::string switchText = m_isRegistering ? "Back to Login" : "Create Account";
    m_ui->switchModeButton = std::make_shared<Panorama::CButton>(switchText, "SwitchBtn");
    m_ui->switchModeButton->SetOnActivate([this]() {
        m_isRegistering = !m_isRegistering;
        ClearError();
        m_needsUIRebuild = true;
    });
    m_ui->buttonRow->AddChild(m_ui->switchModeButton);
    m_ui->focusOrder.push_back(m_ui->switchModeButton);
    
    // Guest button (styled by #GuestBtn in CSS)
    m_ui->guestButton = std::make_shared<Panorama::CButton>("Play as Guest", "GuestBtn");
    m_ui->guestButton->SetOnActivate([this]() { OnGuestClicked(); });
    m_ui->buttonRow->AddChild(m_ui->guestButton);
    m_ui->focusOrder.push_back(m_ui->guestButton);
    
    // Keyboard hint (styled by #HintLabel in CSS)
    auto hintLabel = std::make_shared<Panorama::CLabel>("Tab: next | Enter: submit | Esc: back", "HintLabel");
    m_ui->loginBox->AddChild(hintLabel);
    
    // Loading overlay (styled by #LoadingOverlay in CSS)
    m_ui->loadingOverlay = std::make_shared<Panorama::CPanel2D>("LoadingOverlay");
    m_ui->loadingOverlay->SetVisible(false);
    m_ui->root->AddChild(m_ui->loadingOverlay);
    
    // Loading label (styled by #LoadingLabel in CSS)
    m_ui->loadingLabel = std::make_shared<Panorama::CLabel>("Connecting...", "LoadingLabel");
    m_ui->loadingLabel->GetStyle().marginLeft = Panorama::Length::Px(std::round((sw - 150.0f) / 2));
    m_ui->loadingLabel->GetStyle().marginTop = Panorama::Length::Px(std::round(sh / 2));
    m_ui->loadingOverlay->AddChild(m_ui->loadingLabel);
    
    // Set initial focus to username field
    if (m_ui->usernameInput) {
        m_ui->usernameInput->SetFocus();
    }
}

void LoginState::DestroyUI() {
    if (m_ui->root) {
        auto& engine = Panorama::CUIEngine::Instance();
        
        // Clear input state only if it points into the subtree we're about to destroy
        engine.ClearInputStateForSubtree(m_ui->root.get());
        
        auto* uiRoot = engine.GetRoot();
        if (uiRoot) {
            uiRoot->RemoveChild(m_ui->root.get());
        }
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
        
        // Transition to main menu
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
        
        // Auto-login after registration - transition to main menu
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
        
        // Already authenticated, go to main menu
        m_manager->ChangeState(EGameState::MainMenu);
    });
    
    authClient->SetOnTokenInvalid([this]() {
        LOG_INFO("Stored token invalid or expired");
        // Stay on login screen
    });
}

void LoginState::ShowError(const std::string& message) {
    if (m_ui->errorLabel) {
        m_ui->errorLabel->SetText(message);
        m_ui->errorLabel->SetVisible(true);
    }
}

void LoginState::ClearError() {
    if (m_ui->errorLabel) {
        m_ui->errorLabel->SetVisible(false);
    }
}

void LoginState::OnLoginClicked() {
    auto* authClient = m_manager->GetAuthClient();
    if (!authClient) return;
    
    // Validate input
    if (m_username.empty()) {
        ShowError("Please enter username");
        return;
    }
    if (m_password.empty()) {
        ShowError("Please enter password");
        return;
    }
    
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
    
    authClient->Login(m_username, m_password);
}

void LoginState::OnRegisterClicked() {
    auto* authClient = m_manager->GetAuthClient();
    if (!authClient) return;
    
    // Validate input
    if (m_username.length() < 3 || m_username.length() > 20) {
        ShowError("Username must be 3-20 characters");
        return;
    }
    
    // Check alphanumeric
    for (char c : m_username) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            ShowError("Username must be alphanumeric");
            return;
        }
    }
    
    if (m_password.length() < 8) {
        ShowError("Password must be at least 8 characters");
        return;
    }
    
    if (m_password != m_confirmPassword) {
        ShowError("Passwords do not match");
        return;
    }
    
    // Try to connect if not connected
    if (!authClient->IsConnected()) {
        if (!authClient->Connect("127.0.0.1", 27016)) {
            ShowError("Cannot connect to auth server");
            return;
        }
    }
    
    ClearError();
    if (m_ui->loadingOverlay) {
        m_ui->loadingLabel->SetText("Creating account...");
        m_ui->loadingOverlay->SetVisible(true);
    }
    
    authClient->Register(m_username, m_password);
}

void LoginState::OnGuestClicked() {
    auto* authClient = m_manager->GetAuthClient();
    if (!authClient) return;
    
    LOG_INFO("Playing as guest");
    ConsoleLog("Playing as guest");
    
    u64 guestId = authClient->CreateGuestAccount();
    LOG_INFO("Guest account ID: {}", guestId);
    
    // Transition to main menu
    m_manager->ChangeState(EGameState::MainMenu);
}

bool LoginState::OnKeyDown(i32 key) {
    auto& engine = Panorama::CUIEngine::Instance();
    
    // Tab - cycle through focusable elements
    if (key == VK_TAB) {
        if (m_ui->focusOrder.empty()) return false;
        
        // Find current focused element
        auto* focused = engine.GetFocusedPanel();
        i32 currentIdx = -1;
        for (size_t i = 0; i < m_ui->focusOrder.size(); ++i) {
            if (m_ui->focusOrder[i].get() == focused) {
                currentIdx = static_cast<i32>(i);
                break;
            }
        }
        
        // Check if Shift is held (reverse direction)
        bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        
        i32 nextIdx;
        if (shiftHeld) {
            nextIdx = (currentIdx <= 0) ? static_cast<i32>(m_ui->focusOrder.size()) - 1 : currentIdx - 1;
        } else {
            nextIdx = (currentIdx < 0 || currentIdx >= static_cast<i32>(m_ui->focusOrder.size()) - 1) ? 0 : currentIdx + 1;
        }
        
        m_ui->focusOrder[nextIdx]->SetFocus();
        return true;
    }
    
    // Enter - submit form
    if (key == VK_RETURN) {
        auto* focused = engine.GetFocusedPanel();
        bool isTextEntry = focused && focused->GetPanelType() == Panorama::PanelType::TextEntry;
        
        // If text entry is focused, let UI handle it first, then submit
        if (isTextEntry) {
            engine.OnKeyDown(key);
        }
        
        // Submit form
        if (m_isRegistering) {
            OnRegisterClicked();
        } else {
            OnLoginClicked();
        }
        return true;
    }
    
    // Escape - clear error or switch back to login mode
    if (key == VK_ESCAPE) {
        if (m_ui->errorLabel && m_ui->errorLabel->IsVisible()) {
            ClearError();
        } else if (m_isRegistering) {
            m_isRegistering = false;
            ClearError();
            m_needsUIRebuild = true;
        }
        return true;
    }
    
    // Forward all other keys to UI engine
    engine.OnKeyDown(key);
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

} // namespace Game
