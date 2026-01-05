# World Editor Source CMakeLists

# Core library
add_subdirectory(core)

# Common library (shared between client/server)
add_subdirectory(common)

# Network library (UDP client/server)
add_subdirectory(network)

# Authentication library
add_subdirectory(auth)

# Renderer library
add_subdirectory(renderer)

# World library (ECS components and systems)
add_subdirectory(world)

# Server library (authoritative simulation)
add_subdirectory(server)

# Client library (prediction + interpolation)
add_subdirectory(client)

# Serialization (maps)
add_subdirectory(serialization)

# Properties/reflection (editor)
add_subdirectory(properties)

# UI library (editor-only for now)
add_subdirectory(ui)

# Gameplay library (shared logic for Editor and Game)
add_subdirectory(gameplay)

# Game module with Panorama UI
add_subdirectory(game)
