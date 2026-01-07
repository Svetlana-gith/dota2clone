#include "CTextEntry.h"
#include "../rendering/CUIRenderer.h"

namespace Panorama {

CTextEntry::CTextEntry() {
    m_panelType = PanelType::TextEntry;
    m_acceptsInput = true;
    m_inlineStyle.backgroundColor = Color(0.1f, 0.1f, 0.12f, 0.95f);
    m_inlineStyle.borderRadius = 4.0f;
    m_inlineStyle.borderWidth = 1.0f;
    m_inlineStyle.borderColor = Color(0.3f, 0.3f, 0.35f, 0.8f);
    m_inlineStyle.paddingLeft = Length::Px(8);
    m_inlineStyle.paddingRight = Length::Px(8);
}

CTextEntry::CTextEntry(const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::TextEntry;
    m_acceptsInput = true;
    m_inlineStyle.backgroundColor = Color(0.1f, 0.1f, 0.12f, 0.95f);
    m_inlineStyle.borderRadius = 4.0f;
    m_inlineStyle.borderWidth = 1.0f;
    m_inlineStyle.borderColor = Color(0.3f, 0.3f, 0.35f, 0.8f);
}

void CTextEntry::SetText(const std::string& text) {
    m_text = text;
    m_cursorPos = static_cast<i32>(text.length());
    m_scrollOffset = 0.0f;  // Reset scroll when text is set
}

void CTextEntry::SetFocus() {
    CPanel2D::SetFocus();
    m_cursorBlinkTime = 0.0f;
}

bool CTextEntry::OnKeyDown(i32 key) {
    if (!m_focused) return false;
    
    m_cursorBlinkTime = 0.0f;
    
    bool changed = false;
    if (key == 8 && !m_text.empty() && m_cursorPos > 0) { 
        m_text.erase(m_cursorPos - 1, 1); 
        m_cursorPos--; 
        changed = true; 
    }
    if (key == 46 && m_cursorPos < static_cast<i32>(m_text.length())) { 
        m_text.erase(m_cursorPos, 1); 
        changed = true; 
    }
    if (key == 37 && m_cursorPos > 0) { m_cursorPos--; return true; }
    if (key == 39 && m_cursorPos < static_cast<i32>(m_text.length())) { m_cursorPos++; return true; }
    if (key == 36) { m_cursorPos = 0; return true; }
    if (key == 35) { m_cursorPos = static_cast<i32>(m_text.length()); return true; }
    if (key == 13) { 
        PanelEvent event; 
        event.type = PanelEventType::OnInputSubmit; 
        event.target = this; 
        DispatchEvent(event); 
        return true; 
    }
    if (changed && m_onTextChanged) { m_onTextChanged(m_text); }
    return changed;
}

bool CTextEntry::OnTextInput(const std::string& text) {
    if (!m_focused) return false;
    if (m_maxChars > 0 && static_cast<i32>(m_text.length()) + static_cast<i32>(text.length()) > m_maxChars) 
        return false;
    
    m_cursorBlinkTime = 0.0f;
    
    m_text.insert(m_cursorPos, text);
    m_cursorPos += static_cast<i32>(text.length());
    if (m_onTextChanged) { m_onTextChanged(m_text); }
    return true;
}

bool CTextEntry::OnMouseDown(f32 x, f32 y, i32 button) {
    bool handled = CPanel2D::OnMouseDown(x, y, button);
    
    if (button == 0 && IsPointInPanel(x, y)) {
        m_cursorPos = static_cast<i32>(m_text.length());
        m_cursorBlinkTime = 0.0f;
        return true;
    }
    
    return handled;
}

void CTextEntry::Update(f32 deltaTime) {
    CPanel2D::Update(deltaTime);
    
    if (m_focused) {
        m_cursorBlinkTime += deltaTime;
        if (m_cursorBlinkTime > 1.0f) {
            m_cursorBlinkTime -= 1.0f;
        }
    } else {
        m_cursorBlinkTime = 0.0f;
    }
}

void CTextEntry::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (!renderer) return;
    
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    
    FontInfo font;
    font.size = m_computedStyle.fontSize.value_or(16.0f);
    font.family = m_computedStyle.fontFamily.value_or("Roboto Condensed");
    font.bold = m_computedStyle.fontWeight.has_value() && m_computedStyle.fontWeight.value() == "bold";
    font.letterSpacing = m_computedStyle.letterSpacing.value_or(0.0f);
    
    Color textColor = m_computedStyle.color.value_or(Color::White()); 
    textColor.a *= opacity;
    
    std::string displayText = m_isPassword ? std::string(m_text.length(), '*') : m_text;
    
    // Calculate cursor position for scroll adjustment
    i32 textLen = static_cast<i32>(displayText.length());
    if (m_cursorPos < 0) m_cursorPos = 0;
    if (m_cursorPos > textLen) m_cursorPos = textLen;
    
    f32 cursorXInText = 0.0f;
    if (m_cursorPos > 0 && !displayText.empty()) {
        std::string textBeforeCursor = displayText.substr(0, m_cursorPos);
        cursorXInText = renderer->MeasureText(textBeforeCursor, font).x;
    }
    
    // Calculate visible width (content area)
    f32 visibleWidth = m_contentBounds.width;
    
    // Adjust scroll offset to keep cursor visible
    f32 cursorScreenX = cursorXInText - m_scrollOffset;
    
    // If cursor is past the right edge, scroll right
    if (cursorScreenX > visibleWidth - 2.0f) {
        m_scrollOffset = cursorXInText - visibleWidth + 2.0f;
    }
    // If cursor is past the left edge, scroll left
    if (cursorScreenX < 0.0f) {
        m_scrollOffset = cursorXInText;
    }
    // Don't scroll past the beginning
    if (m_scrollOffset < 0.0f) {
        m_scrollOffset = 0.0f;
    }
    
    // Draw placeholder or text
    if (displayText.empty() && !m_placeholder.empty()) {
        Color pc = textColor; 
        pc.a *= 0.5f;
        renderer->DrawText(m_placeholder, m_contentBounds, pc, font);
    } else if (!displayText.empty()) {
        // Create clipped bounds for text rendering
        Rect2D textBounds = m_contentBounds;
        textBounds.x -= m_scrollOffset;
        textBounds.width += m_scrollOffset;  // Extend width to allow text to render
        
        // Use clip rect to hide overflow
        renderer->PushClipRect(m_contentBounds);
        renderer->DrawText(displayText, textBounds, textColor, font, HorizontalAlign::Left, VerticalAlign::Center);
        renderer->PopClipRect();
    }
    
    // Draw cursor if focused
    if (m_focused) {
        bool cursorVisible = (m_cursorBlinkTime < 0.5f);
        if (cursorVisible) {
            f32 cursorX = m_contentBounds.x + cursorXInText - m_scrollOffset;
            
            // Only draw cursor if it's within visible area
            if (cursorX >= m_contentBounds.x && cursorX <= m_contentBounds.x + m_contentBounds.width) {
                f32 textHeight = renderer->MeasureText("Ag", font).y;
                f32 cursorHeight = textHeight;
                f32 cursorY = m_contentBounds.y + (m_contentBounds.height - cursorHeight) * 0.5f;
                
                renderer->DrawLine(cursorX, cursorY, cursorX, cursorY + cursorHeight, textColor, 2.0f);
            }
        }
    }
}

} // namespace Panorama