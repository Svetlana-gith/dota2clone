#pragma once
/**
 * CPanel2D - Base Panorama Panel Class
 * Equivalent to Valve's Panel base class in Panorama
 */

#include "PanoramaTypes.h"
#include "CStyleSheet.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace Panorama {

class CUIEngine;
class CLayoutFile;

// ============ Panel Base Class ============

class CPanel2D : public std::enable_shared_from_this<CPanel2D> {
public:
    CPanel2D();
    explicit CPanel2D(const std::string& id);
    virtual ~CPanel2D();
    
    // ============ Identification ============
    const std::string& GetID() const { return m_id; }
    void SetID(const std::string& id) { m_id = id; }
    PanelType GetPanelType() const { return m_panelType; }
    const std::string& GetPanelTypeName() const;
    
    // ============ Hierarchy (Valve API) ============
    CPanel2D* GetParent() const { return m_parent; }
    const std::vector<std::shared_ptr<CPanel2D>>& GetChildren() const { return m_children; }
    i32 GetChildCount() const { return static_cast<i32>(m_children.size()); }
    CPanel2D* GetChild(i32 index) const;
    
    void SetParent(CPanel2D* parent);
    void AddChild(std::shared_ptr<CPanel2D> child);
    void RemoveChild(CPanel2D* child);
    void RemoveAndDeleteChildren();
    void MoveChildBefore(CPanel2D* child, CPanel2D* before);
    void MoveChildAfter(CPanel2D* child, CPanel2D* after);
    
    // Find panels (Valve: FindChildTraverse, FindChildInLayoutFile)
    CPanel2D* FindChild(const std::string& id);
    CPanel2D* FindChildTraverse(const std::string& id);
    std::vector<CPanel2D*> FindChildrenWithClass(const std::string& className);
    
    // ============ Classes (CSS-like) ============
    void AddClass(const std::string& className);
    void RemoveClass(const std::string& className);
    void ToggleClass(const std::string& className);
    bool HasClass(const std::string& className) const;
    void SetHasClass(const std::string& className, bool has);
    void SwitchClass(const std::string& oldClass, const std::string& newClass);
    const std::vector<std::string>& GetClasses() const { return m_classes; }
    
    // ============ Style ============
    StyleProperties& GetStyle() { return m_inlineStyle; }
    const StyleProperties& GetStyle() const { return m_inlineStyle; }
    const StyleProperties& GetComputedStyle() const { return m_computedStyle; }
    
    void SetStyleProperty(const std::string& property, const DataValue& value);
    DataValue GetStyleProperty(const std::string& property) const;
    
    void ApplyStyles(const CStyleSheet* stylesheet);
    void InvalidateStyle();
    
    // ============ Layout & Bounds ============
    const Rect2D& GetActualBounds() const { return m_actualBounds; }
    const Rect2D& GetContentBounds() const { return m_contentBounds; }
    
    f32 GetActualWidth() const { return m_actualBounds.width; }
    f32 GetActualHeight() const { return m_actualBounds.height; }
    f32 GetActualX() const { return m_actualBounds.x; }
    f32 GetActualY() const { return m_actualBounds.y; }
    
    Vector2D GetPositionWithinWindow() const;
    bool IsPointInPanel(f32 x, f32 y) const;
    
    virtual void PerformLayout(const Rect2D& parentBounds);
    void InvalidateLayout();
    
