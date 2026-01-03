#include "CStyleSheet.h"
#include "CPanel2D.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Panorama {

// ============ StyleProperties Implementation ============

void StyleProperties::Merge(const StyleProperties& other) {
    // Merge all optional properties - other overrides this
    #define MERGE_OPT(prop) if (other.prop.has_value()) prop = other.prop
    
    MERGE_OPT(width);
    MERGE_OPT(height);
    MERGE_OPT(minWidth);
    MERGE_OPT(minHeight);
    MERGE_OPT(maxWidth);
    MERGE_OPT(maxHeight);
    MERGE_OPT(x);
    MERGE_OPT(y);
    MERGE_OPT(horizontalAlign);
    MERGE_OPT(verticalAlign);
    MERGE_OPT(marginLeft);
    MERGE_OPT(marginRight);
    MERGE_OPT(marginTop);
    MERGE_OPT(marginBottom);
    MERGE_OPT(paddingLeft);
    MERGE_OPT(paddingRight);
    MERGE_OPT(paddingTop);
    MERGE_OPT(paddingBottom);
    MERGE_OPT(backgroundColor);
    MERGE_OPT(backgroundImage);
    MERGE_OPT(backgroundSize);
    MERGE_OPT(backgroundRepeat);
    MERGE_OPT(backgroundGradientStart);
    MERGE_OPT(backgroundGradientEnd);
    MERGE_OPT(backgroundGradientDirection);
    MERGE_OPT(borderWidth);
    MERGE_OPT(borderColor);
    MERGE_OPT(borderStyle);
    MERGE_OPT(borderRadius);
    MERGE_OPT(borderTopLeftRadius);
    MERGE_OPT(borderTopRightRadius);
    MERGE_OPT(borderBottomLeftRadius);
    MERGE_OPT(borderBottomRightRadius);
    MERGE_OPT(boxShadowColor);
    MERGE_OPT(boxShadowOffsetX);
    MERGE_OPT(boxShadowOffsetY);
    MERGE_OPT(boxShadowBlur);
    MERGE_OPT(boxShadowSpread);
    MERGE_OPT(boxShadowInset);
    MERGE_OPT(color);
    MERGE_OPT(fontSize);
    MERGE_OPT(fontFamily);
    MERGE_OPT(fontWeight);
    MERGE_OPT(fontStyle);
    MERGE_OPT(textAlign);
    MERGE_OPT(verticalTextAlign);
    MERGE_OPT(textOverflow);
    MERGE_OPT(textShadowColor);
    MERGE_OPT(textShadowOffsetX);
    MERGE_OPT(textShadowOffsetY);
    MERGE_OPT(letterSpacing);
    MERGE_OPT(lineHeight);
    MERGE_OPT(flowChildren);
    MERGE_OPT(overflow);
    MERGE_OPT(clipChildren);
    MERGE_OPT(visible);
    MERGE_OPT(opacity);
    MERGE_OPT(preTransformScale2D);
    MERGE_OPT(transformOriginX);
    MERGE_OPT(transformOriginY);
    MERGE_OPT(translateX);
    MERGE_OPT(translateY);
    MERGE_OPT(scaleX);
    MERGE_OPT(scaleY);
    MERGE_OPT(rotateZ);
    MERGE_OPT(translateZ);
    MERGE_OPT(rotateX);
    MERGE_OPT(rotateY);
    MERGE_OPT(perspective);
    MERGE_OPT(blur);
    MERGE_OPT(saturation);
    MERGE_OPT(brightness);
    MERGE_OPT(contrast);
    MERGE_OPT(washColor);
    MERGE_OPT(animationName);
    MERGE_OPT(animationDuration);
    MERGE_OPT(animationDelay);
    MERGE_OPT(animationIterations);
    MERGE_OPT(animationTimingFunction);
    MERGE_OPT(soundEnter);
    MERGE_OPT(soundLeave);
    MERGE_OPT(soundClick);
    
    #undef MERGE_OPT
    
    // Merge transitions
    if (!other.transitions.empty()) {
        transitions = other.transitions;
    }
}

