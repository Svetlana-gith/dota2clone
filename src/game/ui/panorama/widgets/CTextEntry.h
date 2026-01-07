#pragma once

#include "../core/CPanel2D.h"
#include "../../../core/Types.h"
#include <string>
#include <functional>

namespace Panorama {

class CUIRenderer;

/**
 * Text entry widget for user input
 */
class CTextEntry : public CPanel2D {
public:
    CTextEntry();
    CTextEntry(const std::string& id);
    
    // Text management
    void SetText(const std::string& text);
    const std::string& GetText() const { return m_text; }
    void SetPlaceholder(const std::string& placeholder) { m_placeholder = placeholder; }
    void SetMaxChars(i32 maxChars) { m_maxChars = maxChars; }
    void SetPassword(bool isPassword) { m_isPassword = isPassword; }
    
    // Events
    void SetOnTextChanged(std::function<void(const std::string&)> handler) { m_onTextChanged = handler; }
    
    // Focus
    void SetFocus() override;
    
    // Input events
    bool OnKeyDown(i32 key) override;
    bool OnTextInput(const std::string& text) override;
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    
    // Update and render
    void Update(f32 deltaTime) override;
    void Render(CUIRenderer* renderer) override;

private:
    std::string m_text;
    std::string m_placeholder;
    i32 m_cursorPos = 0;
    i32 m_maxChars = 0;
    bool m_isPassword = false;
    f32 m_cursorBlinkTime = 0.0f;
    std::function<void(const std::string&)> m_onTextChanged;
};

} // namespace Panorama