#include "CPanel2D.h"
#include "CUIRenderer.h"
#include "CStyleSheet.h"
#include "CUIEngine.h"
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Panorama {

// ============ Panel Type Names ============

static const std::unordered_map<PanelType, std::string> s_panelTypeNames = {
    {PanelType::Panel, "Panel"},
    {PanelType::Label, "Label"},
    {PanelType::Image, "Image"},
    {PanelType::Button, "Button"},
    {PanelType::TextEntry, "TextEntry"},
    {PanelType::DropDown, "DropDown"},
    {PanelType::Slider, "Slider"},
    {PanelType::ProgressBar, "ProgressBar"},
    {PanelType::RadioButton, "RadioButton"},
    {PanelType::ToggleButton, "ToggleButton"},
    {PanelType::Frame, "Frame"},
    {PanelType::ScrollPanel, "ScrollPanel"},
    {PanelType::TabPanel, "TabPanel"},
    {PanelType::DOTAHUDOverlay, "DOTAHUDOverlay"},
    {PanelType::DOTAAbilityPanel, "DOTAAbilityPanel"},
    {PanelType::DOTAItemPanel, "DOTAItemPanel"},
    {PanelType::DOTAHeroImage, "DOTAHeroImage"},
    {PanelType::DOTAUnitFrame, "DOTAUnitFrame"},
    {PanelType::DOTAMinimap, "DOTAMinimap"},
    {PanelType::DOTAShop, "DOTAShop"},
    {PanelType::DOTAScoreboard, "DOTAScoreboard"}
};

// ============ CPanel2D Implementation ============

CPanel2D::CPanel2D() = default;

CPanel2D::CPanel2D(const std::string& id) : m_id(id) {}

CPanel2D::~CPanel2D() {
    RemoveAndDeleteChildren();
}

const std::string& CPanel2D::GetPanelTypeName() const {
    auto it = s_panelTypeNames.find(m_panelType);
    if (it != s_panelTypeNames.end()) return it->second;
    static std::string unknown = "Unknown";
    return unknown;
}

// ============ Hierarchy ============

CPanel2D* CPanel2D::GetChild(i32 index) const {
    if (index >= 0 && index < static_cast<i32>(m_children.size())) {
        return m_children[index].get();
    }
    return nullptr;
}

void CPanel2D::SetParent(CPanel2D* parent) {
    if (m_parent == parent) return;
    
    if (m_parent) {
        m_parent->RemoveChild(this);
    }
    m_parent = parent;
}

void CPanel2D::AddChild(std::shared_ptr<CPanel2D> child) {
    if (!child || child.get() == this) return;
    
    if (child->m_parent) {
        child->m_parent->RemoveChild(child.get());
    }
    
    child->m_parent = this;
    m_children.push_back(child);
    InvalidateLayout();
}

void CPanel2D::RemoveChild(CPanel2D* child) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const std::shared_ptr<CPanel2D>& p) { return p.get() == child; });
    
    if (it != m_children.end()) {
        (*it)->m_parent = nullptr;
        m_children.erase(it);
        InvalidateLayout();
    }
}

void CPanel2D::RemoveAndDeleteChildren() {
    for (auto& child : m_children) {
        child->m_parent = nullptr;
    }
    m_children.clear();
    InvalidateLayout();
}

void CPanel2D::MoveChildBefore(CPanel2D* child, CPanel2D* before) {
    auto childIt = std::find_if(m_children.begin(), m_children.end(),
        [child](const auto& p) { return p.get() == child; });
    if (childIt == m_children.end()) return;
    
    auto ptr = *childIt;
    m_children.erase(childIt);
    
    auto beforeIt = std::find_if(m_children.begin(), m_children.end(),
        [before](const auto& p) { return p.get() == before; });
    m_children.insert(beforeIt, ptr);
}

