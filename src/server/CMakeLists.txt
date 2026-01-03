# Server library (authoritative simulation)

add_library(world_editor_server STATIC
    ServerWorld.cpp
    ServerWorld.h
)

target_include_directories(world_editor_server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(world_editor_server
    PUBLIC
        world_editor_common
        world_editor_core
        world_editor_world
        glm::glm
)

# DirectX support (optional, for editor/visualization)
if(WIN32)
    target_link_libraries(world_editor_server
        PUBLIC
            d3d12.lib
            dxgi.lib
    )
    target_compile_definitions(world_editor_server PUBLIC DIRECTX_RENDERER)
endif()

# ============ Dedicated Server Executable ============

add_executable(DedicatedServer
    DedicatedServer.cpp
)

target_include_directories(DedicatedServer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(DedicatedServer
    world_editor_server
    world_editor_network
    world_editor_renderer
    world_editor_core
    spdlog::spdlog
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
)

target_compile_definitions(DedicatedServer PRIVATE
    USE_SPDLOG
)

# Console application
set_target_properties(DedicatedServer PROPERTIES
    OUTPUT_NAME "DedicatedServer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy to build directory
add_custom_command(TARGET DedicatedServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DedicatedServer>
        ${CMAKE_BINARY_DIR}/DedicatedServer.exe
)

# ============ Matchmaking Coordinator Executable ============

add_executable(MatchmakingCoordinator
    MatchmakingCoordinator.cpp
)

target_include_directories(MatchmakingCoordinator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(MatchmakingCoordinator
    world_editor_network
    world_editor_core
    world_editor_auth
    spdlog::spdlog
)

target_compile_definitions(MatchmakingCoordinator PRIVATE
    USE_SPDLOG
)

set_target_properties(MatchmakingCoordinator PROPERTIES
    OUTPUT_NAME "MatchmakingCoordinator"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET MatchmakingCoordinator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:MatchmakingCoordinator>
        ${CMAKE_BINARY_DIR}/MatchmakingCoordinator.exe
)
