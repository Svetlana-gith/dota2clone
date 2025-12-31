#include <iostream>
#include <filesystem>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <Windows.h>
#include <DbgHelp.h>
#include <tchar.h>
#include <dxgiformat.h>
#include <algorithm>
#include "renderer/DirectXRenderer.h"
#include "renderer/LightingSystem.h"
#include "renderer/SkyRenderer.h"
#include "renderer/WireframeGrid.h"
#include "world/World.h"
#include "world/Components.h"
#include "world/World.h"
#include "ui/EditorUI.h"
#include "ui/EditorCamera.h"
#include "ui/Picking.h"
#include "ui/GameMode.h"
#include "world/TerrainMesh.h"
#include "world/TerrainRaycast.h"
#include "world/TerrainTools.h"
#include "world/TerrainChunks.h"
#include "world/MeshGenerators.h"
#include "core/Timer.h"
#include "properties/Properties.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

// Use MeshGenerators from world library
using WorldEditor::MeshGenerators::GenerateCylinder;
using WorldEditor::MeshGenerators::GenerateSphere;
using WorldEditor::MeshGenerators::GenerateCone;
using WorldEditor::MeshGenerators::GenerateIrregularRock;

void SetupRunlogsAndLogging() {
    namespace fs = std::filesystem;
    try {
        fs::create_directories("runlogs");
    } catch (...) {
        // If we can't create runlogs, continue; at least we'll try to print to console.
    }

    // Always redirect stdout/stderr to files so we can diagnose crashes on machines where the window
    // closes instantly (double-click, Start-Process, etc.). This is an internal dev tool, so file logs
    // are more valuable than console output.
    FILE* stdoutFile = nullptr;
    FILE* stderrFile = nullptr;
    (void)freopen_s(&stdoutFile, "runlogs/WorldEditor.stdout.log", "a", stdout);
    (void)freopen_s(&stderrFile, "runlogs/WorldEditor.stderr.log", "a", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // Also set up spdlog to write into a file (LOG_INFO/ERROR).
    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("runlogs/WorldEditor.spdlog.log", true);
        auto logger = std::make_shared<spdlog::logger>("WorldEditor", spdlog::sinks_init_list{ file_sink });
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        spdlog::flush_on(spdlog::level::info);
    } catch (...) {
        // Fall back silently; stdout/stderr redirection still captures enough.
    }

    // Add a clear session boundary to stdout/stderr logs (append mode).
    try {
        const auto now = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &now);
        char buf[64]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        std::cout << "\n=== WorldEditor session start: " << buf << " ===\n";
        std::cout << "cwd: " << fs::current_path().string() << std::endl;
        std::cerr << "\n=== WorldEditor session start: " << buf << " ===\n";
        std::cerr << "cwd: " << fs::current_path().string() << std::endl;
    } catch (...) {}
}

static void WriteMiniDump(EXCEPTION_POINTERS* ep) {
    // Best-effort crash dump to help debug issues that reproduce only on another PC.
    HANDLE hFile = CreateFileA("runlogs/WorldEditor.dmp",
                              GENERIC_WRITE,
                              FILE_SHARE_READ,
                              nullptr,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    // Include enough info for stacks + loaded modules.
    const MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules |
        MiniDumpWithProcessThreadData
    );

    (void)MiniDumpWriteDump(GetCurrentProcess(),
                            GetCurrentProcessId(),
                            hFile,
                            type,
                            ep ? &mei : nullptr,
                            nullptr,
                            nullptr);
    CloseHandle(hFile);
}

static void PrintStackTraceFromException(EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ContextRecord) {
        return;
    }

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    if (!SymInitialize(process, nullptr, TRUE)) {
        std::cerr << "SymInitialize failed (GetLastError=" << GetLastError() << ")\n";
        return;
    }

    CONTEXT ctx = *ep->ContextRecord; // local copy for StackWalk64 to mutate

    STACKFRAME64 frame{};
    DWORD machineType = 0;
#if defined(_M_X64) || defined(__x86_64__)
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrStack.Offset = ctx.Rsp;
#elif defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = ctx.Eip;
    frame.AddrFrame.Offset = ctx.Ebp;
    frame.AddrStack.Offset = ctx.Esp;
#else
    // Unsupported arch for now.
    SymCleanup(process);
    return;
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    std::cerr << "StackTrace:\n";
    for (int i = 0; i < 64; ++i) {
        if (!StackWalk64(machineType, process, thread, &frame, &ctx, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }
        if (frame.AddrPC.Offset == 0) {
            break;
        }

        DWORD64 addr = frame.AddrPC.Offset;

        // Module name
        IMAGEHLP_MODULE64 mod{};
        mod.SizeOfStruct = sizeof(mod);
        const char* modName = "<unknown>";
        if (SymGetModuleInfo64(process, addr, &mod)) {
            modName = mod.ModuleName;
        }

        // Symbol name
        char symBuf[sizeof(SYMBOL_INFO) + 512]{};
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 512;
        DWORD64 disp = 0;
        const char* symName = "<no symbol>";
        if (SymFromAddr(process, addr, &disp, sym)) {
            symName = sym->Name;
        }

        // Source line (best-effort)
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        if (SymGetLineFromAddr64(process, addr, &lineDisp, &line)) {
            std::cerr << "  #" << i << " " << modName << "!" << symName
                      << " +0x" << std::hex << disp << std::dec
                      << " (" << line.FileName << ":" << line.LineNumber << ")\n";
        } else {
            std::cerr << "  #" << i << " " << modName << "!" << symName
                      << " +0x" << std::hex << disp << std::dec << "\n";
        }
    }

    SymCleanup(process);
}

static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* ep) {
    // This catches SEH exceptions (access violation, etc.) that C++ try/catch won't.
    const DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    const void* addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;

    std::ostringstream oss;
    oss << "FATAL: Unhandled exception. Code=0x" << std::hex << code
        << " Address=" << addr << std::dec << "\n";
    std::cerr << oss.str();

    PrintStackTraceFromException(ep);
    WriteMiniDump(ep);

    // Also show a MessageBox so a double-click launch doesn't "silently close".
    const std::string msg = oss.str() + "A crash dump was written to runlogs/WorldEditor.dmp.\nSee runlogs/WorldEditor.*.log for details.";
    MessageBoxA(nullptr, msg.c_str(), "WorldEditor crash", MB_OK | MB_ICONERROR);

    return EXCEPTION_EXECUTE_HANDLER;
}
} // namespace

// Window close is handled by the editor (unsaved changes prompt).
static bool g_closeRequested = false;

