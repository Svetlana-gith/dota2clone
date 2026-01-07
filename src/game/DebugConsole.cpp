#include "DebugConsole.h"
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace Game {

DebugConsole& DebugConsole::Instance() {
    static DebugConsole instance;
    return instance;
}

void DebugConsole::Initialize() {
    if (m_initialized) return;
    
    m_ui = std::make_unique<ConsoleUI>();
    m_initialized = true;
    
    // Measure character width using DirectWrite
    auto* renderer = Panorama::CUIEngine::Instance().GetRenderer();
    if (renderer) {
        Panorama::FontInfo font;
        font.family = "Consolas"; // Monospace font for console
        font.size = m_fontSize;
        auto size = renderer->MeasureText("M", font);
        m_charWidth = size.x;
    }
    
    AddLog("=== Debug Console Initialized ===");
    AddLog("Press ~ (tilde) to toggle console");
    AddLog("Click and drag to select text");
    AddLog("Ctrl+C to copy, Ctrl+A to select all");
}

void DebugConsole::Shutdown() {
    if (!m_initialized) return;
    DestroyUI();
    m_ui.reset();
    m_initialized = false;
}

void DebugConsole::Toggle() {
    if (m_visible) Hide();
    else Show();
}

void DebugConsole::Show() {
    if (!m_initialized) return;
    m_visible = true;
    CreateUI();
    UpdateUI();
}

void DebugConsole::Hide() {
    m_visible = false;
    m_selecting = false;
    m_selectionStart = TextPosition();
    m_selectionEnd = TextPosition();
    DestroyUI();
}

void DebugConsole::AddLog(const std::string& text) {
    m_logs.push_back(text);
    while (m_logs.size() > MAX_LOGS) {
        m_logs.pop_front();
    }
    if (m_visible && m_ui) {
        UpdateUI();
    }
    spdlog::info("[Console] {}", text);
}

void DebugConsole::Clear() {
    m_logs.clear();
    m_selectionStart = TextPosition();
    m_selectionEnd = TextPosition();
    if (m_visible && m_ui) {
        UpdateUI();
    }
}

// ============ Text Measurement Helpers ============

float DebugConsole::MeasureTextWidth(const std::string& text) {
    auto* renderer = Panorama::CUIEngine::Instance().GetRenderer();
    if (renderer) {
        Panorama::FontInfo font;
        font.family = "Consolas";
        font.size = m_fontSize;
        return renderer->MeasureText(text, font).x;
    }
    return text.length() * m_charWidth;
}

int DebugConsole::GetCharacterAtX(const std::string& text, float x) {
    if (text.empty() || x <= 0) return 0;
    
    auto* renderer = Panorama::CUIEngine::Instance().GetRenderer();
    if (!renderer) {
        return (int)(x / m_charWidth);
    }
    
    Panorama::FontInfo font;
    font.family = "Consolas";
    font.size = m_fontSize;
    
    // Binary search for character position
    int left = 0;
    int right = (int)text.length();
    
    while (left < right) {
        int mid = (left + right + 1) / 2;
        float width = renderer->MeasureText(text.substr(0, mid), font).x;
        if (width <= x) {
            left = mid;
        } else {
            right = mid - 1;
        }
    }
    
    // Check if we're closer to the next character
    if (left < (int)text.length()) {
        float widthLeft = renderer->MeasureText(text.substr(0, left), font).x;
        float widthRight = renderer->MeasureText(text.substr(0, left + 1), font).x;
        if (x - widthLeft > widthRight - x) {
            left++;
        }
    }
    
    return left;
}

float DebugConsole::GetCharacterX(const std::string& text, int charIndex) {
    if (charIndex <= 0 || text.empty()) return 0;
    if (charIndex >= (int)text.length()) charIndex = (int)text.length();
    
    auto* renderer = Panorama::CUIEngine::Instance().GetRenderer();
    if (renderer) {
        Panorama::FontInfo font;
        font.family = "Consolas";
        font.size = m_fontSize;
        return renderer->MeasureText(text.substr(0, charIndex), font).x;
    }
    return charIndex * m_charWidth;
}

TextPosition DebugConsole::ScreenToTextPosition(float x, float y) {
    TextPosition pos;
    
    float logAreaTop = m_consoleTop + 35.0f;
    float relativeY = y - logAreaTop;
    
    int lineIndex = (int)(relativeY / m_lineHeight);
    
    size_t startIdx = m_logs.size() > MAX_VISIBLE_LOGS ? m_logs.size() - MAX_VISIBLE_LOGS : 0;
    size_t visibleCount = m_logs.size() - startIdx;
    
    if (lineIndex < 0) lineIndex = 0;
    if (lineIndex >= (int)visibleCount) lineIndex = (int)visibleCount - 1;
    if (lineIndex < 0) return pos;
    
    pos.line = (int)startIdx + lineIndex;
    
    // Get character position
    float relativeX = x - m_logAreaLeft;
    if (relativeX < 0) relativeX = 0;
    
    if (pos.line < (int)m_logs.size()) {
        pos.character = GetCharacterAtX(m_logs[pos.line], relativeX);
    } else {
        pos.character = 0;
    }
    
    return pos;
}

std::string DebugConsole::GetSelectedText() {
    if (!m_selectionStart.IsValid() || !m_selectionEnd.IsValid()) {
        return "";
    }
    
    TextPosition start = m_selectionStart;
    TextPosition end = m_selectionEnd;
    
    // Normalize selection (start before end)
    if (end < start) std::swap(start, end);
    
    std::stringstream ss;
    
    for (int line = start.line; line <= end.line && line < (int)m_logs.size(); line++) {
        const std::string& logLine = m_logs[line];
        
        int startChar = (line == start.line) ? start.character : 0;
        int endChar = (line == end.line) ? end.character : (int)logLine.length();
        
        // Clamp to valid range
        if (startChar < 0) startChar = 0;
        if (endChar > (int)logLine.length()) endChar = (int)logLine.length();
        if (startChar > endChar) startChar = endChar;
        
        if (startChar < endChar) {
            ss << logLine.substr(startChar, endChar - startChar);
        }
        
        if (line < end.line) {
            ss << "\r\n";
        }
    }
    
    return ss.str();
}

void DebugConsole::CopyToClipboard(const std::string& text) {
#ifdef _WIN32
    if (text.empty()) return;
    
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            char* pMem = (char*)GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, text.c_str(), text.size() + 1);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
        }
        CloseClipboard();
    }