void StyleProperties::Reset() {
    *this = StyleProperties{};
}

// ============ StyleSelector Implementation ============

i32 StyleSelector::GetSpecificity() const {
    // CSS specificity: (id, class, element)
    i32 spec = 0;
    if (!id.empty()) spec += 100;
    spec += static_cast<i32>(classes.size()) * 10;
    if (!element.empty()) spec += 1;
    if (!pseudoClass.empty()) spec += 10;
    return spec;
}

bool StyleSelector::Matches(const CPanel2D* panel) const {
    if (!panel) return false;
    
    // Check element type
    if (!element.empty() && panel->GetPanelTypeName() != element) {
        return false;
    }
    
    // Check ID
    if (!id.empty() && panel->GetID() != id) {
        return false;
    }
    
    // Check classes
    for (const auto& cls : classes) {
        if (!panel->HasClass(cls)) {
            return false;
        }
    }
    
    // Check pseudo-class
    if (!pseudoClass.empty()) {
        if (pseudoClass == "hover" && !panel->IsHovered()) return false;
        if (pseudoClass == "active" && !panel->IsPressed()) return false;
        if (pseudoClass == "focus" && !panel->IsFocused()) return false;
        if (pseudoClass == "disabled" && panel->IsEnabled()) return false;
        if (pseudoClass == "selected" && !panel->IsSelected()) return false;
    }
    
    return true;
}

// ============ CStyleSheet Implementation ============

bool CStyleSheet::Parse(const std::string& css) {
    // Simple CSS parser
    size_t pos = 0;
    
    while (pos < css.length()) {
        // Skip whitespace and comments
        while (pos < css.length() && (std::isspace(css[pos]) || css[pos] == '/')) {
            if (css[pos] == '/' && pos + 1 < css.length() && css[pos + 1] == '*') {
                // Skip block comment
                pos += 2;
                while (pos + 1 < css.length() && !(css[pos] == '*' && css[pos + 1] == '/')) {
                    pos++;
                }
                pos += 2;
            } else if (std::isspace(css[pos])) {
                pos++;
            } else {
                break;
            }
        }
        
        if (pos >= css.length()) break;
        
        // Check for @keyframes
        if (css.substr(pos, 10) == "@keyframes") {
            // Skip keyframes for now
            size_t braceCount = 0;
            while (pos < css.length()) {
                if (css[pos] == '{') braceCount++;
                else if (css[pos] == '}') {
                    braceCount--;
                    if (braceCount == 0) {
                        pos++;
                        break;
                    }
                }
                pos++;
            }
            continue;
        }
        
        // Find selector (everything before '{')
        size_t selectorStart = pos;
        while (pos < css.length() && css[pos] != '{') {
            pos++;
        }
        
        if (pos >= css.length()) break;
        
        std::string selectorStr = css.substr(selectorStart, pos - selectorStart);
        pos++;  // Skip '{'
        
        // Find properties block (everything before '}')
        size_t blockStart = pos;
        i32 braceCount = 1;
        while (pos < css.length() && braceCount > 0) {
            if (css[pos] == '{') braceCount++;
            else if (css[pos] == '}') braceCount--;
            pos++;
        }
        
        std::string blockStr = css.substr(blockStart, pos - blockStart - 1);
        
        // Parse and add rule
        StyleRule rule;
        rule.selector = ParseSelector(selectorStr);
        rule.properties = ParseProperties(blockStr);
        rule.sourceOrder = m_ruleCounter++;
        m_rules.push_back(rule);
    }
    
    return true;
}

bool CStyleSheet::LoadFromFile(const std::string& path) {
    // Would load file and call Parse()
    // For now, return false
    return false;
}

