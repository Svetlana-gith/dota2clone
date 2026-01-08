#include "RegisterForm.h"
#include "../panorama/core/CUIEngine.h"
#include "../panorama/widgets/CLabel.h"
#include "../panorama/widgets/CTextEntry.h"
#include "../panorama/widgets/CButton.h"
#include <cctype>

namespace Game {

void RegisterForm::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight) {
    if (!parent) return;
    
    // Container position using percentages (centered)
    // Container: 40% width, 60% height (taller than login for 3 fields), centered horizontally
    // Header: 15% height, gap: 3%
    const f32 containerWidthPct = 40.0f;
    const f32 containerHeightPct = 60.0f;
    const f32 headerHeightPct = 15.0f;
    const f32 gapPct = 3.0f;
    
    // Calculate center position
    const f32 containerX = (100.0f - containerWidthPct) / 2.0f;  // 30%
    const f32 totalBlockPct = headerHeightPct + gapPct + containerHeightPct;  // 78%
    const f32 blockStartY = (100.0f - totalBlockPct) / 2.0f;  // 11%
    const f32 containerY = blockStartY + headerHeightPct + gapPct;
    
    // Container for form elements (styled by #LoginFormContainer in CSS - same style as LoginForm)
    m_container = std::make_shared<Panorama::CPanel2D>("LoginFormContainer");
    m_container->AddClass("RegisterForm");
    // NOTE: Position managed by CSS Flexbox parent (#LoginRoot with justify-content: center)
    // Only set size, not position
    m_container->GetStyle().width = Panorama::Length::Pct(containerWidthPct);
    m_container->GetStyle().height = Panorama::Length::Pct(containerHeightPct);
    parent->AddChild(m_container);
    
    // Title "CREATE ACCOUNT" (styled by #FormTitle in CSS)
    m_titleLabel = std::make_shared<Panorama::CLabel>("CREATE ACCOUNT", "FormTitle");
    m_container->AddChild(m_titleLabel);
    
    // Username label (styled by #UsernameLabel in CSS)
    m_usernameLabel = std::make_shared<Panorama::CLabel>("USERNAME", "UsernameLabel");
    m_usernameLabel->AddClass("FieldLabel");
    m_container->AddChild(m_usernameLabel);
    
    // Username input (styled by #UsernameInput in CSS)
    m_usernameInput = std::make_shared<Panorama::CTextEntry>("UsernameInput");
    m_usernameInput->AddClass("LoginInput");
    m_usernameInput->SetPlaceholder("Choose a username");
    m_container->AddChild(m_usernameInput);

    // Password label (styled by #PasswordLabel in CSS)
    m_passwordLabel = std::make_shared<Panorama::CLabel>("PASSWORD", "PasswordLabel");
    m_passwordLabel->AddClass("FieldLabel");
    m_container->AddChild(m_passwordLabel);
    
    // Password input (styled by #PasswordInput in CSS)
    m_passwordInput = std::make_shared<Panorama::CTextEntry>("PasswordInput");
    m_passwordInput->AddClass("LoginInput");
    m_passwordInput->SetPlaceholder("Create a password");
    m_passwordInput->SetPassword(true);
    m_container->AddChild(m_passwordInput);
    
    // Confirm Password label (styled by #ConfirmPasswordLabel in CSS)
    m_confirmPasswordLabel = std::make_shared<Panorama::CLabel>("CONFIRM PASSWORD", "ConfirmPasswordLabel");
    m_confirmPasswordLabel->AddClass("FieldLabel");
    m_container->AddChild(m_confirmPasswordLabel);
    
    // Confirm Password input (styled by #ConfirmPasswordInput in CSS)
    m_confirmPasswordInput = std::make_shared<Panorama::CTextEntry>("ConfirmPasswordInput");
    m_confirmPasswordInput->AddClass("LoginInput");
    m_confirmPasswordInput->SetPlaceholder("Confirm your password");
    m_confirmPasswordInput->SetPassword(true);
    m_container->AddChild(m_confirmPasswordInput);
    
    // Error label (hidden by default, styled by #ErrorLabel in CSS)
    m_errorLabel = std::make_shared<Panorama::CLabel>("", "ErrorLabel");
    m_errorLabel->AddClass("RegisterError");
    m_errorLabel->SetVisible(false);
    m_container->AddChild(m_errorLabel);
    
    // Primary button "CREATE ACCOUNT" (styled by #PrimaryButton in CSS)
    m_primaryButton = std::make_shared<Panorama::CButton>("CREATE ACCOUNT", "PrimaryButton");
    m_primaryButton->AddClass("RegisterPrimary");
    m_primaryButton->SetOnActivate([this]() {
        if (m_onSubmit) m_onSubmit();
    });
    m_container->AddChild(m_primaryButton);
    
    // Secondary button "BACK TO LOGIN" (styled by #SecondaryButton in CSS)
    m_secondaryButton = std::make_shared<Panorama::CButton>("BACK TO LOGIN", "SecondaryButton");
    m_secondaryButton->AddClass("RegisterSecondary");
    m_secondaryButton->SetOnActivate([this]() {
        if (m_onBackToLogin) m_onBackToLogin();
    });
    m_container->AddChild(m_secondaryButton);
    
    // Setup focus order for Tab navigation (3 fields + 2 buttons)
    m_focusOrder.clear();
    m_focusOrder.push_back(m_usernameInput.get());
    m_focusOrder.push_back(m_passwordInput.get());
    m_focusOrder.push_back(m_confirmPasswordInput.get());
    m_focusOrder.push_back(m_primaryButton.get());
    m_focusOrder.push_back(m_secondaryButton.get());
    m_currentFocusIndex = 0;
}

