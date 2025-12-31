# Properties/reflection library (editor-oriented, but UI-agnostic)

set(TARGET_NAME world_editor_properties)

add_library(${TARGET_NAME} STATIC
    Properties.cpp
    Properties.h
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        world_editor_core
        world_editor_world
)


