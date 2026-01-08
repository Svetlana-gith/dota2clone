#include "LoginForm.h"
#include "../panorama/core/CUIEngine.h"
#include "../panorama/widgets/CLabel.h"
#include "../panorama/widgets/CTextEntry.h"
#include "../panorama/widgets/CButton.h"

namespace Game {

void LoginForm::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight) {
    if (!parent) return;
    
    // Container position using percentages (centered)
    // Container: 40% width, 47% height, centered horizontally
    // Header: 15% height, gap: 3%
    const f32 containerWidthPct = 40.0f;
    const f32 containerHeightPct = 47.0f;
    const f32 headerHeightPct = 15.0f;
    const f32 gapPct = 3.0f;
    
    // Calculate center position
    const f32 containerX = (100.0f - containerWidthPct) / 2.0f;  // 30%
    const f32 totalBlockPct = headerHeightPct + gapPct + containerHeightPct;  // 65%
    const f32 blockStartY = (100.0f - totalBlockPct) / 2.0f;  // 17.5%
    const f32 containerY = blockStartY + headerHeightPct + gapPct;
    
    // Container for form elements (styled by #LoginFormContainer in CSS)
    m_container = std::make_shared<Panorama::CPanel2D>("LoginFormContainer");
    // NOTE: Size and position managed by CSS Flexbox
    parent->AddChild(m_container);
    
    // Layout constants (percentages relative to container)
    // const f32 elementX = 6.0f;
    // const f32 elementWidth = 87.0f;
    
    // Title "WELCOME BACK" - styled by #FormTitle in CSS
    m_titleLabel = std::make_shared<Panorama::CLabel>("WELCOME BACK", "FormTitle");
    // m_titleLabel->GetStyle().x = Panorama::Length::Pct(elementX);
    // m_titleLabel->GetStyle().y = Panorama::Length::Pct(3.0f);
    // m_titleLabel->GetStyle().width = Panorama::Length::Pct(elementWidth);
    // m_titleLabel->GetStyle().height = Panorama::Length::Pct(7.0f);
    m_container->AddChild(m_titleLabel);
    
    // Username label - styled by #UsernameLabel in CSS
    m_usernameLabel = std::make_shared<Panorama::CLabel>("USERNAME", "UsernameLabel");
    m_usernameLabel->AddClass("FieldLabel");
    // m_usernameLabel->GetStyle().x = Panorama::Length::Pct(elementX);
    // m_usernameLabel->GetStyle().y = Panorama::Length::Pct(12.0f);
    // m_usernameLabel->GetStyle().width = Panorama::Length::Pct(elementWidth);
    // m_usernameLabel->GetStyle().height = Panorama::Length::Pct(4.0f);
    m_container->AddChild(m_usernameLabel);
    
    // Username input - styled by #UsernameInput in CSS
    m_usernameInput = std::make_shared<Panorama::CTextEntry>("UsernameInput");
    m_usernameInput->AddClass("LoginInput");
    m_usernameInput->SetPlaceholder("Enter your username");
    // m_usernameInput->GetStyle().x = Panorama::Length::Pct(elementX);
    // m_usernameInput->GetStyle().y = Panorama::Length::Pct(16.0f);
    // m_usernameInput->GetStyle().width = Panorama::Length::Pct(elementWidth);
    // m_usernameInput->GetStyle().height = Panorama::Length::Pct(9.0f);
    m_container->AddChild(m_usernameInput);

    // Password label - styled by #PasswordLabel in CSS
    m_passwordLabel = std::make_shared<Panorama::CLabel>("PASSWORD", "PasswordLabel");
    m_passwordLabel->AddClass("FieldLabel");
    // m_passwordLabel->GetStyle().x = Panorama::Length::Pct(elementX);
    // m_passwordLabel->GetStyle().y = Panorama::Length::Pct(27.0f);
    // m_passwordLabel->GetStyle().width = Panorama::Length::Pct(elementWidth);
    // m_passwordLabel->GetStyle().height = Panorama::Length::Pct(4.0f);
    m_container->AddChild(m_passwordLabel);
    
    // Password input - styled by #PasswordInput in CSS
    m_passwordInput = std::make_shared<Panorama::CTextEntry>("PasswordInput");
    m_passwordInput->AddClass("LoginInput");
    m_passwordInput->SetPlaceholder("Enter your password");
    m_passwordInput->SetPassword(true);
    // m_passwordInput->GetStyle().x = Panorama::Length::Pct(elementX);
    // m_passwordInput->GetStyle().y = Panorama::Length::Pct(31.0f);
    // m_passwordInput->GetStyle().width = Panorama::Length::Pct(elementWidth);
    // m_passwordInput->GetStyle().height = Panorama::Length::Pct(9.0f);
    m_container->AddChild(m_passwordInput);
    
    // Error label - styled by #ErrorLabel in CSS
    m_errorLabel = std::make_shared<Panorama::CLabel>("", "ErrorLabel");
    // m_errorLabel->GetStyle().x = Panorama::Length::Pct(elementX);
    // m_errorLabel->GetStyle().y = Panorama::Length::Pct(42.0f);
    // m_errorLabel->GetStyle().width = Panorama::Length::Pct(elementWidth);
    // m_errorLabel->GetStyle().height = Panorama::Length::Pct(4.0f);
    m_errorLabel->SetVisible(false);
    m_container->AddChild(m_errorLabel);
    
    // Primary button "ENTER THE GAME" - styled by #PrimaryButton in CSS
    m_primaryButton = std::make_shared<Panorama::CButton>("ENTER THE GAME", "PrimaryButton");
    // m_primaryButton->GetStyle().x = Panorama::Length::Pct(elementX);
    // m_primaryButton->GetStyle().y = Panorama::Length::Pct(48.0f);
    // m_primaryButton->GetStyle().width = Panorama::Length::Pct(elementWidth);
    // m_primaryButton->GetStyle().height = Panorama::Length::Pct(10.0f);
    m_primaryButton->SetOnActivate([this]() {
        if (m_onSubmit) m_onSubmit();
    });
    m_container->AddChild(m_primaryButton);
    
    // Secondary button "CREATE ACCOUNT" - styled by #SecondaryButton in CSS
    m_secondaryButton = std::make_shared<Panorama::CButton>("CREATE ACCOUNT", "SecondaryButton");
    // m_secondaryButton->GetStyle().x = Panorama::Length::Pct(elementX);
    // m_secondaryButton->GetStyle().y = Panorama::Length::Pct(61.0f);
    // m_secondaryButton->GetStyle().width = Panorama::Length::Pct(elementWidth);
    // m_secondaryButton->GetStyle().height = Panorama::Length::Pct(8.0f);
    m_secondaryButton->SetOnActivate([this]() {
        if (m_onCreateAccount) m_onCreateAccount();
    });
    m_container->AddChild(m_secondaryButton);
    
    // Setup focus order for Tab navigation
    m_focusOrder.clear();
    m_focusOrder.push_back(m_usernameInput.get());
    m_focusOrder.push_back(m_passwordInput.get());
    m_focusOrder.push_back(m_primaryButton.get());
    m_focusOrder.push_back(m_secondaryButton.get());
    m_currentFocusIndex = 0;
}