StyleProperties CStyleSheet::ComputeStyle(const CPanel2D* panel) const {
    StyleProperties result;
    
    // Collect matching rules
    std::vector<const StyleRule*> matchingRules;
    for (const auto& rule : m_rules) {
        if (rule.selector.Matches(panel)) {
            matchingRules.push_back(&rule);
        }
    }
    
    // Sort by specificity, then source order
    std::sort(matchingRules.begin(), matchingRules.end(),
        [](const StyleRule* a, const StyleRule* b) {
            i32 specA = a->selector.GetSpecificity();
            i32 specB = b->selector.GetSpecificity();
            if (specA != specB) return specA < specB;
            return a->sourceOrder < b->sourceOrder;
        });
    
    // Apply rules in order
    for (const auto* rule : matchingRules) {
        result.Merge(rule->properties);
    }
    
    return result;
}

void CStyleSheet::AddRule(const StyleRule& rule) {
    StyleRule r = rule;
    r.sourceOrder = m_ruleCounter++;
    m_rules.push_back(r);
}

void CStyleSheet::Clear() {
    m_rules.clear();
    m_animations.clear();
    m_ruleCounter = 0;
}

void CStyleSheet::RegisterAnimation(const std::string& name, const AnimationDef& anim) {
    m_animations[name] = anim;
}

const AnimationDef* CStyleSheet::GetAnimation(const std::string& name) const {
    auto it = m_animations.find(name);
    return it != m_animations.end() ? &it->second : nullptr;
}

StyleSelector CStyleSheet::ParseSelector(const std::string& selectorStr) {
    StyleSelector sel;
    std::string s = selectorStr;
    
    // Trim whitespace
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start != std::string::npos) {
        s = s.substr(start, end - start + 1);
    }
    
    size_t pos = 0;
    
    // Parse element type (if starts with letter)
    if (pos < s.length() && std::isalpha(s[pos])) {
        size_t elemEnd = pos;
        while (elemEnd < s.length() && (std::isalnum(s[elemEnd]) || s[elemEnd] == '-' || s[elemEnd] == '_')) {
            elemEnd++;
        }
        sel.element = s.substr(pos, elemEnd - pos);
        pos = elemEnd;
    }
    
    // Parse ID, classes, pseudo-classes
    while (pos < s.length()) {
        if (s[pos] == '#') {
            // ID
            pos++;
            size_t idEnd = pos;
            while (idEnd < s.length() && (std::isalnum(s[idEnd]) || s[idEnd] == '-' || s[idEnd] == '_')) {
                idEnd++;
            }
            sel.id = s.substr(pos, idEnd - pos);
            pos = idEnd;
        } else if (s[pos] == '.') {
            // Class
            pos++;
            size_t classEnd = pos;
            while (classEnd < s.length() && (std::isalnum(s[classEnd]) || s[classEnd] == '-' || s[classEnd] == '_')) {
                classEnd++;
            }
            sel.classes.push_back(s.substr(pos, classEnd - pos));
            pos = classEnd;
        } else if (s[pos] == ':') {
            // Pseudo-class
            pos++;
            if (pos < s.length() && s[pos] == ':') {
                // Pseudo-element (::before, ::after)
                pos++;
                size_t pseudoEnd = pos;
                while (pseudoEnd < s.length() && std::isalpha(s[pseudoEnd])) {
                    pseudoEnd++;
                }
                sel.pseudoElement = s.substr(pos, pseudoEnd - pos);
                pos = pseudoEnd;
            } else {
                size_t pseudoEnd = pos;
                while (pseudoEnd < s.length() && std::isalpha(s[pseudoEnd])) {
                    pseudoEnd++;
                }
                sel.pseudoClass = s.substr(pos, pseudoEnd - pos);
                pos = pseudoEnd;
            }
        } else {
            pos++;
        }
    }
    
    return sel;
}

