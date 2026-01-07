#include "CStyleSheet.h"
#include "../core/CPanel2D.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>
#include <filesystem>

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
    for (const auto& step : steps) {
        const auto& c = step.compound;
        if (!c.id.empty()) spec += 100;
        spec += static_cast<i32>(c.classes.size()) * 10;
        if (!c.element.empty()) spec += 1;
        if (!c.pseudoClass.empty()) spec += 10;
    }
    return spec;
}

static bool MatchesCompound(const SelectorCompound& compound, const CPanel2D* panel) {
    if (!panel) return false;

    // Element type
    if (!compound.element.empty() && panel->GetPanelTypeName() != compound.element) return false;

    // ID
    if (!compound.id.empty() && panel->GetID() != compound.id) return false;

    // Classes
    for (const auto& cls : compound.classes) {
        if (!panel->HasClass(cls)) return false;
    }

    // Pseudo-class
    if (!compound.pseudoClass.empty()) {
        const std::string& pc = compound.pseudoClass;
        if (pc == "hover" && !panel->IsHovered()) return false;
        if (pc == "active" && !panel->IsPressed()) return false;
        if (pc == "focus" && !panel->IsFocused()) return false;
        if (pc == "disabled" && panel->IsEnabled()) return false;
        if (pc == "selected" && !panel->IsSelected()) return false;
    }

    return true;
}

bool StyleSelector::Matches(const CPanel2D* panel) const {
    if (!panel) return false;
    if (steps.empty()) return false;

    // Match from rightmost to leftmost across ancestors/parent chain
    const CPanel2D* current = panel;
    if (!MatchesCompound(steps[0].compound, current)) return false;

    for (size_t i = 1; i < steps.size(); ++i) {
        const auto comb = steps[i - 1].combinatorToPrev;
        const auto& target = steps[i].compound;

        if (comb == SelectorCombinator::Child) {
            current = current ? current->GetParent() : nullptr;
            if (!MatchesCompound(target, current)) return false;
        } else {
            // Default/Descendant: walk up until a match
            const CPanel2D* p = current ? current->GetParent() : nullptr;
            bool found = false;
            while (p) {
                if (MatchesCompound(target, p)) {
                    found = true;
                    current = p;
                    break;
                }
                p = p->GetParent();
            }
            if (!found) return false;
        }
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

        // Parse properties once, then create one rule per selector in selector list (split by ',')
        StyleProperties props = ParseProperties(blockStr);

        size_t sPos = 0;
        while (sPos < selectorStr.size()) {
            size_t comma = selectorStr.find(',', sPos);
            std::string one = (comma == std::string::npos)
                ? selectorStr.substr(sPos)
                : selectorStr.substr(sPos, comma - sPos);

            // Skip empty selectors
            auto trimCopy = [](std::string t) {
                size_t a = t.find_first_not_of(" \t\n\r");
                size_t b = t.find_last_not_of(" \t\n\r");
                if (a == std::string::npos) return std::string{};
                return t.substr(a, b - a + 1);
            };
            one = trimCopy(one);
            if (!one.empty()) {
                StyleRule rule;
                rule.selector = ParseSelector(one);
                rule.properties = props;
                rule.sourceOrder = m_ruleCounter++;
                m_rules.push_back(rule);
            }

            if (comma == std::string::npos) break;
            sPos = comma + 1;
        }
    }
    
    return true;
}

static std::string NormalizePathForKey(const std::filesystem::path& p) {
    std::error_code ec;
    auto abs = std::filesystem::absolute(p, ec);
    if (!ec) return abs.lexically_normal().u8string();
    return p.lexically_normal().u8string();
}

static std::filesystem::path ResolveResourcePath(const std::string& rawPath) {
    // Supports Valve-like: file://{resources}/styles/foo.css
    // We map {resources} to workspace "resources/" folder.
    const std::string prefix = "file://{resources}/";
    if (rawPath.rfind(prefix, 0) == 0) {
        std::string rest = rawPath.substr(prefix.size());
        // Keep it as a normal relative path so it can be resolved by the search logic below.
        return std::filesystem::path("resources") / std::filesystem::path(rest);
    }

    // If it's already an absolute path, keep it.
    std::filesystem::path p(rawPath);
    if (p.is_absolute()) return p;

    // Best-effort resolution for runtime builds:
    // The executable may run with cwd like ".../build/bin/Debug", while the assets live in:
    //   - "<repo>/resources/..."
    //   - "<repo>/build/resources/..." (copied)
    // So we search upward from cwd for a directory where (base / p) exists.
    std::error_code ec;
    std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec) {
        return p; // fallback
    }

    std::filesystem::path base = cwd;
    for (int depth = 0; depth <= 8; ++depth) {
        std::filesystem::path candidate = base / p;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate;
        }
        std::filesystem::path parent = base.parent_path();
        if (parent == base) break;
        base = parent;
    }

    // Default: keep it relative (open will fail and logs will show cwd + requested path).
    return p;
}

