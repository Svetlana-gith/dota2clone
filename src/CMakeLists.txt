# World Editor Source CMakeLists

# Core library
add_subdirectory(core)

# Renderer library
add_subdirectory(renderer)

# World library
add_subdirectory(world)

# Serialization (maps)
add_subdirectory(serialization)

# Properties/reflection (editor)
add_subdirectory(properties)

# UI library (editor-only for now)
add_subdirectory(ui)