#endif
}

// ============ UI Creation ============

void DebugConsole::CreateUI() {
    if (!m_ui || m_ui->root) return;
    
    auto& engine = Panorama::CUIEngine::Instance();
    auto* uiRoot = engine.GetRoot();
    if (!uiRoot) return;
    
    float screenW = engine.GetScreenWidth();
    float screenH = engine.GetScreenHeight();
    
    m_consoleTop = 0.0f;
    m_consoleHeight = screenH * 0.6f;
    m_logAreaLeft = 10.0f;
    
    // Root panel
    m_ui->root = std::make_shared<Panorama::CPanel2D>("ConsoleRoot");
    m_ui->root->GetStyle().width = Panorama::Length::Fill();
    m_ui->root->GetStyle().height = Panorama::Length::Fill();
    uiRoot->AddChild(m_ui->root);
    
    // Console background
    m_ui->background = std::make_shared<Panorama::CPanel2D>("ConsoleBackground");
    m_ui->background->GetStyle().width = Panorama::Length::Fill();
    m_ui->background->GetStyle().height = Panorama::Length::Px(m_consoleHeight);
    m_ui->background->GetStyle().backgroundColor = Panorama::Color(0.05f, 0.05f, 0.08f, 0.95f);
    m_ui->background->GetStyle().verticalAlign = Panorama::VerticalAlign::Top;
    m_ui->background->GetStyle().borderWidth = 2.0f;
    m_ui->background->GetStyle().borderColor = Panorama::Color(0.2f, 0.3f, 0.4f, 0.8f);
    m_ui->root->AddChild(m_ui->background);
    
    // Title bar
    auto titleBar = std::make_shared<Panorama::CPanel2D>("ConsoleTitleBar");
    titleBar->GetStyle().width = Panorama::Length::Fill();
    titleBar->GetStyle().height = Panorama::Length::Px(30);
    titleBar->GetStyle().backgroundColor = Panorama::Color(0.1f, 0.15f, 0.2f, 0.95f);
    m_ui->background->AddChild(titleBar);
    
    auto title = std::make_shared<Panorama::CLabel>("DEVELOPER CONSOLE", "ConsoleTitle");
    title->GetStyle().fontSize = 16.0f;
    title->GetStyle().color = Panorama::Color(0.8f, 0.9f, 1.0f, 1.0f);
    title->GetStyle().marginLeft = Panorama::Length::Px(10);
    title->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    titleBar->AddChild(title);
    
    auto closeHint = std::make_shared<Panorama::CLabel>("~ close | Ctrl+C copy | Ctrl+A select all", "CloseHint");
    closeHint->GetStyle().fontSize = 11.0f;
    closeHint->GetStyle().color = Panorama::Color(0.5f, 0.5f, 0.5f, 1.0f);
    closeHint->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Right;
    closeHint->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    closeHint->GetStyle().marginRight = Panorama::Length::Px(140);
    titleBar->AddChild(closeHint);
    
    // Open Log File button
    m_ui->openLogButton = std::make_shared<Panorama::CButton>("Open Log", "OpenLogBtn");
    m_ui->openLogButton->GetStyle().width = Panorama::Length::Px(80);
    m_ui->openLogButton->GetStyle().height = Panorama::Length::Px(22);
    m_ui->openLogButton->GetStyle().backgroundColor = Panorama::Color(0.2f, 0.3f, 0.4f, 0.9f);
    m_ui->openLogButton->GetStyle().borderRadius = 3.0f;
    m_ui->openLogButton->GetStyle().fontSize = 10.0f;
    m_ui->openLogButton->GetStyle().color = Panorama::Color::White();
    m_ui->openLogButton->GetStyle().horizontalAlign = Panorama::HorizontalAlign::Right;
    m_ui->openLogButton->GetStyle().verticalAlign = Panorama::VerticalAlign::Center;
    m_ui->openLogButton->GetStyle().marginRight = Panorama::Length::Px(10);
    m_ui->openLogButton->SetOnActivate([]() {
        system("notepad.exe logs\\game.log");
    });
    titleBar->AddChild(m_ui->openLogButton);
    
    // Log container
    m_ui->logContainer = std::make_shared<Panorama::CPanel2D>("LogContainer");
    m_ui->logContainer->GetStyle().width = Panorama::Length::Fill();
    m_ui->logContainer->GetStyle().height = Panorama::Length::Px(m_consoleHeight - 40);
    m_ui->logContainer->GetStyle().marginTop = Panorama::Length::Px(35);
    m_ui->logContainer->GetStyle().marginLeft = Panorama::Length::Px(5);
    m_ui->logContainer->GetStyle().marginRight = Panorama::Length::Px(5);
    m_ui->logContainer->GetStyle().backgroundColor = Panorama::Color(0.02f, 0.02f, 0.04f, 0.8f);
    m_ui->background->AddChild(m_ui->logContainer);
}