bool CStyleSheet::LoadFromFile(const std::string& path) {
    // Load CSS file (with minimal @import support) and Parse().
    // LOG_INFO("CStyleSheet::LoadFromFile request='{}' cwd='{}'",
    //     path, std::filesystem::current_path().u8string());

    Clear();
    std::unordered_set<std::string> visited;

    std::function<bool(const std::filesystem::path&)> loadInternal = [&](const std::filesystem::path& p) -> bool {
        std::filesystem::path resolved = ResolveResourcePath(p.u8string());
        const std::string key = NormalizePathForKey(resolved);
        if (visited.count(key)) {
            LOG_WARN("CStyleSheet::LoadFromFile skipping already-visited css='{}' (resolved='{}')",
                p.u8string(), resolved.u8string());
            return true; // avoid cycles
        }
        visited.insert(key);

        // LOG_INFO("CStyleSheet::LoadFromFile opening css='{}' resolved='{}'",
        //     p.u8string(), resolved.u8string());

        std::ifstream file(resolved);
        if (!file.is_open()) {
            LOG_WARN("CStyleSheet::LoadFromFile failed to open resolved='{}' (cwd='{}')",
                resolved.u8string(), std::filesystem::current_path().u8string());
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string css = buffer.str();

        // Handle simple @import statements (best-effort).
        // Supported forms:
        //   @import "path.css";
        //   @import url("path.css");
        // We resolve imports relative to current file directory.
        const std::filesystem::path baseDir = resolved.parent_path();
        size_t ipos = 0;
        while (true) {
            ipos = css.find("@import", ipos);
            if (ipos == std::string::npos) break;

            size_t semi = css.find(';', ipos);
            if (semi == std::string::npos) break;

            std::string stmt = css.substr(ipos, semi - ipos + 1);
            std::string target;

            auto extractQuoted = [&](const std::string& s) -> std::string {
                size_t q1 = s.find('"');
                char q = '"';
                if (q1 == std::string::npos) { q1 = s.find('\''); q = '\''; }
                if (q1 == std::string::npos) return {};
                size_t q2 = s.find(q, q1 + 1);
                if (q2 == std::string::npos) return {};
                return s.substr(q1 + 1, q2 - q1 - 1);
            };

            target = extractQuoted(stmt);
            if (!target.empty()) {
                std::filesystem::path impPath = ResolveResourcePath(target);
                if (!impPath.is_absolute()) impPath = baseDir / impPath;
                bool okImp = loadInternal(impPath);
                if (!okImp) {
                    LOG_WARN("CStyleSheet::LoadFromFile @import failed target='{}' from='{}' -> resolved='{}'",
                        target, resolved.u8string(), impPath.u8string());
                }
                // else {
                //     LOG_INFO("CStyleSheet::LoadFromFile @import ok target='{}' from='{}' -> resolved='{}'",
                //         target, resolved.u8string(), impPath.u8string());
                // }
            } else {
                LOG_WARN("CStyleSheet::LoadFromFile malformed @import stmt='{}' (in '{}')",
                    stmt, resolved.u8string());
            }

            // Remove @import from css so Parse() doesn't choke on it later.
            css.erase(ipos, semi - ipos + 1);
        }

        bool okParse = Parse(css);
        if (!okParse) {
            LOG_WARN("CStyleSheet::LoadFromFile Parse() failed for resolved='{}'", resolved.u8string());
            return false;
        }

        // LOG_INFO("CStyleSheet::LoadFromFile parsed resolved='{}' (rules={}, animations={})",
        //     resolved.u8string(), (int)m_rules.size(), (int)m_animations.size());
        return true;
    };

    bool ok = loadInternal(std::filesystem::path(path));
    // LOG_INFO("CStyleSheet::LoadFromFile done request='{}' -> {} (total rules={}, animations={})",
    //     path, ok ? "OK" : "FAILED", (int)m_rules.size(), (int)m_animations.size());
    return ok;
}

StyleProperties CStyleSheet::ComputeStyle(const CPanel2D* panel) const {
    StyleProperties result;
    
    // Debug: log matching attempt
    // static int computeLogCount = 0;
    // if (computeLogCount < 20 && panel) {
    //     LOG_INFO("CStyleSheet::ComputeStyle panel id='{}' class='{}' totalRules={}",
    //         panel->GetID(), 
    //         panel->HasClass("LoginInput") ? "LoginInput" : (panel->HasClass("FieldLabel") ? "FieldLabel" : "none"),
    //         m_rules.size());
    //     computeLogCount++;
    // }
    
    // Collect matching rules
    std::vector<const StyleRule*> matchingRules;
    for (const auto& rule : m_rules) {
        if (rule.selector.Matches(panel)) {
            matchingRules.push_back(&rule);
        }
    }
    
    // Debug: log matched rules count
    // if (computeLogCount <= 20 && panel && !matchingRules.empty()) {
    //     LOG_INFO("CStyleSheet::ComputeStyle panel id='{}' matchedRules={}", 
    //         panel->GetID(), matchingRules.size());
    // }
    
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

    auto trimCopy = [](std::string t) {
        size_t a = t.find_first_not_of(" \t\n\r");
        size_t b = t.find_last_not_of(" \t\n\r");
        if (a == std::string::npos) return std::string{};
        return t.substr(a, b - a + 1);
    };

    std::string s = trimCopy(selectorStr);
    if (s.empty()) return sel;

    auto isSelectorStart = [](char c) {
        return std::isalpha((unsigned char)c) || c == '.' || c == '#' || c == ':';
    };

    auto parseCompound = [&](size_t& pos) -> SelectorCompound {
        SelectorCompound out;

        // element
        if (pos < s.size() && std::isalpha((unsigned char)s[pos])) {
            size_t elemEnd = pos;
            while (elemEnd < s.size() && (std::isalnum((unsigned char)s[elemEnd]) || s[elemEnd] == '-' || s[elemEnd] == '_')) {
                elemEnd++;
            }
            out.element = s.substr(pos, elemEnd - pos);
            pos = elemEnd;
        }

        while (pos < s.size()) {
            if (s[pos] == '#') {
                pos++;
                size_t idEnd = pos;
                while (idEnd < s.size() && (std::isalnum((unsigned char)s[idEnd]) || s[idEnd] == '-' || s[idEnd] == '_')) idEnd++;
                out.id = s.substr(pos, idEnd - pos);
                pos = idEnd;
            } else if (s[pos] == '.') {
                pos++;
                size_t classEnd = pos;
                while (classEnd < s.size() && (std::isalnum((unsigned char)s[classEnd]) || s[classEnd] == '-' || s[classEnd] == '_')) classEnd++;
                out.classes.push_back(s.substr(pos, classEnd - pos));
                pos = classEnd;
            } else if (s[pos] == ':') {
                pos++;
                if (pos < s.size() && s[pos] == ':') {
                    // pseudo-element
                    pos++;
                    size_t pseudoEnd = pos;
                    while (pseudoEnd < s.size() && std::isalpha((unsigned char)s[pseudoEnd])) pseudoEnd++;
                    sel.pseudoElement = s.substr(pos, pseudoEnd - pos);
                    pos = pseudoEnd;
                } else {
                    size_t pseudoEnd = pos;
                    while (pseudoEnd < s.size() && std::isalpha((unsigned char)s[pseudoEnd])) pseudoEnd++;
                    out.pseudoClass = s.substr(pos, pseudoEnd - pos);
                    pos = pseudoEnd;
                }
            } else {
                break;
            }
        }

        return out;
    };

    std::vector<SelectorCompound> compounds;
    std::vector<SelectorCombinator> combinators; // index i: relation from compounds[i-1] to compounds[i]
    combinators.push_back(SelectorCombinator::None);

    size_t pos = 0;
    while (pos < s.size()) {
        // skip whitespace
        bool hadSpace = false;
        while (pos < s.size() && std::isspace((unsigned char)s[pos])) { hadSpace = true; pos++; }
        if (pos >= s.size()) break;

        // Combinator explicit
        if (s[pos] == '>') {
            // Explicit child combinator - affects next compound
            pos++;
            while (pos < s.size() && std::isspace((unsigned char)s[pos])) pos++;
            // Mark that next relation is child
            if (!combinators.empty()) {
                combinators.back() = SelectorCombinator::Child;
            }
            continue;
        }

        if (!isSelectorStart(s[pos])) { pos++; continue; }

        // For implicit descendant combinator (whitespace), set relation for this compound (except first)
        if (!compounds.empty()) {
            if (combinators.back() == SelectorCombinator::None && hadSpace) {
                combinators.back() = SelectorCombinator::Descendant;
            }
        }

        SelectorCompound c = parseCompound(pos);
        compounds.push_back(std::move(c));
        combinators.push_back(SelectorCombinator::None);
    }

    if (compounds.empty()) return sel;

    // Build right-to-left steps.
    sel.steps.clear();
    for (int i = (int)compounds.size() - 1; i >= 0; --i) {
        SelectorStep st;
        st.compound = compounds[(size_t)i];
        // relation from prev (i-1) to i is combinators[i]
        st.combinatorToPrev = (i > 0) ? combinators[(size_t)i] : SelectorCombinator::None;
        sel.steps.push_back(std::move(st));
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
        
        auto parseLengthList = [&](const std::string& v) -> std::vector<Length> {
            // Split by whitespace
            std::istringstream iss(v);
            std::vector<Length> out;
            std::string tok;
            while (iss >> tok) {
                out.push_back(ParseLength(tok));
            }
            return out;
        };

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
        } else if (propName == "font-weight") {
            props.fontWeight = propValue;
        } else if (propName == "font-style") {
            props.fontStyle = propValue;
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
            // CSS shorthand: 1-4 values
            auto vals = parseLengthList(propValue);
            if (vals.size() == 1) {
                props.marginTop = props.marginRight = props.marginBottom = props.marginLeft = vals[0];
            } else if (vals.size() == 2) {
                props.marginTop = props.marginBottom = vals[0];
                props.marginLeft = props.marginRight = vals[1];
            } else if (vals.size() == 3) {
                props.marginTop = vals[0];
                props.marginLeft = props.marginRight = vals[1];
                props.marginBottom = vals[2];
            } else if (vals.size() >= 4) {
                props.marginTop = vals[0];
                props.marginRight = vals[1];
                props.marginBottom = vals[2];
                props.marginLeft = vals[3];
            }
        } else if (propName == "margin-left") {
            props.marginLeft = ParseLength(propValue);
        } else if (propName == "margin-right") {
            props.marginRight = ParseLength(propValue);
        } else if (propName == "margin-top") {
            props.marginTop = ParseLength(propValue);
        } else if (propName == "margin-bottom") {
            props.marginBottom = ParseLength(propValue);
        } else if (propName == "padding") {
            auto vals = parseLengthList(propValue);
            if (vals.size() == 1) {
                props.paddingTop = props.paddingRight = props.paddingBottom = props.paddingLeft = vals[0];
            } else if (vals.size() == 2) {
                props.paddingTop = props.paddingBottom = vals[0];
                props.paddingLeft = props.paddingRight = vals[1];
            } else if (vals.size() == 3) {
                props.paddingTop = vals[0];
                props.paddingLeft = props.paddingRight = vals[1];
                props.paddingBottom = vals[2];
            } else if (vals.size() >= 4) {
                props.paddingTop = vals[0];
                props.paddingRight = vals[1];
                props.paddingBottom = vals[2];
                props.paddingLeft = vals[3];
            }
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
        } else if (propName == "text-align") {
            if (propValue == "center") props.textAlign = HorizontalAlign::Center;
            else if (propValue == "right") props.textAlign = HorizontalAlign::Right;
            else props.textAlign = HorizontalAlign::Left;
        } else if (propName == "vertical-text-align") {
            if (propValue == "center") props.verticalTextAlign = VerticalAlign::Center;
            else if (propValue == "bottom") props.verticalTextAlign = VerticalAlign::Bottom;
            else props.verticalTextAlign = VerticalAlign::Top;
        } else if (propName == "x") {
            props.x = ParseLength(propValue);
        } else if (propName == "y") {
            props.y = ParseLength(propValue);
        } else if (propName == "padding-left") {
            props.paddingLeft = ParseLength(propValue);
        } else if (propName == "padding-right") {
            props.paddingRight = ParseLength(propValue);
        } else if (propName == "padding-top") {
            props.paddingTop = ParseLength(propValue);
        } else if (propName == "padding-bottom") {
            props.paddingBottom = ParseLength(propValue);
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
    // Text defaults are intentionally left unset here.
    // Text rendering code uses sane fallbacks (e.g. 16px, white), and inheritable text
    // properties can cascade from parent panels when explicitly set.
    m_defaultStyle.color.reset();
    m_defaultStyle.fontSize.reset();
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
