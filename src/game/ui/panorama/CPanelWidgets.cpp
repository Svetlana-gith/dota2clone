#include "CPanel2D.h"
#include "CUIRenderer.h"
#include <algorithm>
#include <cmath>

// Prevent Windows min/max macros
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Panorama {

// ============ CLabel Implementation ============

CLabel::CLabel() {
    m_panelType = PanelType::Label;
}

CLabel::CLabel(const std::string& text, const std::string& id) 
    : CPanel2D(id), m_text(text) {
    m_panelType = PanelType::Label;
}

void CLabel::SetText(const std::string& text) {
    m_text = text;
}

void CLabel::SetLocString(const std::string& token) {
    m_locToken = token;
}

void CLabel::Render(CUIRenderer* renderer) {
    if (!m_visible) return;  // Check visibility before rendering
    
    CPanel2D::Render(renderer);
    if (m_text.empty() || !renderer) return;
    
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    if (opacity <= 0) return;
    
    FontInfo font;
    font.size = m_computedStyle.fontSize.value_or(16.0f);
    font.bold = m_computedStyle.fontWeight.has_value() && m_computedStyle.fontWeight.value() == "bold";
    font.family = m_computedStyle.fontFamily.value_or(font.family);
    // Letter spacing is interpreted as PIXELS (CSS-like `letter-spacing`).
    // Default to a subtle tracking; can be overridden per-label via style `letterSpacing`.
    // Clamp to a sane range to avoid accidentally exploding spacing.
    const f32 defaultLetterSpacingPx = 0.7f;
    font.letterSpacing = std::max(0.0f, m_computedStyle.letterSpacing.value_or(defaultLetterSpacingPx));
    font.letterSpacing = std::min(font.letterSpacing, font.size * 0.25f);
    
    Color textColor = m_computedStyle.color.value_or(Color::White());
    textColor.a *= opacity;
    
    HorizontalAlign hAlign = m_computedStyle.textAlign.value_or(HorizontalAlign::Left);
    VerticalAlign vAlign = m_computedStyle.verticalTextAlign.value_or(VerticalAlign::Top);
    
    renderer->DrawText(m_text, m_contentBounds, textColor, font, hAlign, vAlign);
}

// ============ CImage Implementation ============

CImage::CImage() { m_panelType = PanelType::Image; }

CImage::CImage(const std::string& src, const std::string& id)
    : CPanel2D(id), m_imagePath(src) { m_panelType = PanelType::Image; }

void CImage::SetImage(const std::string& path) { m_imagePath = path; }

void CImage::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (m_imagePath.empty() || !renderer) return;
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    if (opacity <= 0) return;
    renderer->DrawImage(m_imagePath, m_actualBounds, opacity);
}

// ============ CButton Implementation ============

CButton::CButton() {
    m_panelType = PanelType::Button;
    m_acceptsInput = true;
    m_label = std::make_shared<CLabel>();
    // Center the LABEL panel inside the button...
    m_label->GetStyle().horizontalAlign = HorizontalAlign::Center;
    m_label->GetStyle().verticalAlign = VerticalAlign::Center;
    // ...and center the TEXT inside the label (CLabel reads textAlign/verticalTextAlign).
    m_label->GetStyle().textAlign = HorizontalAlign::Center;
    m_label->GetStyle().verticalTextAlign = VerticalAlign::Center;
    // Make label fill the button so centering uses full button bounds.
    m_label->GetStyle().width = Length::Fill();
    m_label->GetStyle().height = Length::Fill();
    m_label->SetAcceptsInput(false); // Label should not block button clicks
    AddChild(m_label);
    m_inlineStyle.backgroundColor = Color(0.25f, 0.25f, 0.3f, 0.9f);
    m_inlineStyle.borderRadius = 6.0f;
    m_inlineStyle.borderWidth = 1.0f;
    m_inlineStyle.borderColor = Color(0.4f, 0.4f, 0.45f, 0.8f);
}

