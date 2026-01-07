#include "CLayoutFile.h"
#include "CStyleSheet.h"
#include "../widgets/CLabel.h"
#include "../widgets/CImage.h"
#include "../widgets/CButton.h"
#include "../widgets/CProgressBar.h"
#include "../widgets/CTextEntry.h"
#include "../widgets/CSlider.h"
#include "../widgets/CDropDown.h"
#include <fstream>
#include <sstream>

namespace Panorama {

// ============ CLayoutFile Implementation ============

bool CLayoutFile::Parse(const std::string& xml) {
    size_t pos = 0;
    
    // Skip XML declaration if present
    if (xml.substr(0, 5) == "<?xml") {
        pos = xml.find("?>", pos);
        if (pos != std::string::npos) pos += 2;
    }
    
    SkipWhitespace(xml, pos);
    
    // Parse root node
    m_root = ParseXMLNode(xml, pos);
    
    if (m_root) {
        m_rootType = m_root->tag;
        
        // Extract styles and scripts from root
        for (const auto& child : m_root->children) {
            if (child->tag == "styles") {
                for (const auto& include : child->children) {
                    if (include->tag == "include") {
                        auto it = include->attributes.find("src");
                        if (it != include->attributes.end()) {
                            m_stylesheetPaths.push_back(it->second);
                        }
                    }
                }
            } else if (child->tag == "scripts") {
                for (const auto& include : child->children) {
                    if (include->tag == "include") {
                        auto it = include->attributes.find("src");
                        if (it != include->attributes.end()) {
                            m_scripts.push_back(it->second);
                        }
                    }
                }
            }
        }
        
        return true;
    }
    
    return false;
}

bool CLayoutFile::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return Parse(buffer.str());
}

std::shared_ptr<CPanel2D> CLayoutFile::CreatePanels() {
    if (!m_root) return nullptr;
    
    // Find the actual panel content (skip styles/scripts nodes)
    for (const auto& child : m_root->children) {
        if (child->tag != "styles" && child->tag != "scripts") {
            return CreatePanelFromNode(child.get());
        }
    }
    
    // If root is the panel itself
    if (m_root->tag != "root") {
        return CreatePanelFromNode(m_root.get());
    }
    
    return nullptr;
}

std::shared_ptr<CPanel2D> CLayoutFile::CreatePanelFromNode(const XMLNode* node) {
    if (!node) return nullptr;
    
    auto panel = CreatePanelByType(node->tag);
    if (!panel) {
        panel = std::make_shared<CPanel2D>();
    }
    
    ApplyAttributes(panel.get(), node->attributes);
    
    // Create children
    for (const auto& childNode : node->children) {
        auto childPanel = CreatePanelFromNode(childNode.get());
        if (childPanel) {
            panel->AddChild(childPanel);
        }
    }
    
    // IMPORTANT: do NOT apply text during element creation.
    // Store text content as an attribute so CUITextSystem can apply it later.
    if (!node->textContent.empty() && !panel->HasAttribute("text")) {
        panel->SetAttribute("text", node->textContent);
    }
    
    return panel;
}

std::shared_ptr<CPanel2D> CLayoutFile::CreatePanelByType(const std::string& type) {
    return CLayoutManager::Instance().CreatePanel(type);
}

void CLayoutFile::ApplyAttributes(CPanel2D* panel, const std::unordered_map<std::string, std::string>& attrs) {
    if (!panel) return;
    
    for (const auto& [name, value] : attrs) {
        if (name == "id") {
            panel->SetID(value);
        } else if (name == "class") {
            // Split by space and add each class
            std::istringstream iss(value);
            std::string className;
            while (iss >> className) {
                panel->AddClass(className);
            }
        } else if (name == "src") {
            if (auto* image = dynamic_cast<CImage*>(panel)) {
                image->SetImage(value);
            }
        } else if (name == "value") {
            if (auto* progress = dynamic_cast<CProgressBar*>(panel)) {
                progress->SetValue(std::stof(value));
            } else if (auto* slider = dynamic_cast<CSlider*>(panel)) {
                slider->SetValue(std::stof(value));
            }
        } else if (name == "visible") {
            panel->SetVisible(value == "true" || value == "1");
        } else if (name == "enabled") {
            panel->SetEnabled(value == "true" || value == "1");
        } else if (name == "hittest") {
            panel->SetAcceptsInput(value == "true" || value == "1");
        } else {
            // Store as generic attribute
            panel->SetAttribute(name, value);
        }
    }
}

// XML Parsing helpers

void CLayoutFile::SkipWhitespace(const std::string& xml, size_t& pos) {
    while (pos < xml.length() && std::isspace(xml[pos])) {
        pos++;
    }
}

