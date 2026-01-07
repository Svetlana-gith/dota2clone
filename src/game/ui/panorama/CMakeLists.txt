# Panorama UI Library - Valve-style UI Framework
add_library(panorama_ui STATIC
    # Core types and engine
    core/PanoramaTypes.h
    core/CPanel2D.h
    core/CPanel2D.cpp
    core/CUIEngine.h
    core/CUIEngine.cpp
    core/GameEvents.h
    core/GameEvents.cpp
    
    # Layout system
    layout/CStyleSheet.h
    layout/CStyleSheet.cpp
    layout/CLayoutFile.h
    layout/CLayoutFile.cpp
    
    # Widgets
    widgets/CPanelWidgets.h
    widgets/CPanelWidgets.cpp
    widgets/CLabel.h
    widgets/CLabel.cpp
    widgets/CImage.h
    widgets/CImage.cpp
    widgets/CButton.h
    widgets/CButton.cpp
    widgets/CProgressBar.h
    widgets/CProgressBar.cpp
    widgets/CTextEntry.h
    widgets/CTextEntry.cpp
    widgets/CSlider.h
    widgets/CSlider.cpp
    widgets/CDropDown.h
    widgets/CDropDown.cpp
    
    # Rendering system
    rendering/CUIRenderer.h
    rendering/CUIRenderer_DX12.cpp
    rendering/CUITextSystem.h
    rendering/CUITextSystem.cpp
    rendering/FontAtlas.h
    rendering/FontAtlas.cpp
    
    # HUD components
    hud/CHUDManager.h
    hud/CHUDManager.cpp
    hud/HUDComponents.h
    hud/HUDComponents.cpp
    hud/CHeroUnitFrame.h
    hud/CHeroUnitFrame.cpp
    hud/CAbilityPanel.h
    hud/CAbilityPanel.cpp
    hud/CItemPanel.h
    hud/CItemPanel.cpp
    hud/CMinimap.h
    hud/CMinimap.cpp
    hud/CScoreboard.h
    hud/CScoreboard.cpp
    hud/CTooltip.h
    hud/CTooltip.cpp
    hud/CNotificationManager.h
    hud/CNotificationManager.cpp
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
