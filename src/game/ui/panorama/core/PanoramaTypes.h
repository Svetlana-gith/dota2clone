#pragma once
/**
 * Panorama UI System - Valve-style UI Framework
 * Core types and definitions
 */

#include "../../../core/Types.h"
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>

namespace Panorama {

// Forward declarations
class Panel;
class CPanel2D;
class CStyleSheet;
class CLayoutFile;

// ============ Core Types ============

struct Color {
    f32 r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    
    Color() = default;
    Color(f32 r, f32 g, f32 b, f32 a = 1.0f) : r(r), g(g), b(b), a(a) {}
    
    static Color FromHex(u32 hex) {
        return Color(
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            (hex & 0xFF) / 255.0f,
            1.0f
        );
    }
    
    static Color FromRGBA(u32 hex) {
        return Color(
            ((hex >> 24) & 0xFF) / 255.0f,
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            (hex & 0xFF) / 255.0f
        );
    }
    
    static Color White() { return Color(1, 1, 1, 1); }
    static Color Black() { return Color(0, 0, 0, 1); }
    static Color Transparent() { return Color(0, 0, 0, 0); }
    static Color Red() { return Color(0.9f, 0.2f, 0.2f, 1); }
    static Color Green() { return Color(0.2f, 0.8f, 0.2f, 1); }
    static Color Blue() { return Color(0.2f, 0.4f, 0.9f, 1); }
    static Color Gold() { return Color(0.85f, 0.65f, 0.13f, 1); }
    
    Color operator*(f32 s) const { return Color(r * s, g * s, b * s, a); }
    Color WithAlpha(f32 alpha) const { return Color(r, g, b, alpha); }
};

struct Vector2D {
    f32 x = 0, y = 0;
    Vector2D() = default;
    Vector2D(f32 x, f32 y) : x(x), y(y) {}
    Vector2D operator+(const Vector2D& o) const { return {x + o.x, y + o.y}; }
    Vector2D operator-(const Vector2D& o) const { return {x - o.x, y - o.y}; }
    Vector2D operator*(f32 s) const { return {x * s, y * s}; }
};

struct Rect2D {
    f32 x = 0, y = 0, width = 0, height = 0;
    
    bool Contains(f32 px, f32 py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
    bool Contains(const Vector2D& p) const { return Contains(p.x, p.y); }
    Vector2D GetCenter() const { return {x + width / 2, y + height / 2}; }
};

// ============ CSS-like Units ============

enum class LengthUnit {
    Pixels,         // px
    Percent,        // %
    ViewportWidth,  // vw
    ViewportHeight, // vh
    Fill,           // fill-parent-flow
    FitChildren,    // fit-children
    Auto
};

struct Length {
    f32 value = 0;
    LengthUnit unit = LengthUnit::Pixels;
    
    Length() = default;
    Length(f32 v, LengthUnit u = LengthUnit::Pixels) : value(v), unit(u) {}
    
