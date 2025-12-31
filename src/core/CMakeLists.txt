# Core library CMakeLists

set(CORE_SOURCES
    Timer.cpp
    MathUtils.cpp
)

set(CORE_HEADERS
    Types.h
    MathUtils.h
    Timer.h
)

add_library(world_editor_core STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_include_directories(world_editor_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${glm_SOURCE_DIR}
    ${spdlog_SOURCE_DIR}/include
    ${entt_SOURCE_DIR}/src
    ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(world_editor_core PUBLIC
    glm
    spdlog::spdlog
    EnTT::EnTT
    nlohmann_json::nlohmann_json
)
