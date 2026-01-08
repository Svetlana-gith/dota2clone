#pragma once
/**
 * CStyleHotReload - CSS Hot Reload System
 * 
 * Monitors CSS files for changes and automatically reloads them.
 * Useful for rapid UI iteration without restarting the game.
 * 
 * Usage:
 *   CStyleHotReload::Instance().Enable();
 *   CStyleHotReload::Instance().WatchFile("resources/styles/login.css");
 *   
 *   // In game loop:
 *   CStyleHotReload::Instance().Update();
 */

#include "PanoramaTypes.h"
#include <string>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <functional>

namespace Panorama {

struct WatchedFile {
    std::string path;
    std::filesystem::file_time_type lastWriteTime;
    std::function<void(const std::string&)> onChanged;
};

class CStyleHotReload {
public:
    static CStyleHotReload& Instance();
    
    // ============ Configuration ============
    
    /**
     * Enable/disable hot reload system
     */
    void Enable(bool enabled = true) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    
    /**
     * Set check interval (default: 0.5 seconds)
     */
    void SetCheckInterval(f32 seconds) { m_checkInterval = seconds; }
    
    // ============ File Watching ============
    
    /**
     * Watch a CSS file for changes
     * @param path Path to CSS file
     * @param onChanged Optional callback when file changes (default: reload stylesheet)
     */
    void WatchFile(const std::string& path, std::function<void(const std::string&)> onChanged = nullptr);
    
    /**
     * Stop watching a file
     */
    void UnwatchFile(const std::string& path);
    
    /**
     * Stop watching all files
     */
    void UnwatchAll();
    
    /**
     * Get list of watched files
     */
    std::vector<std::string> GetWatchedFiles() const;
    
    // ============ Update ============
    
    /**
     * Check for file changes (call in game loop)
     */
    void Update(f32 deltaTime);
    
    /**
     * Force check all files immediately
     */
    void CheckNow();
    
    // ============ Statistics ============
    
    struct Stats {
        u32 totalReloads = 0;
        u32 failedReloads = 0;
        f32 lastReloadTime = 0.0f;
        std::string lastReloadedFile;
    };
    
    const Stats& GetStats() const { return m_stats; }
    void ResetStats() { m_stats = Stats{}; }
    
private:
    CStyleHotReload() = default;
    ~CStyleHotReload() = default;
    
    bool m_enabled = false;
    f32 m_checkInterval = 0.5f;  // Check every 0.5 seconds
    f32 m_timeSinceLastCheck = 0.0f;
    
    std::unordered_map<std::string, WatchedFile> m_watchedFiles;
    Stats m_stats;
    
    void CheckFile(WatchedFile& file);
    void DefaultReloadCallback(const std::string& path);
};

} // namespace Panorama