std::shared_ptr<CLayoutFile::XMLNode> CLayoutFile::ParseXMLNode(const std::string& xml, size_t& pos) {
    SkipWhitespace(xml, pos);
    
    if (pos >= xml.length() || xml[pos] != '<') {
        return nullptr;
    }
    
    pos++;  // Skip '<'
    
    // Check for comment
    if (xml.substr(pos, 3) == "!--") {
        // Skip comment
        pos = xml.find("-->", pos);
        if (pos != std::string::npos) pos += 3;
        return ParseXMLNode(xml, pos);
    }
    
    // Check for closing tag
    if (xml[pos] == '/') {
        return nullptr;
    }
    
    auto node = std::make_shared<XMLNode>();
    
    // Parse tag name
    node->tag = ParseTagName(xml, pos);
    
    // Parse attributes
    node->attributes = ParseAttributes(xml, pos);
    
    SkipWhitespace(xml, pos);
    
    // Check for self-closing tag
    if (xml[pos] == '/') {
        pos++;  // Skip '/'
        if (xml[pos] == '>') pos++;  // Skip '>'
        return node;
    }
    
    if (xml[pos] == '>') {
        pos++;  // Skip '>'
    }
    
    // Parse children and text content
    while (pos < xml.length()) {
        SkipWhitespace(xml, pos);
        
        if (pos >= xml.length()) break;
        
        if (xml[pos] == '<') {
            if (pos + 1 < xml.length() && xml[pos + 1] == '/') {
                // Closing tag
                pos = xml.find('>', pos);
                if (pos != std::string::npos) pos++;
                break;
            }
            
            // Child element
            auto child = ParseXMLNode(xml, pos);
            if (child) {
                node->children.push_back(child);
            }
        } else {
            // Text content
            size_t textEnd = xml.find('<', pos);
            if (textEnd != std::string::npos) {
                node->textContent = xml.substr(pos, textEnd - pos);
                // Trim whitespace
                size_t start = node->textContent.find_first_not_of(" \t\n\r");
                size_t end = node->textContent.find_last_not_of(" \t\n\r");
                if (start != std::string::npos) {
                    node->textContent = node->textContent.substr(start, end - start + 1);
                } else {
                    node->textContent.clear();
                }
                pos = textEnd;
            } else {
                break;
            }
        }
    }
    
    return node;
}

std::string CLayoutFile::ParseTagName(const std::string& xml, size_t& pos) {
    size_t start = pos;
    while (pos < xml.length() && (std::isalnum(xml[pos]) || xml[pos] == '-' || xml[pos] == '_' || xml[pos] == ':')) {
        pos++;
    }
    return xml.substr(start, pos - start);
}

std::unordered_map<std::string, std::string> CLayoutFile::ParseAttributes(const std::string& xml, size_t& pos) {
    std::unordered_map<std::string, std::string> attrs;
    
    while (pos < xml.length()) {
        SkipWhitespace(xml, pos);
        
        if (xml[pos] == '>' || xml[pos] == '/') {
            break;
        }
        
        // Parse attribute name
        std::string attrName = ParseTagName(xml, pos);
        if (attrName.empty()) break;
        
        SkipWhitespace(xml, pos);
        
        if (xml[pos] == '=') {
            pos++;  // Skip '='
            SkipWhitespace(xml, pos);
            
            // Parse attribute value
            std::string attrValue = ParseAttributeValue(xml, pos);
            attrs[attrName] = attrValue;
        } else {
            // Boolean attribute
            attrs[attrName] = "true";
        }
    }
    
    return attrs;
}

std::string CLayoutFile::ParseAttributeValue(const std::string& xml, size_t& pos) {
    if (pos >= xml.length()) return "";
    
    char quote = xml[pos];
    if (quote != '"' && quote != '\'') {
        // Unquoted value
        size_t start = pos;
        while (pos < xml.length() && !std::isspace(xml[pos]) && xml[pos] != '>' && xml[pos] != '/') {
            pos++;
        }
        return xml.substr(start, pos - start);
    }
    
    pos++;  // Skip opening quote
    size_t start = pos;
    while (pos < xml.length() && xml[pos] != quote) {
        pos++;
    }
    std::string value = xml.substr(start, pos - start);
    if (pos < xml.length()) pos++;  // Skip closing quote
    
    return value;
}

// ============ CLayoutManager Implementation ============

CLayoutManager& CLayoutManager::Instance() {
    static CLayoutManager instance;
    return instance;
}

CLayoutManager::CLayoutManager() {
    RegisterDefaultPanelTypes();
}

void CLayoutManager::RegisterDefaultPanelTypes() {
    // Register built-in panel types
    RegisterPanelType("Panel", []() { return std::make_shared<CPanel2D>(); });
    RegisterPanelType("Label", []() { return std::make_shared<CLabel>(); });
    RegisterPanelType("Image", []() { return std::make_shared<CImage>(); });
    RegisterPanelType("Button", []() { return std::make_shared<CButton>(); });
    RegisterPanelType("ProgressBar", []() { return std::make_shared<CProgressBar>(); });
    RegisterPanelType("TextEntry", []() { return std::make_shared<CTextEntry>(); });
    RegisterPanelType("Slider", []() { return std::make_shared<CSlider>(); });
    RegisterPanelType("DropDown", []() { return std::make_shared<CDropDown>(); });
    
    // Aliases
    RegisterPanelType("Frame", []() { return std::make_shared<CPanel2D>(); });
    RegisterPanelType("DOTAHUDOverlay", []() { return std::make_shared<CPanel2D>(); });
}

std::shared_ptr<CLayoutFile> CLayoutManager::LoadLayout(const std::string& path) {
    // Check cache
    auto it = m_layoutCache.find(path);
    if (it != m_layoutCache.end()) {
        return it->second;
    }
    
    // Load and cache
    auto layout = std::make_shared<CLayoutFile>();
    if (layout->LoadFromFile(path)) {
        m_layoutCache[path] = layout;
        return layout;
    }
    
    return nullptr;
}

std::shared_ptr<CPanel2D> CLayoutManager::CreatePanelFromLayout(const std::string& path) {
    auto layout = LoadLayout(path);
    if (layout) {
        return layout->CreatePanels();
    }
    return nullptr;
}

void CLayoutManager::RegisterPanelType(const std::string& typeName, PanelFactory factory) {
    m_panelFactories[typeName] = factory;
}

std::shared_ptr<CPanel2D> CLayoutManager::CreatePanel(const std::string& typeName) {
    auto it = m_panelFactories.find(typeName);
    if (it != m_panelFactories.end()) {
        return it->second();
    }
    
    // Default to base panel
    return std::make_shared<CPanel2D>();
}

void CLayoutManager::ClearCache() {
    m_layoutCache.clear();
}

} // namespace Panorama
