cmake_minimum_required(VERSION 3.20)
project(WorldEditor VERSION 0.1.0 LANGUAGES CXX C)

# Set CMake policies for compatibility
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0148 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0069 NEW)
cmake_policy(SET CMP0074 NEW)
cmake_policy(SET CMP0079 NEW)
cmake_policy(SET CMP0083 NEW)
cmake_policy(SET CMP0086 NEW)
cmake_policy(SET CMP0092 NEW)

# Set minimum required CMake version for subprojects
set(CMAKE_POLICY_DEFAULT_CMP0057 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0148 NEW)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Make produced binaries more portable when copying between machines:
# link the MSVC runtime statically (/MT, /MTd) so the app doesn't require
# "Microsoft Visual C++ Redistributable" to be installed on the target PC.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable testing
enable_testing()

# Windows SDK and DirectX requirements
if(NOT WIN32)
    message(FATAL_ERROR "DirectX integration requires Windows platform")
endif()

# Set Windows SDK version (minimum Windows 10)
set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)


# Third-party dependencies
include(FetchContent)

# GLM - using header-only version
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# Make GLM header-only and enable experimental features
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)


# stb
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)


# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.11.0
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)


# EnTT
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.12.2
)
FetchContent_MakeAvailable(entt)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# Catch2 for testing
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# SQLite3 amalgamation
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(sqlite3)

# Create SQLite3 library
if(NOT TARGET sqlite3_lib)
    add_library(sqlite3_lib STATIC
        ${sqlite3_SOURCE_DIR}/sqlite3.c
    )
    target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})
    target_compile_definitions(sqlite3_lib PRIVATE 
        SQLITE_ENABLE_COLUMN_METADATA
        SQLITE_ENABLE_FTS5
        SQLITE_ENABLE_RTREE
    )
    if(WIN32)
        target_compile_definitions(sqlite3_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# BCrypt library (custom implementation)
if(NOT TARGET bcrypt_lib)
    add_library(bcrypt_lib STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/bcrypt/bcrypt_hash.cpp
    )
    target_include_directories(bcrypt_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/bcrypt)
    if(WIN32)
        target_link_libraries(bcrypt_lib PUBLIC bcrypt)
        target_compile_definitions(bcrypt_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# ImGui Docking (editor needs DockSpace API; it's only available in docking branch).
# We use a separate FetchContent name to avoid reusing any previously populated imgui-src.
FetchContent_Declare(
    imgui_docking
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_GetProperties(imgui_docking)
if(NOT imgui_docking_POPULATED)
    FetchContent_Populate(imgui_docking)
endif()

if(NOT TARGET imgui)
    add_library(imgui STATIC
        ${imgui_docking_SOURCE_DIR}/imgui.cpp
        ${imgui_docking_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_docking_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_docking_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_docking_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_docking_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_docking_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    )

    target_include_directories(imgui
        PUBLIC
            ${imgui_docking_SOURCE_DIR}
            ${imgui_docking_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui
        PUBLIC
            d3d12.lib
            dxgi.lib
            d3dcompiler.lib
            dxguid.lib
    )
endif()


# DirectX libraries will be linked in the main executable

# Create stb library
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Main executable (Editor)
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    world_editor_core
    world_editor_common
    world_editor_server
    world_editor_world
    world_editor_renderer
    world_editor_ui
    glm::glm
    imgui
    dbghelp
)

# Install targets
install(TARGETS ${PROJECT_NAME} Game
    RUNTIME DESTINATION bin
)

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Copy resources
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/..)

# Print configuration
message(STATUS "World Editor Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