void DebugConsole::DestroyUI() {
    if (!m_ui || !m_ui->root) return;
    
    auto& engine = Panorama::CUIEngine::Instance();
    engine.ClearInputStateForSubtree(m_ui->root.get());
    auto* uiRoot = engine.GetRoot();
    if (uiRoot) {
        uiRoot->RemoveChild(m_ui->root.get());
    }
    
    m_ui->root.reset();
    m_ui->background.reset();
    m_ui->logContainer.reset();
    m_ui->logLabels.clear();
    m_ui->selectionHighlights.clear();
}

void DebugConsole::UpdateUI() {
    if (!m_ui || !m_ui->logContainer) return;
    
    // Clear old elements
    for (auto& label : m_ui->logLabels) {
        m_ui->logContainer->RemoveChild(label.get());
    }
    m_ui->logLabels.clear();
    
    for (auto& highlight : m_ui->selectionHighlights) {
        m_ui->logContainer->RemoveChild(highlight.get());
    }
    m_ui->selectionHighlights.clear();
    
    // Get visible log range
    size_t startIdx = m_logs.size() > MAX_VISIBLE_LOGS ? m_logs.size() - MAX_VISIBLE_LOGS : 0;
    
    // Normalize selection
    TextPosition selStart = m_selectionStart;
    TextPosition selEnd = m_selectionEnd;
    if (selEnd < selStart) std::swap(selStart, selEnd);
    
    float yOffset = 5.0f;
    
    for (size_t i = startIdx; i < m_logs.size(); i++) {
        const std::string& logText = m_logs[i];
        int lineNum = (int)i;
        
        // Check if this line has selection
        bool hasSelection = selStart.IsValid() && selEnd.IsValid() &&
                           lineNum >= selStart.line && lineNum <= selEnd.line;
        
        if (hasSelection) {
            // Calculate selection highlight bounds
            int startChar = (lineNum == selStart.line) ? selStart.character : 0;
            int endChar = (lineNum == selEnd.line) ? selEnd.character : (int)logText.length();
            
            if (startChar < 0) startChar = 0;
            if (endChar > (int)logText.length()) endChar = (int)logText.length();
            
            if (startChar < endChar) {
                float highlightX = GetCharacterX(logText, startChar);
                float highlightWidth = GetCharacterX(logText, endChar) - highlightX;
                
                auto highlight = std::make_shared<Panorama::CPanel2D>("Sel" + std::to_string(i));
                highlight->GetStyle().width = Panorama::Length::Px(highlightWidth);
                highlight->GetStyle().height = Panorama::Length::Px(m_lineHeight);
                highlight->GetStyle().marginLeft = Panorama::Length::Px(m_logAreaLeft + highlightX - 5);
                highlight->GetStyle().marginTop = Panorama::Length::Px(yOffset);
                highlight->GetStyle().backgroundColor = Panorama::Color(0.2f, 0.4f, 0.7f, 0.5f);
                
                m_ui->logContainer->AddChild(highlight);
                m_ui->selectionHighlights.push_back(highlight);
            }
        }
        
        // Create text label
        auto label = std::make_shared<Panorama::CLabel>(logText, "Log" + std::to_string(i));
        label->GetStyle().fontSize = m_fontSize;
        label->GetStyle().color = Panorama::Color(0.85f, 0.85f, 0.85f, 1.0f);
        label->GetStyle().marginLeft = Panorama::Length::Px(m_logAreaLeft - 5);
        label->GetStyle().marginTop = Panorama::Length::Px(yOffset);
        label->GetStyle().width = Panorama::Length::Fill();
        label->GetStyle().height = Panorama::Length::Px(m_lineHeight);
        
        m_ui->logContainer->AddChild(label);
        m_ui->logLabels.push_back(label);
        
        yOffset += m_lineHeight;
    }
}

