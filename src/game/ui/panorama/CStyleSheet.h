#pragma once
/**
 * Panorama CSS-like Stylesheet System
 * Parses and applies styles similar to Valve's Panorama CSS
 */

#include "PanoramaTypes.h"
#include <unordered_map>
#include <optional>

namespace Panorama {

// ============ Style Properties ============

struct StyleProperties {
    // Dimensions
    std::optional<Length> width;
    std::optional<Length> height;
    std::optional<Length> minWidth;
    std::optional<Length> minHeight;
    std::optional<Length> maxWidth;
    std::optional<Length> maxHeight;
    
    // Position
    std::optional<Length> x;
    std::optional<Length> y;
    std::optional<HorizontalAlign> horizontalAlign;
    std::optional<VerticalAlign> verticalAlign;
    
    // Margin (Valve uses margin-left, margin-top, etc.)
    std::optional<Length> marginLeft;
    std::optional<Length> marginRight;
    std::optional<Length> marginTop;
    std::optional<Length> marginBottom;
    
    // Padding
    std::optional<Length> paddingLeft;
    std::optional<Length> paddingRight;
    std::optional<Length> paddingTop;
    std::optional<Length> paddingBottom;
    
    // Background
    std::optional<Color> backgroundColor;
    std::optional<std::string> backgroundImage;
    std::optional<f32> backgroundSize;
    std::optional<std::string> backgroundRepeat;
    
    // Background gradient (Valve style)
    std::optional<Color> backgroundGradientStart;
    std::optional<Color> backgroundGradientEnd;
    std::optional<std::string> backgroundGradientDirection;
    
    // Border
    std::optional<f32> borderWidth;
    std::optional<Color> borderColor;
    std::optional<std::string> borderStyle;
    std::optional<f32> borderRadius;
    std::optional<f32> borderTopLeftRadius;
    std::optional<f32> borderTopRightRadius;
    std::optional<f32> borderBottomLeftRadius;
    std::optional<f32> borderBottomRightRadius;
    
    // Box shadow (Valve: box-shadow)
    std::optional<Color> boxShadowColor;
    std::optional<f32> boxShadowOffsetX;
    std::optional<f32> boxShadowOffsetY;
    std::optional<f32> boxShadowBlur;
    std::optional<f32> boxShadowSpread;
    std::optional<bool> boxShadowInset;
    
    // Text
    std::optional<Color> color;
    std::optional<f32> fontSize;
    std::optional<std::string> fontFamily;
    std::optional<std::string> fontWeight;
    std::optional<std::string> fontStyle;
    std::optional<HorizontalAlign> textAlign;
    std::optional<VerticalAlign> verticalTextAlign;
    std::optional<bool> textOverflow;
    std::optional<Color> textShadowColor;
    std::optional<f32> textShadowOffsetX;
    std::optional<f32> textShadowOffsetY;
    std::optional<f32> letterSpacing;
    std::optional<f32> lineHeight;
    
    // Layout
    std::optional<FlowDirection> flowChildren;
    std::optional<Overflow> overflow;
    std::optional<bool> clipChildren;
    
    // Visibility & Opacity
    std::optional<bool> visible;
    std::optional<f32> opacity;
    std::optional<f32> preTransformScale2D;
    std::optional<f32> transformOriginX;
    std::optional<f32> transformOriginY;
    
    // Transform (Valve: transform)
    std::optional<f32> translateX;
    std::optional<f32> translateY;
    std::optional<f32> scaleX;
    std::optional<f32> scaleY;
    std::optional<f32> rotateZ;
    
    // 3D Transform (Valve supports 3D)
    std::optional<f32> translateZ;
    std::optional<f32> rotateX;
    std::optional<f32> rotateY;
    std::optional<f32> perspective;
    
    // Filters (Valve: blur, saturation, etc.)
    std::optional<f32> blur;
    std::optional<f32> saturation;
    std::optional<f32> brightness;
    std::optional<f32> contrast;
    std::optional<Color> washColor;
    
    // Transitions
    std::vector<TransitionDef> transitions;
    
    // Animation
    std::optional<std::string> animationName;
    std::optional<f32> animationDuration;
    std::optional<f32> animationDelay;
    std::optional<i32> animationIterations;
    std::optional<EasingFunction> animationTimingFunction;
    
    // Sound (Valve: sound-out)
    std::optional<std::string> soundEnter;
    std::optional<std::string> soundLeave;
    std::optional<std::string> soundClick;
    
    // Merge another style (later properties override)
    void Merge(const StyleProperties& other);
    
    // Reset to defaults
    void Reset();
};

// ============ Style Rule ============

struct StyleSelector {
    std::string element;           // Panel type: "Button", "Label"
    std::string id;                // #my-panel
    std::vector<std::string> classes;  // .my-class
    std::string pseudoClass;       // :hover, :active, :focus, :disabled
    std::string pseudoElement;     // ::before, ::after
    
    // Combinators
    std::string descendant;        // "Panel Label" (space)
    std::string child;             // "Panel > Label" (>)
    
    i32 GetSpecificity() const;
    bool Matches(const class CPanel2D* panel) const;
};

struct StyleRule {
    StyleSelector selector;
    StyleProperties properties;
    i32 sourceOrder = 0;
};

// ============ Stylesheet ============

class CStyleSheet {
public:
    CStyleSheet() = default;
    ~CStyleSheet() = default;
    
    // Parse CSS-like stylesheet from string
    bool Parse(const std::string& css);
    
    // Parse from file
    bool LoadFromFile(const std::string& path);
    
    // Get computed style for a panel
    StyleProperties ComputeStyle(const class CPanel2D* panel) const;
    
    // Add a rule programmatically
    void AddRule(const StyleRule& rule);
    
    // Clear all rules
    void Clear();
    
    // Get all rules
    const std::vector<StyleRule>& GetRules() const { return m_rules; }
    
    // Register animation keyframes
    void RegisterAnimation(const std::string& name, const AnimationDef& anim);
    const AnimationDef* GetAnimation(const std::string& name) const;
    
private:
    std::vector<StyleRule> m_rules;
    std::unordered_map<std::string, AnimationDef> m_animations;
    i32 m_ruleCounter = 0;
    
    // CSS parsing helpers
    StyleSelector ParseSelector(const std::string& selectorStr);
    StyleProperties ParseProperties(const std::string& block);
    Length ParseLength(const std::string& value);
    Color ParseColor(const std::string& value);
    EasingFunction ParseEasing(const std::string& value);
};

// ============ Global Style Manager ============

class CStyleManager {
public:
    static CStyleManager& Instance();
    
    // Load global stylesheet
    void LoadGlobalStyles(const std::string& path);
    
    // Register panel-specific stylesheet
    void RegisterStyleSheet(const std::string& panelType, std::shared_ptr<CStyleSheet> sheet);
    
    // Get computed style for panel
    StyleProperties ComputeStyle(const class CPanel2D* panel) const;
    
    // Default styles
    const StyleProperties& GetDefaultStyle() const { return m_defaultStyle; }
    
private:
    CStyleManager();
    
    std::shared_ptr<CStyleSheet> m_globalStyles;
    std::unordered_map<std::string, std::shared_ptr<CStyleSheet>> m_panelStyles;
    StyleProperties m_defaultStyle;
};

} // namespace Panorama
