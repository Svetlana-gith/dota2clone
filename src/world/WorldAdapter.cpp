#include "World.h"
#include "WorldLegacy.h"
#include "CreepSystem.h"
#include "ProjectileSystem.h"
#include "TowerSystem.h"
#include "CreepSpawnSystem.h"
#include "CollisionSystem.h"
#include "HeroSystem.h"

namespace WorldEditor {

World::World() {
    serverWorld_ = std::make_unique<ServerWorld>();
    // Set world reference for systems that need it
    serverWorld_->getEntityManager().setWorld(this);
}

#ifdef DIRECTX_RENDERER
World::World(ID3D12Device* device) {
    serverWorld_ = std::make_unique<ServerWorld>(device);
    
    auto& entityManager = serverWorld_->getEntityManager();
    
    // Add RenderSystem for editor visualization
    serverWorld_->addSystem(std::make_unique<RenderSystem>(entityManager, device));
    
    // Add collision system
    serverWorld_->addSystem(std::make_unique<CollisionSystem>(entityManager));
    
    // Initialize core MOBA systems
    serverWorld_->addSystem(std::make_unique<CreepSystem>(entityManager));
    serverWorld_->addSystem(std::make_unique<ProjectileSystem>(entityManager));
    serverWorld_->addSystem(std::make_unique<TowerSystem>(entityManager));
    serverWorld_->addSystem(std::make_unique<CreepSpawnSystem>(entityManager));
    
    // Add Hero System
    serverWorld_->addSystem(std::make_unique<HeroSystem>(entityManager));
    
    // Set world reference for systems that need it (after all systems are added)
    entityManager.setWorld(this);
}
#endif

World::~World() = default;

} // namespace WorldEditor