void LoginForm::Destroy() {
    if (m_container) {
        if (auto* parent = m_container->GetParent()) {
            parent->RemoveChild(m_container.get());
        }
        m_focusOrder.clear();
        m_secondaryButton.reset();
        m_primaryButton.reset();
        m_errorLabel.reset();
        m_passwordInput.reset();
        m_passwordLabel.reset();
        m_usernameInput.reset();
        m_usernameLabel.reset();
        m_titleLabel.reset();
        m_container.reset();
    }
}

std::string LoginForm::GetUsername() const {
    return m_usernameInput ? m_usernameInput->GetText() : "";
}

std::string LoginForm::GetPassword() const {
    return m_passwordInput ? m_passwordInput->GetText() : "";
}

void LoginForm::ShowError(const std::string& message) {
    if (m_errorLabel) {
        m_errorLabel->SetText(message);
        m_errorLabel->SetVisible(true);
    }
}

void LoginForm::ClearError() {
    if (m_errorLabel) {
        m_errorLabel->SetText("");
        m_errorLabel->SetVisible(false);
    }
}

void LoginForm::FocusUsername() {
    if (m_usernameInput) {
        Panorama::CUIEngine::Instance().SetFocus(m_usernameInput.get());
        m_currentFocusIndex = 0;
    }
}

void LoginForm::FocusNext() {
    if (m_focusOrder.empty()) return;
    m_currentFocusIndex = (m_currentFocusIndex + 1) % static_cast<i32>(m_focusOrder.size());
    if (m_focusOrder[m_currentFocusIndex]) {
        Panorama::CUIEngine::Instance().SetFocus(m_focusOrder[m_currentFocusIndex]);
    }
}

void LoginForm::FocusPrevious() {
    if (m_focusOrder.empty()) return;
    m_currentFocusIndex = (m_currentFocusIndex - 1 + static_cast<i32>(m_focusOrder.size())) % static_cast<i32>(m_focusOrder.size());
    if (m_focusOrder[m_currentFocusIndex]) {
        Panorama::CUIEngine::Instance().SetFocus(m_focusOrder[m_currentFocusIndex]);
    }
}

void LoginForm::SetOnSubmit(std::function<void()> callback) {
    m_onSubmit = std::move(callback);
}

void LoginForm::SetOnCreateAccount(std::function<void()> callback) {
    m_onCreateAccount = std::move(callback);
}

std::vector<Panorama::CPanel2D*>& LoginForm::GetFocusOrder() {
    return m_focusOrder;
}

bool LoginForm::ValidateInputs() {
    const std::string username = GetUsername();
    const std::string password = GetPassword();
    
    // Check empty username
    if (username.empty()) {
        ShowError("Please enter username");
        return false;
    }
    
    // Check whitespace-only username
    bool usernameOnlyWhitespace = true;
    for (char c : username) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            usernameOnlyWhitespace = false;
            break;
        }
    }
    if (usernameOnlyWhitespace) {
        ShowError("Please enter username");
        return false;
    }
    
    // Check empty password
    if (password.empty()) {
        ShowError("Please enter password");
        return false;
    }
    
    // Check whitespace-only password
    bool passwordOnlyWhitespace = true;
    for (char c : password) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            passwordOnlyWhitespace = false;
            break;
        }
    }
    if (passwordOnlyWhitespace) {
        ShowError("Please enter password");
        return false;
    }
    
    ClearError();
    return true;
}

} // namespace Game
