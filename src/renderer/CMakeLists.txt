set(TARGET_NAME world_editor_renderer)

add_library(${TARGET_NAME} STATIC
    DirectXRenderer.cpp
    DirectXRenderer.h
    LightingSystem.cpp
    LightingSystem.h
    ShadowMapping.cpp
    ShadowMapping.h
    SkyRenderer.cpp
    SkyRenderer.h
    WireframeGrid.cpp
    WireframeGrid.h
)

target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${glm_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
    PUBLIC
        d3d12.lib
        dxgi.lib
        d3dcompiler.lib
        dxguid.lib
        world_editor_core
        spdlog::spdlog
)

# Enable DirectX debug layer in debug builds
target_compile_definitions(${TARGET_NAME}
    PRIVATE
        $<$<CONFIG:Debug>:DX12_ENABLE_DEBUG_LAYER>
        DIRECTX_RENDERER
)