void DebugConsole::Update(float deltaTime) {
    // Nothing to update
}

void DebugConsole::Render() {
    // Rendering handled by Panorama
}

// ============ Mouse Handling ============

bool DebugConsole::OnMouseDown(float x, float y) {
    if (!m_visible || !m_ui || !m_ui->logContainer) return false;
    
    float logAreaTop = m_consoleTop + 35.0f;
    float logAreaBottom = m_consoleTop + m_consoleHeight;
    
    if (y < logAreaTop || y > logAreaBottom) {
        // Click outside log area - clear selection
        m_selectionStart = TextPosition();
        m_selectionEnd = TextPosition();
        m_selecting = false;
        UpdateUI();
        return false;
    }
    
    m_selecting = true;
    m_selectionStart = ScreenToTextPosition(x, y);
    m_selectionEnd = m_selectionStart;
    
    UpdateUI();
    return true;
}

bool DebugConsole::OnMouseMove(float x, float y) {
    if (!m_visible || !m_selecting) return false;
    
    m_selectionEnd = ScreenToTextPosition(x, y);
    UpdateUI();
    return true;
}

bool DebugConsole::OnMouseUp(float x, float y) {
    if (!m_visible) return false;
    m_selecting = false;
    return true;
}

bool DebugConsole::OnKeyDown(int key) {
    if (!m_visible) return false;
    
#ifdef _WIN32
    // Ctrl+C to copy
    if (key == 'C' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
        std::string selected = GetSelectedText();
        if (!selected.empty()) {
            CopyToClipboard(selected);
            
            // Count lines for message
            int lineCount = 1;
            for (char c : selected) {
                if (c == '\n') lineCount++;
            }
            AddLog("Copied " + std::to_string(selected.length()) + " chars (" + 
                   std::to_string(lineCount) + " lines) to clipboard");
        }
        return true;
    }
    
    // Ctrl+A to select all
    if (key == 'A' && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
        if (!m_logs.empty()) {
            m_selectionStart.line = 0;
            m_selectionStart.character = 0;
            m_selectionEnd.line = (int)m_logs.size() - 1;
            m_selectionEnd.character = (int)m_logs.back().length();
            UpdateUI();
            AddLog("Selected all text");
        }
        return true;
    }
#endif
    
    return false;
}

} // namespace Game