void CPanel2D::MoveChildAfter(CPanel2D* child, CPanel2D* after) {
    auto childIt = std::find_if(m_children.begin(), m_children.end(),
        [child](const auto& p) { return p.get() == child; });
    if (childIt == m_children.end()) return;
    
    auto ptr = *childIt;
    m_children.erase(childIt);
    
    auto afterIt = std::find_if(m_children.begin(), m_children.end(),
        [after](const auto& p) { return p.get() == after; });
    if (afterIt != m_children.end()) ++afterIt;
    m_children.insert(afterIt, ptr);
}

CPanel2D* CPanel2D::FindChild(const std::string& id) {
    for (auto& child : m_children) {
        if (child && child->m_id == id) return child.get();
    }
    return nullptr;
}

CPanel2D* CPanel2D::FindChildTraverse(const std::string& id) {
    if (m_id == id) return this;
    for (auto& child : m_children) {
        if (child) {
            if (auto found = child->FindChildTraverse(id)) {
                return found;
            }
        }
    }
    return nullptr;
}

std::vector<CPanel2D*> CPanel2D::FindChildrenWithClass(const std::string& className) {
    std::vector<CPanel2D*> result;
    if (HasClass(className)) result.push_back(this);
    for (auto& child : m_children) {
        if (child) {
            auto childResults = child->FindChildrenWithClass(className);
            result.insert(result.end(), childResults.begin(), childResults.end());
        }
    }
    return result;
}

// ============ Classes ============

void CPanel2D::AddClass(const std::string& className) {
    if (!HasClass(className)) {
        m_classes.push_back(className);
        InvalidateStyle();
    }
}

void CPanel2D::RemoveClass(const std::string& className) {
    auto it = std::find(m_classes.begin(), m_classes.end(), className);
    if (it != m_classes.end()) {
        m_classes.erase(it);
        InvalidateStyle();
    }
}

void CPanel2D::ToggleClass(const std::string& className) {
    if (HasClass(className)) RemoveClass(className);
    else AddClass(className);
}

bool CPanel2D::HasClass(const std::string& className) const {
    return std::find(m_classes.begin(), m_classes.end(), className) != m_classes.end();
}

void CPanel2D::SetHasClass(const std::string& className, bool has) {
    if (has) AddClass(className);
    else RemoveClass(className);
}

void CPanel2D::SwitchClass(const std::string& oldClass, const std::string& newClass) {
    RemoveClass(oldClass);
    AddClass(newClass);
}

// ============ Style ============

void CPanel2D::SetStyleProperty(const std::string& property, const DataValue& value) {
    // Map property name to style property
    if (property == "opacity" && std::holds_alternative<f32>(value)) {
        m_inlineStyle.opacity = std::get<f32>(value);
    } else if (property == "visible" && std::holds_alternative<bool>(value)) {
        m_inlineStyle.visible = std::get<bool>(value);
    }
    // Add more property mappings as needed
    InvalidateStyle();
}

DataValue CPanel2D::GetStyleProperty(const std::string& property) const {
    if (property == "opacity" && m_computedStyle.opacity.has_value()) {
        return m_computedStyle.opacity.value();
    } else if (property == "visible" && m_computedStyle.visible.has_value()) {
        return m_computedStyle.visible.value();
    }
    return std::monostate{};
}

void CPanel2D::ApplyStyles(const CStyleSheet* stylesheet) {
    if (stylesheet) {
        m_computedStyle = stylesheet->ComputeStyle(this);
    }
    m_computedStyle.Merge(m_inlineStyle);

    // Keep inheritance consistent with ComputeStyle() path.
    if (m_parent) {
        const StyleProperties& ps = m_parent->m_computedStyle;
        if (!m_computedStyle.color.has_value() && ps.color.has_value()) m_computedStyle.color = ps.color;
        if (!m_computedStyle.fontSize.has_value() && ps.fontSize.has_value()) m_computedStyle.fontSize = ps.fontSize;
        if (!m_computedStyle.fontFamily.has_value() && ps.fontFamily.has_value()) m_computedStyle.fontFamily = ps.fontFamily;
        if (!m_computedStyle.fontWeight.has_value() && ps.fontWeight.has_value()) m_computedStyle.fontWeight = ps.fontWeight;
        if (!m_computedStyle.fontStyle.has_value() && ps.fontStyle.has_value()) m_computedStyle.fontStyle = ps.fontStyle;
        if (!m_computedStyle.textAlign.has_value() && ps.textAlign.has_value()) m_computedStyle.textAlign = ps.textAlign;
        if (!m_computedStyle.verticalTextAlign.has_value() && ps.verticalTextAlign.has_value()) m_computedStyle.verticalTextAlign = ps.verticalTextAlign;
        if (!m_computedStyle.letterSpacing.has_value() && ps.letterSpacing.has_value()) m_computedStyle.letterSpacing = ps.letterSpacing;
        if (!m_computedStyle.lineHeight.has_value() && ps.lineHeight.has_value()) m_computedStyle.lineHeight = ps.lineHeight;
    }

    m_styleInvalid = false;
}

