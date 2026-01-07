#pragma once
/**
 * LoginFooter - Shared footer component for Login/Register screens
 * 
 * Displays keyboard shortcut hints at the bottom of the login/register forms.
 * Used by both LoginState and RegisterState.
 * 
 * Requirements: 1.2, 9.2, 9.3
 */

#include "../panorama/core/CPanel2D.h"
#include <memory>

namespace Panorama {
    class CLabel;
}

namespace Game {

class LoginFooter {
public:
    LoginFooter() = default;
    ~LoginFooter() = default;
    
    /**
     * Create the footer component
     * @param parent Parent panel to attach to
     * @param screenWidth Current screen width for positioning
     * @param screenHeight Current screen height for positioning
     */
    void Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight);
    
    /**
     * Destroy and cleanup the footer component
     */
    void Destroy();
    
private:
    std::shared_ptr<Panorama::CPanel2D> m_container;
    std::shared_ptr<Panorama::CLabel> m_hintLabel;
};

} // namespace Game
