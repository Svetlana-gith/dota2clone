/**
 * LoginState - Authentication screen
 * 
 * Displays login/registration UI and handles authentication flow.
 * Requirements: 7.1, 7.2, 7.3, 7.4, 7.5
 */

#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/CUIEngine.h"
#include "../ui/panorama/CPanel2D.h"
#include "../ui/panorama/CStyleSheet.h"
#include "auth/AuthClient.h"

namespace Game {

struct LoginState::LoginUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    std::shared_ptr<Panorama::CPanel2D> loginBox;
    
    // Login form
    std::shared_ptr<Panorama::CLabel> titleLabel;
    std::shared_ptr<Panorama::CTextEntry> usernameInput;
    std::shared_ptr<Panorama::CTextEntry> passwordInput;
    std::shared_ptr<Panorama::CTextEntry> confirmPasswordInput;  // For registration
    std::shared_ptr<Panorama::CButton> loginButton;
    std::shared_ptr<Panorama::CButton> registerButton;
    std::shared_ptr<Panorama::CButton> guestButton;
    std::shared_ptr<Panorama::CButton> switchModeButton;
    
    // Status
    std::shared_ptr<Panorama::CLabel> errorLabel;
    std::shared_ptr<Panorama::CLabel> statusLabel;
    
    // Loading indicator
    std::shared_ptr<Panorama::CPanel2D> loadingOverlay;
    std::shared_ptr<Panorama::CLabel> loadingLabel;
};

LoginState::LoginState() 
    : m_ui(std::make_unique<LoginUI>())
{
    // AuthClient теперь глобальный в GameStateManager
}

LoginState::~LoginState() = default;

void LoginState::OnEnter() {
    LOG_INFO("LoginState::OnEnter()");
    
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
    auto* authClient = m_manager->GetAuthClient();
    if (authClient) {
        authClient->Update();
    }
    
    Panorama::CUIEngine::Instance().Update(deltaTime);
}

