# Network module
add_library(world_editor_network STATIC
    NetworkCommon.h
    NetworkServer.h
    NetworkServer.cpp
    NetworkClient.h
    NetworkClient.cpp
    MatchmakingTypes.h
    MatchmakingProtocol.h
    MatchmakingProtocol.cpp
    MatchmakingClient.h
    MatchmakingClient.cpp
)

target_include_directories(world_editor_network PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(world_editor_network
    world_editor_core
    spdlog::spdlog
    ws2_32  # Winsock2
)

target_compile_definitions(world_editor_network PRIVATE
    USE_SPDLOG
)

# Set output directory
set_target_properties(world_editor_network PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
