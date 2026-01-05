# Panorama UI Library - Valve-style UI Framework
add_library(panorama_ui STATIC
    # Core types
    PanoramaTypes.h
    
    # Stylesheet system
    CStyleSheet.h
    CStyleSheet.cpp
    
    # Panel system
    CPanel2D.h
    CPanel2D.cpp
    CPanelWidgets.cpp
    
    # Layout parser
    CLayoutFile.h
    CLayoutFile.cpp

    # Text system (decoupled from creation)
    CUITextSystem.h
    CUITextSystem.cpp
    
    # Event system
    GameEvents.h
    GameEvents.cpp
    
    # Font system
    FontAtlas.h
    FontAtlas.cpp
    
    # Renderer
    CUIRenderer.h
    CUIRenderer_DX12.cpp
    
    # Main engine
    CUIEngine.h
    CUIEngine.cpp
)

target_include_directories(panorama_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/renderer
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external
)

# Link dependencies
target_link_libraries(panorama_ui
    world_editor_core
    world_editor_renderer
    d3d12
    dxgi
    d3dcompiler
)

# Optional: Link to spdlog if available
if(TARGET spdlog::spdlog)
    target_link_libraries(panorama_ui spdlog::spdlog)
endif()
