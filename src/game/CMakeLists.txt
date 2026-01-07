# Panorama UI submodule (must be first)
add_subdirectory(ui/panorama)

# Game module library
add_library(world_editor_game STATIC
    # Core game state system
    GameState.h
    GameState.cpp
    GameData.h
    GameStateManager.cpp
    DebugConsole.h
    DebugConsole.cpp
    SettingsManager.h
    SettingsManager.cpp
    
    # Game states
    states/LoginState.cpp
    states/MainMenuState.cpp
    states/HeroesState.cpp
    states/HeroPickState.cpp
    states/LoadingState.cpp
    states/InGameState.cpp
    
    # Reusable UI panels (can be used in multiple states)
    ui/panels/SettingsPanel.h
    ui/panels/SettingsPanel.cpp
    ui/panels/MatchmakingPanel.h
    ui/panels/MatchmakingPanel.cpp
    ui/panels/ReconnectPanel.h
    ui/panels/ReconnectPanel.cpp
    
    # Main menu specific UI components
    ui/mainmenu/MainMenuTopBar.h
    ui/mainmenu/MainMenuTopBar.cpp
    ui/mainmenu/MainMenuBottomBar.h
    ui/mainmenu/MainMenuBottomBar.cpp
    ui/mainmenu/MainMenuContent.h
    ui/mainmenu/MainMenuContent.cpp
)

target_include_directories(world_editor_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/world
)

target_link_libraries(world_editor_game
    world_editor_core
    world_editor_world
    world_editor_client
    world_editor_server
    world_editor_network
    world_editor_auth
    world_editor_serialization
    world_editor_ui
    world_editor_gameplay
    panorama_ui
)

# ============ Game.exe - Standalone Game Executable ============
add_executable(Game WIN32
    GameMain.cpp
)

target_include_directories(Game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(Game
    world_editor_game
    world_editor_renderer
    panorama_ui
    d3d12
    dxgi
    d3dcompiler
)

# Set output name
set_target_properties(Game PROPERTIES
    OUTPUT_NAME "Game"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy to build directory
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Game>
        ${CMAKE_BINARY_DIR}/Game.exe
)
