#pragma once

#include "ui/panorama/core/CUIEngine.h"
#include "ui/panorama/core/CPanel2D.h"
#include "ui/panorama/widgets/CLabel.h"
#include "ui/panorama/widgets/CButton.h"
#include <deque>
#include <string>
#include <memory>

namespace Game {

// ============ Text Selection Position ============
struct TextPosition {
    int line = -1;
    int character = -1;
    
    bool IsValid() const { return line >= 0 && character >= 0; }
    
    bool operator<(const TextPosition& other) const {
        if (line != other.line) return line < other.line;
        return character < other.character;
    }
    
    bool operator==(const TextPosition& other) const {
        return line == other.line && character == other.character;
    }
    
    bool operator!=(const TextPosition& other) const {
        return !(*this == other);
    }
};

// ============ Debug Console ============
// In-game console for viewing logs and debugging
// Press ~ (tilde) to toggle

class DebugConsole {
public:
    static DebugConsole& Instance();
    
    void Initialize();
    void Shutdown();
    void Toggle();
    void Show();
    void Hide();
    bool IsVisible() const { return m_visible; }
    
    void AddLog(const std::string& text);
    void Clear();
    
    void Update(float deltaTime);
    void Render();
    
    // Mouse handling for text selection
    bool OnMouseDown(float x, float y);
    bool OnMouseMove(float x, float y);
    bool OnMouseUp(float x, float y);
    bool OnKeyDown(int key); // For Ctrl+C
    
private:
    DebugConsole() = default;
    
    void CreateUI();
    void DestroyUI();
    void UpdateUI();
    
    // Text measurement helpers
    float MeasureTextWidth(const std::string& text);
    int GetCharacterAtX(const std::string& text, float x);
    float GetCharacterX(const std::string& text, int charIndex);
    TextPosition ScreenToTextPosition(float x, float y);
    std::string GetSelectedText();
    void CopyToClipboard(const std::string& text);
    
    bool m_visible = false;
    bool m_initialized = false;
    
    // Text selection state
    bool m_selecting = false;
    TextPosition m_selectionStart;
    TextPosition m_selectionEnd;
    float m_consoleTop = 0.0f;
    float m_consoleHeight = 0.0f;
    float m_logAreaLeft = 10.0f;
    
    // Font metrics (cached)
    float m_fontSize = 13.0f;
    float m_lineHeight = 16.0f;
    float m_charWidth = 7.5f; // Approximate, will be measured
    
    std::deque<std::string> m_logs;
    static constexpr size_t MAX_LOGS = 100;
    static constexpr size_t MAX_VISIBLE_LOGS = 25;
    
    struct ConsoleUI {
        std::shared_ptr<Panorama::CPanel2D> root;
        std::shared_ptr<Panorama::CPanel2D> background;
        std::shared_ptr<Panorama::CPanel2D> logContainer;
        std::vector<std::shared_ptr<Panorama::CLabel>> logLabels;
        std::shared_ptr<Panorama::CButton> openLogButton;
        std::shared_ptr<Panorama::CButton> copyAllButton;
        // Selection highlight panels (one per visible line)
        std::vector<std::shared_ptr<Panorama::CPanel2D>> selectionHighlights;
    };
    
    std::unique_ptr<ConsoleUI> m_ui;
};

// Convenience function
inline void ConsoleLog(const std::string& text) {
    DebugConsole::Instance().AddLog(text);
}

} // namespace Game
