#include "World.h"
#include <d3d12.h>
#include <memory>

namespace WorldEditor {

World::World(ID3D12Device* device) : device_(device) {
    // Minimal constructor
}

World::World() {
    // Default constructor
}

World::~World() {
    // Destructor
}

void World::addSystem(UniquePtr<System> system) {
    if (system) {
        systems_[system->getName()] = std::move(system);
    }
}

void World::removeSystem(const String& name) {
    systems_.erase(name);
}

System* World::getSystem(const String& name) {
    auto it = systems_.find(name);
    return (it != systems_.end()) ? it->second.get() : nullptr;
}

void World::update(f32 deltaTime, bool isPaused) {
    if (isPaused) return;
    
    for (auto& [name, system] : systems_) {
        system->update(deltaTime);
    }
}

void World::render(ID3D12GraphicsCommandList* commandList, const Mat4& viewProjMatrix, const Vec3& cameraPosition, bool showPathLines) {
    // Minimal render implementation
}

void World::clear() {
    systems_.clear();
    entityManager_.clear();
}

void World::clearEntities() {
    entityManager_.clear();
}

size_t World::getEntityCount() const {
    return entityManager_.getEntityCount();
}

} // namespace WorldEditor