CButton::CButton(const std::string& text, const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::Button;
    m_acceptsInput = true;
    m_label = std::make_shared<CLabel>(text);
    // Center the LABEL panel inside the button...
    m_label->GetStyle().horizontalAlign = HorizontalAlign::Center;
    m_label->GetStyle().verticalAlign = VerticalAlign::Center;
    // ...and center the TEXT inside the label (CLabel reads textAlign/verticalTextAlign).
    m_label->GetStyle().textAlign = HorizontalAlign::Center;
    m_label->GetStyle().verticalTextAlign = VerticalAlign::Center;
    m_label->GetStyle().width = Length::Fill();
    m_label->GetStyle().height = Length::Fill();
    m_label->SetAcceptsInput(false); // Label should not block button clicks
    AddChild(m_label);
    m_inlineStyle.backgroundColor = Color(0.25f, 0.25f, 0.3f, 0.9f);
    m_inlineStyle.borderRadius = 6.0f;
    m_inlineStyle.borderWidth = 1.0f;
    m_inlineStyle.borderColor = Color(0.4f, 0.4f, 0.45f, 0.8f);
}

void CButton::SetText(const std::string& text) { if (m_label) m_label->SetText(text); }
const std::string& CButton::GetText() const { static std::string e; return m_label ? m_label->GetText() : e; }
void CButton::SetOnActivate(std::function<void()> handler) { m_onActivate = handler; }

bool CButton::OnMouseUp(f32 x, f32 y, i32 button) {
    bool wasPressed = m_pressed;
    bool result = CPanel2D::OnMouseUp(x, y, button);
    if (wasPressed && IsPointInPanel(x, y) && button == 0 && m_onActivate) m_onActivate();
    return result;
}

void CButton::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    
    // Save original background color
    auto originalBg = m_computedStyle.backgroundColor;
    
    // Get base color (prefer computed, fallback to inline)
    Color baseColor = m_computedStyle.backgroundColor.value_or(
        m_inlineStyle.backgroundColor.value_or(Color(0.25f, 0.25f, 0.3f, 0.9f))
    );
    
    // Apply hover/pressed effects
    if (m_pressed) {
        // Darken when pressed (30% darker)
        baseColor.r *= 0.7f;
        baseColor.g *= 0.7f;
        baseColor.b *= 0.7f;
        m_computedStyle.backgroundColor = baseColor;
    } else if (m_hovered) {
        // Brighten when hovered (20% brighter)
        baseColor.r = std::min(1.0f, baseColor.r * 1.2f);
        baseColor.g = std::min(1.0f, baseColor.g * 1.2f);
        baseColor.b = std::min(1.0f, baseColor.b * 1.2f);
        m_computedStyle.backgroundColor = baseColor;
    }
    
    // Render with modified color
    CPanel2D::Render(renderer);
    
    // Restore original color
    m_computedStyle.backgroundColor = originalBg;
}

bool CButton::OnKeyDown(i32 key) {
    if (m_focused && (key == 13 || key == 32)) { if (m_onActivate) m_onActivate(); return true; }
    return false;
}

// ============ CProgressBar Implementation ============

CProgressBar::CProgressBar() {
    m_panelType = PanelType::ProgressBar;
    m_inlineStyle.backgroundColor = Color(0.15f, 0.15f, 0.2f, 0.9f);
    m_inlineStyle.borderRadius = 4.0f;
}

CProgressBar::CProgressBar(const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::ProgressBar;
    m_inlineStyle.backgroundColor = Color(0.15f, 0.15f, 0.2f, 0.9f);
    m_inlineStyle.borderRadius = 4.0f;
}

void CProgressBar::SetValue(f32 value) {
    if (value < m_min) value = m_min;
    if (value > m_max) value = m_max;
    m_value = value;
}

