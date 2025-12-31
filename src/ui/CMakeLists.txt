# UI library (editor-only)

set(TARGET_NAME world_editor_ui)

add_library(${TARGET_NAME} STATIC
    EditorUI.cpp
    EditorUI.h
    EditorCamera.cpp
    EditorCamera.h
    Picking.cpp
    Picking.h
    GameMode.cpp
    GameMode.h
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        world_editor_core
        world_editor_world
        world_editor_properties
        world_editor_serialization
        imgui
)