void CPanel2D::InvalidateStyle() {
    m_styleInvalid = true;
    for (auto& child : m_children) {
        if (child) child->InvalidateStyle();
    }
}

void CPanel2D::ComputeStyle() {
    // Debug: log style computation
    static int styleLogCount = 0;
    if (styleLogCount < 10) {
        bool hasInlineBg = m_inlineStyle.backgroundColor.has_value();
        LOG_INFO("CPanel2D::ComputeStyle id='{}' hasInlineBg={}", m_id, hasInlineBg);
        styleLogCount++;
    }
    
    // Apply styles from global stylesheet
    m_computedStyle = CStyleManager::Instance().ComputeStyle(this);
    m_computedStyle.Merge(m_inlineStyle);

    // Basic CSS-like inheritance for common text properties.
    // Panorama panels are separate nodes; without this, setting font/color on a parent
    // (e.g. a Button) does not affect a child Label, leading to wrong sizing.
    if (m_parent) {
        const StyleProperties& ps = m_parent->m_computedStyle;

        // Inheritable text props (only inherit if not explicitly set on this panel).
        if (!m_computedStyle.color.has_value() && ps.color.has_value()) m_computedStyle.color = ps.color;
        if (!m_computedStyle.fontSize.has_value() && ps.fontSize.has_value()) m_computedStyle.fontSize = ps.fontSize;
        if (!m_computedStyle.fontFamily.has_value() && ps.fontFamily.has_value()) m_computedStyle.fontFamily = ps.fontFamily;
        if (!m_computedStyle.fontWeight.has_value() && ps.fontWeight.has_value()) m_computedStyle.fontWeight = ps.fontWeight;
        if (!m_computedStyle.fontStyle.has_value() && ps.fontStyle.has_value()) m_computedStyle.fontStyle = ps.fontStyle;
        if (!m_computedStyle.textAlign.has_value() && ps.textAlign.has_value()) m_computedStyle.textAlign = ps.textAlign;
        if (!m_computedStyle.verticalTextAlign.has_value() && ps.verticalTextAlign.has_value()) m_computedStyle.verticalTextAlign = ps.verticalTextAlign;
        if (!m_computedStyle.letterSpacing.has_value() && ps.letterSpacing.has_value()) m_computedStyle.letterSpacing = ps.letterSpacing;
        if (!m_computedStyle.lineHeight.has_value() && ps.lineHeight.has_value()) m_computedStyle.lineHeight = ps.lineHeight;
    }

    m_styleInvalid = false;
}

// ============ Layout ============

f32 CPanel2D::ResolveLength(const Length& len, f32 parentSize, f32 viewportSize) const {
    switch (len.unit) {
        case LengthUnit::Pixels: return len.value;
        case LengthUnit::Percent: return parentSize * len.value / 100.0f;
        case LengthUnit::ViewportWidth: return viewportSize * len.value / 100.0f;
        case LengthUnit::ViewportHeight: return viewportSize * len.value / 100.0f;
        case LengthUnit::Fill: return parentSize;
        default: return len.value;
    }
}

