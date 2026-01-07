# Tests CMakeLists

# Пока без GoogleTest: держим простой smoke-тест, чтобы сборка не падала на gtest_discover_tests.
add_executable(world_editor_tests
    smoke_main.cpp
)

target_link_libraries(world_editor_tests
    world_editor_core
    world_editor_world
)

# Enable CTest
add_test(NAME world_editor_smoke COMMAND world_editor_tests)

# Authentication system tests with Catch2
add_executable(auth_tests
    test_database_manager.cpp
    test_security_manager.cpp
    test_security_properties.cpp
    test_auth_protocol.cpp
    test_auth_client.cpp
    test_matchmaking_auth.cpp
    test_login_validation.cpp
)

target_link_libraries(auth_tests
    PRIVATE
        world_editor_auth
        world_editor_network
        Catch2::Catch2WithMain
)

# Register Catch2 tests with CTest
include(Catch)
catch_discover_tests(auth_tests)

# Minidump inspector (helps diagnose crashes on other machines without WinDbg installed)
if (WIN32)
    add_executable(minidump_inspect
        minidump_inspect.cpp
    )
    target_link_libraries(minidump_inspect
        dbghelp
    )
endif()