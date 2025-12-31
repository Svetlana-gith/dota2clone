#include "World.h"
#include "CreepSystem.h"
#include "CollisionSystem.h"
#include "../renderer/LightingSystem.h"
#include "../renderer/WireframeGrid.h"
#include "TerrainChunks.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <iostream>
#include <map>
#include <memory>
#include <algorithm>

// Test compilation without namespace
void testFunction() {
    std::cout << "Test compilation" << std::endl;
    auto ptr = std::make_unique<int>(42);
    std::map<int, int> testMap;
}