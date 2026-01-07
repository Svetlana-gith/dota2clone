#pragma once

#include "core/Types.h"
#include <nlohmann/json.hpp>
#include <functional>

namespace Game {

enum class WindowMode : u8 {
    Windowed = 0,
    Borderless = 1,
    Fullscreen = 2
};

struct VideoSettings {
    u32 resolutionWidth = 1920;
    u32 resolutionHeight = 1080;
    WindowMode windowMode = WindowMode::Borderless;
    bool vsync = true;
    u16 maxFPS = 0;  // 0 = unlimited, supports values > 255
    u8 textureQuality = 2;  // 0=Low, 1=Medium, 2=High, 3=Ultra
    u8 shadowQuality = 2;
    u8 effectsQuality = 2;
    bool antiAliasing = true;
    f32 renderScale = 1.0f;
};

struct AudioSettings {
    f32 masterVolume = 1.0f;
    f32 musicVolume = 0.7f;
    f32 sfxVolume = 1.0f;
    f32 voiceVolume = 1.0f;
    f32 announcerVolume = 1.0f;
    bool muteWhenMinimized = true;
};

struct ControlSettings {
    bool cameraEdgePan = true;
    f32 cameraPanSpeed = 1.0f;
    bool invertCameraY = false;
    f32 mouseSensitivity = 1.0f;
    bool quickCast = false;
    bool autoAttack = true;
    
    // Keybinds (scancode values)
    u32 keyAbility1 = 'Q';
    u32 keyAbility2 = 'W';
    u32 keyAbility3 = 'E';
    u32 keyAbility4 = 'R';
    u32 keyAttack = 'A';
    u32 keyMove = 'M';
    u32 keyStop = 'S';
    u32 keyHold = 'H';
    u32 keyOpenShop = 'B';
    u32 keyScoreboard = 0x09;  // Tab
};

struct GameSettings {
    String language = "en";
    bool showHealthBars = true;
    bool showManaBars = true;
    bool showDamageNumbers = true;
    bool minimapOnRight = false;
    f32 minimapScale = 1.0f;
    bool autoSelectSummons = false;
    bool unitQueryOverride = false;
};

struct AllSettings {
    VideoSettings video;
    AudioSettings audio;
    ControlSettings controls;
    GameSettings game;
};

class SettingsManager {
public:
    static SettingsManager& Instance();
    
    bool Load(const String& path = "settings.json");
    bool Save(const String& path = "settings.json");
    
    VideoSettings& Video() { return m_settings.video; }
    AudioSettings& Audio() { return m_settings.audio; }
    ControlSettings& Controls() { return m_settings.controls; }
    GameSettings& Game() { return m_settings.game; }
    
    const VideoSettings& Video() const { return m_settings.video; }
    const AudioSettings& Audio() const { return m_settings.audio; }
    const ControlSettings& Controls() const { return m_settings.controls; }
    const GameSettings& Game() const { return m_settings.game; }
    
    void ResetToDefaults();
    
    void SetOnSettingsChanged(std::function<void()> callback) { m_onChanged = callback; }
    void NotifyChanged() { if (m_onChanged) m_onChanged(); }
    
    static std::vector<std::pair<u32, u32>> GetAvailableResolutions();
    
private:
    SettingsManager() = default;
    
    AllSettings m_settings;
    std::function<void()> m_onChanged;
};

// JSON serialization
void to_json(nlohmann::json& j, const VideoSettings& s);
void from_json(const nlohmann::json& j, VideoSettings& s);
void to_json(nlohmann::json& j, const AudioSettings& s);
void from_json(const nlohmann::json& j, AudioSettings& s);
void to_json(nlohmann::json& j, const ControlSettings& s);
void from_json(const nlohmann::json& j, ControlSettings& s);
void to_json(nlohmann::json& j, const GameSettings& s);
void from_json(const nlohmann::json& j, GameSettings& s);

} // namespace Game
