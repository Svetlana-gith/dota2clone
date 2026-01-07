#pragma once

#include "../core/CPanel2D.h"
#include "../../../core/Types.h"
#include <string>
#include <functional>
#include <memory>

namespace Panorama {

class CLabel;
class CUIRenderer;

/**
 * Button widget with text and click handling
 */
class CButton : public CPanel2D {
public:
    CButton();
    CButton(const std::string& text, const std::string& id = "");
    
    // Text management
    void SetText(const std::string& text);
    const std::string& GetText() const;
    
    // Event handling
    void SetOnActivate(std::function<void()> handler);
    
    // Input events
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    bool OnKeyDown(i32 key) override;
    
    // Rendering
    void Render(CUIRenderer* renderer) override;

private:
    std::shared_ptr<CLabel> m_label;
    std::function<void()> m_onActivate;
};

} // namespace Panorama