void LoginState::Render() {
    Panorama::CUIEngine::Instance().Render();
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

static std::shared_ptr<Panorama::CLabel> L(const std::string& text, float size, Panorama::Color col) {
    auto l = std::make_shared<Panorama::CLabel>(text, text);
    l->GetStyle().fontSize = S(size);
    l->GetStyle().color = col;
    return l;
}

void LoginState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    float sw = engine.GetScreenWidth();
    float sh = engine.GetScreenHeight();
    
    // Colors - same as MainMenuState
    Panorama::Color bg(0.02f, 0.04f, 0.08f, 1.0f);
    Panorama::Color panel(0.08f, 0.09f, 0.11f, 0.95f);
    Panorama::Color inputBg(0.12f, 0.13f, 0.15f, 1.0f);
    Panorama::Color greenBtn(0.18f, 0.45f, 0.18f, 1.0f);
    Panorama::Color blueBtn(0.18f, 0.35f, 0.55f, 1.0f);
    Panorama::Color grayBtn(0.25f, 0.25f, 0.28f, 1.0f);
    Panorama::Color white(1.0f, 1.0f, 1.0f, 1.0f);
    Panorama::Color gray(0.6f, 0.6f, 0.6f, 1.0f);
    Panorama::Color red(0.85f, 0.25f, 0.25f, 1.0f);
    Panorama::Color none(0, 0, 0, 0);
    
    // ROOT - full screen background
    m_ui->root = P("LoginRoot", 0, 0, bg);
    uiRoot->AddChild(m_ui->root);
    
    // Title at top
    auto title = L("WORLD EDITOR", 28, white);
    title->GetStyle().marginLeft = Panorama::Length::Px((sw - S(200)) / 2);
    title->GetStyle().marginTop = Panorama::Length::Px(sh * 0.15f);
    m_ui->root->AddChild(title);
    
    // Login box (centered)
    float boxW = 380;
    float boxH = m_isRegistering ? 400 : 322;
    m_ui->loginBox = P("LoginBox", boxW, boxH, panel);
    m_ui->loginBox->GetStyle().borderRadius = S(6);
    m_ui->loginBox->GetStyle().marginLeft = Panorama::Length::Px((sw - S(boxW)) / 2);
    m_ui->loginBox->GetStyle().marginTop = Panorama::Length::Px(sh * 0.3f);
    m_ui->root->AddChild(m_ui->loginBox);
    
    // Title label inside box
    std::string titleText = m_isRegistering ? "CREATE ACCOUNT" : "LOGIN";
    m_ui->titleLabel = L(titleText, 16, white);
    m_ui->titleLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->titleLabel->GetStyle().marginTop = Panorama::Length::Px(S(20));
    m_ui->loginBox->AddChild(m_ui->titleLabel);
    
    // Username label
    auto usernameLabel = L("Username", 10, gray);
    usernameLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    usernameLabel->GetStyle().marginTop = Panorama::Length::Px(S(55));
    m_ui->loginBox->AddChild(usernameLabel);
    
    // Username input
    m_ui->usernameInput = std::make_shared<Panorama::CTextEntry>("UsernameInput");
    m_ui->usernameInput->GetStyle().width = Panorama::Length::Px(S(340));
    m_ui->usernameInput->GetStyle().height = Panorama::Length::Px(S(32));
    m_ui->usernameInput->GetStyle().backgroundColor = inputBg;
    m_ui->usernameInput->GetStyle().borderRadius = S(4);
    m_ui->usernameInput->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->usernameInput->GetStyle().marginTop = Panorama::Length::Px(S(92));
    m_ui->usernameInput->GetStyle().fontSize = S(12);
    m_ui->usernameInput->GetStyle().color = white;
    m_ui->usernameInput->SetText(m_username);
    m_ui->usernameInput->SetOnTextChanged([this](const std::string& text) {
        m_username = text;
        ClearError();
    });
    m_ui->loginBox->AddChild(m_ui->usernameInput);
    
    // Password label
    auto passwordLabel = L("Password", 10, gray);
    passwordLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    passwordLabel->GetStyle().marginTop = Panorama::Length::Px(S(127));
    m_ui->loginBox->AddChild(passwordLabel);
    
    // Password input
    m_ui->passwordInput = std::make_shared<Panorama::CTextEntry>("PasswordInput");
    m_ui->passwordInput->GetStyle().width = Panorama::Length::Px(S(340));
    m_ui->passwordInput->GetStyle().height = Panorama::Length::Px(S(32));
    m_ui->passwordInput->GetStyle().backgroundColor = inputBg;
    m_ui->passwordInput->GetStyle().borderRadius = S(4);
    m_ui->passwordInput->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->passwordInput->GetStyle().marginTop = Panorama::Length::Px(S(164));
    m_ui->passwordInput->GetStyle().fontSize = S(12);
    m_ui->passwordInput->GetStyle().color = white;
    m_ui->passwordInput->SetPasswordMode(true);
    m_ui->passwordInput->SetText(m_password);
    m_ui->passwordInput->SetOnTextChanged([this](const std::string& text) {
        m_password = text;
        ClearError();
    });
    m_ui->loginBox->AddChild(m_ui->passwordInput);
    
    float buttonY = 201;
    
    // Confirm password (registration only)
    if (m_isRegistering) {
        auto confirmLabel = L("Confirm Password", 10, gray);
        confirmLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
        confirmLabel->GetStyle().marginTop = Panorama::Length::Px(S(201));
        m_ui->loginBox->AddChild(confirmLabel);
        
        m_ui->confirmPasswordInput = std::make_shared<Panorama::CTextEntry>("ConfirmPasswordInput");
        m_ui->confirmPasswordInput->GetStyle().width = Panorama::Length::Px(S(340));
        m_ui->confirmPasswordInput->GetStyle().height = Panorama::Length::Px(S(32));
        m_ui->confirmPasswordInput->GetStyle().backgroundColor = inputBg;
        m_ui->confirmPasswordInput->GetStyle().borderRadius = S(4);
        m_ui->confirmPasswordInput->GetStyle().marginLeft = Panorama::Length::Px(S(20));
        m_ui->confirmPasswordInput->GetStyle().marginTop = Panorama::Length::Px(S(238));
        m_ui->confirmPasswordInput->GetStyle().fontSize = S(12);
        m_ui->confirmPasswordInput->GetStyle().color = white;
        m_ui->confirmPasswordInput->SetPasswordMode(true);
        m_ui->confirmPasswordInput->SetText(m_confirmPassword);
        m_ui->confirmPasswordInput->SetOnTextChanged([this](const std::string& text) {
            m_confirmPassword = text;
            ClearError();
        });
        m_ui->loginBox->AddChild(m_ui->confirmPasswordInput);
        
        buttonY = 238;
    }
    
    // Error label
    m_ui->errorLabel = L("", 10, red);
    m_ui->errorLabel->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->errorLabel->GetStyle().marginTop = Panorama::Length::Px(S(buttonY - 18));
    m_ui->errorLabel->SetVisible(false);
    m_ui->loginBox->AddChild(m_ui->errorLabel);
    
    // Login/Register button
    std::string btnText = m_isRegistering ? "CREATE ACCOUNT" : "LOGIN";
    m_ui->loginButton = std::make_shared<Panorama::CButton>(btnText, "LoginBtn");
    m_ui->loginButton->GetStyle().width = Panorama::Length::Px(S(340));
    m_ui->loginButton->GetStyle().height = Panorama::Length::Px(S(36));
    m_ui->loginButton->GetStyle().backgroundColor = greenBtn;
    m_ui->loginButton->GetStyle().borderRadius = S(4);
    m_ui->loginButton->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->loginButton->GetStyle().marginTop = Panorama::Length::Px(S(buttonY + 44));
    m_ui->loginButton->GetStyle().fontSize = S(12);
    m_ui->loginButton->GetStyle().color = white;
    m_ui->loginButton->SetOnActivate([this]() {
        if (m_isRegistering) {
            OnRegisterClicked();
        } else {
            OnLoginClicked();
        }
    });
    m_ui->loginBox->AddChild(m_ui->loginButton);
    
    // Switch mode button
    std::string switchText = m_isRegistering ? "Back to Login" : "Create Account";
    m_ui->switchModeButton = std::make_shared<Panorama::CButton>(switchText, "SwitchBtn");
    m_ui->switchModeButton->GetStyle().width = Panorama::Length::Px(S(165));
    m_ui->switchModeButton->GetStyle().height = Panorama::Length::Px(S(30));
    m_ui->switchModeButton->GetStyle().backgroundColor = blueBtn;
    m_ui->switchModeButton->GetStyle().borderRadius = S(4);
    m_ui->switchModeButton->GetStyle().marginLeft = Panorama::Length::Px(S(20));
    m_ui->switchModeButton->GetStyle().marginTop = Panorama::Length::Px(S(buttonY + 75));
    m_ui->switchModeButton->GetStyle().fontSize = S(10);
    m_ui->switchModeButton->GetStyle().color = white;
    m_ui->switchModeButton->SetOnActivate([this]() {
        m_isRegistering = !m_isRegistering;
        ClearError();
        DestroyUI();
        CreateUI();
    });
    m_ui->loginBox->AddChild(m_ui->switchModeButton);
    
    // Guest button
    m_ui->guestButton = std::make_shared<Panorama::CButton>("Play as Guest", "GuestBtn");
    m_ui->guestButton->GetStyle().width = Panorama::Length::Px(S(165));
    m_ui->guestButton->GetStyle().height = Panorama::Length::Px(S(30));
    m_ui->guestButton->GetStyle().backgroundColor = grayBtn;
    m_ui->guestButton->GetStyle().borderRadius = S(4);
    m_ui->guestButton->GetStyle().marginLeft = Panorama::Length::Px(S(195));
    m_ui->guestButton->GetStyle().marginTop = Panorama::Length::Px(S(buttonY + 75));
    m_ui->guestButton->GetStyle().fontSize = S(10);
    m_ui->guestButton->GetStyle().color = white;
    m_ui->guestButton->SetOnActivate([this]() { OnGuestClicked(); });
    m_ui->loginBox->AddChild(m_ui->guestButton);
    
    // Loading overlay
    m_ui->loadingOverlay = P("LoadingOverlay", 0, 0, Panorama::Color(0, 0, 0, 0.7f));
    m_ui->loadingOverlay->SetVisible(false);
    m_ui->root->AddChild(m_ui->loadingOverlay);
    
    m_ui->loadingLabel = L("Connecting...", 14, white);
    m_ui->loadingLabel->GetStyle().marginLeft = Panorama::Length::Px((sw - S(100)) / 2);
    m_ui->loadingLabel->GetStyle().marginTop = Panorama::Length::Px(sh / 2);
    m_ui->loadingOverlay->AddChild(m_ui->loadingLabel);
}

void LoginState::DestroyUI() {
    if (m_ui->root) {
        auto& engine = Panorama::CUIEngine::Instance();
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
    // Always forward keyboard events to UI engine for text input
    auto& engine = Panorama::CUIEngine::Instance();
    
    // Enter key to submit - check focus first, then forward to UI
    if (key == 13) { // VK_RETURN
        auto* focused = engine.GetFocusedPanel();
        bool isTextEntry = focused && focused->GetPanelType() == Panorama::PanelType::TextEntry;
        
        // If text entry is focused, let UI handle it
        if (isTextEntry) {
            engine.OnKeyDown(key);
            return true;
        }
        
        // Otherwise, submit form
        if (m_isRegistering) {
            OnRegisterClicked();
        } else {
            OnLoginClicked();
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
