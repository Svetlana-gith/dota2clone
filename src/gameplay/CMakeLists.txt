# Gameplay library - shared gameplay logic for Editor and Game Client

add_library(world_editor_gameplay STATIC
    GameplayController.cpp
    GameplayController.h
)

target_include_directories(world_editor_gameplay PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/glm
    ${CMAKE_SOURCE_DIR}/external/entt/src
    ${CMAKE_SOURCE_DIR}/external/spdlog/include
)

target_link_libraries(world_editor_gameplay PUBLIC
    world_editor_core
    world_editor_world
    world_editor_ui
)

target_compile_definitions(world_editor_gameplay PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
