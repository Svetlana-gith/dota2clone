#include "SettingsPanel.h"
#include "../../DebugConsole.h"
#include <cstdio>

namespace Game {

static const Panorama::Color COL_BG_DARK(0.02f, 0.04f, 0.08f, 1.0f);
static const Panorama::Color COL_PANEL(0.08f, 0.09f, 0.12f, 1.0f);
static const Panorama::Color COL_HEADER(0.05f, 0.06f, 0.08f, 1.0f);
static const Panorama::Color COL_TAB_ACTIVE(0.2f, 0.22f, 0.28f, 1.0f);
static const Panorama::Color COL_TAB_INACTIVE(0.12f, 0.13f, 0.16f, 1.0f);
static const Panorama::Color COL_SECTION(0.06f, 0.07f, 0.09f, 1.0f);
static const Panorama::Color COL_LABEL(0.7f, 0.7f, 0.7f, 1.0f);
static const Panorama::Color COL_VALUE(0.9f, 0.9f, 0.9f, 1.0f);
static const Panorama::Color COL_GREEN(0.18f, 0.45f, 0.18f, 1.0f);
static const Panorama::Color COL_GREEN_ON(0.5f, 0.8f, 0.5f, 1.0f);
static const Panorama::Color COL_RED(0.5f, 0.15f, 0.15f, 0.9f);
static const Panorama::Color COL_RED_OFF(0.6f, 0.3f, 0.3f, 1.0f);

static const f32 WINDOW_WIDTH = 700.0f;
static const f32 WINDOW_HEIGHT = 520.0f;
static const f32 TAB_WIDTH = 130.0f;
static const f32 TAB_HEIGHT = 38.0f;
static const f32 ROW_HEIGHT = 40.0f;

SettingsPanel::SettingsPanel() = default;

void SettingsPanel::Create(Panorama::CPanel2D* parent, f32 screenWidth, f32 screenHeight) {
    SettingsManager::Instance().Load();
    
    m_overlay = std::make_shared<Panorama::CPanel2D>("SettingsOverlay");
    m_overlay->GetStyle().width = Panorama::Length::Fill();
    m_overlay->GetStyle().height = Panorama::Length::Fill();
    // Modal dimming overlay (separate from the window itself).
    // If you want a "hard" modal background, use alpha = 1.0f here.
    m_overlay->GetStyle().backgroundColor = Panorama::Color(0.0f, 0.0f, 0.0f, 0.85f);
    m_overlay->SetVisible(false);
    parent->AddChild(m_overlay);
    
    m_window = std::make_shared<Panorama::CPanel2D>("SettingsWindow");
    m_window->GetStyle().width = Panorama::Length::Px(WINDOW_WIDTH);
    m_window->GetStyle().height = Panorama::Length::Px(WINDOW_HEIGHT);
    m_window->GetStyle().backgroundColor = COL_PANEL;
    m_window->GetStyle().borderRadius = 8.0f;
    m_window->GetStyle().marginLeft = Panorama::Length::Px((screenWidth - WINDOW_WIDTH) / 2);
    m_window->GetStyle().marginTop = Panorama::Length::Px((screenHeight - WINDOW_HEIGHT) / 2);
    m_overlay->AddChild(m_window);
    
    CreateHeader(m_window.get(), WINDOW_WIDTH);
    
    auto content = std::make_shared<Panorama::CPanel2D>("SettingsContent");
    content->GetStyle().width = Panorama::Length::Fill();
    content->GetStyle().height = Panorama::Length::Px(WINDOW_HEIGHT - 50);
    content->GetStyle().marginTop = Panorama::Length::Px(50);
    m_window->AddChild(content);
    
    CreateTabs(content.get());
    
    m_tabPanels.resize(static_cast<size_t>(SettingsTab::Count));
    
    for (int i = 0; i < static_cast<int>(SettingsTab::Count); ++i) {
        auto panel = std::make_shared<Panorama::CPanel2D>("TabPanel_" + std::to_string(i));
        panel->GetStyle().width = Panorama::Length::Px(WINDOW_WIDTH - 40);
        panel->GetStyle().height = Panorama::Length::Px(340);
        panel->GetStyle().backgroundColor = COL_SECTION;
        panel->GetStyle().borderRadius = 4.0f;
        panel->GetStyle().marginLeft = Panorama::Length::Px(20);
        panel->GetStyle().marginTop = Panorama::Length::Px(60);
        panel->SetVisible(i == 0);
        content->AddChild(panel);
        m_tabPanels[i] = panel;
    }
    
    CreateVideoTab(m_tabPanels[0].get());
    CreateAudioTab(m_tabPanels[1].get());
    CreateControlsTab(m_tabPanels[2].get());
    CreateGameTab(m_tabPanels[3].get());
    CreateFooter(content.get());
}

void SettingsPanel::Destroy() {
    m_tabButtons.clear();
    m_tabPanels.clear();
    m_overlay.reset();
    m_window.reset();
}

void SettingsPanel::Show() {
    if (m_overlay) {
        RefreshUI();
        m_overlay->SetVisible(true);
    }
}

void SettingsPanel::Hide() {
    if (m_overlay) m_overlay->SetVisible(false);
}

bool SettingsPanel::IsVisible() const {
    return m_overlay && m_overlay->IsVisible();
}

void SettingsPanel::CreateHeader(Panorama::CPanel2D* window, f32 windowWidth) {
    auto header = std::make_shared<Panorama::CPanel2D>("SettingsHeader");
    header->GetStyle().width = Panorama::Length::Fill();
    header->GetStyle().height = Panorama::Length::Px(50);
    header->GetStyle().backgroundColor = COL_HEADER;
    window->AddChild(header);
    
    auto title = std::make_shared<Panorama::CLabel>("SETTINGS", "SettingsTitle");
    title->GetStyle().fontSize = 22.0f;
    title->GetStyle().color = COL_VALUE;
    title->GetStyle().marginLeft = Panorama::Length::Px(20);
    title->GetStyle().marginTop = Panorama::Length::Px(12);
    header->AddChild(title);
    
    auto closeBtn = std::make_shared<Panorama::CButton>("✕", "SettingsClose");
    closeBtn->GetStyle().width = Panorama::Length::Px(40);
    closeBtn->GetStyle().height = Panorama::Length::Px(40);
    closeBtn->GetStyle().backgroundColor = COL_RED;
    closeBtn->GetStyle().borderRadius = 4.0f;
    closeBtn->GetStyle().fontSize = 18.0f;
    closeBtn->GetStyle().color = Panorama::Color::White();
    closeBtn->GetStyle().marginLeft = Panorama::Length::Px(windowWidth - 50);
    closeBtn->GetStyle().marginTop = Panorama::Length::Px(5);
    closeBtn->SetOnActivate([this]() {
        Hide();
        if (m_onClose) m_onClose();
    });
    header->AddChild(closeBtn);
}

void SettingsPanel::CreateTabs(Panorama::CPanel2D* content) {
    const char* tabNames[] = {"VIDEO", "AUDIO", "CONTROLS", "GAME"};
    
    for (int i = 0; i < 4; ++i) {
        auto btn = std::make_shared<Panorama::CButton>(tabNames[i], "SettingsTab_" + std::to_string(i));
        btn->GetStyle().width = Panorama::Length::Px(TAB_WIDTH);
        btn->GetStyle().height = Panorama::Length::Px(TAB_HEIGHT);
        btn->GetStyle().backgroundColor = (i == 0) ? COL_TAB_ACTIVE : COL_TAB_INACTIVE;
        btn->GetStyle().borderRadius = 4.0f;
        btn->GetStyle().fontSize = 14.0f;
        btn->GetStyle().color = COL_VALUE;
        btn->GetStyle().marginLeft = Panorama::Length::Px(20 + i * (TAB_WIDTH + 10));
        btn->GetStyle().marginTop = Panorama::Length::Px(10);
        
        SettingsTab tab = static_cast<SettingsTab>(i);
        btn->SetOnActivate([this, tab]() { SwitchTab(tab); });
        
        content->AddChild(btn);
        m_tabButtons.push_back(btn);
    }
}

void SettingsPanel::SwitchTab(SettingsTab tab) {
    m_currentTab = tab;
    
    for (size_t i = 0; i < m_tabButtons.size(); ++i) {
        m_tabButtons[i]->GetStyle().backgroundColor = 
            (i == static_cast<size_t>(tab)) ? COL_TAB_ACTIVE : COL_TAB_INACTIVE;
    }
    
    for (size_t i = 0; i < m_tabPanels.size(); ++i) {
        m_tabPanels[i]->SetVisible(i == static_cast<size_t>(tab));
    }
}

void SettingsPanel::CreateVideoTab(Panorama::CPanel2D* container) {
    auto& video = SettingsManager::Instance().Video();
    f32 y = 20;
    
    CreateSettingRow(container, "Resolution", y);
    m_resolutionDropdown = CreateDropdown(container, "ResDropdown", 200, y, 220);
    auto resolutions = SettingsManager::GetAvailableResolutions();
    for (auto& [w, h] : resolutions) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%dx%d", w, h);
        m_resolutionDropdown->AddOption(buf, buf);
    }
    char currentRes[32];
    std::snprintf(currentRes, sizeof(currentRes), "%dx%d", video.resolutionWidth, video.resolutionHeight);
    m_resolutionDropdown->SetSelected(currentRes);
    m_resolutionDropdown->SetOnSelectionChanged([](const std::string& sel) {
        u32 w, h;
        if (std::sscanf(sel.c_str(), "%ux%u", &w, &h) == 2) {
            SettingsManager::Instance().Video().resolutionWidth = w;
            SettingsManager::Instance().Video().resolutionHeight = h;
        }
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Window Mode", y);
    m_windowModeDropdown = CreateDropdown(container, "WindowMode", 200, y, 220);
    m_windowModeDropdown->AddOption("windowed", "Windowed");
    m_windowModeDropdown->AddOption("borderless", "Borderless");
    m_windowModeDropdown->AddOption("fullscreen", "Fullscreen");
    const char* modes[] = {"windowed", "borderless", "fullscreen"};
    m_windowModeDropdown->SetSelected(modes[static_cast<int>(video.windowMode)]);
    m_windowModeDropdown->SetOnSelectionChanged([](const std::string& sel) {
        if (sel == "windowed") SettingsManager::Instance().Video().windowMode = WindowMode::Windowed;
        else if (sel == "borderless") SettingsManager::Instance().Video().windowMode = WindowMode::Borderless;
        else SettingsManager::Instance().Video().windowMode = WindowMode::Fullscreen;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "VSync", y);
    m_vsyncToggle = CreateToggle(container, "VsyncToggle", 200, y, video.vsync, [](bool val) {
        SettingsManager::Instance().Video().vsync = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Anti-Aliasing", y);
    CreateToggle(container, "AAToggle", 200, y, video.antiAliasing, [](bool val) {
        SettingsManager::Instance().Video().antiAliasing = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Texture Quality", y);
    auto texDropdown = CreateDropdown(container, "TexQuality", 200, y, 150);
    texDropdown->AddOption("0", "Low");
    texDropdown->AddOption("1", "Medium");
    texDropdown->AddOption("2", "High");
    texDropdown->AddOption("3", "Ultra");
    texDropdown->SetSelected(std::to_string(video.textureQuality));
    texDropdown->SetOnSelectionChanged([](const std::string& sel) {
        SettingsManager::Instance().Video().textureQuality = static_cast<u8>(std::stoi(sel));
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Shadow Quality", y);
    auto shadowDropdown = CreateDropdown(container, "ShadowQuality", 200, y, 150);
    shadowDropdown->AddOption("0", "Low");
    shadowDropdown->AddOption("1", "Medium");
    shadowDropdown->AddOption("2", "High");
    shadowDropdown->AddOption("3", "Ultra");
    shadowDropdown->SetSelected(std::to_string(video.shadowQuality));
    shadowDropdown->SetOnSelectionChanged([](const std::string& sel) {
        SettingsManager::Instance().Video().shadowQuality = static_cast<u8>(std::stoi(sel));
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Render Scale", y);
    auto scaleLabel = std::make_shared<Panorama::CLabel>("100%", "RenderScaleVal");
    scaleLabel->GetStyle().fontSize = 14.0f;
    scaleLabel->GetStyle().color = COL_VALUE;
    scaleLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    scaleLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(scaleLabel);
    
    auto scaleSlider = CreateSlider(container, "RenderScale", 200, y + 5, 220, 0.5f, 1.5f, video.renderScale,
        [scaleLabel](f32 val) {
            SettingsManager::Instance().Video().renderScale = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            scaleLabel->SetText(buf);
        });
}

void SettingsPanel::CreateAudioTab(Panorama::CPanel2D* container) {
    auto& audio = SettingsManager::Instance().Audio();
    f32 y = 20;
    
    CreateSettingRow(container, "Master Volume", y);
    m_masterVolumeLabel = std::make_shared<Panorama::CLabel>("100%", "MasterVolVal");
    m_masterVolumeLabel->GetStyle().fontSize = 14.0f;
    m_masterVolumeLabel->GetStyle().color = COL_VALUE;
    m_masterVolumeLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    m_masterVolumeLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(m_masterVolumeLabel);
    
    m_masterVolumeSlider = CreateSlider(container, "MasterVol", 200, y + 5, 220, 0.0f, 1.0f, audio.masterVolume,
        [this](f32 val) {
            SettingsManager::Instance().Audio().masterVolume = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            m_masterVolumeLabel->SetText(buf);
        });
    
    y += ROW_HEIGHT + 10;
    CreateSettingRow(container, "Music Volume", y);
    m_musicVolumeLabel = std::make_shared<Panorama::CLabel>("70%", "MusicVolVal");
    m_musicVolumeLabel->GetStyle().fontSize = 14.0f;
    m_musicVolumeLabel->GetStyle().color = COL_VALUE;
    m_musicVolumeLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    m_musicVolumeLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(m_musicVolumeLabel);
    
    m_musicVolumeSlider = CreateSlider(container, "MusicVol", 200, y + 5, 220, 0.0f, 1.0f, audio.musicVolume,
        [this](f32 val) {
            SettingsManager::Instance().Audio().musicVolume = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            m_musicVolumeLabel->SetText(buf);
        });
    
    y += ROW_HEIGHT + 10;
    CreateSettingRow(container, "SFX Volume", y);
    m_sfxVolumeLabel = std::make_shared<Panorama::CLabel>("100%", "SfxVolVal");
    m_sfxVolumeLabel->GetStyle().fontSize = 14.0f;
    m_sfxVolumeLabel->GetStyle().color = COL_VALUE;
    m_sfxVolumeLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    m_sfxVolumeLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(m_sfxVolumeLabel);
    
    m_sfxVolumeSlider = CreateSlider(container, "SfxVol", 200, y + 5, 220, 0.0f, 1.0f, audio.sfxVolume,
        [this](f32 val) {
            SettingsManager::Instance().Audio().sfxVolume = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            m_sfxVolumeLabel->SetText(buf);
        });
    
    y += ROW_HEIGHT + 10;
    CreateSettingRow(container, "Voice Volume", y);
    auto voiceLabel = std::make_shared<Panorama::CLabel>("100%", "VoiceVolVal");
    voiceLabel->GetStyle().fontSize = 14.0f;
    voiceLabel->GetStyle().color = COL_VALUE;
    voiceLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    voiceLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(voiceLabel);
    
    CreateSlider(container, "VoiceVol", 200, y + 5, 220, 0.0f, 1.0f, audio.voiceVolume,
        [voiceLabel](f32 val) {
            SettingsManager::Instance().Audio().voiceVolume = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            voiceLabel->SetText(buf);
        });
    
    y += ROW_HEIGHT + 10;
    CreateSettingRow(container, "Announcer Volume", y);
    auto announceLabel = std::make_shared<Panorama::CLabel>("100%", "AnnounceVolVal");
    announceLabel->GetStyle().fontSize = 14.0f;
    announceLabel->GetStyle().color = COL_VALUE;
    announceLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    announceLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(announceLabel);
    
    CreateSlider(container, "AnnounceVol", 200, y + 5, 220, 0.0f, 1.0f, audio.announcerVolume,
        [announceLabel](f32 val) {
            SettingsManager::Instance().Audio().announcerVolume = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            announceLabel->SetText(buf);
        });
    
    y += ROW_HEIGHT + 15;
    CreateSettingRow(container, "Mute When Minimized", y);
    CreateToggle(container, "MuteMinimized", 200, y, audio.muteWhenMinimized, [](bool val) {
        SettingsManager::Instance().Audio().muteWhenMinimized = val;
    });
}

void SettingsPanel::CreateControlsTab(Panorama::CPanel2D* container) {
    auto& controls = SettingsManager::Instance().Controls();
    f32 y = 20;
    
    CreateSettingRow(container, "Camera Edge Pan", y);
    CreateToggle(container, "EdgePan", 200, y, controls.cameraEdgePan, [](bool val) {
        SettingsManager::Instance().Controls().cameraEdgePan = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Camera Pan Speed", y);
    auto panLabel = std::make_shared<Panorama::CLabel>("100%", "PanSpeedVal");
    panLabel->GetStyle().fontSize = 14.0f;
    panLabel->GetStyle().color = COL_VALUE;
    panLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    panLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(panLabel);
    
    CreateSlider(container, "PanSpeed", 200, y + 5, 220, 0.2f, 2.0f, controls.cameraPanSpeed,
        [panLabel](f32 val) {
            SettingsManager::Instance().Controls().cameraPanSpeed = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            panLabel->SetText(buf);
        });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Invert Camera Y", y);
    CreateToggle(container, "InvertY", 200, y, controls.invertCameraY, [](bool val) {
        SettingsManager::Instance().Controls().invertCameraY = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Mouse Sensitivity", y);
    auto sensLabel = std::make_shared<Panorama::CLabel>("100%", "SensVal");
    sensLabel->GetStyle().fontSize = 14.0f;
    sensLabel->GetStyle().color = COL_VALUE;
    sensLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    sensLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(sensLabel);
    
    CreateSlider(container, "MouseSens", 200, y + 5, 220, 0.2f, 2.0f, controls.mouseSensitivity,
        [sensLabel](f32 val) {
            SettingsManager::Instance().Controls().mouseSensitivity = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            sensLabel->SetText(buf);
        });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Quick Cast", y);
    CreateToggle(container, "QuickCast", 200, y, controls.quickCast, [](bool val) {
        SettingsManager::Instance().Controls().quickCast = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Auto Attack", y);
    CreateToggle(container, "AutoAttack", 200, y, controls.autoAttack, [](bool val) {
        SettingsManager::Instance().Controls().autoAttack = val;
    });
    
    y += ROW_HEIGHT + 15;
    auto keybindsNote = std::make_shared<Panorama::CLabel>("Keybinds: Q W E R (abilities), A (attack), S (stop), H (hold), B (shop)", "KeybindsNote");
    keybindsNote->GetStyle().fontSize = 12.0f;
    keybindsNote->GetStyle().color = Panorama::Color(0.5f, 0.5f, 0.55f, 1.0f);
    keybindsNote->GetStyle().marginLeft = Panorama::Length::Px(20);
    keybindsNote->GetStyle().marginTop = Panorama::Length::Px(y);
    container->AddChild(keybindsNote);
}

void SettingsPanel::CreateGameTab(Panorama::CPanel2D* container) {
    auto& game = SettingsManager::Instance().Game();
    f32 y = 20;
    
    CreateSettingRow(container, "Language", y);
    auto langDropdown = CreateDropdown(container, "Language", 200, y, 150);
    langDropdown->AddOption("en", "English");
    langDropdown->AddOption("ru", "Русский");
    langDropdown->SetSelected(game.language);
    langDropdown->SetOnSelectionChanged([](const std::string& sel) {
        SettingsManager::Instance().Game().language = sel;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Show Health Bars", y);
    CreateToggle(container, "HealthBars", 200, y, game.showHealthBars, [](bool val) {
        SettingsManager::Instance().Game().showHealthBars = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Show Mana Bars", y);
    CreateToggle(container, "ManaBars", 200, y, game.showManaBars, [](bool val) {
        SettingsManager::Instance().Game().showManaBars = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Show Damage Numbers", y);
    CreateToggle(container, "DmgNumbers", 200, y, game.showDamageNumbers, [](bool val) {
        SettingsManager::Instance().Game().showDamageNumbers = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Minimap on Right", y);
    CreateToggle(container, "MinimapRight", 200, y, game.minimapOnRight, [](bool val) {
        SettingsManager::Instance().Game().minimapOnRight = val;
    });
    
    y += ROW_HEIGHT;
    CreateSettingRow(container, "Minimap Scale", y);
    auto mapLabel = std::make_shared<Panorama::CLabel>("100%", "MinimapScaleVal");
    mapLabel->GetStyle().fontSize = 14.0f;
    mapLabel->GetStyle().color = COL_VALUE;
    mapLabel->GetStyle().marginLeft = Panorama::Length::Px(430);
    mapLabel->GetStyle().marginTop = Panorama::Length::Px(y + 8);
    container->AddChild(mapLabel);
    
    CreateSlider(container, "MinimapScale", 200, y + 5, 220, 0.5f, 1.5f, game.minimapScale,
        [mapLabel](f32 val) {
            SettingsManager::Instance().Game().minimapScale = val;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
            mapLabel->SetText(buf);
        });
}

void SettingsPanel::CreateFooter(Panorama::CPanel2D* content) {
    auto resetBtn = std::make_shared<Panorama::CButton>("RESET DEFAULTS", "SettingsReset");
    resetBtn->GetStyle().width = Panorama::Length::Px(150);
    resetBtn->GetStyle().height = Panorama::Length::Px(40);
    resetBtn->GetStyle().backgroundColor = Panorama::Color(0.4f, 0.25f, 0.15f, 1.0f);
    resetBtn->GetStyle().borderRadius = 4.0f;
    resetBtn->GetStyle().fontSize = 13.0f;
    resetBtn->GetStyle().color = Panorama::Color::White();
    resetBtn->GetStyle().marginLeft = Panorama::Length::Px(20);
    resetBtn->GetStyle().marginTop = Panorama::Length::Px(415);
    resetBtn->SetOnActivate([this]() { ResetDefaults(); });
    content->AddChild(resetBtn);
    
    auto applyBtn = std::make_shared<Panorama::CButton>("APPLY", "SettingsApply");
    applyBtn->GetStyle().width = Panorama::Length::Px(120);
    applyBtn->GetStyle().height = Panorama::Length::Px(40);
    applyBtn->GetStyle().backgroundColor = COL_GREEN;
    applyBtn->GetStyle().borderRadius = 4.0f;
    applyBtn->GetStyle().fontSize = 14.0f;
    applyBtn->GetStyle().color = Panorama::Color::White();
    applyBtn->GetStyle().marginLeft = Panorama::Length::Px(WINDOW_WIDTH - 160);
    applyBtn->GetStyle().marginTop = Panorama::Length::Px(415);
    applyBtn->SetOnActivate([this]() { ApplySettings(); });
    content->AddChild(applyBtn);
}

void SettingsPanel::ApplySettings() {
    SettingsManager::Instance().Save();
    SettingsManager::Instance().NotifyChanged();
    ConsoleLog("Settings applied and saved");
    Hide();
    if (m_onClose) m_onClose();
}

void SettingsPanel::ResetDefaults() {
    SettingsManager::Instance().ResetToDefaults();
    RefreshUI();
    ConsoleLog("Settings reset to defaults");
}

void SettingsPanel::RefreshUI() {
    auto& video = SettingsManager::Instance().Video();
    auto& audio = SettingsManager::Instance().Audio();
    
    if (m_masterVolumeSlider) m_masterVolumeSlider->SetValue(audio.masterVolume);
    if (m_musicVolumeSlider) m_musicVolumeSlider->SetValue(audio.musicVolume);
    if (m_sfxVolumeSlider) m_sfxVolumeSlider->SetValue(audio.sfxVolume);
    
    if (m_masterVolumeLabel) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f%%", audio.masterVolume * 100);
        m_masterVolumeLabel->SetText(buf);
    }
    if (m_musicVolumeLabel) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f%%", audio.musicVolume * 100);
        m_musicVolumeLabel->SetText(buf);
    }
    if (m_sfxVolumeLabel) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f%%", audio.sfxVolume * 100);
        m_sfxVolumeLabel->SetText(buf);
    }
}

std::shared_ptr<Panorama::CLabel> SettingsPanel::CreateSettingRow(
    Panorama::CPanel2D* parent, const std::string& label, f32 yOffset) {
    auto lbl = std::make_shared<Panorama::CLabel>(label, label + "_Label");
    lbl->GetStyle().fontSize = 14.0f;
    lbl->GetStyle().color = COL_LABEL;
    lbl->GetStyle().marginLeft = Panorama::Length::Px(20);
    lbl->GetStyle().marginTop = Panorama::Length::Px(yOffset + 8);
    parent->AddChild(lbl);
    return lbl;
}

std::shared_ptr<Panorama::CSlider> SettingsPanel::CreateSlider(
    Panorama::CPanel2D* parent, const std::string& id, f32 x, f32 y, f32 width,
    f32 min, f32 max, f32 value, std::function<void(f32)> onChange) {
    auto slider = std::make_shared<Panorama::CSlider>(id);
    slider->GetStyle().width = Panorama::Length::Px(width);
    slider->GetStyle().height = Panorama::Length::Px(20);
    slider->GetStyle().marginLeft = Panorama::Length::Px(x);
    slider->GetStyle().marginTop = Panorama::Length::Px(y);
    slider->SetRange(min, max);
    slider->SetValue(value);
    slider->SetOnValueChanged(onChange);
    parent->AddChild(slider);
    return slider;
}

std::shared_ptr<Panorama::CDropDown> SettingsPanel::CreateDropdown(
    Panorama::CPanel2D* parent, const std::string& id, f32 x, f32 y, f32 width) {
    auto dropdown = std::make_shared<Panorama::CDropDown>(id);
    dropdown->GetStyle().width = Panorama::Length::Px(width);
    dropdown->GetStyle().height = Panorama::Length::Px(30);
    dropdown->GetStyle().marginLeft = Panorama::Length::Px(x);
    dropdown->GetStyle().marginTop = Panorama::Length::Px(y + 2);
    dropdown->GetStyle().backgroundColor = Panorama::Color(0.12f, 0.13f, 0.16f, 1.0f);
    dropdown->GetStyle().borderRadius = 4.0f;
    parent->AddChild(dropdown);
    return dropdown;
}

std::shared_ptr<Panorama::CButton> SettingsPanel::CreateToggle(
    Panorama::CPanel2D* parent, const std::string& id, f32 x, f32 y,
    bool initialValue, std::function<void(bool)> onChange) {
    auto toggle = std::make_shared<Panorama::CButton>(initialValue ? "ON" : "OFF", id);
    toggle->GetStyle().width = Panorama::Length::Px(60);
    toggle->GetStyle().height = Panorama::Length::Px(28);
    toggle->GetStyle().marginLeft = Panorama::Length::Px(x);
    toggle->GetStyle().marginTop = Panorama::Length::Px(y + 3);
    toggle->GetStyle().backgroundColor = initialValue ? COL_GREEN : Panorama::Color(0.25f, 0.25f, 0.28f, 1.0f);
    toggle->GetStyle().borderRadius = 4.0f;
    toggle->GetStyle().fontSize = 12.0f;
    toggle->GetStyle().color = Panorama::Color::White();
    
    bool* state = new bool(initialValue);
    toggle->SetOnActivate([toggle, state, onChange]() {
        *state = !*state;
        toggle->SetText(*state ? "ON" : "OFF");
        toggle->GetStyle().backgroundColor = *state ? COL_GREEN : Panorama::Color(0.25f, 0.25f, 0.28f, 1.0f);
        if (onChange) onChange(*state);
    });
    
    parent->AddChild(toggle);
    return toggle;
}

} // namespace Game
