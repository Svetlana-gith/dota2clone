#pragma once
/**
 * LoginHeader - Shared header component for Login/Register screens
 * 
 * Displays game logo "WORLD EDITOR" with golden glow effect and decorative accent line.
 * Used by both LoginState and RegisterState.
 * 
 * Requirements: 1.2, 2.2, 9.2, 9.3
 */

#include "../panorama/core/CPanel2D.h"
#include <memory>

namespace Panorama {
    class CLabel;
}

namespace Game {

class LoginHeader {
public:
    LoginHeader() = default;
    ~LoginHeader() = default;
    
    /**
     * Create the header component
     * @param parent Parent panel to attach to
     * @param screenWidth Current screen width for positioning
     * @param screenHeight Current screen height for positioning
     */
    void Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight);
    
    /**
     * Destroy and cleanup the header component
     */
    void Destroy();
    
private:
    std::shared_ptr<Panorama::CPanel2D> m_container;
    std::shared_ptr<Panorama::CLabel> m_logoLabel;
    std::shared_ptr<Panorama::CPanel2D> m_accentLine;
};

} // namespace Game