void CProgressBar::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (!renderer) return;
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    if (opacity <= 0) return;
    
    f32 normalizedValue = (m_value - m_min) / (m_max - m_min);
    f32 fillWidth = m_actualBounds.width * normalizedValue;
    
    if (fillWidth > 0) {
        Rect2D fillRect = m_actualBounds;
        fillRect.width = fillWidth;
        fillRect.x += 2; fillRect.y += 2;
        fillRect.width -= 4; fillRect.height -= 4;
        Color fillColor = Color(0.2f, 0.7f, 0.2f, 0.9f);
        fillColor.a *= opacity;
        f32 radius = m_computedStyle.borderRadius.value_or(4.0f) - 2;
        if (radius > 0) renderer->DrawRoundedRect(fillRect, fillColor, radius);
        else renderer->DrawRect(fillRect, fillColor);
    }
}

// ============ CTextEntry Implementation ============

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
}

void CTextEntry::SetFocus() {
    CPanel2D::SetFocus();
    // Сбросить мерцание курсора при получении фокуса
    m_cursorBlinkTime = 0.0f;
}

bool CTextEntry::OnKeyDown(i32 key) {
    if (!m_focused) return false;
    
    // Сбросить мерцание курсора при любой активности
    m_cursorBlinkTime = 0.0f;
    
    bool changed = false;
    if (key == 8 && !m_text.empty() && m_cursorPos > 0) { m_text.erase(m_cursorPos - 1, 1); m_cursorPos--; changed = true; }
    if (key == 46 && m_cursorPos < static_cast<i32>(m_text.length())) { m_text.erase(m_cursorPos, 1); changed = true; }
    if (key == 37 && m_cursorPos > 0) { m_cursorPos--; return true; }
    if (key == 39 && m_cursorPos < static_cast<i32>(m_text.length())) { m_cursorPos++; return true; }
    if (key == 36) { m_cursorPos = 0; return true; }
    if (key == 35) { m_cursorPos = static_cast<i32>(m_text.length()); return true; }
    if (key == 13) { PanelEvent event; event.type = PanelEventType::OnInputSubmit; event.target = this; DispatchEvent(event); return true; }
    if (changed && m_onTextChanged) { m_onTextChanged(m_text); }
    return changed;
}

bool CTextEntry::OnTextInput(const std::string& text) {
    if (!m_focused) return false;
    if (m_maxChars > 0 && static_cast<i32>(m_text.length()) + static_cast<i32>(text.length()) > m_maxChars) return false;
    
    // Сбросить мерцание курсора при вводе текста
    m_cursorBlinkTime = 0.0f;
    
    m_text.insert(m_cursorPos, text);
    m_cursorPos += static_cast<i32>(text.length());
    if (m_onTextChanged) { m_onTextChanged(m_text); }
    return true;
}

bool CTextEntry::OnMouseDown(f32 x, f32 y, i32 button) {
    // Вызвать базовый обработчик для установки фокуса
    bool handled = CPanel2D::OnMouseDown(x, y, button);
    
    // При клике левой кнопкой мыши установить курсор в конец текста
    if (button == 0 && IsPointInPanel(x, y)) {
        m_cursorPos = static_cast<i32>(m_text.length());
        m_cursorBlinkTime = 0.0f;  // Сбросить мерцание
        return true;
    }
    
    return handled;
}