void CPanel2D::PerformLayout(const Rect2D& parentBounds) {
    if (m_styleInvalid) ComputeStyle();
    
    // Calculate dimensions
    f32 width = parentBounds.width;
    f32 height = parentBounds.height;
    
    if (m_computedStyle.width.has_value()) {
        width = ResolveLength(m_computedStyle.width.value(), parentBounds.width, parentBounds.width);
    }
    if (m_computedStyle.height.has_value()) {
        height = ResolveLength(m_computedStyle.height.value(), parentBounds.height, parentBounds.height);
    }
    
    // Apply min/max constraints
    if (m_computedStyle.minWidth.has_value()) {
        width = std::max(width, ResolveLength(m_computedStyle.minWidth.value(), parentBounds.width, parentBounds.width));
    }
    if (m_computedStyle.maxWidth.has_value()) {
        width = std::min(width, ResolveLength(m_computedStyle.maxWidth.value(), parentBounds.width, parentBounds.width));
    }
    if (m_computedStyle.minHeight.has_value()) {
        height = std::max(height, ResolveLength(m_computedStyle.minHeight.value(), parentBounds.height, parentBounds.height));
    }
    if (m_computedStyle.maxHeight.has_value()) {
        height = std::min(height, ResolveLength(m_computedStyle.maxHeight.value(), parentBounds.height, parentBounds.height));
    }
    
    // Calculate position
    f32 x = parentBounds.x;
    f32 y = parentBounds.y;
    
    // Apply margins (margins are already included in parentBounds for flow layouts)
    f32 marginLeft = m_computedStyle.marginLeft.has_value() ? ResolveLength(m_computedStyle.marginLeft.value(), parentBounds.width, parentBounds.width) : 0;
    f32 marginTop = m_computedStyle.marginTop.has_value() ? ResolveLength(m_computedStyle.marginTop.value(), parentBounds.height, parentBounds.height) : 0;
    f32 marginRight = m_computedStyle.marginRight.has_value() ? ResolveLength(m_computedStyle.marginRight.value(), parentBounds.width, parentBounds.width) : 0;
    f32 marginBottom = m_computedStyle.marginBottom.has_value() ? ResolveLength(m_computedStyle.marginBottom.value(), parentBounds.height, parentBounds.height) : 0;
    
    // Only apply margins if not in a flow layout (parent will handle positioning)
    // We detect flow layout by checking if parent has flow direction set
    bool inFlowLayout = (m_parent && m_parent->m_computedStyle.flowChildren.has_value() && 
                         m_parent->m_computedStyle.flowChildren.value() != FlowDirection::None);
    
    if (!inFlowLayout) {
        x += marginLeft;
        y += marginTop;
    }
    
    // Apply alignment
    HorizontalAlign hAlign = m_computedStyle.horizontalAlign.value_or(HorizontalAlign::Left);
    VerticalAlign vAlign = m_computedStyle.verticalAlign.value_or(VerticalAlign::Top);
    
    switch (hAlign) {
        case HorizontalAlign::Center:
            x = parentBounds.x + (parentBounds.width - width) / 2;
            break;
        case HorizontalAlign::Right:
            x = parentBounds.x + parentBounds.width - width - marginRight;
            break;
        default: break;
    }
    
    switch (vAlign) {
        case VerticalAlign::Center:
            y = parentBounds.y + (parentBounds.height - height) / 2;
            break;
        case VerticalAlign::Bottom:
            y = parentBounds.y + parentBounds.height - height - marginBottom;
            break;
        default: break;
    }
    
    m_actualBounds = {x, y, width, height};
    
    // Calculate content bounds (minus padding)
    f32 padLeft = m_computedStyle.paddingLeft.has_value() ? ResolveLength(m_computedStyle.paddingLeft.value(), width, width) : 0;
    f32 padTop = m_computedStyle.paddingTop.has_value() ? ResolveLength(m_computedStyle.paddingTop.value(), height, height) : 0;
    f32 padRight = m_computedStyle.paddingRight.has_value() ? ResolveLength(m_computedStyle.paddingRight.value(), width, width) : 0;
    f32 padBottom = m_computedStyle.paddingBottom.has_value() ? ResolveLength(m_computedStyle.paddingBottom.value(), height, height) : 0;
    
    m_contentBounds = {
        x + padLeft,
        y + padTop,
        width - padLeft - padRight,
        height - padTop - padBottom
    };
    
    // Layout children based on flow direction
    FlowDirection flow = m_computedStyle.flowChildren.value_or(FlowDirection::None);
    f32 childX = m_contentBounds.x;
    f32 childY = m_contentBounds.y;
    
    for (auto& child : m_children) {
        if (!child || !child->IsVisible()) continue;
        
        // Get child margins
        f32 marginLeft = child->m_computedStyle.marginLeft.has_value() ? 
            child->ResolveLength(child->m_computedStyle.marginLeft.value(), m_contentBounds.width, m_contentBounds.width) : 0;
        f32 marginTop = child->m_computedStyle.marginTop.has_value() ? 
            child->ResolveLength(child->m_computedStyle.marginTop.value(), m_contentBounds.height, m_contentBounds.height) : 0;
        f32 marginRight = child->m_computedStyle.marginRight.has_value() ? 
            child->ResolveLength(child->m_computedStyle.marginRight.value(), m_contentBounds.width, m_contentBounds.width) : 0;
        f32 marginBottom = child->m_computedStyle.marginBottom.has_value() ? 
            child->ResolveLength(child->m_computedStyle.marginBottom.value(), m_contentBounds.height, m_contentBounds.height) : 0;
        
        Rect2D childParentBounds = m_contentBounds;
        if (flow == FlowDirection::Down) {
            // Apply top margin and set position
            childY += marginTop;
            childParentBounds.y = childY;
            childParentBounds.height = m_contentBounds.y + m_contentBounds.height - childY;
        } else if (flow == FlowDirection::Right || flow == FlowDirection::RightWrap) {
            // Apply left margin and set position
            childX += marginLeft;
            childParentBounds.x = childX;
            childParentBounds.width = m_contentBounds.x + m_contentBounds.width - childX;
        }
        
        child->PerformLayout(childParentBounds);
        
        if (flow == FlowDirection::Down) {
            childY += child->GetActualHeight() + marginBottom;
        } else if (flow == FlowDirection::Right) {
            childX += child->GetActualWidth() + marginRight;
        }
    }
    
    m_layoutInvalid = false;
}