StyleProperties CStyleSheet::ParseProperties(const std::string& block) {
    StyleProperties props;
    
    std::istringstream stream(block);
    std::string line;
    
    while (std::getline(stream, line, ';')) {
        // Find property name and value
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string propName = line.substr(0, colonPos);
        std::string propValue = line.substr(colonPos + 1);
        
        // Trim whitespace
        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            if (start != std::string::npos) {
                s = s.substr(start, end - start + 1);
            } else {
                s.clear();
            }
        };
        
        trim(propName);
        trim(propValue);
        
        // Convert to lowercase for comparison
        std::transform(propName.begin(), propName.end(), propName.begin(), ::tolower);
        
        // Parse property
        if (propName == "width") {
            props.width = ParseLength(propValue);
        } else if (propName == "height") {
            props.height = ParseLength(propValue);
        } else if (propName == "background-color") {
            props.backgroundColor = ParseColor(propValue);
        } else if (propName == "color") {
            props.color = ParseColor(propValue);
        } else if (propName == "font-size") {
            props.fontSize = ParseLength(propValue).value;
        } else if (propName == "font-family") {
            props.fontFamily = propValue;
        } else if (propName == "letter-spacing") {
            props.letterSpacing = ParseLength(propValue).value;
        } else if (propName == "line-height") {
            props.lineHeight = ParseLength(propValue).value;
        } else if (propName == "border-radius") {
            props.borderRadius = ParseLength(propValue).value;
        } else if (propName == "border-width") {
            props.borderWidth = ParseLength(propValue).value;
        } else if (propName == "border-color") {
            props.borderColor = ParseColor(propValue);
        } else if (propName == "opacity") {
            props.opacity = std::stof(propValue);
        } else if (propName == "margin") {
            Length m = ParseLength(propValue);
            props.marginLeft = props.marginRight = props.marginTop = props.marginBottom = m;
        } else if (propName == "margin-left") {
            props.marginLeft = ParseLength(propValue);
        } else if (propName == "margin-right") {
            props.marginRight = ParseLength(propValue);
        } else if (propName == "margin-top") {
            props.marginTop = ParseLength(propValue);
        } else if (propName == "margin-bottom") {
            props.marginBottom = ParseLength(propValue);
        } else if (propName == "padding") {
            Length p = ParseLength(propValue);
            props.paddingLeft = props.paddingRight = props.paddingTop = props.paddingBottom = p;
        } else if (propName == "flow-children") {
            if (propValue == "down") props.flowChildren = FlowDirection::Down;
            else if (propValue == "right") props.flowChildren = FlowDirection::Right;
            else if (propValue == "right-wrap") props.flowChildren = FlowDirection::RightWrap;
            else props.flowChildren = FlowDirection::None;
        } else if (propName == "horizontal-align") {
            if (propValue == "center") props.horizontalAlign = HorizontalAlign::Center;
            else if (propValue == "right") props.horizontalAlign = HorizontalAlign::Right;
            else props.horizontalAlign = HorizontalAlign::Left;
        } else if (propName == "vertical-align") {
            if (propValue == "center") props.verticalAlign = VerticalAlign::Center;
            else if (propValue == "bottom") props.verticalAlign = VerticalAlign::Bottom;
            else props.verticalAlign = VerticalAlign::Top;
        }
        // Add more property parsing as needed
    }
    
    return props;
}

Length CStyleSheet::ParseLength(const std::string& value) {
    if (value == "fill-parent-flow" || value == "100%") {
        return Length::Fill();
    }
    if (value == "fit-children" || value == "auto") {
        return Length::FitChildren();
    }
    
    // Parse numeric value with unit
    f32 num = 0;
    size_t pos = 0;
    
    try {
        num = std::stof(value, &pos);
    } catch (...) {
        return Length::Px(0);
    }
    
    std::string unit = value.substr(pos);
    
    if (unit == "%" || unit == "pct") {
        return Length::Pct(num);
    } else if (unit == "vw") {
        return Length(num, LengthUnit::ViewportWidth);
    } else if (unit == "vh") {
        return Length(num, LengthUnit::ViewportHeight);
    }
    
    return Length::Px(num);
}