void CTextEntry::Update(f32 deltaTime) {
    CPanel2D::Update(deltaTime);
    
    // Обновить мерцание курсора
    if (m_focused) {
        m_cursorBlinkTime += deltaTime;
        // Мерцание каждые 0.5 секунды (полный цикл 1 секунда)
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
    
    // Настроить шрифт из стиля
    FontInfo font;
    font.size = m_computedStyle.fontSize.value_or(16.0f);
    font.family = m_computedStyle.fontFamily.value_or("Roboto Condensed");
    font.bold = m_computedStyle.fontWeight.has_value() && m_computedStyle.fontWeight.value() == "bold";
    
    Color textColor = m_computedStyle.color.value_or(Color::White()); 
    textColor.a *= opacity;
    
    std::string displayText = m_isPassword ? std::string(m_text.length(), '*') : m_text;
    
    // Отрисовать placeholder или текст
    if (displayText.empty() && !m_placeholder.empty()) {
        Color pc = textColor; 
        pc.a *= 0.5f;
        renderer->DrawText(m_placeholder, m_contentBounds, pc, font);
    } else {
        renderer->DrawText(displayText, m_contentBounds, textColor, font, HorizontalAlign::Left, VerticalAlign::Center);
    }
    
    // Нарисовать курсор, если поле в фокусе
    if (m_focused) {
        // Мерцание: видим первую половину секунды, невидим вторую
        bool cursorVisible = (m_cursorBlinkTime < 0.5f);
        if (cursorVisible) {
            // Убедиться что cursorPos в допустимых пределах
            if (m_cursorPos < 0) m_cursorPos = 0;
            if (m_cursorPos > static_cast<i32>(displayText.length())) {
                m_cursorPos = static_cast<i32>(displayText.length());
            }
            
            // Вычислить позицию курсора: измерить ширину текста до позиции курсора
            std::string textBeforeCursor = displayText.substr(0, m_cursorPos);
            f32 cursorX = m_contentBounds.x;
            
            if (!textBeforeCursor.empty()) {
                Vector2D textSize = renderer->MeasureText(textBeforeCursor, font);
                cursorX += textSize.x;
            }
            
            // Вычислить высоту текста для центрирования
            f32 textHeight = renderer->MeasureText("Ag", font).y;
            f32 cursorHeight = textHeight;
            
            // Центрировать курсор по вертикали в contentBounds
            f32 cursorY = m_contentBounds.y + (m_contentBounds.height - cursorHeight) * 0.5f;
            
            Color cursorColor = textColor;
            renderer->DrawLine(cursorX, cursorY, cursorX, cursorY + cursorHeight, cursorColor, 2.0f);
        }
    }
}

// ============ CSlider Implementation ============

CSlider::CSlider() {
    m_panelType = PanelType::Slider;
    m_acceptsInput = true;
    m_inlineStyle.backgroundColor = Color(0.15f, 0.15f, 0.2f, 0.9f);
    m_inlineStyle.borderRadius = 4.0f;
}

CSlider::CSlider(const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::Slider;
    m_acceptsInput = true;
}

void CSlider::SetValue(f32 value) {
    f32 oldValue = m_value;
    if (value < m_min) value = m_min;
    if (value > m_max) value = m_max;
    m_value = value;
    if (m_step > 0) m_value = std::round((m_value - m_min) / m_step) * m_step + m_min;
    if (m_value != oldValue && m_onValueChanged) m_onValueChanged(m_value);
}

void CSlider::SetRange(f32 min, f32 max) { m_min = min; m_max = max; SetValue(m_value); }

bool CSlider::OnMouseDown(f32 x, f32 y, i32 button) {
    if (button == 0 && IsPointInPanel(x, y)) { m_dragging = true; OnMouseMove(x, y); return true; }
    return CPanel2D::OnMouseDown(x, y, button);
}

bool CSlider::OnMouseMove(f32 x, f32 y) {
    CPanel2D::OnMouseMove(x, y);
    if (m_dragging) {
        f32 ratio;
        if (m_vertical) ratio = 1.0f - (y - m_actualBounds.y) / m_actualBounds.height;
        else ratio = (x - m_actualBounds.x) / m_actualBounds.width;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        SetValue(m_min + ratio * (m_max - m_min));
        return true;
    }
    return false;
}

bool CSlider::OnMouseUp(f32 x, f32 y, i32 button) {
    if (button == 0) m_dragging = false;
    return CPanel2D::OnMouseUp(x, y, button);
}

void CSlider::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (!renderer) return;
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    
    Rect2D trackRect = m_actualBounds;
    if (m_vertical) { trackRect.x += m_actualBounds.width / 2 - 3; trackRect.width = 6; }
    else { trackRect.y += m_actualBounds.height / 2 - 3; trackRect.height = 6; }
    Color trackColor(0.2f, 0.2f, 0.25f, 0.9f); trackColor.a *= opacity;
    renderer->DrawRoundedRect(trackRect, trackColor, 3);
    
    f32 ratio = (m_value - m_min) / (m_max - m_min);
    f32 thumbX, thumbY;
    if (m_vertical) { thumbX = m_actualBounds.x + m_actualBounds.width / 2; thumbY = m_actualBounds.y + m_actualBounds.height * (1.0f - ratio); }
    else { thumbX = m_actualBounds.x + m_actualBounds.width * ratio; thumbY = m_actualBounds.y + m_actualBounds.height / 2; }
    Color thumbColor = m_hovered || m_dragging ? Color(0.5f, 0.5f, 0.55f, 1.0f) : Color(0.4f, 0.4f, 0.45f, 1.0f);
    thumbColor.a *= opacity;
    renderer->DrawCircle(thumbX, thumbY, 8, thumbColor, true);
}

