/**
 * Game.exe - Main Entry Point
 * Standalone game executable with Panorama UI and DirectX 12
 * Uses DX11 for UI (Panorama) and DX12 for world rendering
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "GameState.h"
#include "DebugConsole.h"
#include "SettingsManager.h"
#include "ui/panorama/core/CUIEngine.h"
#include "../network/NetworkCommon.h"
#include "../renderer/DirectXRenderer.h"
#include "../world/World.h"
#include "../serialization/MapIO.h"

#pragma comment(lib, "d3d11.lib")

// Simple logging
static std::ofstream g_logFile;
static void Log(const char* msg) {
    if (!g_logFile.is_open()) {
        g_logFile.open("game_debug.log", std::ios::out | std::ios::trunc);
    }
    if (g_logFile.is_open()) {
        g_logFile << msg << std::endl;
        g_logFile.flush();
    }
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

// Window and DirectX 12 globals
HWND g_hWnd = nullptr;
DirectXRenderer* g_renderer = nullptr;

// Game World (for map rendering)
WorldEditor::World* g_gameWorld = nullptr;

int g_screenWidth = 0;
int g_screenHeight = 0;
const wchar_t* WINDOW_TITLE = L"Game - Panorama UI";

bool g_running = true;
bool g_exitRequested = false;
bool g_fullscreen = false;  // Use borderless window instead

// Forward declarations
bool InitWindow(HINSTANCE hInstance);
bool InitDirectX();
void CleanupDirectX();
void Render();
void GetNativeResolution(int& width, int& height);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============ Main Entry Point ============

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    
    // Initialize spdlog to file with PID to separate multiple clients
    try {
        DWORD pid = GetCurrentProcessId();
        std::string logPath = "logs/game_" + std::to_string(pid) + ".log";
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath, true);
        auto logger = std::make_shared<spdlog::logger>("game", file_sink);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
        spdlog::flush_on(spdlog::level::debug);  // Flush immediately
    } catch (...) {
        // Fallback to stdout if file fails
        auto console = spdlog::stdout_color_mt("game");
        spdlog::set_default_logger(console);
    }
    
    Log("=== Game.exe starting ===");
    LOG_INFO("Game.exe starting with spdlog");
    
    // Set window size for windowed mode (or get native for fullscreen)
    if (g_fullscreen) {
        GetNativeResolution(g_screenWidth, g_screenHeight);
    } else {
        // Default windowed size - 1280x720 (720p)
        g_screenWidth = 1280;
        g_screenHeight = 720;
    }
    Log(("Resolution: " + std::to_string(g_screenWidth) + "x" + std::to_string(g_screenHeight)).c_str());
    LOG_INFO("Resolution: {}x{}", g_screenWidth, g_screenHeight);
    
    // Initialize window
    if (!InitWindow(hInstance)) {
        Log("ERROR: Failed to create window");
        MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    Log("Window created OK");
    
    // Initialize DirectX 12
    if (!InitDirectX()) {
        Log("ERROR: Failed to initialize DirectX 12");
        MessageBoxW(nullptr, L"Failed to initialize DirectX 12", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    Log("DirectX 12 initialized OK");
    
    // Initialize Panorama UI
    Panorama::UIEngineConfig uiConfig;
    uiConfig.screenWidth = (float)g_screenWidth;
    uiConfig.screenHeight = (float)g_screenHeight;
    uiConfig.uiScale = 1.0f;
    
    if (!Panorama::CUIEngine::Instance().Initialize(g_renderer->GetDevice(), g_renderer, uiConfig)) {
        Log("ERROR: Failed to initialize Panorama UI");
        MessageBoxW(nullptr, L"Failed to initialize Panorama UI", L"Error", MB_OK | MB_ICONERROR);
        CleanupDirectX();
        return 1;
    }
    Log("Panorama UI initialized OK");
    
    // Initialize Network System (Winsock)
    bool networkOk = WorldEditor::Network::NetworkSystem::Initialize();
    Log(networkOk ? "Network system initialized OK" : "WARNING: Network system init failed");
    LOG_INFO("Network system initialization: {}", networkOk ? "OK" : "FAILED");
    
    // Set render target for Panorama (DX12 doesn't need this step)
    // Panorama will render to the back buffer directly
    Log("Panorama render target configured");
    
    // Initialize Game State Manager
    Game::GameStateManager::Instance().Initialize();
    Log("Game state manager initialized OK");
    
    // Initialize Debug Console
    Game::DebugConsole::Instance().Initialize();
    Game::ConsoleLog("Debug Console Ready!");
    Game::ConsoleLog("Press ~ to toggle console");
    Game::ConsoleLog("Game starting...");
    Log("Debug console initialized OK");
    
    // Subscribe to exit event
    Panorama::GameEvents_Subscribe("Game_RequestExit", [](const Panorama::CGameEventData&) {
        g_exitRequested = true;
    });
    
    // Show window (fullscreen)
    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);
    Log("Window shown, entering main loop");
    
    // Main loop
    auto lastTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    
    MSG msg = {};
    static int loopCount = 0;
    while (g_running && !g_exitRequested) {
        loopCount++;
        if (loopCount <= 10 || loopCount % 1000 == 0) {
            LOG_INFO("Main loop iteration #{}", loopCount);
        }
        
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                LOG_INFO("Main loop: WM_QUIT received");
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Log if we're about to exit
        if (!g_running) {
            LOG_INFO("Main loop: g_running=false, exiting");
            spdlog::default_logger()->flush();
        }
        if (g_exitRequested) {
            LOG_INFO("Main loop: g_exitRequested=true, exiting");
            spdlog::default_logger()->flush();
        }
        if (!g_running) break;
        
        // Log first few frames
        frameCount++;
        if (frameCount <= 3) {
            Log(("Frame " + std::to_string(frameCount)).c_str());
        }
        
        // Calculate delta time
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        
        // Apply FPS limit if set (maxFPS > 0)
        // Load settings to get current maxFPS value
        static bool settingsLoaded = false;
        static u16 maxFPS = 300; // Default to 300 FPS
        if (!settingsLoaded) {
            Game::SettingsManager::Instance().Load("settings.json");
            maxFPS = Game::SettingsManager::Instance().Video().maxFPS;
            settingsLoaded = true;
            if (maxFPS > 0) {
                Log(("FPS limit set to " + std::to_string(maxFPS)).c_str());
            } else {
                Log("FPS unlimited");
            }
        }
        
        if (maxFPS > 0) {
            const float targetFrameTime = 1.0f / maxFPS;
            if (deltaTime < targetFrameTime) {
                // Sleep for the remaining time
                auto sleepTime = std::chrono::duration<float>(targetFrameTime - deltaTime);
                std::this_thread::sleep_for(sleepTime);
                
                // Recalculate delta time after sleep
                currentTime = std::chrono::high_resolution_clock::now();
                deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            }
        }
        
        lastTime = currentTime;
        
        // Cap delta time
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        
        // Update game state
        Game::GameStateManager::Instance().Update(deltaTime);
        
        // Update debug console
        Game::DebugConsole::Instance().Update(deltaTime);
        
        // Render
        try {
            Render();
        } catch (const DirectXException& e) {
            // High-signal diagnostics for the common "stuck on last frame" issue:
            // if rendering fails, the window shows the last successfully presented frame.
            LOG_ERROR("Render exception (DirectXException): {}", e.what());
            LOG_ERROR("  - HRESULT: 0x{:08X}", (unsigned)e.GetHRESULT());
            if (g_renderer && g_renderer->GetDevice()) {
                const HRESULT removed = g_renderer->GetDevice()->GetDeviceRemovedReason();
                LOG_ERROR("  - DeviceRemovedReason: 0x{:08X}", (unsigned)removed);
            }
            spdlog::default_logger()->flush();
        } catch (const std::exception& e) {
            LOG_ERROR("Render exception: {}", e.what());
            spdlog::default_logger()->flush();
        } catch (...) {
            LOG_ERROR("Render unknown exception");
            spdlog::default_logger()->flush();
        }
    }
    
    // Cleanup
    Game::DebugConsole::Instance().Shutdown();
    Game::GameStateManager::Instance().Shutdown();
    Panorama::CUIEngine::Instance().Shutdown();
    WorldEditor::Network::NetworkSystem::Shutdown();
    CleanupDirectX();
    
    return 0;
}

// ============ Get Native Display Resolution ============

void GetNativeResolution(int& width, int& height) {
    // Get the primary monitor's native resolution using EnumDisplaySettings
    DEVMODEW devMode = {};
    devMode.dmSize = sizeof(DEVMODEW);
    
    // ENUM_CURRENT_SETTINGS gets the current display mode
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &devMode)) {
        width = devMode.dmPelsWidth;
        height = devMode.dmPelsHeight;
    } else {
        // Fallback to system metrics
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
    }
    
    // Ensure minimum resolution
    if (width < 800) width = 1920;
    if (height < 600) height = 1080;
}

// ============ Window Initialization ============

bool InitWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"GameWindowClass";
    
    if (!RegisterClassExW(&wc)) {
        return false;
    }
    
    // Window style depends on fullscreen mode
    DWORD style, exStyle;
    int x, y, w, h;
    
    if (g_fullscreen) {
        // Borderless fullscreen
        style = WS_POPUP;
        exStyle = WS_EX_APPWINDOW;
        x = 0;
        y = 0;
        w = g_screenWidth;
        h = g_screenHeight;
    } else {
        // Windowed mode with title bar and borders, but NO free resize
        // WS_OVERLAPPEDWINDOW = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
        // Remove WS_THICKFRAME to disable edge resizing, keep maximize/minimize buttons
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        exStyle = WS_EX_APPWINDOW;
        
        // Calculate window size to get desired client area
        RECT windowRect = { 0, 0, static_cast<LONG>(g_screenWidth), static_cast<LONG>(g_screenHeight) };
        AdjustWindowRectEx(&windowRect, style, FALSE, exStyle);
        
        w = windowRect.right - windowRect.left;
        h = windowRect.bottom - windowRect.top;
        
        // Center window on screen
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        x = (screenW - w) / 2;
        y = (screenH - h) / 2;
    }
    
    g_hWnd = CreateWindowExW(
        exStyle,
        L"GameWindowClass",
        WINDOW_TITLE,
        style,
        x, y,
        w, h,
        nullptr, nullptr,
        hInstance, nullptr
    );
    
    return g_hWnd != nullptr;
}

// ============ DirectX 12 Initialization ============

bool InitDirectX() {
    try {
        g_renderer = new DirectXRenderer();
        
        if (!g_renderer->Initialize(g_hWnd, g_screenWidth, g_screenHeight)) {
            Log("ERROR: DirectXRenderer::Initialize failed");
            delete g_renderer;
            g_renderer = nullptr;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        Log(("ERROR: DirectX initialization exception: " + std::string(e.what())).c_str());
        if (g_renderer) {
            delete g_renderer;
            g_renderer = nullptr;
        }
        return false;
    }
}

void CleanupDirectX() {
    if (g_renderer) {
        delete g_renderer;
        g_renderer = nullptr;
    }
}

// ============ Render ============

void Render() {
    if (!g_renderer) {
        LOG_WARN("Render: g_renderer is null");
        return;
    }
    
    static int renderCount = 0;
    renderCount++;
    if (renderCount <= 10 || renderCount % 1000 == 0) {
        LOG_INFO("Render() call #{}, screen={}x{}", renderCount, g_screenWidth, g_screenHeight);
    }
    
    // Skip rendering if minimized
    if (g_screenWidth == 0 || g_screenHeight == 0) {
        if (renderCount % 1000 == 0) {
            LOG_INFO("Render: skipping (minimized)");
        }
        return;
    }
    
    try {
        // Begin frame
        if (renderCount <= 10 || renderCount % 1000 == 0) {
            LOG_INFO("Render: calling BeginFrame...");
        }
        g_renderer->BeginFrame();
        
        if (renderCount <= 10 || renderCount % 1000 == 0) {
            LOG_INFO("Render: calling BeginSwapchainPass...");
        }
        
        // Clear and begin rendering to swapchain
        float clearColor[4] = { 0.02f, 0.04f, 0.08f, 1.0f };  // Dark blue background
        g_renderer->BeginSwapchainPass(clearColor);
        
        if (renderCount <= 10 || renderCount % 1000 == 0) {
            LOG_INFO("Render: calling GameStateManager::Render...");
        }
        
        // Render game state (includes world and UI)
        Game::GameStateManager::Instance().Render();
        
        if (renderCount <= 10 || renderCount % 1000 == 0) {
            LOG_INFO("Render: calling EndFrame...");
        }
        
        // End frame and present
        g_renderer->EndFrame();
        
        if (renderCount <= 10 || renderCount % 1000 == 0) {
            LOG_INFO("Render: calling Present...");
        }
        g_renderer->Present();
    } catch (const DirectXException& e) {
        LOG_ERROR("Render DirectXException: {} (HRESULT: 0x{:08X})", e.what(), (unsigned)e.GetHRESULT());
        spdlog::default_logger()->flush();
    } catch (const std::exception& e) {
        LOG_ERROR("Render exception: {}", e.what());
        spdlog::default_logger()->flush();
    } catch (...) {
        LOG_ERROR("Render unknown exception");
        spdlog::default_logger()->flush();
    }
}

// ============ Window Procedure ============

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& gameState = Game::GameStateManager::Instance();
    
    switch (msg) {
        case WM_DESTROY:
            LOG_INFO("WM_DESTROY received");
            PostQuitMessage(0);
            return 0;
            
        case WM_CLOSE:
            LOG_INFO("WM_CLOSE received");
            // Ensure the window is actually destroyed; otherwise we may stop pumping messages
            // while a borderless fullscreen window still exists, which can look like a "glitch".
            g_running = false;
            DestroyWindow(hWnd);
            return 0;
            
        case WM_KEYDOWN:
            // Tilde (~) to toggle console
            if (wParam == 192 || wParam == VK_OEM_3) { // ~ key
                Game::DebugConsole::Instance().Toggle();
                return 0;
            }
            // Alt+Enter to toggle fullscreen (optional)
            if (wParam == VK_RETURN && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
                // Toggle fullscreen would go here
                return 0;
            }
            // ESC handling depends on current state
            if (wParam == VK_ESCAPE) {
                LOG_INFO("VK_ESCAPE pressed");
                auto currentState = gameState.GetCurrentStateType();
                // In game states, ESC opens pause menu (handled by state)
                // In menu states, ESC exits the game
                if (currentState == Game::EGameState::MainMenu || 
                    currentState == Game::EGameState::Login) {
                    LOG_INFO("ESC in Login/MainMenu - exiting");
                    g_running = false;
                    DestroyWindow(hWnd);
                    return 0;
                }
                // Otherwise let the state handle it (pause menu, etc.)
            }
            gameState.OnKeyDown((int)wParam);
            return 0;
            
        case WM_KEYUP:
            gameState.OnKeyUp((int)wParam);
            return 0;
            
        case WM_CHAR: {
            // Handle text input for UI
            if (wParam >= 32 && wParam < 127) { // Printable ASCII
                char ch = (char)wParam;
                std::string text(1, ch);
                Panorama::CUIEngine::Instance().OnTextInput(text);
            }
            return 0;
        }
            
        case WM_SYSKEYDOWN:
            // Handle Alt+F4
            if (wParam == VK_F4 && (lParam & (1 << 29))) {
                g_running = false;
                DestroyWindow(hWnd);
                return 0;
            }
            break;
            
        case WM_MOUSEMOVE: {
            float x = (float)LOWORD(lParam);
            float y = (float)HIWORD(lParam);
            gameState.OnMouseMove(x, y);
            return 0;
        }
        
        case WM_LBUTTONDOWN: {
            float x = (float)LOWORD(lParam);
            float y = (float)HIWORD(lParam);
            gameState.OnMouseDown(x, y, 0);
            SetCapture(hWnd);
            return 0;
        }
        
        case WM_LBUTTONUP: {
            float x = (float)LOWORD(lParam);
            float y = (float)HIWORD(lParam);
            gameState.OnMouseUp(x, y, 0);
            ReleaseCapture();
            return 0;
        }
        
        case WM_RBUTTONDOWN: {
            float x = (float)LOWORD(lParam);
            float y = (float)HIWORD(lParam);
            gameState.OnMouseDown(x, y, 1);
            return 0;
        }
        
        case WM_RBUTTONUP: {
            float x = (float)LOWORD(lParam);
            float y = (float)HIWORD(lParam);
            gameState.OnMouseUp(x, y, 1);
            return 0;
        }
        
        case WM_SIZE: {
            const char* sizeType = "UNKNOWN";
            if (wParam == SIZE_RESTORED) sizeType = "SIZE_RESTORED";
            else if (wParam == SIZE_MINIMIZED) sizeType = "SIZE_MINIMIZED";
            else if (wParam == SIZE_MAXIMIZED) sizeType = "SIZE_MAXIMIZED";
            else if (wParam == SIZE_MAXSHOW) sizeType = "SIZE_MAXSHOW";
            else if (wParam == SIZE_MAXHIDE) sizeType = "SIZE_MAXHIDE";
            
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            LOG_INFO("WM_SIZE: type={}, size={}x{}, current={}x{}", sizeType, width, height, g_screenWidth, g_screenHeight);
            
            // Handle minimize - set screen size to 0 to skip rendering
            if (wParam == SIZE_MINIMIZED) {
                g_screenWidth = 0;
                g_screenHeight = 0;
                LOG_INFO("Window minimized, skipping render");
                return 0;
            }
            
            if (g_renderer && wParam != SIZE_MINIMIZED) {
                if (width > 0 && height > 0) {
                    try {
                        g_screenWidth = width;
                        g_screenHeight = height;
                        LOG_INFO("Calling g_renderer->Resize({}x{})", width, height);
                        g_renderer->Resize(width, height);
                        LOG_INFO("Calling CUIEngine::SetScreenSize({}x{})", width, height);
                        Panorama::CUIEngine::Instance().SetScreenSize((float)width, (float)height);
                        LOG_INFO("Calling GameStateManager::OnResize({}x{})", width, height);
                        Game::GameStateManager::Instance().OnResize((float)width, (float)height);
                        LOG_INFO("WM_SIZE handling complete");
                    } catch (const std::exception& e) {
                        LOG_ERROR("WM_SIZE exception: {}", e.what());
                        spdlog::default_logger()->flush();
                    } catch (...) {
                        LOG_ERROR("WM_SIZE unknown exception");
                        spdlog::default_logger()->flush();
                    }
                }
            }
            return 0;
        }
    }
    
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