Color CStyleSheet::ParseColor(const std::string& value) {
    // Handle named colors
    if (value == "white") return Color::White();
    if (value == "black") return Color::Black();
    if (value == "red") return Color::Red();
    if (value == "green") return Color::Green();
    if (value == "blue") return Color::Blue();
    if (value == "transparent") return Color::Transparent();
    if (value == "gold") return Color::Gold();
    
    // Handle hex colors
    if (value[0] == '#') {
        std::string hex = value.substr(1);
        u32 color = 0;
        
        if (hex.length() == 6) {
            color = std::stoul(hex, nullptr, 16);
            return Color::FromHex(color);
        } else if (hex.length() == 8) {
            color = std::stoul(hex, nullptr, 16);
            return Color::FromRGBA(color);
        }
    }
    
    // Handle rgb/rgba
    if (value.substr(0, 4) == "rgba") {
        // Parse rgba(r, g, b, a)
        size_t start = value.find('(');
        size_t end = value.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string inner = value.substr(start + 1, end - start - 1);
            std::istringstream ss(inner);
            f32 r, g, b, a;
            char comma;
            ss >> r >> comma >> g >> comma >> b >> comma >> a;
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, a);
        }
    } else if (value.substr(0, 3) == "rgb") {
        size_t start = value.find('(');
        size_t end = value.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string inner = value.substr(start + 1, end - start - 1);
            std::istringstream ss(inner);
            f32 r, g, b;
            char comma;
            ss >> r >> comma >> g >> comma >> b;
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
        }
    }
    
    return Color::White();
}

EasingFunction CStyleSheet::ParseEasing(const std::string& value) {
    if (value == "linear") return EasingFunction::Linear;
    if (value == "ease-in") return EasingFunction::EaseIn;
    if (value == "ease-out") return EasingFunction::EaseOut;
    if (value == "ease-in-out") return EasingFunction::EaseInOut;
    if (value == "ease-in-quad") return EasingFunction::EaseInQuad;
    if (value == "ease-out-quad") return EasingFunction::EaseOutQuad;
    if (value == "ease-in-cubic") return EasingFunction::EaseInCubic;
    if (value == "ease-out-cubic") return EasingFunction::EaseOutCubic;
    if (value == "ease-in-back") return EasingFunction::EaseInBack;
    if (value == "ease-out-back") return EasingFunction::EaseOutBack;
    if (value == "ease-in-bounce") return EasingFunction::EaseInBounce;
    if (value == "ease-out-bounce") return EasingFunction::EaseOutBounce;
    if (value == "spring") return EasingFunction::Spring;
    return EasingFunction::Linear;
}

// ============ CStyleManager Implementation ============

CStyleManager& CStyleManager::Instance() {
    static CStyleManager instance;
    return instance;
}

CStyleManager::CStyleManager() {
    m_globalStyles = std::make_shared<CStyleSheet>();
    
    // Set default style
    m_defaultStyle.opacity = 1.0f;
    m_defaultStyle.visible = true;
    m_defaultStyle.color = Color::White();
    m_defaultStyle.fontSize = 16.0f;
}

void CStyleManager::LoadGlobalStyles(const std::string& path) {
    m_globalStyles->LoadFromFile(path);
}

void CStyleManager::RegisterStyleSheet(const std::string& panelType, std::shared_ptr<CStyleSheet> sheet) {
    m_panelStyles[panelType] = sheet;
}

StyleProperties CStyleManager::ComputeStyle(const CPanel2D* panel) const {
    StyleProperties result = m_defaultStyle;
    
    // Apply global styles
    if (m_globalStyles) {
        result.Merge(m_globalStyles->ComputeStyle(panel));
    }
    
    // Apply panel-type specific styles
    if (panel) {
        auto it = m_panelStyles.find(panel->GetPanelTypeName());
        if (it != m_panelStyles.end() && it->second) {
            result.Merge(it->second->ComputeStyle(panel));
        }
    }
    
    return result;
}

} // namespace Panorama
