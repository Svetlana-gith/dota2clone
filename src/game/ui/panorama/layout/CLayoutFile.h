#pragma once
/**
 * CLayoutFile - XML Layout Parser
 * Parses Valve-style XML layout files for Panorama UI
 */

#include "../core/PanoramaTypes.h"
#include "../core/CPanel2D.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Panorama {

class CStyleSheet;

// ============ Layout File ============

class CLayoutFile {
public:
    CLayoutFile() = default;
    ~CLayoutFile() = default;
    
    // Parse XML layout from string
    bool Parse(const std::string& xml);
    
    // Parse from file
    bool LoadFromFile(const std::string& path);
    
    // Create panel hierarchy from parsed layout
    std::shared_ptr<CPanel2D> CreatePanels();
    
    // Get associated stylesheet
    std::shared_ptr<CStyleSheet> GetStyleSheet() const { return m_stylesheet; }
    
    // Get root panel type
    const std::string& GetRootType() const { return m_rootType; }
    
    // Get included scripts
    const std::vector<std::string>& GetScripts() const { return m_scripts; }
    
    // Get included stylesheets
    const std::vector<std::string>& GetStyleSheets() const { return m_stylesheetPaths; }
    
private:
    struct XMLNode {
        std::string tag;
        std::unordered_map<std::string, std::string> attributes;
        std::vector<std::shared_ptr<XMLNode>> children;
        std::string textContent;
    };
    
    std::shared_ptr<XMLNode> m_root;
    std::string m_rootType;
    std::vector<std::string> m_scripts;
    std::vector<std::string> m_stylesheetPaths;
    std::shared_ptr<CStyleSheet> m_stylesheet;
    
    // XML parsing helpers
    std::shared_ptr<XMLNode> ParseXMLNode(const std::string& xml, size_t& pos);
    std::string ParseTagName(const std::string& xml, size_t& pos);
    std::unordered_map<std::string, std::string> ParseAttributes(const std::string& xml, size_t& pos);
    std::string ParseAttributeValue(const std::string& xml, size_t& pos);
    void SkipWhitespace(const std::string& xml, size_t& pos);
    
    // Panel creation
    std::shared_ptr<CPanel2D> CreatePanelFromNode(const XMLNode* node);
    std::shared_ptr<CPanel2D> CreatePanelByType(const std::string& type);
    void ApplyAttributes(CPanel2D* panel, const std::unordered_map<std::string, std::string>& attrs);
};

// ============ Layout Manager ============

class CLayoutManager {
public:
    static CLayoutManager& Instance();
    
    // Load and cache layout file
    std::shared_ptr<CLayoutFile> LoadLayout(const std::string& path);
    
    // Create panel from layout
    std::shared_ptr<CPanel2D> CreatePanelFromLayout(const std::string& path);
    
    // Register custom panel type factory
    using PanelFactory = std::function<std::shared_ptr<CPanel2D>()>;
    void RegisterPanelType(const std::string& typeName, PanelFactory factory);
    
    // Create panel by type name
    std::shared_ptr<CPanel2D> CreatePanel(const std::string& typeName);
    
    // Clear cache
    void ClearCache();
    
private:
    CLayoutManager();
    
    std::unordered_map<std::string, std::shared_ptr<CLayoutFile>> m_layoutCache;
    std::unordered_map<std::string, PanelFactory> m_panelFactories;
    
    void RegisterDefaultPanelTypes();
};

// ============ Example XML Layout Format ============
/*
Valve Panorama XML format example:

<root>
    <styles>
        <include src="file://{resources}/styles/hud.css" />
    </styles>
    <scripts>
        <include src="file://{resources}/scripts/hud.js" />
    </scripts>
    <Panel class="HUDRoot" hittest="false">
        <Panel id="TopBar" class="TopBar">
            <Label id="GameTime" class="GameTimeLabel" text="00:00" />
        </Panel>
        
        <Panel id="HeroHUD" class="HeroHUD">
            <DOTAHeroImage id="HeroPortrait" heroname="npc_dota_hero_axe" />
            <Panel class="HealthManaContainer">
                <ProgressBar id="HealthBar" class="HealthBar" value="0.8" />
                <ProgressBar id="ManaBar" class="ManaBar" value="0.6" />
            </Panel>
        </Panel>
        
        <Panel id="AbilityBar" class="AbilityBar">
            <DOTAAbilityPanel id="Ability0" abilityslot="0" />
            <DOTAAbilityPanel id="Ability1" abilityslot="1" />
            <DOTAAbilityPanel id="Ability2" abilityslot="2" />
            <DOTAAbilityPanel id="Ability3" abilityslot="3" />
        </Panel>
        
        <Button id="ShopButton" class="ShopButton" onactivate="OpenShop()">
            <Label text="#DOTA_Shop" />
        </Button>
    </Panel>
</root>
*/

} // namespace Panorama
