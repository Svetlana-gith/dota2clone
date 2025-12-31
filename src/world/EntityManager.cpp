#include "EntityManager.h"
#include <algorithm>

namespace WorldEditor {

EntityManager::EntityManager() {
    LOG_INFO("EntityManager initialized");
}

EntityManager::~EntityManager() {
    LOG_INFO("EntityManager destroyed");
}

Entity EntityManager::createEntity(const String& name) {
    Entity entity = registry_.create();
    addComponent<NameComponent>(entity, name);
    // Don't automatically add TransformComponent - let users add it when needed

    LOG_DEBUG("Created entity '{}' with ID {}", name, static_cast<u32>(entity));
    return entity;
}

void EntityManager::destroyEntity(Entity entity) {
    if (isValid(entity)) {
        auto& nameComp = getComponent<NameComponent>(entity);
        LOG_DEBUG("Destroying entity '{}' with ID {}", nameComp.name, static_cast<u32>(entity));
        registry_.destroy(entity);
    }
}

bool EntityManager::isValid(Entity entity) const {
    return registry_.valid(entity);
}

size_t EntityManager::getEntityCount() const {
    return registry_.alive();
}

void EntityManager::clear() {
    registry_.clear();
}

Vector<Entity> EntityManager::getEntitiesWithName(const String& name) const {
    Vector<Entity> result;

    auto view = registry_.view<NameComponent>();
    for (auto entity : view) {
        if (view.get<NameComponent>(entity).name == name) {
            result.push_back(entity);
        }
    }

    return result;
}

}