void CPanel2D::InvalidateLayout() {
    m_layoutInvalid = true;
}

Vector2D CPanel2D::GetPositionWithinWindow() const {
    Vector2D pos = {m_actualBounds.x, m_actualBounds.y};
    // Could traverse up to get absolute position
    return pos;
}

bool CPanel2D::IsPointInPanel(f32 x, f32 y) const {
    return m_actualBounds.Contains(x, y);
}

// ============ Visibility & State ============

void CPanel2D::SetVisible(bool visible) {
    if (m_visible != visible) {
        m_visible = visible;
        InvalidateLayout();
    }
}

void CPanel2D::SetEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled) {
        m_hovered = false;
        m_pressed = false;
    }
}

void CPanel2D::SetFocus() {
    m_focused = true;
    PanelEvent event;
    event.type = PanelEventType::OnFocus;
    event.target = this;
    DispatchEvent(event);
}

void CPanel2D::RemoveFocus() {
    m_focused = false;
    PanelEvent event;
    event.type = PanelEventType::OnBlur;
    event.target = this;
    DispatchEvent(event);
}

// ============ Events ============

void CPanel2D::AddEventHandler(PanelEventType type, EventHandler handler) {
    m_eventHandlers[type].push_back(handler);
}

void CPanel2D::RemoveEventHandler(PanelEventType type) {
    m_eventHandlers.erase(type);
}

void CPanel2D::DispatchEvent(PanelEvent& event) {
    event.currentTarget = this;
    
    auto it = m_eventHandlers.find(event.type);
    if (it != m_eventHandlers.end()) {
        for (auto& handler : it->second) {
            handler(event);
            if (!event.bubbles) return;
        }
    }
}

void CPanel2D::DispatchEventUp(PanelEvent& event) {
    DispatchEvent(event);
    if (event.bubbles && m_parent) {
        m_parent->DispatchEventUp(event);
    }
}

