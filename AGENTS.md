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
