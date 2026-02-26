# AGENTS.md

## Build & Test Commands
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64   # Configure
cmake --build build --config Debug                       # Build all (Debug)
cmake --build build --config Debug --target <Target>     # Build single target
ctest --test-dir build -C Debug                          # Run all tests
ctest --test-dir build -C Debug -R <test_name>           # Run single test
```
Targets: WorldEditor, Game, DedicatedServer, AuthServer, MatchmakingCoordinator, auth_tests

## Architecture
- **C++17** project using CMake 3.20+, DirectX 12 (Windows only), EnTT ECS
- Static libraries: world_editor_core, world_editor_common, world_editor_world, world_editor_renderer, world_editor_network, world_editor_auth, world_editor_server, world_editor_client, world_editor_ui, world_editor_game, panorama_ui
- Auth uses SQLite (auth.db), ports: Game 27015, Auth 27016, Matchmaking 27017
- Tests use Catch2 (auth_tests) and simple smoke tests (world_editor_tests)

## Code Style
- Use spdlog for logging, nlohmann/json for JSON, GLM for math
- ImGui (docking branch) for editor UI, Panorama UI for game client
- MSVC static runtime (/MT, /MTd), Windows SDK 10.0+
- No exceptions in hot paths; use return codes or optional types

## Cursor Rules
Always use Context7 MCP server for documentation queries and API references.

## Cursor Cloud specific instructions

### Platform limitation
This project targets **Windows 10/11 x64** with DirectX 12, Winsock2, and Windows BCrypt API. The Cloud Agent Linux environment **cannot build or run the full application**. The CMakeLists.txt files have been patched to support building portable components on Linux (see below).

### Linux build (Cloud Agent)
```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc
cmake --build build --config Debug -j$(nproc)
ctest --test-dir build -C Debug
```

On Linux, only these targets are built:
- `world_editor_core` (portable: glm, spdlog, EnTT, nlohmann_json)
- `world_editor_tests` (smoke test, links only to core on Linux)
- Third-party libraries: sqlite3_lib, Catch2, glm, spdlog

All Windows-only targets (WorldEditor, Game, DedicatedServer, AuthServer, MatchmakingCoordinator, auth_tests, and all libraries depending on DirectX/Winsock/BCrypt) are skipped via `if(WIN32)` guards in CMakeLists.txt.

### Linting
```bash
cppcheck --enable=warning,style --std=c++17 --language=c++ \
  --suppress=missingInclude --suppress=unmatchedSuppression \
  --suppress=internalAstError --quiet --max-configs=1 \
  src/core/ src/common/
```
Note: cppcheck's `internalAstError` triggers on EnTT headers — suppress it. Full project analysis with `-I` includes for all FetchContent deps can time out; lint individual directories instead.

### Key gotchas
- GCC rejects `view.get<Component>(entity)` in template contexts (requires `view.template get<...>()`). MSVC is lenient. This prevents building `world_editor_world` and downstream libraries on Linux without source changes.
- `MeshGenerators.cpp` accesses `MeshComponent::gpuBuffersCreated` outside of `#ifdef DIRECTX_RENDERER` guards — another blocker for Linux compilation of the world library.
- FetchContent downloads dependencies during `cmake` configure (takes ~20s on first run with network access). The `build/_deps/` directory caches them.
- Always pass `-DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc` on Linux; the default `c++` symlink may point to Clang which may lack `libstdc++`.
