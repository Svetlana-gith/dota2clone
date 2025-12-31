#pragma once

#include "core/Types.h"
#include "Components.h"

namespace WorldEditor {

class EntityManager {
public:
    EntityManager();
    ~EntityManager();

    // Entity management
    Entity createEntity(const String& name = "Entity");
    void destroyEntity(Entity entity);
    bool isValid(Entity entity) const;

    // Component management
    template<typename Component, typename... Args>
    Component& addComponent(Entity entity, Args&&... args) {
        return registry_.emplace<Component>(entity, std::forward<Args>(args)...);
    }

    template<typename Component>
    void removeComponent(Entity entity) {
        registry_.remove<Component>(entity);
    }

    template<typename Component>
    bool hasComponent(Entity entity) const {
        return registry_.all_of<Component>(entity);
    }

    template<typename Component>
    Component& getComponent(Entity entity) {
        return registry_.get<Component>(entity);
    }

    template<typename Component>
    const Component& getComponent(Entity entity) const {
        return registry_.get<Component>(entity);
    }

    // Batch operations
    template<typename Component, typename Func>
    void forEach(Func func) {
        auto view = registry_.view<Component>();
        for (auto entity : view) {
            func(entity, view.get<Component>(entity));
        }
    }

    template<typename Component, typename Func>
    void forEach(Func func) const {
        auto view = registry_.view<Component>();
        for (auto entity : view) {
            func(entity, view.get<Component>(entity));
        }
    }

    template<typename Component1, typename Component2, typename Func>
    void forEach(Func func) {
        auto view = registry_.view<Component1, Component2>();
        for (auto entity : view) {
            func(entity, view.get<Component1>(entity), view.get<Component2>(entity));
        }
    }

    // Utility functions
    size_t getEntityCount() const;
    Vector<Entity> getEntitiesWithName(const String& name) const;
    void clear();

    // Registry access (for advanced operations)
    Registry& getRegistry() { return registry_; }
    const Registry& getRegistry() const { return registry_; }

private:
    Registry registry_;
};

}
