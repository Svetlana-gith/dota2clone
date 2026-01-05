#include "SettingsManager.h"
#include <fstream>
#include <set>
#include <algorithm>
#include <Windows.h>

namespace Game {

SettingsManager& SettingsManager::Instance() {
    static SettingsManager instance;
    return instance;
}

bool SettingsManager::Load(const String& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_INFO("Settings file not found, using defaults");
        return false;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        if (j.contains("video")) j["video"].get_to(m_settings.video);
        if (j.contains("audio")) j["audio"].get_to(m_settings.audio);
        if (j.contains("controls")) j["controls"].get_to(m_settings.controls);
        if (j.contains("game")) j["game"].get_to(m_settings.game);
        
        LOG_INFO("Settings loaded from {}", path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse settings: {}", e.what());
        return false;
    }
}

bool SettingsManager::Save(const String& path) {
    try {
        nlohmann::json j;
        j["video"] = m_settings.video;
        j["audio"] = m_settings.audio;
        j["controls"] = m_settings.controls;
        j["game"] = m_settings.game;
        
        std::ofstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open settings file for writing: {}", path);
            return false;
        }
        
        file << j.dump(2);
        LOG_INFO("Settings saved to {}", path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save settings: {}", e.what());
        return false;
    }
}

void SettingsManager::ResetToDefaults() {
    m_settings = AllSettings{};
    NotifyChanged();
}

std::vector<std::pair<u32, u32>> SettingsManager::GetAvailableResolutions() {
    std::vector<std::pair<u32, u32>> resolutions;
    DEVMODEW devMode = {};
    devMode.dmSize = sizeof(DEVMODEW);
    
    std::set<std::pair<u32, u32>> uniqueRes;
    
    for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &devMode); ++i) {
        if (devMode.dmBitsPerPel >= 32 && devMode.dmPelsWidth >= 800) {
            uniqueRes.insert({devMode.dmPelsWidth, devMode.dmPelsHeight});
        }
    }
    
    resolutions.assign(uniqueRes.begin(), uniqueRes.end());
    std::sort(resolutions.begin(), resolutions.end(), [](auto& a, auto& b) {
        return a.first * a.second > b.first * b.second;
    });
    
    return resolutions;
}

// JSON serialization implementations
void to_json(nlohmann::json& j, const VideoSettings& s) {
    j = nlohmann::json{
        {"resolutionWidth", s.resolutionWidth},
        {"resolutionHeight", s.resolutionHeight},
        {"windowMode", static_cast<int>(s.windowMode)},
        {"vsync", s.vsync},
        {"maxFPS", s.maxFPS},
        {"textureQuality", s.textureQuality},
        {"shadowQuality", s.shadowQuality},
        {"effectsQuality", s.effectsQuality},
        {"antiAliasing", s.antiAliasing},
        {"renderScale", s.renderScale}
    };
}

void from_json(const nlohmann::json& j, VideoSettings& s) {
    if (j.contains("resolutionWidth")) j["resolutionWidth"].get_to(s.resolutionWidth);
    if (j.contains("resolutionHeight")) j["resolutionHeight"].get_to(s.resolutionHeight);
    if (j.contains("windowMode")) s.windowMode = static_cast<WindowMode>(j["windowMode"].get<int>());
    if (j.contains("vsync")) j["vsync"].get_to(s.vsync);
    if (j.contains("maxFPS")) j["maxFPS"].get_to(s.maxFPS);
    if (j.contains("textureQuality")) j["textureQuality"].get_to(s.textureQuality);
    if (j.contains("shadowQuality")) j["shadowQuality"].get_to(s.shadowQuality);
    if (j.contains("effectsQuality")) j["effectsQuality"].get_to(s.effectsQuality);
    if (j.contains("antiAliasing")) j["antiAliasing"].get_to(s.antiAliasing);
    if (j.contains("renderScale")) j["renderScale"].get_to(s.renderScale);
}

void to_json(nlohmann::json& j, const AudioSettings& s) {
    j = nlohmann::json{
        {"masterVolume", s.masterVolume},
        {"musicVolume", s.musicVolume},
        {"sfxVolume", s.sfxVolume},
        {"voiceVolume", s.voiceVolume},
        {"announcerVolume", s.announcerVolume},
        {"muteWhenMinimized", s.muteWhenMinimized}
    };
}

