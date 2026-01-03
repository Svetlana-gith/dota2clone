# Client library (prediction + interpolation)

add_library(world_editor_client STATIC
    ClientWorld.cpp
    ClientWorld.h
)

target_include_directories(world_editor_client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(world_editor_client
    PUBLIC
        world_editor_common
        world_editor_core
        world_editor_world
        glm::glm
)
