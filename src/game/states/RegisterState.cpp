/**
 * RegisterState - Registration screen
 * 
 * Uses modular components: LoginHeader, RegisterForm, LoginFooter
 * Requirements: 1.1, 1.4, 4.1-4.6, 6.2, 6.3, 6.5, 6.6, 6.8
 * 
 * Keyboard shortcuts:
 *   Tab       - Next input field
 *   Shift+Tab - Previous input field  
 *   Enter     - Submit form (register)
 *   Escape    - Clear error or back to login
 */

#include "../GameState.h"
#include "../DebugConsole.h"
#include "../ui/panorama/core/CUIEngine.h"
#include "../ui/panorama/core/CPanel2D.h"
#include "../ui/panorama/widgets/CLabel.h"
#include "../ui/panorama/layout/CStyleSheet.h"
#include "../ui/login/LoginHeader.h"
#include "../ui/login/RegisterForm.h"
#include "../ui/login/LoginFooter.h"
#include "../../auth/AuthClient.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Game {

struct RegisterState::RegisterUI {
    std::shared_ptr<Panorama::CPanel2D> root;
    
    // Modular components
    std::unique_ptr<LoginHeader> header;
    std::unique_ptr<RegisterForm> form;
    std::unique_ptr<LoginFooter> footer;
    
    // Loading overlay
    std::shared_ptr<Panorama::CPanel2D> loadingOverlay;
    std::shared_ptr<Panorama::CLabel> loadingLabel;
};

RegisterState::RegisterState() 
    : m_ui(std::make_unique<RegisterUI>())
{
}

RegisterState::~RegisterState() = default;

void RegisterState::OnEnter() {
    LOG_INFO("RegisterState::OnEnter()");
    
    // Load login stylesheet (Flexbox + Tailwind utilities, shared with LoginState)
    Panorama::CUIEngine::Instance().LoadStyleSheet("resources/styles/login-modern.css");
    
    CreateUI();
    SetupAuthCallbacks();
    
    LOG_INFO("RegisterState UI created");
    ConsoleLog("Registration screen loaded");
}

void RegisterState::OnExit() {
    DestroyUI();
}

void RegisterState::Update(f32 deltaTime) {
    auto* authClient = m_manager->GetAuthClient();
    if (authClient) {
        authClient->Update();
    }
    
    Panorama::CUIEngine::Instance().Update(deltaTime);
}

void RegisterState::Render() {
    Panorama::CUIEngine::Instance().Render();
}

void RegisterState::CreateUI() {
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    float sw = engine.GetScreenWidth();
    float sh = engine.GetScreenHeight();
    
    // ROOT - full screen background (styled by #LoginRoot in CSS)
    m_ui->root = std::make_shared<Panorama::CPanel2D>("LoginRoot");
    m_ui->root->GetStyle().width = Panorama::Length::Fill();
    m_ui->root->GetStyle().height = Panorama::Length::Fill();
    m_ui->root->GetStyle().flowChildren = Panorama::FlowDirection::Down;
    uiRoot->AddChild(m_ui->root);
    
    // Create modular components (reuses LoginHeader and LoginFooter)
    m_ui->header = std::make_unique<LoginHeader>();
    m_ui->header->Create(m_ui->root.get(), sw, sh);
    
    m_ui->form = std::make_unique<RegisterForm>();
    m_ui->form->Create(m_ui->root.get(), sw, sh);
    
    // Setup form callbacks
    m_ui->form->SetOnSubmit([this]() {
        OnRegisterClicked();
    });
    
    m_ui->form->SetOnBackToLogin([this]() {
        OnBackToLoginClicked();
    });
    
    m_ui->footer = std::make_unique<LoginFooter>();
    m_ui->footer->Create(m_ui->root.get(), sw, sh);
    
    // Loading overlay (styled by #LoadingOverlay in CSS)
    m_ui->loadingOverlay = std::make_shared<Panorama::CPanel2D>("LoadingOverlay");
    m_ui->loadingOverlay->GetStyle().width = Panorama::Length::Fill();
    m_ui->loadingOverlay->GetStyle().height = Panorama::Length::Fill();
    m_ui->loadingOverlay->SetVisible(false);
    m_ui->root->AddChild(m_ui->loadingOverlay);
    
    // Loading label (styled by #LoadingLabel in CSS)
    m_ui->loadingLabel = std::make_shared<Panorama::CLabel>("Creating account...", "LoadingLabel");
    m_ui->loadingLabel->GetStyle().marginLeft = Panorama::Length::Px(std::round((sw - 180.0f) / 2));
    m_ui->loadingLabel->GetStyle().marginTop = Panorama::Length::Px(std::round(sh / 2));
    m_ui->loadingOverlay->AddChild(m_ui->loadingLabel);
    
    // Set initial focus to username field
    m_ui->form->FocusUsername();
}

void RegisterState::DestroyUI() {
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

void RegisterState::SetupAuthCallbacks() {
    auto* authClient = m_manager->GetAuthClient();
    if (!authClient) return;
    
    authClient->SetOnRegisterSuccess([this](u64 accountId, const std::string& token) {
        LOG_INFO("Registration successful! Account ID: {}", accountId);
        if (m_ui->loadingOverlay) m_ui->loadingOverlay->SetVisible(false);
        ConsoleLog("Account created successfully!");
        
        // Auto-login after registration - go to main menu
        m_manager->ChangeState(EGameState::MainMenu);
    });
    
    authClient->SetOnRegisterFailed([this](const std::string& error) {
        LOG_WARN("Registration failed: {}", error);
        if (m_ui->loadingOverlay) m_ui->loadingOverlay->SetVisible(false);
        ShowError(error);
    });
}

void RegisterState::ShowError(const std::string& message) {
    if (m_ui->form) {
        m_ui->form->ShowError(message);
    }
}

void RegisterState::ClearError() {
    if (m_ui->form) {
        m_ui->form->ClearError();
    }
}

void RegisterState::OnRegisterClicked() {
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
        m_ui->loadingLabel->SetText("Creating account...");
        m_ui->loadingOverlay->SetVisible(true);
    }
    
    authClient->Register(username, password);
}

void RegisterState::OnBackToLoginClicked() {
    m_manager->ChangeState(EGameState::Login);
}

bool RegisterState::OnKeyDown(i32 key) {
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
        OnRegisterClicked();
        return true;
    }
    
    // Escape - clear error first, then back to login
    if (key == VK_ESCAPE) {
        if (m_ui->form) {
            // If error is visible, clear it first
            // Otherwise go back to login
            // Note: For simplicity, always go back to login on Escape
            // (error clears automatically when form is destroyed)
            OnBackToLoginClicked();
        }
        return true;
    }
    
    // Forward all other keys to UI engine
    Panorama::CUIEngine::Instance().OnKeyDown(key);
    return false;
}

bool RegisterState::OnMouseMove(f32 x, f32 y) {
    Panorama::CUIEngine::Instance().OnMouseMove(x, y);
    return true;
}

bool RegisterState::OnMouseDown(f32 x, f32 y, i32 button) {
    Panorama::CUIEngine::Instance().OnMouseDown(x, y, button);
    return true;
}

bool RegisterState::OnMouseUp(f32 x, f32 y, i32 button) {
    Panorama::CUIEngine::Instance().OnMouseUp(x, y, button);
    return true;
}

} // namespace Game