    // ============ Visibility & State ============
    bool IsVisible() const { return m_visible; }
    void SetVisible(bool visible);
    
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled);
    
    bool IsHovered() const { return m_hovered; }
    bool IsPressed() const { return m_pressed; }
    bool IsFocused() const { return m_focused; }
    bool IsSelected() const { return m_selected; }
    
    virtual void SetFocus();
    void RemoveFocus();
    
    bool IsAcceptingInput() const { return m_acceptsInput && m_enabled && m_visible; }
    void SetAcceptsInput(bool accepts) { m_acceptsInput = accepts; }
    
    // ============ Events (Valve API) ============
    void AddEventHandler(PanelEventType type, EventHandler handler);
    void RemoveEventHandler(PanelEventType type);
    void DispatchEvent(PanelEvent& event);
    void DispatchEventUp(PanelEvent& event);  // Bubble up
    
    // Convenience event setters (like Valve's panel.SetPanelEvent)
    void SetPanelEvent(const std::string& eventName, EventHandler handler);
    
    // ============ Data Binding ============
    void SetDialogVariable(const std::string& name, const DataValue& value);
    DataValue GetDialogVariable(const std::string& name) const;
    void SetDialogVariableInt(const std::string& name, i32 value);
    void SetDialogVariableFloat(const std::string& name, f32 value);
    void SetDialogVariableString(const std::string& name, const std::string& value);
    
    void AddDataBinding(const DataBinding& binding);
    void UpdateBindings();
    
    // ============ Animation ============
    void StartAnimation(const std::string& animationName);
    void StopAnimation(const std::string& animationName);
    void StopAllAnimations();
    bool IsAnimating() const { return !m_activeAnimations.empty(); }
    
    // Transition helpers
    void TransitionToClass(const std::string& className, f32 duration = 0.3f);
    
    // ============ Update & Render ============
    virtual void Update(f32 deltaTime);
    virtual void Render(class CUIRenderer* renderer);
    
    // ============ Input Handling ============
    virtual bool OnMouseMove(f32 x, f32 y);
    virtual bool OnMouseDown(f32 x, f32 y, i32 button);
    virtual bool OnMouseUp(f32 x, f32 y, i32 button);
    virtual bool OnMouseWheel(f32 x, f32 y, i32 delta);
    virtual bool OnKeyDown(i32 key);
    virtual bool OnKeyUp(i32 key);
    virtual bool OnTextInput(const std::string& text);
    
    // ============ Attributes (XML attributes) ============
    void SetAttribute(const std::string& name, const std::string& value);
    std::string GetAttribute(const std::string& name) const;
    bool HasAttribute(const std::string& name) const;
    
protected:
    // Panel identity
    std::string m_id;
    PanelType m_panelType = PanelType::Panel;
    
    // Hierarchy
    CPanel2D* m_parent = nullptr;
    std::vector<std::shared_ptr<CPanel2D>> m_children;
    
    // Style
    std::vector<std::string> m_classes;
    StyleProperties m_inlineStyle;
    StyleProperties m_computedStyle;
    bool m_styleInvalid = true;
    
    // Layout
    Rect2D m_actualBounds;
    Rect2D m_contentBounds;
    bool m_layoutInvalid = true;
    
    // State
    bool m_visible = true;
    bool m_enabled = true;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_focused = false;
    bool m_selected = false;
    bool m_acceptsInput = true;
    
    // Events
    std::unordered_map<PanelEventType, std::vector<EventHandler>> m_eventHandlers;
    
    // Data binding
    std::unordered_map<std::string, DataValue> m_dialogVariables;
    std::vector<DataBinding> m_dataBindings;
    
    // Animation state
    struct ActiveAnimation {
        std::string name;
        f32 elapsed = 0;
        f32 duration = 1.0f;
        i32 iteration = 0;
        i32 maxIterations = 1;
        bool alternate = false;
        bool forward = true;
    };
    std::vector<ActiveAnimation> m_activeAnimations;
    
    // Attributes
    std::unordered_map<std::string, std::string> m_attributes;
    
    // Internal helpers
    void ComputeStyle();
    void UpdateHoverState(f32 x, f32 y);
    f32 ResolveLength(const Length& len, f32 parentSize, f32 viewportSize) const;
};

// ============ Label Panel ============

class CLabel : public CPanel2D {
public:
    CLabel();
    explicit CLabel(const std::string& text, const std::string& id = "");
    
    void SetText(const std::string& text);
    const std::string& GetText() const { return m_text; }
    
    // Localization (Valve: SetLocString)
    void SetLocString(const std::string& token);
    
    // HTML-like text (Valve supports basic HTML in labels)
    void SetHTML(bool html) { m_isHTML = html; }
    bool IsHTML() const { return m_isHTML; }
    
    void Render(CUIRenderer* renderer) override;
    
private:
    std::string m_text;
    std::string m_locToken;
    bool m_isHTML = false;
};

// ============ Image Panel ============

class CImage : public CPanel2D {
public:
    CImage();
    explicit CImage(const std::string& src, const std::string& id = "");
    
    void SetImage(const std::string& path);
    const std::string& GetImage() const { return m_imagePath; }
    
    // Scaling mode (Valve: scaling)
    enum class ScalingMode { None, Stretch, StretchToFitPreserveAspect, StretchToFitXPreserveAspect, StretchToFitYPreserveAspect, StretchToCover };
    void SetScaling(ScalingMode mode) { m_scaling = mode; }
    