void CPanel2D::SetPanelEvent(const std::string& eventName, EventHandler handler) {
    static const std::unordered_map<std::string, PanelEventType> eventMap = {
        {"onmouseover", PanelEventType::OnMouseOver},
        {"onmouseout", PanelEventType::OnMouseOut},
        {"onactivate", PanelEventType::OnLeftClick},
        {"oncontextmenu", PanelEventType::OnRightClick},
        {"onfocus", PanelEventType::OnFocus},
        {"onblur", PanelEventType::OnBlur},
        {"onload", PanelEventType::OnLoad}
    };
    
    auto it = eventMap.find(eventName);
    if (it != eventMap.end()) {
        AddEventHandler(it->second, handler);
    }
}

// ============ Data Binding ============

void CPanel2D::SetDialogVariable(const std::string& name, const DataValue& value) {
    m_dialogVariables[name] = value;
    UpdateBindings();
}

DataValue CPanel2D::GetDialogVariable(const std::string& name) const {
    auto it = m_dialogVariables.find(name);
    return it != m_dialogVariables.end() ? it->second : DataValue{};
}

void CPanel2D::SetDialogVariableInt(const std::string& name, i32 value) {
    SetDialogVariable(name, value);
}

void CPanel2D::SetDialogVariableFloat(const std::string& name, f32 value) {
    SetDialogVariable(name, value);
}

void CPanel2D::SetDialogVariableString(const std::string& name, const std::string& value) {
    SetDialogVariable(name, value);
}

void CPanel2D::AddDataBinding(const DataBinding& binding) {
    m_dataBindings.push_back(binding);
}

void CPanel2D::UpdateBindings() {
    // Update bound properties from dialog variables
    for (const auto& binding : m_dataBindings) {
        auto it = m_dialogVariables.find(binding.expression);
        if (it != m_dialogVariables.end()) {
            SetStyleProperty(binding.property, it->second);
        }
    }
}

// ============ Animation ============

void CPanel2D::StartAnimation(const std::string& animationName) {
    // Would get animation from registry
    // const AnimationDef* anim = CStyleManager::Instance().GetAnimation(animationName);
    
    ActiveAnimation active;
    active.name = animationName;
    active.elapsed = 0;
    active.duration = 1.0f;
    active.iteration = 0;
    active.maxIterations = 1;
    m_activeAnimations.push_back(active);
}

void CPanel2D::StopAnimation(const std::string& animationName) {
    m_activeAnimations.erase(
        std::remove_if(m_activeAnimations.begin(), m_activeAnimations.end(),
            [&](const ActiveAnimation& a) { return a.name == animationName; }),
        m_activeAnimations.end());
}

void CPanel2D::StopAllAnimations() {
    m_activeAnimations.clear();
}

void CPanel2D::TransitionToClass(const std::string& className, f32 duration) {
    AddClass(className);
    // Would trigger CSS transitions
}

// ============ Update & Render ============

void CPanel2D::Update(f32 deltaTime) {
    // Update animations
    for (auto& anim : m_activeAnimations) {
        anim.elapsed += deltaTime;
        if (anim.elapsed >= anim.duration) {
            anim.iteration++;
            if (anim.maxIterations > 0 && anim.iteration >= anim.maxIterations) {
                // Animation complete
            } else {
                anim.elapsed = 0;
                if (anim.alternate) anim.forward = !anim.forward;
            }
        }
    }
    
    // Remove completed animations
    m_activeAnimations.erase(
        std::remove_if(m_activeAnimations.begin(), m_activeAnimations.end(),
            [](const ActiveAnimation& a) { 
                return a.maxIterations > 0 && a.iteration >= a.maxIterations; 
            }),
        m_activeAnimations.end());
    
    // Update children - copy list to avoid issues if tree is modified
    auto children = m_children;
    for (auto& child : children) {
        if (child) {
            child->Update(deltaTime);
        }
    }
}