// Window procedure for the demo window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_GETMINMAXINFO: {
            // Ensure maximized window fits the monitor work area (excludes taskbar).
            // This also prevents the client area from spilling off-screen on some DPI/scale configs.
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfo(monitor, &mi)) {
                const RECT rcWork = mi.rcWork;
                const RECT rcMon = mi.rcMonitor;
                mmi->ptMaxPosition.x = rcWork.left - rcMon.left;
                mmi->ptMaxPosition.y = rcWork.top - rcMon.top;
                mmi->ptMaxSize.x = rcWork.right - rcWork.left;
                mmi->ptMaxSize.y = rcWork.bottom - rcWork.top;
            }
            return 0;
        }
        case WM_DPICHANGED: {
            // Recommended rect is in lParam. Apply it to avoid weird sizing on DPI changes.
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(hwnd, nullptr,
                         suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SIZE: {
            if (wParam != SIZE_MINIMIZED) {
                auto* renderer = reinterpret_cast<DirectXRenderer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
                if (renderer) {
                    const uint32_t w = static_cast<uint32_t>(LOWORD(lParam));
                    const uint32_t h = static_cast<uint32_t>(HIWORD(lParam));
                    renderer->Resize(w, h);
                }
            }
            break;
        }
        case WM_CLOSE: {
            g_closeRequested = true;
            return 0; // prevent default DestroyWindow; editor will decide
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            break;
    }

    // Let ImGui process the rest of messages.
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) {
        return true;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main(int argc, char* argv[]) {
    try {
        SetupRunlogsAndLogging();
        SetUnhandledExceptionFilter(UnhandledExceptionHandler);
        std::cout << "DirectX World Editor starting..." << std::endl;

        // Prefer per-monitor DPI awareness so fullscreen/maximize sizing is correct on scaled displays.
        // (Safe to call on Win10+; on older systems it simply fails.)
        (void)SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Register window class
        WNDCLASSEX windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEX);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = GetModuleHandle(nullptr);
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.lpszClassName = _T("DXWorldEditorWindow");

        if (!RegisterClassEx(&windowClass)) {
            const DWORD err = GetLastError();
            std::ostringstream oss;
            oss << "Failed to register window class (GetLastError=" << err << ")";
            throw std::runtime_error(oss.str());
        }

        // Create window
        RECT windowRect = { 0, 0, 1280, 720 };
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        HWND hwnd = CreateWindowEx(
            0,
            _T("DXWorldEditorWindow"),
            _T("DirectX World Editor"),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );

        if (!hwnd) {
            const DWORD err = GetLastError();
            std::ostringstream oss;
            oss << "Failed to create window (GetLastError=" << err << ")";
            throw std::runtime_error(oss.str());
        }

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        std::cout << "Window created successfully. hwnd=" << hwnd << std::endl;

        // Create DirectX renderer
        DirectXRenderer renderer;
        RECT clientRect{};
        GetClientRect(hwnd, &clientRect);
        const UINT initW = std::max<UINT>(1u, static_cast<UINT>(clientRect.right - clientRect.left));
        const UINT initH = std::max<UINT>(1u, static_cast<UINT>(clientRect.bottom - clientRect.top));
        if (!renderer.Initialize(hwnd, initW, initH)) {
            throw std::runtime_error("Failed to initialize DirectX renderer");
        }

        // Allow WindowProc to access the renderer for WM_SIZE resize handling.
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&renderer));

        // Setup ImGui (Win32 + DX12).
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui_ImplWin32_Init(hwnd);

        // DX12 backend (docking branch) requires InitInfo with CommandQueue and SRV allocator callbacks.
        struct ImGuiSrvAllocator {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuBase{};
            D3D12_GPU_DESCRIPTOR_HANDLE gpuBase{};
            UINT increment = 0;
            UINT capacity = 0;
            UINT next = 0;
        };
        static ImGuiSrvAllocator g_imguiSrvAlloc;
        // Reserve descriptor 0 for the offscreen viewport texture SRV.
        const UINT kSrvReserved = 16;
        g_imguiSrvAlloc.cpuBase = renderer.GetSrvCpuHandle();
        g_imguiSrvAlloc.cpuBase.ptr += SIZE_T(kSrvReserved) * SIZE_T(renderer.GetSrvDescriptorSize());
        g_imguiSrvAlloc.gpuBase = renderer.GetSrvGpuHandle();
        g_imguiSrvAlloc.gpuBase.ptr += UINT64(kSrvReserved) * UINT64(renderer.GetSrvDescriptorSize());
        g_imguiSrvAlloc.increment = renderer.GetSrvDescriptorSize();
        g_imguiSrvAlloc.capacity = 64 - kSrvReserved;
        g_imguiSrvAlloc.next = 0;

        ImGui_ImplDX12_InitInfo dx12Info;
        dx12Info.Device = renderer.GetDevice();
        dx12Info.CommandQueue = renderer.GetCommandQueue();
        dx12Info.NumFramesInFlight = 3;
        dx12Info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        dx12Info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        dx12Info.SrvDescriptorHeap = renderer.GetSrvHeap();
        dx12Info.UserData = &g_imguiSrvAlloc;
        dx12Info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu) {
            auto* alloc = static_cast<ImGuiSrvAllocator*>(info->UserData);
            // For now, a simple bump allocator from a dedicated heap is enough for editor UI.
            // (The backend may request more than one descriptor in future versions; capacity is reserved.)
            if (alloc->next >= alloc->capacity) {
                *out_cpu = {};
                *out_gpu = {};
                return;
            }
            const UINT idx = alloc->next++;
            out_cpu->ptr = alloc->cpuBase.ptr + SIZE_T(idx) * SIZE_T(alloc->increment);
            out_gpu->ptr = alloc->gpuBase.ptr + UINT64(idx) * UINT64(alloc->increment);
        };
        dx12Info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {
            // No-op (simple allocator). Fine for editor session lifetime.
        };

        if (!ImGui_ImplDX12_Init(&dx12Info)) {
            throw std::runtime_error("Failed to initialize ImGui DX12 backend");
        }

        // Create world with ECS
        WorldEditor::World world(renderer.GetDevice());
        
        // Connect lighting system to render system
        auto* renderSystem = static_cast<WorldEditor::RenderSystem*>(world.getSystem("RenderSystem"));
        if (renderSystem && renderer.GetLightingSystem()) {
            renderSystem->setLightingSystem(renderer.GetLightingSystem());
        }
        
        // Connect wireframe grid to render system
        if (renderSystem && renderer.GetWireframeGrid()) {
            renderSystem->setWireframeGrid(renderer.GetWireframeGrid());
        }
        
        // Connect wireframe grid to render system
        if (renderSystem && renderer.GetWireframeGrid()) {
            renderSystem->setWireframeGrid(renderer.GetWireframeGrid());
        }
        
        // Initialize static renderer reference for safe resource cleanup
        WorldEditor::MeshComponent::s_renderer = &renderer;
        WorldEditor::EditorUI editorUI;
        editorUI.setRenderer(&renderer);
        WorldEditor::EditorCamera camera;
        camera.reset();

        WorldEditor::Properties::registerDefaults();
        // Start with an empty scene; everything is created via the editor UI.

        std::cout << "DirectX renderer and ECS initialized successfully!" << std::endl;
        std::cout << "Rendering scene... Press ESC to exit." << std::endl;

        // Main loop
        MSG msg = {};
        double lastTime = WorldEditor::Timer::now();
        int frameCount = 0;
        bool lastDirty = false;
        std::cout << "Entering main loop." << std::endl;
        while (msg.message != WM_QUIT) {
            frameCount++;

            // Process all available messages
            bool hasMessages = false;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                hasMessages = true;
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    break;
                }
            }

            if (msg.message == WM_QUIT) {
                break;
            }

            // Render frame after processing messages
            renderer.BeginFrame();

            // Determine viewport size from last frame (fallback to window size on first frame).
            ImVec2 vpSize = editorUI.getViewportSize();
            uint32_t vpW = (vpSize.x > 1.0f) ? static_cast<uint32_t>(vpSize.x) : renderer.GetWidth();
            uint32_t vpH = (vpSize.y > 1.0f) ? static_cast<uint32_t>(vpSize.y) : renderer.GetHeight();

            // Offscreen pass: render world into viewport texture.
            float offClear[4] = { 0.10f, 0.10f, 0.10f, 1.0f };
            renderer.BeginOffscreenPass(vpW, vpH, offClear);
            // Provide viewport texture to UI early (so hover/click detection works in the same frame).
            const auto vpSrvEarly = renderer.GetViewportSrvGpuHandle();
            editorUI.setViewportTexture((ImTextureID)(uintptr_t)vpSrvEarly.ptr);

            // ImGui frame
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            editorUI.draw(world);
            editorUI.drawCameraPanel(camera);
            
            // Update wireframe grid state from UI
            auto* renderSystem = static_cast<WorldEditor::RenderSystem*>(world.getSystem("RenderSystem"));
            if (renderSystem) {
                renderSystem->setWireframeEnabled(editorUI.isWireframeEnabled());
                
                // Also update wireframe grid enabled state
                if (auto* wireframeGrid = renderer.GetWireframeGrid()) {
                    wireframeGrid->setEnabled(editorUI.isWireframeEnabled());
                }
            }

            // Handle close requests (Alt+F4 / window X) via editor prompt.
            if (g_closeRequested) {
                editorUI.requestExit();
                g_closeRequested = false;
            }

            // Update window title to show dirty state.
            const bool dirtyNow = editorUI.isDirty();
            if (dirtyNow != lastDirty) {
                lastDirty = dirtyNow;
                if (dirtyNow) SetWindowText(hwnd, _T("DirectX World Editor *"));
                else SetWindowText(hwnd, _T("DirectX World Editor"));
            }

            // Delta time (seconds)
            const double nowTime = WorldEditor::Timer::now();
            float dt = static_cast<float>(nowTime - lastTime);
            lastTime = nowTime;
            if (dt < 0.0f) dt = 0.0f;
            if (dt > 0.1f) dt = 0.1f; // clamp on hitches

            // Update game mode if active
            auto* gameMode = editorUI.getGameMode();
            f32 actualDeltaTime = dt;
            
            if (gameMode && gameMode->isGameModeActive()) {
                // Apply time scale from game mode
                if (!gameMode->isPaused()) {
                    actualDeltaTime = dt * gameMode->getTimeScale();
                } else {
                    actualDeltaTime = 0.0f; // Paused
                }
                gameMode->update(world, dt);
            }
            
            // Update world with actual delta time
            // Only update game systems when game mode is active
            bool gameModeActive = (gameMode && gameMode->isGameModeActive());
            world.update(actualDeltaTime, gameModeActive);

            // Update camera input. Avoid conflicts with UI interaction.
            const ImGuiIO& io = ImGui::GetIO();
            const bool uiActive = ImGui::IsAnyItemActive();
            const bool viewportHovered = editorUI.isViewportHovered();
            const bool viewportFocused = editorUI.isViewportFocused();
            const bool gameViewHovered = editorUI.isGameViewHovered();
            const bool gameViewFocused = editorUI.isGameViewFocused();

            // Dota-like camera while in game mode (edge-pan + zoom + fixed angle).
            // Otherwise keep editor camera controls (WASD + RMB look).
            static bool lastGameModeActive = false;
            static Vec3 dotaFocus(150.0f, 0.0f, 150.0f);
            static float dotaHeight = 50.0f;
            static float dotaDistance = 95.0f; // derived from height+pitch (kept for pan scaling)
            static bool dotaDragging = false;
            static ImVec2 dotaDragLast = ImVec2(0, 0);

            if (gameModeActive) {
                // Initialize on enter.
                if (!lastGameModeActive) {
                    // Try to center on terrain.
                    dotaFocus = Vec3(150.0f, 0.0f, 150.0f);
                    auto& reg = world.getEntityManager().getRegistry();
                    auto viewT = reg.view<WorldEditor::TerrainComponent>();
                    if (auto it = viewT.begin(); it != viewT.end()) {
                        const auto& t = viewT.get<WorldEditor::TerrainComponent>(*it);
                        dotaFocus = Vec3(t.size * 0.5f, 0.0f, t.size * 0.5f);
                    }

                    // Prefer starting on Radiant base (team 1) if present.
                    {
                        auto viewBases = reg.view<WorldEditor::ObjectComponent, WorldEditor::TransformComponent>();
                        for (auto e : viewBases) {
                            const auto& obj = viewBases.get<WorldEditor::ObjectComponent>(e);
                            if (obj.type == WorldEditor::ObjectType::Base && obj.teamId == 1) {
                                const auto& tr = viewBases.get<WorldEditor::TransformComponent>(e);
                                dotaFocus = tr.position;
                                break;
                            }
                        }
                    }

                    dotaHeight = 50.0f;
                    dotaDistance = 95.0f;
                }

                // Fixed Dota-ish angle
                camera.orthographic = false;
                camera.lockTopDown = false;
                camera.yawDeg = -45.0f;
                camera.pitchDeg = -45.0f;
                camera.fovDeg = 60.0f;

                // Active rect for Dota controls: prefer Game View when it exists.
                const ImVec2 rectMin = (gameViewHovered || gameViewFocused) ? editorUI.getGameViewRectMin() : editorUI.getViewportRectMin();
                const ImVec2 rectMax = (gameViewHovered || gameViewFocused) ? editorUI.getGameViewRectMax() : editorUI.getViewportRectMax();
                const bool rectValid = (rectMax.x > rectMin.x + 4.0f && rectMax.y > rectMin.y + 4.0f);

                const bool inputAllowed = rectValid && (gameViewHovered || gameViewFocused) && !io.WantTextInput;

                // Zoom (mouse wheel) -> changes camera height (Dota-like zoom)
                if (inputAllowed && std::abs(io.MouseWheel) > 0.0001f) {
                    const float zoomStep = 4.0f;
                    dotaHeight = std::clamp(dotaHeight - io.MouseWheel * zoomStep, 20.0f, 120.0f);
                }

                // Edge pan + WASD pan + MMB drag
                Vec3 fwd = camera.getForwardLH();
                Vec3 fwdXZ = Vec3(fwd.x, 0.0f, fwd.z);
                const float fLen = glm::length(fwdXZ);
                if (fLen > 0.0001f) fwdXZ /= fLen;
                Vec3 rightXZ = camera.getRightLH();
                rightXZ.y = 0.0f;
                const float rLen = glm::length(rightXZ);
                if (rLen > 0.0001f) rightXZ /= rLen;

                Vec3 pan(0.0f);
                if (inputAllowed) {
                    const float edge = 18.0f;
                    const ImVec2 mp = io.MousePos;
                    if (mp.x <= rectMin.x + edge) pan -= rightXZ;
                    if (mp.x >= rectMax.x - edge) pan += rightXZ;
                    if (mp.y <= rectMin.y + edge) pan += fwdXZ;
                    if (mp.y >= rectMax.y - edge) pan -= fwdXZ;

                    // Keyboard pan (optional, feels nice in editor)
                    if (!io.WantCaptureKeyboard) {
                        if (GetAsyncKeyState('W') & 0x8000) pan += fwdXZ;
                        if (GetAsyncKeyState('S') & 0x8000) pan -= fwdXZ;
                        if (GetAsyncKeyState('D') & 0x8000) pan += rightXZ;
                        if (GetAsyncKeyState('A') & 0x8000) pan -= rightXZ;
                    }

                    // MMB drag
                    const bool mmbDown = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
                    if (mmbDown && !dotaDragging) {
                        dotaDragging = true;
                        dotaDragLast = io.MousePos;
                    } else if (!mmbDown && dotaDragging) {
                        dotaDragging = false;
                    }
                    if (dotaDragging) {
                        const ImVec2 d = ImVec2(io.MousePos.x - dotaDragLast.x, io.MousePos.y - dotaDragLast.y);
                        dotaDragLast = io.MousePos;
                        const float dragScale = 0.12f * (dotaDistance / 90.0f);
                        dotaFocus -= rightXZ * (d.x * dragScale);
                        dotaFocus += fwdXZ * (d.y * dragScale);
                    }
                } else {
                    dotaDragging = false;
                }

                const float panLen = glm::length(pan);
                if (panLen > 0.0001f) {
                    const float speed = 85.0f * (dotaDistance / 90.0f);
                    dotaFocus += (pan / panLen) * speed * dt;
                }

                // Clamp focus to terrain bounds (prevents flying away)
                float terrainSize = 300.0f;
                {
                    auto& reg = world.getEntityManager().getRegistry();
                    auto viewT = reg.view<WorldEditor::TerrainComponent>();
                    if (auto it = viewT.begin(); it != viewT.end()) {
                        terrainSize = viewT.get<WorldEditor::TerrainComponent>(*it).size;
                    }
                }
                dotaFocus.x = std::clamp(dotaFocus.x, 0.0f, terrainSize);
                dotaFocus.z = std::clamp(dotaFocus.z, 0.0f, terrainSize);

                // Recompute camera from focus+distance.
                const Vec3 forward = camera.getForwardLH();
                // Keep camera at a fixed height (Y) above the ground plane, and keep looking at dotaFocus.
                // Solve: position = focus - forward * t, with position.y = dotaHeight.
                const float fy = forward.y;
                if (std::abs(fy) > 0.0001f) {
                    const float t = (dotaFocus.y - dotaHeight) / fy; // fy is negative for downward pitch
                    dotaDistance = std::clamp(t, 5.0f, 10000.0f);
                    camera.position = dotaFocus - forward * dotaDistance;
                } else {
                    // Degenerate, fallback.
                    camera.position = dotaFocus - forward * dotaDistance;
                    camera.position.y = dotaHeight;
                }
            } else {
                // Important: allow RMB mouse-look while hovering the viewport image (even though ImGui "captures" mouse).
                const bool allowMouseLook = viewportHovered && !uiActive;
                const bool allowKeyboardMove = !io.WantCaptureKeyboard;
                camera.updateFromInput(hwnd, dt, allowMouseLook, allowKeyboardMove);
            }
            lastGameModeActive = gameModeActive;

            // Update lighting system
            static float totalTime = 0.0f;
            totalTime += dt;
            renderer.UpdateLighting(camera.position, totalTime);
            if (auto* lighting = renderer.GetLightingSystem()) {
                lighting->setEditorCheckerCellSize(editorUI.isUnrealViewportEnabled() ? editorUI.getCheckerCellSize() : 0.0f);
            }

            // Advanced Terrain Tools: Ctrl+LMB sculpt, T+LMB texture paint
            // Editor tools still use the editor viewport, not the Game View.
            // While holding LMB over the viewport image, ImGui marks that Image as "active".
            // We must NOT treat that as "UI active" for editor tools, otherwise continuous sculpting would never trigger.
            const bool uiActiveNonViewport = uiActive && !viewportHovered;
            const bool ctrl = io.KeyCtrl;
            const bool shift = io.KeyShift;
            const bool tKey = ImGui::IsKeyDown(ImGuiKey_T);
            const bool lmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

            // Unreal-like tool hotkeys:
            // - Allow when the viewport is hovered or focused (common editor UX).
            // - Do NOT require !WantCaptureKeyboard because ImGui may mark the viewport Image as "active".
            // - But avoid stealing keys while typing in text inputs.
            const bool toolHotkeysAllowed = (viewportHovered || viewportFocused) && !io.WantTextInput;
            if (toolHotkeysAllowed) {
                if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
                    // Select mode
                    editorUI.setTerrainEditEnabled(false);
                    editorUI.setTexturePaintEnabled(false);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
                    // Sculpt mode
                    editorUI.setTerrainEditEnabled(true);
                    editorUI.setTexturePaintEnabled(false);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
                    // Paint mode
                    editorUI.setTerrainEditEnabled(false);
                    editorUI.setTexturePaintEnabled(true);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_4, false)) {
                    // Object placement mode
                    editorUI.setTerrainEditEnabled(false);
                    editorUI.setTexturePaintEnabled(false);
                    // Object placement enabled via UI checkbox
                }
            }

            // Throttle terrain updates to prevent GPU overload
            static double lastSculptApplyTime = 0.0;
            static double lastTerrainMeshUpdateTime = 0.0;
            static bool terrainNeedsRebuild = false;
            static Entity lastModifiedTerrain = INVALID_ENTITY;
            const double sculptApplyInterval = 1.0 / 60.0;     // 60 Hz brush application (feels continuous while holding LMB)
            const double terrainUpdateInterval = 1.0 / 20.0;   // 20 Hz mesh/chunk updates (keeps GPU stable)
            
            // Terrain height sculpting
            const bool requireCtrlToSculpt = editorUI.isTerrainSculptRequireCtrl();
            const bool sculptChordHeld = lmbDown && !rmbDown && !uiActiveNonViewport && viewportHovered;
            const bool sculptAllowed = !requireCtrlToSculpt || ctrl;

            // Brush cursor overlay (UE-like): show brush ring on the terrain under the mouse.
            const bool toolSculptMode = editorUI.isTerrainEditEnabled();
            const bool toolPaintMode = editorUI.isTexturePaintEnabled();
            if ((toolSculptMode || toolPaintMode) && viewportHovered && !uiActiveNonViewport) {
                const ImVec2 vMin = editorUI.getViewportRectMin();
                const ImVec2 vMax = editorUI.getViewportRectMax();
                const float mx = io.MousePos.x;
                const float my = io.MousePos.y;
                if (mx >= vMin.x && my >= vMin.y && mx < vMax.x && my < vMax.y) {
                    const Vec2 localPos(mx - vMin.x, my - vMin.y);
                    const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                    if (localSize.x > 4.0f && localSize.y > 4.0f) {
                        const float aspectPick = localSize.x / localSize.y;
                        const Mat4 viewProjForPick = camera.getViewProjLH_ZO(aspectPick);
                        const Mat4 invViewProj = glm::inverse(viewProjForPick);
                        const WorldEditor::Math::Ray ray = WorldEditor::Math::screenToWorldRay(localPos, invViewProj, localSize);

                        // Pick terrain: prefer selected terrain, else first available.
                        Entity terrainE = INVALID_ENTITY;
                        const Entity selected = editorUI.getSelected();
                        if (selected != INVALID_ENTITY && world.isValid(selected) && world.hasComponent<WorldEditor::TerrainComponent>(selected)) {
                            terrainE = selected;
                        } else {
                            auto& reg = world.getEntityManager().getRegistry();
                            auto viewT = reg.view<WorldEditor::TerrainComponent, WorldEditor::MeshComponent>();
                            auto it = viewT.begin();
                            if (it != viewT.end()) terrainE = *it;
                        }

                        if (terrainE != INVALID_ENTITY) {
                            auto& terrain = world.getComponent<WorldEditor::TerrainComponent>(terrainE);
                            WorldEditor::TerrainMesh::ensureHeightmap(terrain);

                            const WorldEditor::TransformComponent tr =
                                world.hasComponent<WorldEditor::TransformComponent>(terrainE)
                                    ? world.getComponent<WorldEditor::TransformComponent>(terrainE)
                                    : WorldEditor::TransformComponent{};

                            Vec3 hit{};
                            if (WorldEditor::TerrainRaycast::raycastHeightfield(terrain, tr, ray, hit, nullptr, nullptr)) {
                                // Cursor radius matches the active tool.
                                const float radiusWU = toolSculptMode
                                    ? std::clamp(editorUI.getTerrainBrushRadius(), 1.0f, 8.0f)
                                    : std::clamp(editorUI.getTextureBrushRadius(), 0.5f, 20.0f);
                                // Projected-on-terrain brush outline (UE-like): sample points on the heightfield circle,
                                // then project each to screen and draw a polyline (perspective-correct).
                                const Vec3 hitLocal = hit - tr.position;

                                auto sampleHeightBilinear = [&terrain](float x, float z) -> float {
                                    const int w = (std::max)(2, terrain.resolution.x);
                                    const int h = (std::max)(2, terrain.resolution.y);
                                    const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
                                    if (terrain.heightmap.size() != wanted || terrain.size <= 0.0f) {
                                        return 0.0f;
                                    }

                                    const float cell = terrain.size / float(w - 1);
                                    const float gx = (std::clamp)(x / cell, 0.0f, float(w - 1));
                                    const float gz = (std::clamp)(z / cell, 0.0f, float(h - 1));

                                    const int x0 = (std::max)(0, (std::min)(w - 1, int(std::floor(gx))));
                                    const int z0 = (std::max)(0, (std::min)(h - 1, int(std::floor(gz))));
                                    const int x1 = (std::min)(w - 1, x0 + 1);
                                    const int z1 = (std::min)(h - 1, z0 + 1);

                                    const float tx = gx - float(x0);
                                    const float tz = gz - float(z0);

                                    auto idx = [w](int ix, int iz) -> size_t {
                                        return static_cast<size_t>(iz) * static_cast<size_t>(w) + static_cast<size_t>(ix);
                                    };

                                    const float h00 = terrain.heightmap[idx(x0, z0)];
                                    const float h10 = terrain.heightmap[idx(x1, z0)];
                                    const float h01 = terrain.heightmap[idx(x0, z1)];
                                    const float h11 = terrain.heightmap[idx(x1, z1)];

                                    const float hx0 = h00 + (h10 - h00) * tx;
                                    const float hx1 = h01 + (h11 - h01) * tx;
                                    return hx0 + (hx1 - hx0) * tz;
                                };

                                ImDrawList* dl = ImGui::GetForegroundDrawList();
                                // Clip overlay drawings to the viewport rect.
                                dl->PushClipRect(vMin, vMax, true);

                                // Color by tool/brush type (UE-like).
                                ImU32 colOuter = IM_COL32(255, 255, 255, 170);
                                ImU32 colInner = IM_COL32(255, 255, 255, 90);
                                if (toolPaintMode) {
                                    colOuter = IM_COL32(70, 150, 255, 200);
                                    colInner = IM_COL32(70, 150, 255, 110);
                                } else {
                                    using BT = WorldEditor::TerrainTools::BrushType;
                                    BT bt = editorUI.getCurrentBrushType();
                                    // Shift acts as invert (force Lower).
                                    if (shift) bt = BT::Lower;
                                    switch (bt) {
                                        case BT::Raise:   colOuter = IM_COL32(80, 220, 120, 200); colInner = IM_COL32(80, 220, 120, 110); break;
                                        case BT::Lower:   colOuter = IM_COL32(240, 80, 80, 200);  colInner = IM_COL32(240, 80, 80, 110);  break;
                                        case BT::Smooth:  colOuter = IM_COL32(120, 180, 255, 200); colInner = IM_COL32(120, 180, 255, 110); break;
                                        case BT::Flatten: colOuter = IM_COL32(255, 220, 90, 200);  colInner = IM_COL32(255, 220, 90, 110);  break;
                                        case BT::Noise:   colOuter = IM_COL32(190, 120, 255, 200); colInner = IM_COL32(190, 120, 255, 110); break;
                                        case BT::Erode:   colOuter = IM_COL32(255, 160, 80, 200);  colInner = IM_COL32(255, 160, 80, 110);  break;
                                        default: break;
                                    }
                                }

                                auto drawProjectedRing = [&](float ringRadiusWU, ImU32 col, float thickness) {
                                    // Keep it lightweight: 48 segments is enough and fast.
                                    constexpr int kSeg = 48;
                                    ImVec2 pts[kSeg + 1];
                                    int count = 0;

                                    for (int i = 0; i <= kSeg; ++i) {
                                        const float a = (float(i) / float(kSeg)) * 6.28318530718f;
                                        const float lx = hitLocal.x + std::cos(a) * ringRadiusWU;
                                        const float lz = hitLocal.z + std::sin(a) * ringRadiusWU;

                                        // Clamp ring points to terrain bounds to avoid invalid sampling.
                                        const float xClamped = (std::clamp)(lx, 0.0f, terrain.size);
                                        const float zClamped = (std::clamp)(lz, 0.0f, terrain.size);
                                        const float y = sampleHeightBilinear(xClamped, zClamped);

                                        const Vec3 pw = Vec3(xClamped, y, zClamped) + tr.position;
                                        const Vec2 pLocal = WorldEditor::Math::worldToScreen(pw, viewProjForPick, localSize);
                                        pts[count++] = ImVec2(vMin.x + pLocal.x, vMin.y + pLocal.y);
                                    }

                                    if (count >= 2) {
                                        dl->AddPolyline(pts, count, col, ImDrawFlags_Closed, thickness);
                                    }
                                };

                                drawProjectedRing(radiusWU, colOuter, 2.0f);
                                drawProjectedRing(radiusWU * 0.5f, colInner, 1.0f);

                                // Center marker (projected).
                                const Vec2 cLocal = WorldEditor::Math::worldToScreen(hit, viewProjForPick, localSize);
                                const ImVec2 center(vMin.x + cLocal.x, vMin.y + cLocal.y);
                                dl->AddCircleFilled(center, 2.0f, IM_COL32(0, 0, 0, 180), 12);

                                // Text label near cursor (in screen space).
                                char label[256] = {};
                                if (toolPaintMode) {
                                    std::snprintf(label, sizeof(label), "Paint | R=%.2f S=%.2f",
                                                  std::clamp(editorUI.getTextureBrushRadius(), 0.5f, 20.0f),
                                                  std::clamp(editorUI.getTextureBrushStrength(), 0.1f, 10.0f));
                                } else {
                                    const char* btName = "Brush";
                                    switch (editorUI.getCurrentBrushType()) {
                                        case WorldEditor::TerrainTools::BrushType::Raise: btName = "Raise"; break;
                                        case WorldEditor::TerrainTools::BrushType::Lower: btName = "Lower"; break;
                                        case WorldEditor::TerrainTools::BrushType::Flatten: btName = "Flatten"; break;
                                        case WorldEditor::TerrainTools::BrushType::Smooth: btName = "Smooth"; break;
                                        case WorldEditor::TerrainTools::BrushType::Noise: btName = "Noise"; break;
                                        case WorldEditor::TerrainTools::BrushType::Erode: btName = "Erode"; break;
                                        default: break;
                                    }
                                    std::snprintf(label, sizeof(label), "Sculpt (%s%s) | R=%.2f S=%.2f",
                                                  btName, shift ? " inverted" : "",
                                                  std::clamp(editorUI.getTerrainBrushRadius(), 1.0f, 8.0f),
                                                  std::clamp(editorUI.getTerrainBrushStrength(), 0.5f, 8.0f));
                                }

                                const ImVec2 textPos(center.x + 12.0f, center.y + 12.0f);
                                const ImVec2 ts = ImGui::CalcTextSize(label);
                                const ImU32 bg = IM_COL32(0, 0, 0, 140);
                                dl->AddRectFilled(textPos, ImVec2(textPos.x + ts.x + 8.0f, textPos.y + ts.y + 6.0f), bg, 4.0f);
                                dl->AddText(ImVec2(textPos.x + 4.0f, textPos.y + 3.0f), IM_COL32(255, 255, 255, 220), label);
                                dl->PopClipRect();
                            }
                        }
                    }
                }
            }

            if (editorUI.isTerrainEditEnabled() && sculptChordHeld && sculptAllowed) {
                const ImVec2 vMin = editorUI.getViewportRectMin();
                const ImVec2 vMax = editorUI.getViewportRectMax();
                const float mx = io.MousePos.x;
                const float my = io.MousePos.y;
                
                if (mx >= vMin.x && my >= vMin.y && mx < vMax.x && my < vMax.y) {
                    const Vec2 localPos(mx - vMin.x, my - vMin.y);
                    const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                    const float aspectPick = localSize.x / localSize.y;
                    const Mat4 viewProjForPick = camera.getViewProjLH_ZO(aspectPick);
                    const Mat4 invViewProj = glm::inverse(viewProjForPick);
                    const WorldEditor::Math::Ray ray = WorldEditor::Math::screenToWorldRay(localPos, invViewProj, localSize);

                    // Find terrain entity
                    Entity terrainE = INVALID_ENTITY;
                    const Entity selected = editorUI.getSelected();
                    if (selected != INVALID_ENTITY && world.isValid(selected) && world.hasComponent<WorldEditor::TerrainComponent>(selected)) {
                        terrainE = selected;
                    } else {
                        auto& reg = world.getEntityManager().getRegistry();
                        auto viewT = reg.view<WorldEditor::TerrainComponent, WorldEditor::MeshComponent>();
                        auto it = viewT.begin();
                        if (it != viewT.end()) terrainE = *it;
                    }

                    if (terrainE != INVALID_ENTITY) {
                        auto& terrain = world.getComponent<WorldEditor::TerrainComponent>(terrainE);
                        auto& mesh = world.getComponent<WorldEditor::MeshComponent>(terrainE);
                        WorldEditor::TerrainMesh::ensureHeightmap(terrain);

                        const WorldEditor::TransformComponent tr =
                            world.hasComponent<WorldEditor::TransformComponent>(terrainE)
                                ? world.getComponent<WorldEditor::TransformComponent>(terrainE)
                                : WorldEditor::TransformComponent{};

                        Vec3 hit{};
                        if (WorldEditor::TerrainRaycast::raycastHeightfield(terrain, tr, ray, hit, nullptr, nullptr)) {
                            // Terrain tools currently operate in terrain-local space (origin at transform.position, translation-only MVP).
                            const Vec3 hitLocal = hit - tr.position;
                            // Setup brush settings with ULTRA STRICT safety limits
                            WorldEditor::TerrainTools::BrushSettings brushSettings;
                            brushSettings.type = shift ? WorldEditor::TerrainTools::BrushType::Lower : editorUI.getCurrentBrushType();
                            brushSettings.falloff = editorUI.getCurrentFalloffType();
                            brushSettings.radius = std::clamp(editorUI.getTerrainBrushRadius(), 1.0f, 8.0f); // Еще меньше max radius
                            brushSettings.strength = std::clamp(editorUI.getTerrainBrushStrength(), 0.5f, 8.0f); // Еще меньше max strength
                            brushSettings.targetHeight = std::clamp(editorUI.getTerrainTargetHeight(), -15.0f, 15.0f); // Меньший диапазон
                            brushSettings.noiseScale = std::clamp(editorUI.getTerrainNoiseScale(), 0.1f, 1.0f);
                            brushSettings.smoothFactor = std::clamp(editorUI.getTerrainSmoothFactor(), 0.1f, 1.0f);

                            // Apply brush continuously while holding LMB (high-frequency), rebuild mesh less often.
                            if ((nowTime - lastSculptApplyTime) >= sculptApplyInterval) {
                                lastSculptApplyTime = nowTime;
                                auto result = WorldEditor::TerrainTools::TerrainBrush::applyBrush(terrain, hitLocal, brushSettings, dt);
                                
                                if (result.modified) {
                                    // Mark ALL chunks overlapping the affected vertex region (plus 1-cell padding for normals).
                                    // This prevents visible seams/cracks between chunks while sculpting.
                                    const int w = (std::max)(2, terrain.resolution.x);
                                    const int h = (std::max)(2, terrain.resolution.y);
                                    Vec2i vMin = result.minAffected - Vec2i(1, 1);
                                    Vec2i vMax = result.maxAffected + Vec2i(1, 1);
                                    vMin.x = (std::max)(0, (std::min)(w - 1, vMin.x));
                                    vMin.y = (std::max)(0, (std::min)(h - 1, vMin.y));
                                    vMax.x = (std::max)(0, (std::min)(w - 1, vMax.x));
                                    vMax.y = (std::max)(0, (std::min)(h - 1, vMax.y));

                                    const Vec2i chunkMin(vMin.x / WorldEditor::TerrainChunks::CHUNK_SIZE,
                                                         vMin.y / WorldEditor::TerrainChunks::CHUNK_SIZE);
                                    const Vec2i chunkMax(vMax.x / WorldEditor::TerrainChunks::CHUNK_SIZE,
                                                         vMax.y / WorldEditor::TerrainChunks::CHUNK_SIZE);
                                    
                                    // Mark chunks dirty (will be rebuilt on next render)
                                    auto& chunks = WorldEditor::TerrainChunks::getChunks(mesh);
                                    for (auto& chunk : chunks) {
                                        if (chunk.chunkCoord.x >= chunkMin.x && chunk.chunkCoord.x <= chunkMax.x &&
                                            chunk.chunkCoord.y >= chunkMin.y && chunk.chunkCoord.y <= chunkMax.y) {
                                            chunk.isDirty = true;
                                        }
                                    }
                                    
                                    terrainNeedsRebuild = true;
                                    lastModifiedTerrain = terrainE;
                                    editorUI.markDirty();
                                }
                            }
                        }
                    }
                }
            }

            // Throttled terrain mesh rebuild - MUCH more conservative
            if (terrainNeedsRebuild && (nowTime - lastTerrainMeshUpdateTime) >= terrainUpdateInterval) {
                if (lastModifiedTerrain != INVALID_ENTITY && world.isValid(lastModifiedTerrain)) {
                    auto& terrain = world.getComponent<WorldEditor::TerrainComponent>(lastModifiedTerrain);
                    auto& mesh = world.getComponent<WorldEditor::MeshComponent>(lastModifiedTerrain);
                    
                    // Try chunk-based update first
                    auto& chunks = WorldEditor::TerrainChunks::getChunks(mesh);
                    if (!chunks.empty()) {
                        // Update only dirty chunks (much more efficient)
                        WorldEditor::TerrainChunks::updateDirtyChunks(terrain, mesh, renderer.GetDevice());
                    } else {
                        // Initialize chunk system for this terrain
                        if (WorldEditor::TerrainChunks::initializeChunks(terrain, mesh)) {
                            // Successfully initialized chunks, update them
                            WorldEditor::TerrainChunks::updateDirtyChunks(terrain, mesh, renderer.GetDevice());
                        } else {
                            // Fall back to full mesh rebuild for very large terrains
                            WorldEditor::TerrainMesh::invalidateGpu(mesh);
                            WorldEditor::TerrainMesh::buildMesh(terrain, mesh);
                        }
                        
                        // Update wireframe grid after terrain mesh update
                        if (auto* wireframeGrid = renderer.GetWireframeGrid()) {
                            wireframeGrid->generateGrid(terrain, mesh);
                        }
                    }
                    
                    terrainNeedsRebuild = false;
                    lastTerrainMeshUpdateTime = nowTime;
                    lastModifiedTerrain = INVALID_ENTITY;
                }
            }

            // Texture painting: tool mode (LMB) OR hold T as a temporary chord.
            const bool paintChordHeld = viewportHovered && lmbDown && !rmbDown && !uiActiveNonViewport;
            const bool paintToolActive = editorUI.isTexturePaintEnabled();
            if (paintChordHeld && (paintToolActive || tKey)) {
                const ImVec2 vMin = editorUI.getViewportRectMin();
                const ImVec2 vMax = editorUI.getViewportRectMax();
                const float mx = io.MousePos.x;
                const float my = io.MousePos.y;
                
                if (mx >= vMin.x && my >= vMin.y && mx < vMax.x && my < vMax.y) {
                    const Vec2 localPos(mx - vMin.x, my - vMin.y);
                    const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                    const float aspectPick = localSize.x / localSize.y;
                    const Mat4 viewProjForPick = camera.getViewProjLH_ZO(aspectPick);
                    const Mat4 invViewProj = glm::inverse(viewProjForPick);
                    const WorldEditor::Math::Ray ray = WorldEditor::Math::screenToWorldRay(localPos, invViewProj, localSize);

                    // Find terrain entity with material component
                    Entity terrainE = INVALID_ENTITY;
                    const Entity selected = editorUI.getSelected();
                    if (selected != INVALID_ENTITY && world.isValid(selected) && 
                        world.hasComponent<WorldEditor::TerrainComponent>(selected) &&
                        world.hasComponent<WorldEditor::TerrainMaterialComponent>(selected)) {
                        terrainE = selected;
                    } else {
                        auto& reg = world.getEntityManager().getRegistry();
                        auto viewT = reg.view<WorldEditor::TerrainComponent, WorldEditor::TerrainMaterialComponent>();
                        auto it = viewT.begin();
                        if (it != viewT.end()) terrainE = *it;
                    }

                    if (terrainE != INVALID_ENTITY) {
                        auto& terrain = world.getComponent<WorldEditor::TerrainComponent>(terrainE);
                        auto& terrainMat = world.getComponent<WorldEditor::TerrainMaterialComponent>(terrainE);

                        const WorldEditor::TransformComponent tr =
                            world.hasComponent<WorldEditor::TransformComponent>(terrainE)
                                ? world.getComponent<WorldEditor::TransformComponent>(terrainE)
                                : WorldEditor::TransformComponent{};

                        Vec3 hit{};
                        if (WorldEditor::TerrainRaycast::raycastHeightfield(terrain, tr, ray, hit, nullptr, nullptr)) {
                            // Terrain tools currently operate in terrain-local space (origin at transform.position, translation-only MVP).
                            const Vec3 hitLocal = hit - tr.position;
                            // Convert TerrainMaterialComponent to TerrainTools::TerrainMaterial
                            WorldEditor::TerrainTools::TerrainMaterial material;
                            material.layers.resize(terrainMat.layers.size());
                            for (size_t i = 0; i < terrainMat.layers.size(); ++i) {
                                material.layers[i].diffuseTexture = terrainMat.layers[i].diffuseTexture;
                                material.layers[i].normalTexture = terrainMat.layers[i].normalTexture;
                                material.layers[i].tiling = terrainMat.layers[i].tiling;
                                material.layers[i].strength = terrainMat.layers[i].strength;
                            }
                            material.blendWeights = terrainMat.blendWeights;
                            material.activeLayer = terrainMat.activeLayer;

                            // Apply texture painting
                            bool modified = WorldEditor::TerrainTools::TexturePainter::paintTexture(
                                material, terrain, hitLocal, editorUI.getActiveTextureLayer(),
                                editorUI.getTextureBrushRadius(), editorUI.getTextureBrushStrength(), dt
                            );

                            if (modified) {
                                // Copy back to component
                                terrainMat.blendWeights = material.blendWeights;
                                editorUI.markDirty();
                            }
                        }
                    }
                }
            }

            // Object placement (LMB click on terrain when placement mode is enabled)
            const bool objectPlacementMode = editorUI.isObjectPlacementEnabled();
            if (objectPlacementMode && viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !uiActiveNonViewport) {
                const ImVec2 vMin = editorUI.getViewportRectMin();
                const ImVec2 vMax = editorUI.getViewportRectMax();
                const float mx = io.MousePos.x;
                const float my = io.MousePos.y;
                if (mx >= vMin.x && my >= vMin.y && mx < vMax.x && my < vMax.y) {
                    const Vec2 localPos(mx - vMin.x, my - vMin.y);
                    const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                    const float aspect = localSize.x / localSize.y;
                    const Mat4 viewProjForPick = camera.getViewProjLH_ZO(aspect);
                    const Mat4 invViewProj = glm::inverse(viewProjForPick);
                    const WorldEditor::Math::Ray ray = WorldEditor::Math::screenToWorldRay(localPos, invViewProj, localSize);
                    
                    // Find terrain entity
                    Entity terrainE = INVALID_ENTITY;
                    auto& reg = world.getEntityManager().getRegistry();
                    auto viewT = reg.view<WorldEditor::TerrainComponent, WorldEditor::TransformComponent>();
                    for (auto e : viewT) {
                        terrainE = e;
                        break;
                    }
                    
                    if (terrainE != INVALID_ENTITY) {
                        auto& terrain = reg.get<WorldEditor::TerrainComponent>(terrainE);
                        auto& tr = reg.get<WorldEditor::TransformComponent>(terrainE);
                        
                        Vec3 hit{};
                        if (WorldEditor::TerrainRaycast::raycastHeightfield(terrain, tr, ray, hit, nullptr, nullptr)) {
                            // Create object entity at hit position
                            const String objTypeNames[] = {
                                "None", "Tower", "CreepSpawn", "NeutralCamp", "Tree", "Rock", "Building", "Waypoint", "Base", "Custom"
                            };
                            const int typeIdx = static_cast<int>(editorUI.getSelectedObjectType());
                            static u64 s_objectSerial = 0;
                            const u64 serial = ++s_objectSerial;
                            const String objName = (typeIdx >= 0 && typeIdx < 10)
                                ? objTypeNames[typeIdx] + "_" + std::to_string(serial)
                                : "Object_" + std::to_string(serial);
                            
                            Entity objE = world.createEntity(objName);
                            auto& objTransform = world.addComponent<WorldEditor::TransformComponent>(objE);
                            objTransform.position = hit;
                            
                            auto& objComp = world.addComponent<WorldEditor::ObjectComponent>(objE);
                            objComp.type = editorUI.getSelectedObjectType();
                            objComp.teamId = editorUI.getObjectTeamId();
                            objComp.spawnRadius = editorUI.getObjectSpawnRadius();
                            objComp.maxUnits = editorUI.getObjectMaxUnits();
                            objComp.spawnLane = editorUI.getObjectSpawnLane();
                            objComp.waypointOrder = editorUI.getObjectWaypointOrder();
                            objComp.waypointLane = editorUI.getObjectWaypointLane();
                            
                            // Add health component for towers, buildings, and bases
                            if (objComp.type == WorldEditor::ObjectType::Tower || 
                                objComp.type == WorldEditor::ObjectType::Building ||
                                objComp.type == WorldEditor::ObjectType::Base) {
                                auto& health = world.addComponent<WorldEditor::HealthComponent>(objE);
                                if (objComp.type == WorldEditor::ObjectType::Tower) {
                                    health.maxHealth = 1600.0f; // Tower health
                                    health.currentHealth = 1600.0f;
                                    health.armor = 10.0f;
                                } else if (objComp.type == WorldEditor::ObjectType::Base) {
                                    health.maxHealth = 5000.0f; // Base health (very high)
                                    health.currentHealth = 5000.0f;
                                    health.armor = 20.0f;
                                } else {
                                    health.maxHealth = 2500.0f; // Building health
                                    health.currentHealth = 2500.0f;
                                    health.armor = 15.0f;
                                }
                                
                            }
                            
                            // Create visual representation based on object type
                            auto& mesh = world.addComponent<WorldEditor::MeshComponent>(objE);
                            mesh.name = objName;
                            mesh.visible = true;
                            
                            // Generate mesh shape based on object type
                            Vec3 objectColor(0.5f, 0.5f, 0.5f);
                            float objectScale = 1.0f;
                            Vec3 collisionSize(3.0f, 5.0f, 3.0f); // Default collision size
                            
                            switch (objComp.type) {
                                case WorldEditor::ObjectType::Tower: {
                                    // Tower: tall cylinder
                                    GenerateCylinder(mesh, 1.5f, 8.0f, 16);
                                    objectColor = Vec3(0.8f, 0.2f, 0.2f); // Red
                                    objTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
                                    collisionSize = Vec3(3.0f, 8.0f, 3.0f); // Match cylinder size
                                    break;
                                }
                                case WorldEditor::ObjectType::CreepSpawn: {
                                    // Creep spawn: sphere
                                    GenerateSphere(mesh, 2.0f, 16);
                                    objectColor = Vec3(0.2f, 0.8f, 0.2f); // Green
                                    objTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
                                    break;
                                }
                                case WorldEditor::ObjectType::NeutralCamp: {
                                    // Neutral camp: cone/pyramid
                                    GenerateCone(mesh, 2.5f, 4.0f, 8);
                                    objectColor = Vec3(0.8f, 0.8f, 0.2f); // Yellow
                                    objTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
                                    break;
                                }
                                case WorldEditor::ObjectType::Tree: {
                                    // Tree: cylinder (trunk) + sphere (foliage) - simplified to cylinder for now
                                    GenerateCylinder(mesh, 0.8f, 4.0f, 12);
                                    objectColor = Vec3(0.2f, 0.6f, 0.2f); // Dark green
                                    objTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
                                    break;
                                }
                                case WorldEditor::ObjectType::Rock: {
                                    // Rock: irregular shape
                                    GenerateIrregularRock(mesh, 2.0f);
                                    objectColor = Vec3(0.4f, 0.4f, 0.4f); // Gray
                                    objTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
                                    break;
                                }
                                case WorldEditor::ObjectType::Building: {
                                    // Building: larger cube
                                    mesh.vertices = {
                                        Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, -1.0f, -1.0f), Vec3(1.0f, 2.0f, -1.0f), Vec3(-1.0f, 2.0f, -1.0f),
                                        Vec3(-1.0f, -1.0f, 1.0f), Vec3(1.0f, -1.0f, 1.0f), Vec3(1.0f, 2.0f, 1.0f), Vec3(-1.0f, 2.0f, 1.0f)
                                    };
                                    mesh.normals.assign(8, Vec3(0.0f)); // Will be recalculated
                                    mesh.texCoords.assign(8, Vec2(0.0f, 0.0f));
                                    mesh.indices = {
                                        0, 1, 2, 2, 3, 0, // front (counter-clockwise)
                                        4, 7, 6, 6, 5, 4, // back (counter-clockwise)
                                        0, 4, 5, 5, 1, 0, // bottom (counter-clockwise)
                                        3, 2, 6, 6, 7, 3, // top (counter-clockwise)
                                        0, 3, 7, 7, 4, 0, // left (counter-clockwise)
                                        1, 5, 6, 6, 2, 1  // right (counter-clockwise)
                                    };
                                    // Recalculate normals
                                    mesh.normals.assign(mesh.vertices.size(), Vec3(0.0f));
                                    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                                        Vec3 v0 = mesh.vertices[mesh.indices[i]];
                                        Vec3 v1 = mesh.vertices[mesh.indices[i + 1]];
                                        Vec3 v2 = mesh.vertices[mesh.indices[i + 2]];
                                        Vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                                        mesh.normals[mesh.indices[i]] += normal;
                                        mesh.normals[mesh.indices[i + 1]] += normal;
                                        mesh.normals[mesh.indices[i + 2]] += normal;
                                    }
                                    for (auto& n : mesh.normals) {
                                        if (glm::length(n) > 0.001f) {
                                            n = glm::normalize(n);
                                        } else {
                                            n = Vec3(0.0f, 1.0f, 0.0f);
                                        }
                                    }
                                    objectColor = Vec3(0.6f, 0.5f, 0.4f); // Brown
                                    objTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
                                    collisionSize = Vec3(2.0f, 3.0f, 2.0f); // Match building size
                                    break;
                                }
                                case WorldEditor::ObjectType::Waypoint: {
                                    // Waypoint: small sphere
                                    GenerateSphere(mesh, 1.5f, 16);
                                    objectColor = Vec3(0.0f, 0.8f, 1.0f); // Cyan
                                    objTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
                                    break;
                                }
                                case WorldEditor::ObjectType::Base: {
                                    // Base: large cube
                                    mesh.vertices = {
                                        Vec3(-2.0f, -2.0f, -2.0f), Vec3(2.0f, -2.0f, -2.0f), Vec3(2.0f, 2.5f, -2.0f), Vec3(-2.0f, 2.5f, -2.0f),
                                        Vec3(-2.0f, -2.0f, 2.0f), Vec3(2.0f, -2.0f, 2.0f), Vec3(2.0f, 2.5f, 2.0f), Vec3(-2.0f, 2.5f, 2.0f)
                                    };
                                    mesh.normals.assign(8, Vec3(0.0f));
                                    mesh.texCoords.assign(8, Vec2(0.0f, 0.0f));
                                    mesh.indices = {
                                        0, 1, 2, 2, 3, 0, 4, 7, 6, 6, 5, 4,
                                        0, 4, 5, 5, 1, 0, 2, 6, 7, 7, 3, 2,
                                        0, 3, 7, 7, 4, 0, 1, 5, 6, 6, 2, 1
                                    };
                                    // Green for Radiant (Team 1), red for Dire (Team 2)
                                    if (objComp.teamId == 1) {
                                        objectColor = Vec3(0.0f, 1.0f, 0.0f); // Bright green
                                    } else if (objComp.teamId == 2) {
                                        objectColor = Vec3(1.0f, 0.0f, 0.0f); // Bright red
                                    } else {
                                        objectColor = Vec3(0.5f, 0.5f, 0.5f); // Gray
                                    }
                                    objTransform.scale = Vec3(1.0f, 1.0f, 1.0f);
                                    break;
                                }
                                default: {
                                    // Default: unit cube
                                    mesh.vertices = {
                                        Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, -0.5f), Vec3(-0.5f, 0.5f, -0.5f),
                                        Vec3(-0.5f, -0.5f, 0.5f), Vec3(0.5f, -0.5f, 0.5f), Vec3(0.5f, 0.5f, 0.5f), Vec3(-0.5f, 0.5f, 0.5f)
                                    };
                                    mesh.normals.assign(8, Vec3(0.0f)); // Will be recalculated
                                    mesh.texCoords.assign(8, Vec2(0.0f, 0.0f));
                                    mesh.indices = {
                                        0, 1, 2, 2, 3, 0, // front (counter-clockwise)
                                        4, 7, 6, 6, 5, 4, // back (counter-clockwise)
                                        0, 4, 5, 5, 1, 0, // bottom (counter-clockwise)
                                        3, 2, 6, 6, 7, 3, // top (counter-clockwise)
                                        0, 3, 7, 7, 4, 0, // left (counter-clockwise)
                                        1, 5, 6, 6, 2, 1  // right (counter-clockwise)
                                    };
                                    // Recalculate normals
                                    mesh.normals.assign(mesh.vertices.size(), Vec3(0.0f));
                                    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                                        Vec3 v0 = mesh.vertices[mesh.indices[i]];
                                        Vec3 v1 = mesh.vertices[mesh.indices[i + 1]];
                                        Vec3 v2 = mesh.vertices[mesh.indices[i + 2]];
                                        Vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                                        mesh.normals[mesh.indices[i]] += normal;
                                        mesh.normals[mesh.indices[i + 1]] += normal;
                                        mesh.normals[mesh.indices[i + 2]] += normal;
                                    }
                                    for (auto& n : mesh.normals) {
                                        if (glm::length(n) > 0.001f) {
                                            n = glm::normalize(n);
                                        } else {
                                            n = Vec3(0.0f, 1.0f, 0.0f);
                                        }
                                    }
                                    objectColor = Vec3(0.5f, 0.5f, 0.5f); // Gray
                                    break;
                                }
                            }
                            
                            // Create material for object
                            Entity matE = world.createEntity(objName + "_Material");
                            auto& mat = world.addComponent<WorldEditor::MaterialComponent>(matE);
                            mat.name = objName + "_Material";
                            mat.baseColor = objectColor;
                            mat.gpuBufferCreated = false;
                            mesh.materialEntity = matE;
                            
                            // Add collision component for towers and buildings (after mesh is created)
                            if (objComp.type == WorldEditor::ObjectType::Tower || 
                                objComp.type == WorldEditor::ObjectType::Building ||
                                objComp.type == WorldEditor::ObjectType::Base) {
                                auto& collision = world.addComponent<WorldEditor::CollisionComponent>(objE);
                                collision.shape = WorldEditor::CollisionShape::Box;
                                collision.boxSize = collisionSize;
                                collision.isStatic = true;
                                collision.isTrigger = false;
                                collision.blocksMovement = true;
                            }
                            
                            editorUI.setSelected(objE);
                            editorUI.markDirty();
                        }
                    }
                }
            }

            // Mouse picking (LMB) only inside Viewport content rect (disabled when Ctrl is held for terrain sculpt).
            const bool sculptMode = editorUI.isTerrainEditEnabled();
            const bool pickingAllowed = !sculptMode && !objectPlacementMode || (requireCtrlToSculpt && !io.KeyCtrl);
            if (pickingAllowed && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 vMin = editorUI.getViewportRectMin();
                const ImVec2 vMax = editorUI.getViewportRectMax();
                const float mx = io.MousePos.x;
                const float my = io.MousePos.y;
                if (mx >= vMin.x && my >= vMin.y && mx < vMax.x && my < vMax.y) {
                    const Vec2 localPos(mx - vMin.x, my - vMin.y);
                    const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                    const float aspect = localSize.x / localSize.y;
                    const Mat4 viewProjForPick = camera.getViewProjLH_ZO(aspect);
                    const Mat4 invViewProj = glm::inverse(viewProjForPick);
                    const WorldEditor::Math::Ray ray = WorldEditor::Math::screenToWorldRay(localPos, invViewProj, localSize);
                    Entity hit = WorldEditor::pickEntityAABB(world, ray);
                            editorUI.setSelected(hit);
                }
            }

            // Visualize spawn radius for selected objects with ObjectComponent
            const Entity selected = editorUI.getSelected();
            if (selected != INVALID_ENTITY && world.isValid(selected) && 
                world.hasComponent<WorldEditor::ObjectComponent>(selected)) {
                auto& objComp = world.getComponent<WorldEditor::ObjectComponent>(selected);
                if ((objComp.type == WorldEditor::ObjectType::CreepSpawn || 
                     objComp.type == WorldEditor::ObjectType::NeutralCamp) && 
                    objComp.spawnRadius > 0.0f) {
                    const ImVec2 vMin = editorUI.getViewportRectMin();
                    const ImVec2 vMax = editorUI.getViewportRectMax();
                    const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                    if (localSize.x > 4.0f && localSize.y > 4.0f) {
                        const float aspect = localSize.x / localSize.y;
                        const Mat4 viewProj = camera.getViewProjLH_ZO(aspect);
                        
                        // Get object transform
                        Vec3 objPos = world.hasComponent<WorldEditor::TransformComponent>(selected)
                            ? world.getComponent<WorldEditor::TransformComponent>(selected).position
                            : Vec3(0.0f);
                        
                        // Find terrain to sample height
                        Entity terrainE = INVALID_ENTITY;
                        auto& reg = world.getEntityManager().getRegistry();
                        auto viewT = reg.view<WorldEditor::TerrainComponent, WorldEditor::TransformComponent>();
                        for (auto e : viewT) {
                            terrainE = e;
                            break;
                        }
                        
                        if (terrainE != INVALID_ENTITY) {
                            auto& terrain = reg.get<WorldEditor::TerrainComponent>(terrainE);
                            auto& tr = reg.get<WorldEditor::TransformComponent>(terrainE);
                            
                            // Sample height at object position using bilinear interpolation
                            const Vec3 objLocal = objPos - tr.position;
                            auto sampleHeightBilinear = [&terrain](float x, float z) -> float {
                                const int w = (std::max)(2, terrain.resolution.x);
                                const int h = (std::max)(2, terrain.resolution.y);
                                const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
                                if (terrain.heightmap.size() != wanted || terrain.size <= 0.0f) {
                                    return 0.0f;
                                }
                                const float cell = terrain.size / float(w - 1);
                                const float gx = (std::clamp)(x / cell, 0.0f, float(w - 1));
                                const float gz = (std::clamp)(z / cell, 0.0f, float(h - 1));
                                const int x0 = (std::max)(0, (std::min)(w - 1, int(std::floor(gx))));
                                const int z0 = (std::max)(0, (std::min)(h - 1, int(std::floor(gz))));
                                const int x1 = (std::min)(w - 1, x0 + 1);
                                const int z1 = (std::min)(h - 1, z0 + 1);
                                const float tx = gx - float(x0);
                                const float tz = gz - float(z0);
                                auto idx = [w](int ix, int iz) -> size_t {
                                    return static_cast<size_t>(iz) * static_cast<size_t>(w) + static_cast<size_t>(ix);
                                };
                                const float h00 = terrain.heightmap[idx(x0, z0)];
                                const float h10 = terrain.heightmap[idx(x1, z0)];
                                const float h01 = terrain.heightmap[idx(x0, z1)];
                                const float h11 = terrain.heightmap[idx(x1, z1)];
                                const float hx0 = h00 + (h10 - h00) * tx;
                                const float hx1 = h01 + (h11 - h01) * tx;
                                return hx0 + (hx1 - hx0) * tz;
                            };
                            
                            float objHeight = sampleHeightBilinear(objLocal.x, objLocal.z);
                            objPos.y = objHeight;
                            
                            // Draw spawn radius circle on terrain
                            ImDrawList* dl = ImGui::GetForegroundDrawList();
                            dl->PushClipRect(vMin, vMax, true);
                            const float radiusWU = objComp.spawnRadius;
                            const float yEps = 0.1f;
                            const int segments = 48;
                            
                            ImU32 colSpawn = (objComp.type == WorldEditor::ObjectType::CreepSpawn)
                                ? IM_COL32(0, 255, 0, 150)  // Green for creep spawns
                                : IM_COL32(255, 255, 0, 150); // Yellow for neutral camps
                            
                            std::vector<ImVec2> polylinePoints;
                            polylinePoints.reserve(segments);
                            for (int i = 0; i < segments; ++i) {
                                float angle = (float)i / segments * 2.0f * glm::pi<float>();
                                Vec3 circlePointLocal = objLocal + Vec3(radiusWU * std::cos(angle), 0.0f, radiusWU * std::sin(angle));
                                float sampledHeight = sampleHeightBilinear(circlePointLocal.x, circlePointLocal.z);
                                circlePointLocal.y = sampledHeight + yEps;
                                
                                Vec3 circlePointWorld = tr.position + circlePointLocal;
                                Vec2 screenPos = WorldEditor::Math::worldToScreen(circlePointWorld, viewProj, localSize);
                                polylinePoints.push_back(ImVec2(vMin.x + screenPos.x, vMin.y + screenPos.y));
                            }
                            
                            dl->AddPolyline(polylinePoints.data(), (int)polylinePoints.size(), colSpawn, ImDrawFlags_Closed, 2.0f);
                            
                            // Draw inner circle (50% radius)
                            polylinePoints.clear();
                            for (int i = 0; i < segments; ++i) {
                                float angle = (float)i / segments * 2.0f * glm::pi<float>();
                                Vec3 circlePointLocal = objLocal + Vec3(radiusWU * 0.5f * std::cos(angle), 0.0f, radiusWU * 0.5f * std::sin(angle));
                                float sampledHeight = sampleHeightBilinear(circlePointLocal.x, circlePointLocal.z);
                                circlePointLocal.y = sampledHeight + yEps;
                                
                                Vec3 circlePointWorld = tr.position + circlePointLocal;
                                Vec2 screenPos = WorldEditor::Math::worldToScreen(circlePointWorld, viewProj, localSize);
                                polylinePoints.push_back(ImVec2(vMin.x + screenPos.x, vMin.y + screenPos.y));
                            }
                            dl->AddPolyline(polylinePoints.data(), (int)polylinePoints.size(), colSpawn, ImDrawFlags_Closed, 1.0f);
                            dl->PopClipRect();
                        }
                    }
                }

                // Visualize tower attack range (ground circle)
                if (objComp.type == WorldEditor::ObjectType::Tower && objComp.attackRange > 0.0f) {
                    const ImVec2 vMin = editorUI.getViewportRectMin();
                    const ImVec2 vMax = editorUI.getViewportRectMax();
                    const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                    if (localSize.x > 4.0f && localSize.y > 4.0f) {
                        const float aspect = localSize.x / localSize.y;
                        const Mat4 viewProj = camera.getViewProjLH_ZO(aspect);

                        Vec3 objPos = world.hasComponent<WorldEditor::TransformComponent>(selected)
                            ? world.getComponent<WorldEditor::TransformComponent>(selected).position
                            : Vec3(0.0f);

                        // Find terrain to sample height
                        Entity terrainE = INVALID_ENTITY;
                        auto& reg = world.getEntityManager().getRegistry();
                        auto viewT = reg.view<WorldEditor::TerrainComponent, WorldEditor::TransformComponent>();
                        for (auto e : viewT) { terrainE = e; break; }

                        if (terrainE != INVALID_ENTITY) {
                            auto& terrain = reg.get<WorldEditor::TerrainComponent>(terrainE);
                            auto& tr = reg.get<WorldEditor::TransformComponent>(terrainE);
                            const Vec3 centerLocal = objPos - tr.position;

                            auto sampleHeightBilinear = [&terrain](float x, float z) -> float {
                                const int w = (std::max)(2, terrain.resolution.x);
                                const int h = (std::max)(2, terrain.resolution.y);
                                const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
                                if (terrain.heightmap.size() != wanted || terrain.size <= 0.0f) return 0.0f;
                                const float cell = terrain.size / float(w - 1);
                                const float gx = (std::clamp)(x / cell, 0.0f, float(w - 1));
                                const float gz = (std::clamp)(z / cell, 0.0f, float(h - 1));
                                const int x0 = (std::max)(0, (std::min)(w - 1, int(std::floor(gx))));
                                const int z0 = (std::max)(0, (std::min)(h - 1, int(std::floor(gz))));
                                const int x1 = (std::min)(w - 1, x0 + 1);
                                const int z1 = (std::min)(h - 1, z0 + 1);
                                const float tx = gx - float(x0);
                                const float tz = gz - float(z0);
                                auto idx = [w](int ix, int iz) -> size_t {
                                    return static_cast<size_t>(iz) * static_cast<size_t>(w) + static_cast<size_t>(ix);
                                };
                                const float h00 = terrain.heightmap[idx(x0, z0)];
                                const float h10 = terrain.heightmap[idx(x1, z0)];
                                const float h01 = terrain.heightmap[idx(x0, z1)];
                                const float h11 = terrain.heightmap[idx(x1, z1)];
                                const float hx0 = h00 + (h10 - h00) * tx;
                                const float hx1 = h01 + (h11 - h01) * tx;
                                return hx0 + (hx1 - hx0) * tz;
                            };

                            ImDrawList* dl = ImGui::GetForegroundDrawList();
                            dl->PushClipRect(vMin, vMax, true);
                            const float radiusWU = objComp.attackRange;
                            const float yEps = 0.12f;
                            const int segments = 56;
                            const ImU32 col = (objComp.teamId == 1) ? IM_COL32(80, 255, 80, 140) : IM_COL32(255, 80, 80, 140);

                            std::vector<ImVec2> polylinePoints;
                            polylinePoints.reserve(segments);
                            for (int i = 0; i < segments; ++i) {
                                float angle = (float)i / segments * 2.0f * glm::pi<float>();
                                Vec3 pLocal = centerLocal + Vec3(radiusWU * std::cos(angle), 0.0f, radiusWU * std::sin(angle));
                                float sampledHeight = sampleHeightBilinear(pLocal.x, pLocal.z);
                                pLocal.y = sampledHeight + yEps;

                                Vec3 pWorld = tr.position + pLocal;
                                Vec2 screenPos = WorldEditor::Math::worldToScreen(pWorld, viewProj, localSize);
                                polylinePoints.push_back(ImVec2(vMin.x + screenPos.x, vMin.y + screenPos.y));
                            }
                            dl->AddPolyline(polylinePoints.data(), (int)polylinePoints.size(), col, ImDrawFlags_Closed, 2.0f);
                            dl->PopClipRect();
                        }
                    }
                }
            }

            // Visualize attack ranges for all creeps (units) to make combat ranges obvious.
            if (editorUI.getShowUnitAttackRanges()) {
                const ImVec2 vMin = editorUI.getViewportRectMin();
                const ImVec2 vMax = editorUI.getViewportRectMax();
                const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                if (localSize.x > 4.0f && localSize.y > 4.0f) {
                    const float aspect = localSize.x / localSize.y;
                    const Mat4 viewProjRanges = camera.getViewProjLH_ZO(aspect);

                    ImDrawList* dl = ImGui::GetForegroundDrawList();
                    dl->PushClipRect(vMin, vMax, true);
                    auto& reg = world.getEntityManager().getRegistry();
                    auto creepView = reg.view<WorldEditor::CreepComponent, WorldEditor::TransformComponent>();

                    // Perf guard: don't draw circles for units too far from camera.
                    const float maxDrawDist = 140.0f;
                    const float maxDrawDist2 = maxDrawDist * maxDrawDist;
                    const int segments = 32;
                    const float yEps = 0.10f;

                    std::vector<ImVec2> polylinePoints;
                    polylinePoints.reserve(segments);

                    for (auto e : creepView) {
                        const auto& creep = creepView.get<WorldEditor::CreepComponent>(e);
                        if (creep.state == WorldEditor::CreepState::Dead) {
                            continue;
                        }

                        const float radiusWU = creep.attackRange;
                        if (radiusWU <= 0.01f) {
                            continue;
                        }

                        const Vec3 center = creepView.get<WorldEditor::TransformComponent>(e).position;
                        Vec3 d = center - camera.position;
                        d.y = 0.0f;
                        if (glm::dot(d, d) > maxDrawDist2) {
                            continue;
                        }

                        const ImU32 col =
                            (creep.teamId == 1) ? IM_COL32(80, 255, 80, 110) :
                            (creep.teamId == 2) ? IM_COL32(255, 80, 80, 110) :
                                                  IM_COL32(200, 200, 200, 90);

                        polylinePoints.clear();
                        const float y = center.y + yEps;
                        for (int i = 0; i < segments; ++i) {
                            const float angle = (float)i / segments * 2.0f * glm::pi<float>();
                            const Vec3 pWorld(center.x + radiusWU * std::cos(angle), y, center.z + radiusWU * std::sin(angle));
                            const Vec2 screenPos = WorldEditor::Math::worldToScreen(pWorld, viewProjRanges, localSize);
                            polylinePoints.push_back(ImVec2(vMin.x + screenPos.x, vMin.y + screenPos.y));
                        }
                        dl->AddPolyline(polylinePoints.data(), (int)polylinePoints.size(), col, ImDrawFlags_Closed, 2.0f);
                    }
                    dl->PopClipRect();
                }
            }

            // Visualize attack range for selected creeps
            if (selected != INVALID_ENTITY && world.isValid(selected) &&
                world.hasComponent<WorldEditor::CreepComponent>(selected) &&
                world.hasComponent<WorldEditor::TransformComponent>(selected)) {
                auto& creep = world.getComponent<WorldEditor::CreepComponent>(selected);
                const float radiusWU = creep.attackRange;
                if (radiusWU > 0.0f) {
                    const ImVec2 vMin = editorUI.getViewportRectMin();
                    const ImVec2 vMax = editorUI.getViewportRectMax();
                    const Vec2 localSize(vMax.x - vMin.x, vMax.y - vMin.y);
                    if (localSize.x > 4.0f && localSize.y > 4.0f) {
                        const float aspect = localSize.x / localSize.y;
                        const Mat4 viewProj = camera.getViewProjLH_ZO(aspect);

                        Vec3 creepPos = world.getComponent<WorldEditor::TransformComponent>(selected).position;

                        // Find terrain to sample height (draw on ground plane)
                        Entity terrainE = INVALID_ENTITY;
                        auto& reg = world.getEntityManager().getRegistry();
                        auto viewT = reg.view<WorldEditor::TerrainComponent, WorldEditor::TransformComponent>();
                        for (auto e : viewT) {
                            terrainE = e;
                            break;
                        }

                        if (terrainE != INVALID_ENTITY) {
                            auto& terrain = reg.get<WorldEditor::TerrainComponent>(terrainE);
                            auto& tr = reg.get<WorldEditor::TransformComponent>(terrainE);

                            const Vec3 centerLocal = creepPos - tr.position;
                            auto sampleHeightBilinear = [&terrain](float x, float z) -> float {
                                const int w = (std::max)(2, terrain.resolution.x);
                                const int h = (std::max)(2, terrain.resolution.y);
                                const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
                                if (terrain.heightmap.size() != wanted || terrain.size <= 0.0f) {
                                    return 0.0f;
                                }
                                const float cell = terrain.size / float(w - 1);
                                const float gx = (std::clamp)(x / cell, 0.0f, float(w - 1));
                                const float gz = (std::clamp)(z / cell, 0.0f, float(h - 1));
                                const int x0 = (std::max)(0, (std::min)(w - 1, int(std::floor(gx))));
                                const int z0 = (std::max)(0, (std::min)(h - 1, int(std::floor(gz))));
                                const int x1 = (std::min)(w - 1, x0 + 1);
                                const int z1 = (std::min)(h - 1, z0 + 1);
                                const float tx = gx - float(x0);
                                const float tz = gz - float(z0);
                                auto idx = [w](int ix, int iz) -> size_t {
                                    return static_cast<size_t>(iz) * static_cast<size_t>(w) + static_cast<size_t>(ix);
                                };
                                const float h00 = terrain.heightmap[idx(x0, z0)];
                                const float h10 = terrain.heightmap[idx(x1, z0)];
                                const float h01 = terrain.heightmap[idx(x0, z1)];
                                const float h11 = terrain.heightmap[idx(x1, z1)];
                                const float hx0 = h00 + (h10 - h00) * tx;
                                const float hx1 = h01 + (h11 - h01) * tx;
                                return hx0 + (hx1 - hx0) * tz;
                            };

                            // Draw attack range circle on terrain
                            ImDrawList* dl = ImGui::GetForegroundDrawList();
                            dl->PushClipRect(vMin, vMax, true);
                            const float yEps = 0.15f;
                            const int segments = 56;
                            const ImU32 colRange = IM_COL32(255, 80, 80, 170);

                            std::vector<ImVec2> polylinePoints;
                            polylinePoints.reserve(segments);
                            for (int i = 0; i < segments; ++i) {
                                float angle = (float)i / segments * 2.0f * glm::pi<float>();
                                Vec3 pLocal = centerLocal + Vec3(radiusWU * std::cos(angle), 0.0f, radiusWU * std::sin(angle));
                                float sampledHeight = sampleHeightBilinear(pLocal.x, pLocal.z);
                                pLocal.y = sampledHeight + yEps;

                                Vec3 pWorld = tr.position + pLocal;
                                Vec2 screenPos = WorldEditor::Math::worldToScreen(pWorld, viewProj, localSize);
                                polylinePoints.push_back(ImVec2(vMin.x + screenPos.x, vMin.y + screenPos.y));
                            }
                            dl->AddPolyline(polylinePoints.data(), (int)polylinePoints.size(), colRange, ImDrawFlags_Closed, 2.0f);
                            dl->PopClipRect();
                        }
                    }
                }
            }

            const float aspect = float(vpW) / float(vpH);
            Mat4 viewProj = camera.getViewProjLH_ZO(aspect);

            // Unreal-like viewport background (sky gradient + sun disc).
            if (editorUI.isUnrealViewportEnabled()) {
                if (auto* sky = renderer.GetSkyRenderer()) {
                    const Mat4 invViewProj = glm::inverse(viewProj);

                    Vec3 sunDir(0.0f, 1.0f, 0.0f);
                    Vec3 sunColor(1.0f, 1.0f, 1.0f);
                    if (auto* lighting = renderer.GetLightingSystem()) {
                        const auto& lc = lighting->getLightingConstants();
                        // LightingConstants::lightDirection is a direction the light travels; sun is the opposite.
                        sunDir = glm::normalize(-Vec3(lc.lightDirection));
                        sunColor = Vec3(lc.lightColor);
                    }

                    sky->render(renderer.GetCommandList(), invViewProj, sunDir, sunColor);
                }
            }

            // Render world into the offscreen target.
            // Get path visualization setting from editor UI
            bool showPathLines = editorUI.getShowPathLines();
            world.render(renderer.GetCommandList(), viewProj, camera.position, showPathLines);
            renderer.EndOffscreenPass();

            // Now draw UI onto swapchain.
            float backClear[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
            renderer.BeginSwapchainPass(backClear);

            // Draw HP/MP bars for units in game mode (before ImGui::Render so it's in background)
            if (gameMode && gameMode->isGameModeActive()) {
                Vec2 viewportSize(static_cast<f32>(vpW), static_cast<f32>(vpH));
                ImVec2 viewportRectMin = editorUI.getViewportRectMin();
                gameMode->drawUnitHealthBars(world, viewProj, viewportSize, viewportRectMin);
            }

            // Render ImGui on top.
            ImGui::Render();
            ID3D12DescriptorHeap* heaps[] = { renderer.GetSrvHeap() };
            renderer.GetCommandList()->SetDescriptorHeaps(1, heaps);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), renderer.GetCommandList());

            renderer.EndFrame();
            if (!renderer.Present()) {
                // Present failure usually means device removed/reset or swapchain is no longer valid.
                // Exit the main loop to avoid infinite error spam.
                std::cerr << "Renderer::Present() returned false. Exiting main loop." << std::endl;
                break;
            }

            // Editor-requested quit (after unsaved changes prompt).
            if (editorUI.consumeQuitRequested()) {
                PostQuitMessage(0);
                break;
            }
        }

        std::cout << "Application finished successfully!" << std::endl;

        // Clear static renderer reference before cleanup
        WorldEditor::MeshComponent::s_renderer = nullptr;

        // Shutdown ImGui.
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return 0;
    } catch (const DirectXException& e) {
        std::cerr << "DirectX Error: " << e.what() << std::endl;
        MessageBoxA(nullptr, e.what(), "WorldEditor DirectX Error", MB_OK | MB_ICONERROR);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        MessageBoxA(nullptr, e.what(), "WorldEditor Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
