#pragma once
/**
 * LoginForm - Login form component with username/password inputs
 * 
 * Contains:
 * - Title "WELCOME BACK"
 * - Username input
 * - Password input (masked)
 * - Error label (hidden by default)
 * - Primary button "ENTER THE GAME"
 * - Secondary button "CREATE ACCOUNT"
 * 
 * Requirements: 1.3, 3.1-3.6, 5.1, 5.2, 6.1, 6.3, 9.2, 9.3
 */

#include "../panorama/core/CPanel2D.h"
#include <memory>
#include <functional>
#include <vector>
#include <string>

namespace Panorama {
    class CLabel;
    class CTextEntry;
    class CButton;
}

namespace Game {

class LoginForm {
public:
    LoginForm() = default;
    ~LoginForm() = default;
    
    /**
     * Create the form component
     * @param parent Parent panel to attach to
     * @param screenWidth Current screen width for positioning
     * @param screenHeight Current screen height for positioning
     */
    void Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight);
    
    /**
     * Destroy and cleanup the form component
     */
    void Destroy();
    
    // Input access
    std::string GetUsername() const;
    std::string GetPassword() const;
    
    // Error display
    void ShowError(const std::string& message);
    void ClearError();
    
    // Focus management
    void FocusUsername();
    void FocusNext();
    void FocusPrevious();
    
    // Callbacks
    void SetOnSubmit(std::function<void()> callback);
    void SetOnCreateAccount(std::function<void()> callback);
    
    // Focus order for Tab navigation
    std::vector<Panorama::CPanel2D*>& GetFocusOrder();
    
    // Validation
    bool ValidateInputs();
    
private:
    std::shared_ptr<Panorama::CPanel2D> m_container;
    std::shared_ptr<Panorama::CLabel> m_titleLabel;
    std::shared_ptr<Panorama::CLabel> m_usernameLabel;
    std::shared_ptr<Panorama::CTextEntry> m_usernameInput;
    std::shared_ptr<Panorama::CLabel> m_passwordLabel;
    std::shared_ptr<Panorama::CTextEntry> m_passwordInput;
    std::shared_ptr<Panorama::CLabel> m_errorLabel;
    std::shared_ptr<Panorama::CButton> m_primaryButton;
    std::shared_ptr<Panorama::CButton> m_secondaryButton;
    
    std::vector<Panorama::CPanel2D*> m_focusOrder;
    i32 m_currentFocusIndex = 0;
    
    std::function<void()> m_onSubmit;
    std::function<void()> m_onCreateAccount;
};

} // namespace Game