void CPanel2D::Render(CUIRenderer* renderer) {
    if (!m_visible || !renderer) return;
    
    f32 opacity = m_computedStyle.opacity.value_or(1.0f);
    if (opacity <= 0) return;
    
    // Debug logging for hero buttons when pressed
    if (m_pressed && m_id.find("HeroBtn") != std::string::npos) {
        LOG_INFO("CPanel2D::Render PRESSED id='{}' bounds=({:.0f},{:.0f},{:.0f},{:.0f})",
            m_id, m_actualBounds.x, m_actualBounds.y, m_actualBounds.width, m_actualBounds.height);
        spdlog::default_logger()->flush();
    }
    
    // Debug: log first few panel renders
    static int renderLogCount = 0;
    if (renderLogCount < 10) {
        bool hasBg = m_computedStyle.backgroundColor.has_value();
        bool hasInlineBg = m_inlineStyle.backgroundColor.has_value();
        LOG_INFO("CPanel2D::Render id='{}' bounds=({:.0f},{:.0f},{:.0f},{:.0f}) hasBg={} hasInlineBg={}",
            m_id, m_actualBounds.x, m_actualBounds.y, m_actualBounds.width, m_actualBounds.height,
            hasBg, hasInlineBg);
        renderLogCount++;
    }
    
    // Draw background
    if (m_computedStyle.backgroundColor.has_value()) {
        Color bg = m_computedStyle.backgroundColor.value();
        bg.a *= opacity;
        
        f32 radius = m_computedStyle.borderRadius.value_or(0);
        if (radius > 0) {
            renderer->DrawRoundedRect(m_actualBounds, bg, radius);
        } else {
            renderer->DrawRect(m_actualBounds, bg);
        }
    }
    
    // Draw gradient if specified
    if (m_computedStyle.backgroundGradientStart.has_value() && m_computedStyle.backgroundGradientEnd.has_value()) {
        renderer->DrawGradientRect(m_actualBounds, 
            m_computedStyle.backgroundGradientStart.value(),
            m_computedStyle.backgroundGradientEnd.value());
    }
    
    // Draw border
    if (m_computedStyle.borderWidth.has_value() && m_computedStyle.borderWidth.value() > 0) {
        Color borderColor = m_computedStyle.borderColor.value_or(Color::White());
        borderColor.a *= opacity;
        renderer->DrawRectOutline(m_actualBounds, borderColor, m_computedStyle.borderWidth.value());
    }
    
    // Draw box shadow
    if (m_computedStyle.boxShadowBlur.has_value()) {
        renderer->DrawBoxShadow(m_actualBounds,
            m_computedStyle.boxShadowColor.value_or(Color(0, 0, 0, 0.5f)),
            m_computedStyle.boxShadowOffsetX.value_or(0),
            m_computedStyle.boxShadowOffsetY.value_or(2),
            m_computedStyle.boxShadowBlur.value(),
            m_computedStyle.boxShadowSpread.value_or(0),
            m_computedStyle.boxShadowInset.value_or(false));
    }
    
    // Apply clipping if enabled (use content bounds to respect padding)
    bool shouldClip = m_computedStyle.clipChildren.value_or(false);
    if (shouldClip) {
        renderer->PushClipRect(m_contentBounds);
    }
    
    // Render children
    for (auto& child : m_children) {
        if (child) child->Render(renderer);
    }
    
    // Remove clipping
    if (shouldClip) {
        renderer->PopClipRect();
    }
}

// ============ Input Handling ============

bool CPanel2D::OnMouseMove(f32 x, f32 y) {
    bool wasHovered = m_hovered;
    m_hovered = IsPointInPanel(x, y) && m_enabled && m_visible;
    
    if (m_hovered != wasHovered) {
        PanelEvent event;
        event.type = m_hovered ? PanelEventType::OnMouseOver : PanelEventType::OnMouseOut;
        event.target = this;
        event.mouseX = x;
        event.mouseY = y;
        DispatchEvent(event);
    }
    
    // Propagate to children - copy list to avoid issues if tree is modified
    auto children = m_children;
    for (auto& child : children) {
        if (child) {
            child->OnMouseMove(x, y);
        }
    }
    
    return m_hovered;
}