void from_json(const nlohmann::json& j, AudioSettings& s) {
    if (j.contains("masterVolume")) j["masterVolume"].get_to(s.masterVolume);
    if (j.contains("musicVolume")) j["musicVolume"].get_to(s.musicVolume);
    if (j.contains("sfxVolume")) j["sfxVolume"].get_to(s.sfxVolume);
    if (j.contains("voiceVolume")) j["voiceVolume"].get_to(s.voiceVolume);
    if (j.contains("announcerVolume")) j["announcerVolume"].get_to(s.announcerVolume);
    if (j.contains("muteWhenMinimized")) j["muteWhenMinimized"].get_to(s.muteWhenMinimized);
}

void to_json(nlohmann::json& j, const ControlSettings& s) {
    j = nlohmann::json{
        {"cameraEdgePan", s.cameraEdgePan},
        {"cameraPanSpeed", s.cameraPanSpeed},
        {"invertCameraY", s.invertCameraY},
        {"mouseSensitivity", s.mouseSensitivity},
        {"quickCast", s.quickCast},
        {"autoAttack", s.autoAttack},
        {"keyAbility1", s.keyAbility1},
        {"keyAbility2", s.keyAbility2},
        {"keyAbility3", s.keyAbility3},
        {"keyAbility4", s.keyAbility4},
        {"keyAttack", s.keyAttack},
        {"keyMove", s.keyMove},
        {"keyStop", s.keyStop},
        {"keyHold", s.keyHold},
        {"keyOpenShop", s.keyOpenShop},
        {"keyScoreboard", s.keyScoreboard}
    };
}

void from_json(const nlohmann::json& j, ControlSettings& s) {
    if (j.contains("cameraEdgePan")) j["cameraEdgePan"].get_to(s.cameraEdgePan);
    if (j.contains("cameraPanSpeed")) j["cameraPanSpeed"].get_to(s.cameraPanSpeed);
    if (j.contains("invertCameraY")) j["invertCameraY"].get_to(s.invertCameraY);
    if (j.contains("mouseSensitivity")) j["mouseSensitivity"].get_to(s.mouseSensitivity);
    if (j.contains("quickCast")) j["quickCast"].get_to(s.quickCast);
    if (j.contains("autoAttack")) j["autoAttack"].get_to(s.autoAttack);
    if (j.contains("keyAbility1")) j["keyAbility1"].get_to(s.keyAbility1);
    if (j.contains("keyAbility2")) j["keyAbility2"].get_to(s.keyAbility2);
    if (j.contains("keyAbility3")) j["keyAbility3"].get_to(s.keyAbility3);
    if (j.contains("keyAbility4")) j["keyAbility4"].get_to(s.keyAbility4);
    if (j.contains("keyAttack")) j["keyAttack"].get_to(s.keyAttack);
    if (j.contains("keyMove")) j["keyMove"].get_to(s.keyMove);
    if (j.contains("keyStop")) j["keyStop"].get_to(s.keyStop);
    if (j.contains("keyHold")) j["keyHold"].get_to(s.keyHold);
    if (j.contains("keyOpenShop")) j["keyOpenShop"].get_to(s.keyOpenShop);
    if (j.contains("keyScoreboard")) j["keyScoreboard"].get_to(s.keyScoreboard);
}

void to_json(nlohmann::json& j, const GameSettings& s) {
    j = nlohmann::json{
        {"language", s.language},
        {"showHealthBars", s.showHealthBars},
        {"showManaBars", s.showManaBars},
        {"showDamageNumbers", s.showDamageNumbers},
        {"minimapOnRight", s.minimapOnRight},
        {"minimapScale", s.minimapScale},
        {"autoSelectSummons", s.autoSelectSummons},
        {"unitQueryOverride", s.unitQueryOverride}
    };
}

void from_json(const nlohmann::json& j, GameSettings& s) {
    if (j.contains("language")) j["language"].get_to(s.language);
    if (j.contains("showHealthBars")) j["showHealthBars"].get_to(s.showHealthBars);
    if (j.contains("showManaBars")) j["showManaBars"].get_to(s.showManaBars);
    if (j.contains("showDamageNumbers")) j["showDamageNumbers"].get_to(s.showDamageNumbers);
    if (j.contains("minimapOnRight")) j["minimapOnRight"].get_to(s.minimapOnRight);
    if (j.contains("minimapScale")) j["minimapScale"].get_to(s.minimapScale);
    if (j.contains("autoSelectSummons")) j["autoSelectSummons"].get_to(s.autoSelectSummons);
    if (j.contains("unitQueryOverride")) j["unitQueryOverride"].get_to(s.unitQueryOverride);
}

} // namespace Game
