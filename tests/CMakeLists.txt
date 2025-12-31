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

# Minidump inspector (helps diagnose crashes on other machines without WinDbg installed)
if (WIN32)
    add_executable(minidump_inspect
        minidump_inspect.cpp
    )
    target_link_libraries(minidump_inspect
        dbghelp
    )
endif()