void RegisterForm::Destroy() {
    if (m_container) {
        if (auto* parent = m_container->GetParent()) {
            parent->RemoveChild(m_container.get());
        }
        m_focusOrder.clear();
        m_secondaryButton.reset();
        m_primaryButton.reset();
        m_errorLabel.reset();
        m_confirmPasswordInput.reset();
        m_confirmPasswordLabel.reset();
        m_passwordInput.reset();
        m_passwordLabel.reset();
        m_usernameInput.reset();
        m_usernameLabel.reset();
        m_titleLabel.reset();
        m_container.reset();
    }
}

std::string RegisterForm::GetUsername() const {
    return m_usernameInput ? m_usernameInput->GetText() : "";
}

std::string RegisterForm::GetPassword() const {
    return m_passwordInput ? m_passwordInput->GetText() : "";
}

std::string RegisterForm::GetConfirmPassword() const {
    return m_confirmPasswordInput ? m_confirmPasswordInput->GetText() : "";
}

void RegisterForm::ShowError(const std::string& message) {
    if (m_errorLabel) {
        m_errorLabel->SetText(message);
        m_errorLabel->SetVisible(true);
    }
}

void RegisterForm::ClearError() {
    if (m_errorLabel) {
        m_errorLabel->SetText("");
        m_errorLabel->SetVisible(false);
    }
}

void RegisterForm::FocusUsername() {
    if (m_usernameInput) {
        Panorama::CUIEngine::Instance().SetFocus(m_usernameInput.get());
        m_currentFocusIndex = 0;
    }
}

void RegisterForm::FocusNext() {
    if (m_focusOrder.empty()) return;
    m_currentFocusIndex = (m_currentFocusIndex + 1) % static_cast<i32>(m_focusOrder.size());
    if (m_focusOrder[m_currentFocusIndex]) {
        Panorama::CUIEngine::Instance().SetFocus(m_focusOrder[m_currentFocusIndex]);
    }
}

void RegisterForm::FocusPrevious() {
    if (m_focusOrder.empty()) return;
    m_currentFocusIndex = (m_currentFocusIndex - 1 + static_cast<i32>(m_focusOrder.size())) % static_cast<i32>(m_focusOrder.size());
    if (m_focusOrder[m_currentFocusIndex]) {
        Panorama::CUIEngine::Instance().SetFocus(m_focusOrder[m_currentFocusIndex]);
    }
}

void RegisterForm::SetOnSubmit(std::function<void()> callback) {
    m_onSubmit = std::move(callback);
}

void RegisterForm::SetOnBackToLogin(std::function<void()> callback) {
    m_onBackToLogin = std::move(callback);
}

std::vector<Panorama::CPanel2D*>& RegisterForm::GetFocusOrder() {
    return m_focusOrder;
}

bool RegisterForm::ValidateInputs() {
    const std::string username = GetUsername();
    const std::string password = GetPassword();
    const std::string confirmPassword = GetConfirmPassword();
    
    // Check empty username
    if (username.empty()) {
        ShowError("Please enter username");
        return false;
    }
    
    // Check username length (3-20 characters)
    if (username.length() < 3 || username.length() > 20) {
        ShowError("Username must be 3-20 characters");
        return false;
    }
    
    // Check empty password
    if (password.empty()) {
        ShowError("Please enter password");
        return false;
    }
    
    // Check password length (minimum 8 characters)
    if (password.length() < 8) {
        ShowError("Password must be at least 8 characters");
        return false;
    }
    
    // Check passwords match
    if (password != confirmPassword) {
        ShowError("Passwords do not match");
        return false;
    }
    
    ClearError();
    return true;
}

} // namespace Game