    void Render(CUIRenderer* renderer) override;
    
private:
    std::string m_imagePath;
    ScalingMode m_scaling = ScalingMode::Stretch;
};

// ============ Button Panel ============

class CButton : public CPanel2D {
public:
    CButton();
    explicit CButton(const std::string& text, const std::string& id = "");
    
    void SetText(const std::string& text);
    const std::string& GetText() const;
    
    // Click handler (convenience)
    void SetOnActivate(std::function<void()> handler);
    
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    bool OnKeyDown(i32 key) override;
    void Render(CUIRenderer* renderer) override;
    
private:
    std::shared_ptr<CLabel> m_label;
    std::function<void()> m_onActivate;
};

// ============ Progress Bar ============

class CProgressBar : public CPanel2D {
public:
    CProgressBar();
    explicit CProgressBar(const std::string& id);
    
    void SetValue(f32 value);  // 0.0 - 1.0
    f32 GetValue() const { return m_value; }
    
    void SetMin(f32 min) { m_min = min; }
    void SetMax(f32 max) { m_max = max; }
    
    void Render(CUIRenderer* renderer) override;
    
private:
    f32 m_value = 0;
    f32 m_min = 0;
    f32 m_max = 1;
};

// ============ Text Entry ============

class CTextEntry : public CPanel2D {
public:
    CTextEntry();
    explicit CTextEntry(const std::string& id);
    
    void SetText(const std::string& text);
    const std::string& GetText() const { return m_text; }
    
    void SetPlaceholder(const std::string& placeholder) { m_placeholder = placeholder; }
    void SetMaxChars(i32 max) { m_maxChars = max; }
    void SetPassword(bool password) { m_isPassword = password; }
    void SetPasswordMode(bool password) { m_isPassword = password; }  // Alias
    
    void SetOnTextChanged(std::function<void(const std::string&)> handler) { m_onTextChanged = handler; }
    
    void SetFocus() override;
    bool OnKeyDown(i32 key) override;
    bool OnTextInput(const std::string& text) override;
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    void Render(CUIRenderer* renderer) override;
    
    void Update(f32 deltaTime) override;
    
private:
    std::string m_text;
    std::string m_placeholder;
    i32 m_cursorPos = 0;
    i32 m_maxChars = -1;
    bool m_isPassword = false;
    f32 m_cursorBlinkTime = 0.0f;
    std::function<void(const std::string&)> m_onTextChanged;
};

// ============ Slider ============

class CSlider : public CPanel2D {
public:
    CSlider();
    explicit CSlider(const std::string& id);
    
    void SetValue(f32 value);
    f32 GetValue() const { return m_value; }
    
    void SetRange(f32 min, f32 max);
    void SetStep(f32 step) { m_step = step; }
    void SetDirection(bool vertical) { m_vertical = vertical; }
    
    void SetOnValueChanged(std::function<void(f32)> handler) { m_onValueChanged = handler; }
    
    bool OnMouseDown(f32 x, f32 y, i32 button) override;
    bool OnMouseMove(f32 x, f32 y) override;
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    void Render(CUIRenderer* renderer) override;
    
private:
    f32 m_value = 0;
    f32 m_min = 0;
    f32 m_max = 1;
    f32 m_step = 0;
    bool m_vertical = false;
    bool m_dragging = false;
    std::function<void(f32)> m_onValueChanged;
};

// ============ Dropdown ============

class CDropDown : public CPanel2D {
public:
    CDropDown();
    explicit CDropDown(const std::string& id);
    
    void AddOption(const std::string& id, const std::string& text);
    void RemoveOption(const std::string& id);
    void ClearOptions();
    
    void SetSelected(const std::string& id);
    const std::string& GetSelected() const { return m_selectedId; }
    
    void SetOnSelectionChanged(std::function<void(const std::string&)> handler) { m_onSelectionChanged = handler; }
    
    bool OnMouseUp(f32 x, f32 y, i32 button) override;
    void Render(CUIRenderer* renderer) override;
    
private:
    struct Option {
        std::string id;
        std::string text;
    };
    std::vector<Option> m_options;
    std::string m_selectedId;
    bool m_isOpen = false;
    std::function<void(const std::string&)> m_onSelectionChanged;
};

} // namespace Panorama
