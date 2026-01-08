#include "CStyleHotReload.h"
#include "CUIEngine.h"
#include "../layout/CStyleSheet.h"
#include <spdlog/spdlog.h>

namespace Panorama {

CStyleHotReload& CStyleHotReload::Instance() {
    static CStyleHotReload instance;
    return instance;
}

void CStyleHotReload::WatchFile(const std::string& path, std::function<void(const std::string&)> onChanged) {
    if (!m_enabled) {
        spdlog::warn("CStyleHotReload::WatchFile - Hot reload is disabled. Call Enable() first.");
        return;
    }
    
    std::error_code ec;
    std::filesystem::path filePath(path);
    
    // Resolve path
    if (!filePath.is_absolute()) {
        filePath = std::filesystem::current_path() / filePath;
    }
    
    if (!std::filesystem::exists(filePath, ec) || ec) {
        spdlog::error("CStyleHotReload::WatchFile - File not found: {}", path);
        return;
    }
    
    WatchedFile file;
    file.path = filePath.u8string();
    file.lastWriteTime = std::filesystem::last_write_time(filePath, ec);
    file.onChanged = onChanged ? std::move(onChanged) : [this](const std::string& p) { DefaultReloadCallback(p); };
    
    m_watchedFiles[file.path] = std::move(file);
    
    spdlog::info("CStyleHotReload: Watching file '{}'", path);
}

void CStyleHotReload::UnwatchFile(const std::string& path) {
    std::filesystem::path filePath(path);
    if (!filePath.is_absolute()) {
        filePath = std::filesystem::current_path() / filePath;
    }
    
    auto it = m_watchedFiles.find(filePath.u8string());
    if (it != m_watchedFiles.end()) {
        spdlog::info("CStyleHotReload: Stopped watching '{}'", path);
        m_watchedFiles.erase(it);
    }
}

void CStyleHotReload::UnwatchAll() {
    spdlog::info("CStyleHotReload: Stopped watching all files");
    m_watchedFiles.clear();
}

std::vector<std::string> CStyleHotReload::GetWatchedFiles() const {
    std::vector<std::string> files;
    files.reserve(m_watchedFiles.size());
    for (const auto& [path, _] : m_watchedFiles) {
        files.push_back(path);
    }
    return files;
}

void CStyleHotReload::Update(f32 deltaTime) {
    if (!m_enabled || m_watchedFiles.empty()) {
        return;
    }
    
    m_timeSinceLastCheck += deltaTime;
    
    if (m_timeSinceLastCheck >= m_checkInterval) {
        CheckNow();
        m_timeSinceLastCheck = 0.0f;
    }
}

void CStyleHotReload::CheckNow() {
    for (auto& [path, file] : m_watchedFiles) {
        CheckFile(file);
    }
}

void CStyleHotReload::CheckFile(WatchedFile& file) {
    std::error_code ec;
    std::filesystem::path filePath(file.path);
    
    if (!std::filesystem::exists(filePath, ec) || ec) {
        spdlog::warn("CStyleHotReload: Watched file no longer exists: {}", file.path);
        return;
    }
    
    auto currentWriteTime = std::filesystem::last_write_time(filePath, ec);
    if (ec) {
        spdlog::error("CStyleHotReload: Failed to get write time for {}: {}", file.path, ec.message());
        return;
    }
    
    // Check if file was modified
    if (currentWriteTime != file.lastWriteTime) {
        spdlog::info("CStyleHotReload: File changed, reloading: {}", file.path);
        
        file.lastWriteTime = currentWriteTime;
        
        // Call reload callback
        try {
            file.onChanged(file.path);
            
            m_stats.totalReloads++;
            m_stats.lastReloadTime = 0.0f;  // Could track actual time if needed
            m_stats.lastReloadedFile = file.path;
            
            spdlog::info("CStyleHotReload: Successfully reloaded '{}'", file.path);
        } catch (const std::exception& e) {
            m_stats.failedReloads++;
            spdlog::error("CStyleHotReload: Failed to reload '{}': {}", file.path, e.what());
        }
    }
}

void CStyleHotReload::DefaultReloadCallback(const std::string& path) {
    // Reload stylesheet through CStyleManager
    spdlog::info("CStyleHotReload: Reloading stylesheet '{}'", path);
    
    // Clear and reload global styles
    Panorama::CStyleManager::Instance().LoadGlobalStyles(path);
    
    // Reapply styles to all panels
    auto* root = CUIEngine::Instance().GetRoot();
    if (root) {
        // Trigger style recomputation for entire tree
        std::function<void(CPanel2D*)> recomputeStyles = [&](CPanel2D* panel) {
            if (!panel) return;
            
            // Force style recomputation
            panel->InvalidateStyle();
            
            // Recurse to children
            for (const auto& child : panel->GetChildren()) {
                recomputeStyles(child.get());
            }
        };
        
        recomputeStyles(root);
    }
    
    spdlog::info("CStyleHotReload: Styles reapplied to UI tree");
}

} // namespace Panorama