bool CPanel2D::OnMouseDown(f32 x, f32 y, i32 button) {
    if (!m_enabled || !m_visible) return false;
    
    // Debug: log entry with depth tracking
    static thread_local int s_mouseDownDepth = 0;
    s_mouseDownDepth++;
    if (s_mouseDownDepth > 100) {
        LOG_ERROR("OnMouseDown recursion too deep! id='{}' depth={}", m_id, s_mouseDownDepth);
        spdlog::default_logger()->flush();
        s_mouseDownDepth--;
        return false;
    }
    
    // Copy children list - tree may be modified during iteration
    std::vector<std::shared_ptr<CPanel2D>> children;
    children.reserve(m_children.size());
    for (const auto& c : m_children) {
        if (c) children.push_back(c);
    }
    
    // Check children first (reverse for z-order)
    for (size_t i = children.size(); i > 0; --i) {
        auto& child = children[i - 1];
        if (child && child.get()) {
            if (child->OnMouseDown(x, y, button)) {
                s_mouseDownDepth--;
                return true;
            }
        }
    }
    
    if (IsPointInPanel(x, y) && m_acceptsInput) {
        m_pressed = true;
        
        // Set focus via CUIEngine so it tracks the focused panel
        Panorama::CUIEngine::Instance().SetFocus(this);
        
        PanelEvent event;
        event.type = PanelEventType::OnMouseDown;
        event.target = this;
        event.mouseX = x;
        event.mouseY = y;
        event.button = button;
        DispatchEvent(event);
        s_mouseDownDepth--;
        return true;
    }
    s_mouseDownDepth--;
    return false;
}

bool CPanel2D::OnMouseUp(f32 x, f32 y, i32 button) {
    bool wasPressed = m_pressed;
    m_pressed = false;
    
    // Debug logging only for hero buttons that were pressed
    if (wasPressed && m_id.find("HeroBtn") != std::string::npos) {
        LOG_INFO("OnMouseUp id='{}' wasPressed=true pos=({:.0f},{:.0f})", m_id, x, y);
        spdlog::default_logger()->flush();
    }
    
    // Copy children list - callback may modify the tree
    auto children = m_children;
    
    // Check children first
    for (size_t i = 0; i < children.size(); ++i) {
        auto& child = children[children.size() - 1 - i]; // reverse order
        if (child && child->OnMouseUp(x, y, button)) return true;
    }
    
    if (wasPressed && IsPointInPanel(x, y)) {
        PanelEvent event;
        event.type = PanelEventType::OnMouseUp;
        event.target = this;
        event.mouseX = x;
        event.mouseY = y;
        event.button = button;
        DispatchEvent(event);
        
        // Fire click event
        event.type = (button == 0) ? PanelEventType::OnLeftClick : PanelEventType::OnRightClick;
        DispatchEvent(event);
        return true;
    }
    return false;
}

bool CPanel2D::OnMouseWheel(f32 x, f32 y, i32 delta) {
    if (!IsPointInPanel(x, y)) return false;
    
    // Copy children list - tree may be modified during iteration
    auto children = m_children;
    for (size_t i = children.size(); i > 0; --i) {
        auto& child = children[i - 1];
        if (child && child->OnMouseWheel(x, y, delta)) return true;
    }
    
    PanelEvent event;
    event.type = PanelEventType::OnMouseWheel;
    event.target = this;
    event.mouseX = x;
    event.mouseY = y;
    event.wheelDelta = delta;
    DispatchEvent(event);
    return true;
}

bool CPanel2D::OnKeyDown(i32 key) {
    return false;
}

bool CPanel2D::OnKeyUp(i32 key) {
    return false;
}

bool CPanel2D::OnTextInput(const std::string& text) {
    return false;
}

// ============ Attributes ============

void CPanel2D::SetAttribute(const std::string& name, const std::string& value) {
    m_attributes[name] = value;
}

std::string CPanel2D::GetAttribute(const std::string& name) const {
    auto it = m_attributes.find(name);
    return it != m_attributes.end() ? it->second : "";
}

bool CPanel2D::HasAttribute(const std::string& name) const {
    return m_attributes.find(name) != m_attributes.end();
}

} // namespace Panorama
