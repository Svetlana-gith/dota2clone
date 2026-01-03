# Common library (shared between client and server)

add_library(world_editor_common INTERFACE)

target_include_directories(world_editor_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_sources(world_editor_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/NetworkTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GameInput.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GameSnapshot.h
    ${CMAKE_CURRENT_SOURCE_DIR}/IGameWorld.h
)

target_link_libraries(world_editor_common INTERFACE
    world_editor_core
    glm::glm
)
