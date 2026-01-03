# Panorama UI Library
add_library(panorama_ui STATIC
    PanoramaTypes.h
    Style.h
    Panel.h
    Panel.cpp
    Animation.h
    Animation.cpp
    UIRenderer.h
    UIRenderer.cpp
    PanoramaUI.h
    PanoramaUI.cpp
)

target_include_directories(panorama_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
)

# Link dependencies
target_link_libraries(panorama_ui
    world_editor_core
    glm::glm
    spdlog::spdlog
    d3d11
    d3dcompiler
)