// ============ CDropDown Implementation ============

CDropDown::CDropDown() {
    m_panelType = PanelType::DropDown;
    m_acceptsInput = true;
    m_inlineStyle.backgroundColor = Color(0.15f, 0.15f, 0.2f, 0.95f);
    m_inlineStyle.borderRadius = 4.0f;
    m_inlineStyle.borderWidth = 1.0f;
    m_inlineStyle.borderColor = Color(0.3f, 0.3f, 0.35f, 0.8f);
}

CDropDown::CDropDown(const std::string& id) : CPanel2D(id) {
    m_panelType = PanelType::DropDown;
    m_acceptsInput = true;
}

void CDropDown::AddOption(const std::string& id, const std::string& text) {
    m_options.push_back({id, text});
    if (m_selectedId.empty()) m_selectedId = id;
}

void CDropDown::RemoveOption(const std::string& id) {
    m_options.erase(std::remove_if(m_options.begin(), m_options.end(),
        [&](const Option& o) { return o.id == id; }), m_options.end());
}

void CDropDown::ClearOptions() { m_options.clear(); m_selectedId.clear(); }

void CDropDown::SetSelected(const std::string& id) {
    for (const auto& opt : m_options) {
        if (opt.id == id) {
            std::string oldId = m_selectedId;
            m_selectedId = id;
            if (oldId != id && m_onSelectionChanged) m_onSelectionChanged(id);
            return;
        }
    }
}

bool CDropDown::OnMouseUp(f32 x, f32 y, i32 button) {
    if (button == 0 && IsPointInPanel(x, y)) { m_isOpen = !m_isOpen; return true; }
    if (m_isOpen && button == 0) m_isOpen = false;
    return CPanel2D::OnMouseUp(x, y, button);
}

void CDropDown::Render(CUIRenderer* renderer) {
    if (!m_visible) return;
    CPanel2D::Render(renderer);
    if (!renderer) return;
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    
    std::string selectedText;
    for (const auto& opt : m_options) if (opt.id == m_selectedId) { selectedText = opt.text; break; }
    
    FontInfo font; font.size = m_computedStyle.fontSize.value_or(16.0f);
    Color textColor = m_computedStyle.color.value_or(Color::White()); textColor.a *= opacity;
    renderer->DrawText(selectedText, m_contentBounds, textColor, font);
    
    if (m_isOpen) {
        f32 optionY = m_actualBounds.y + m_actualBounds.height;
        for (const auto& opt : m_options) {
            Rect2D optRect = {m_actualBounds.x, optionY, m_actualBounds.width, 30};
            Color bgColor(0.2f, 0.2f, 0.25f, 0.95f); bgColor.a *= opacity;
            renderer->DrawRect(optRect, bgColor);
            renderer->DrawText(opt.text, optRect, textColor, font);
            optionY += 30;
        }
    }
}

} // namespace Panorama
