# World library CMakeLists

set(WORLD_SOURCES
    EntityManager.cpp
    Components.cpp
    TerrainTools.cpp
    TerrainMesh.cpp
    MeshGenerators.cpp
    TerrainRaycast.cpp
    TerrainChunks.cpp
    WorldLegacy.cpp
    WorldAdapter.cpp
    CollisionSystem.cpp
    CreepSystem.cpp
    ProjectileSystem.cpp
    TowerSystem.cpp
    CreepSpawnSystem.cpp
    HeroSystem.cpp
    ParticleSystem.cpp
    AnimationSystem.cpp
)

set(WORLD_HEADERS
    WorldLegacy.h
    World.h
    System.h
    EntityManager.h
    Components.h
    TerrainMesh.h
    TerrainRaycast.h
    TerrainTools.h
    TerrainChunks.h
    MeshGenerators.h
    CreepSystem.h
    ProjectileSystem.h
    TowerSystem.h
    CreepSpawnSystem.h
    CollisionSystem.h
    HeroSystem.h
    ParticleSystem.h
    AnimationSystem.h
)

add_library(world_editor_world STATIC
    ${WORLD_SOURCES}
    ${WORLD_HEADERS}
)

target_include_directories(world_editor_world PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(world_editor_world PUBLIC
    world_editor_core
    world_editor_common
)

# Enable DirectX renderer flag
target_compile_definitions(world_editor_world
    PUBLIC
        DIRECTX_RENDERER
)

# Enable /FS flag for parallel PDB writing (fixes C1041 error)
if(MSVC)
    target_compile_options(world_editor_world PRIVATE /FS)
    # Place PDBs next to the final binaries to avoid LNK4099 warnings when linking the main exe.
    # (MSVC sometimes stores only the PDB filename in object debug records, and the linker then
    # searches in the executable output directory.)
    set_target_properties(world_editor_world PROPERTIES
        PDB_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
endif()