    static Length Px(f32 v) { return Length(v, LengthUnit::Pixels); }
    static Length Pct(f32 v) { return Length(v, LengthUnit::Percent); }
    static Length Fill() { return Length(100, LengthUnit::Fill); }
    static Length FitChildren() { return Length(0, LengthUnit::FitChildren); }
    static Length Auto() { return Length(0, LengthUnit::Auto); }
};

// ============ Layout Enums ============

enum class FlowDirection {
    None,
    Down,       // flow-children: down
    Right,      // flow-children: right
    RightWrap   // flow-children: right-wrap
};

enum class HorizontalAlign { Left, Center, Right };
enum class VerticalAlign { Top, Center, Bottom };

enum class Overflow {
    Visible,
    Hidden,     // overflow: clip
    Scroll,     // overflow: scroll
    Squish      // overflow: squish
};

// ============ Flexbox Layout Enums ============

enum class FlexDirection {
    Row,        // flex-direction: row
    Column      // flex-direction: column
};

enum class JustifyContent {
    Start,          // justify-content: start (flex-start)
    Center,         // justify-content: center
    End,            // justify-content: end (flex-end)
    SpaceBetween    // justify-content: space-between
};

enum class AlignItems {
    Start,      // align-items: start (flex-start)
    Center,     // align-items: center
    End,        // align-items: end (flex-end)
    Stretch     // align-items: stretch
};

enum class FlexWrap {
    NoWrap,     // flex-wrap: nowrap (default)
    Wrap,       // flex-wrap: wrap
    WrapReverse // flex-wrap: wrap-reverse
};

enum class AlignContent {
    Start,          // align-content: start (flex-start)
    Center,         // align-content: center
    End,            // align-content: end (flex-end)
    SpaceBetween,   // align-content: space-between
    SpaceAround,    // align-content: space-around
    Stretch         // align-content: stretch
};

// ============ Event System ============

enum class PanelEventType {
    // Mouse events
    OnMouseOver,
    OnMouseOut,
    OnMouseDown,
    OnMouseUp,
    OnLeftClick,
    OnRightClick,
    OnDoubleClick,
    OnMouseMove,
    OnMouseWheel,
    
    // Focus events
    OnFocus,
    OnBlur,
    OnInputSubmit,
    
    // Panel lifecycle
    OnLoad,
    OnUnload,
    OnActivate,
    OnDeactivate,
    
    // Animation events
    OnAnimationStart,
    OnAnimationEnd,
    OnTransitionEnd,
    
    // Custom
    OnPropertyChange
};

struct PanelEvent {
    PanelEventType type;
    class CPanel2D* target = nullptr;
    class CPanel2D* currentTarget = nullptr;
    f32 mouseX = 0, mouseY = 0;
    i32 button = 0;
    i32 wheelDelta = 0;
    bool bubbles = true;
    bool defaultPrevented = false;
    
    void StopPropagation() { bubbles = false; }
    void PreventDefault() { defaultPrevented = true; }
};

using EventHandler = std::function<void(const PanelEvent&)>;

// ============ Data Binding ============

using DataValue = std::variant<std::monostate, bool, i32, f32, std::string>;

struct DataBinding {
    std::string property;
    std::string expression;
    bool twoWay = false;
};

// ============ Transition/Animation ============

enum class EasingFunction {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    EaseInQuad,
    EaseOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInBack,
    EaseOutBack,
    EaseInBounce,
    EaseOutBounce,
    Spring
};

struct TransitionDef {
    std::string property;
    f32 duration = 0.3f;
    f32 delay = 0.0f;
    EasingFunction easing = EasingFunction::EaseOut;
};

struct KeyframeDef {
    f32 time;  // 0.0 - 1.0
    std::map<std::string, DataValue> properties;
};

struct AnimationDef {
    std::string name;
    f32 duration = 1.0f;
    f32 delay = 0.0f;
    i32 iterations = 1;  // -1 = infinite
    bool alternate = false;
    EasingFunction easing = EasingFunction::Linear;
    std::vector<KeyframeDef> keyframes;
};

// ============ Panel Types (Valve naming) ============

enum class PanelType {
    Panel,          // Base panel
    Label,          // Text label
    Image,          // Image display
    Button,         // Clickable button
    TextEntry,      // Text input
    DropDown,       // Dropdown selector
    Slider,         // Slider control
    ProgressBar,    // Progress indicator
    RadioButton,    // Radio button
    ToggleButton,   // Toggle/checkbox
    
    // Container types
    Frame,          // Framed container
    ScrollPanel,    // Scrollable container
    TabPanel,       // Tab container
    
    // Game-specific (Dota 2 style)
    DOTAHUDOverlay,
    DOTAAbilityPanel,
    DOTAItemPanel,
    DOTAHeroImage,
    DOTAUnitFrame,
    DOTAMinimap,
    DOTAShop,
    DOTAScoreboard
};

} // namespace Panorama
