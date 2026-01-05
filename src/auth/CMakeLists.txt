# Authentication System Library

add_library(world_editor_auth STATIC
    DatabaseManager.cpp
    SecurityManager.cpp
    AuthProtocol.cpp
    AuthServer.cpp
    AuthClient.cpp
)

target_include_directories(world_editor_auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(world_editor_auth
    PUBLIC
        world_editor_core
        spdlog::spdlog
        sqlite3_lib
        bcrypt_lib
)

# Link Windows BCrypt API and Winsock
if(WIN32)
    target_link_libraries(world_editor_auth PUBLIC bcrypt ws2_32)
endif()

# Authentication Server Executable
add_executable(AuthServer
    auth_server_main.cpp
)

target_link_libraries(AuthServer
    PRIVATE
        world_editor_auth
        world_editor_core
        spdlog::spdlog
)

# Add Test User Tool
add_executable(AddTestUser
    add_test_user_tool.cpp
)

target_link_libraries(AddTestUser
    PRIVATE
        world_editor_auth
        world_editor_core
        spdlog::spdlog
)

# Add MM Test Users Tool
add_executable(AddMMTestUsers
    ${CMAKE_SOURCE_DIR}/scripts/add_mm_test_users.cpp
)

target_link_libraries(AddMMTestUsers
    PRIVATE
        world_editor_auth
        world_editor_core
        spdlog::spdlog
)

# Install targets
install(TARGETS AuthServer AddTestUser AddMMTestUsers
    RUNTIME DESTINATION bin
)
