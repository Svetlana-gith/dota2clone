#include "Properties.h"

#include "world/Components.h"

namespace WorldEditor::Properties {

namespace {

std::unordered_map<size_t, TypeMeta>& registry() {
    static std::unordered_map<size_t, TypeMeta> r;
    return r;
}

template<class T>
void regType(const char* name, Vector<Property> props) {
    registry()[typeid(T).hash_code()] = TypeMeta{ name, std::move(props) };
}

} // namespace

const TypeMeta* getTypeMeta(size_t typeId) {
    auto& r = registry();
    auto it = r.find(typeId);
    return (it == r.end()) ? nullptr : &it->second;
}

void registerDefaults() {
    // Idempotent: if already registered, do nothing.
    if (getTypeMeta<TransformComponent>() != nullptr) {
        return;
    }

    regType<TransformComponent>("Transform", {
        Property{ "Position", Kind::Vec3, offsetof(TransformComponent, position), 0.0f, 0.0f, 0.05f },
        Property{ "Scale",    Kind::Vec3, offsetof(TransformComponent, scale),    0.0f, 0.0f, 0.02f },
        // Rotation is intentionally handled manually as Euler degrees in UI (quat storage).
    });

    regType<MaterialComponent>("Material", {
        Property{ "BaseColor", Kind::Color3, offsetof(MaterialComponent, baseColor), 0.0f, 0.0f, 0.0f },
        Property{ "Metallic",  Kind::Float,  offsetof(MaterialComponent, metallic),  0.0f, 1.0f, 0.01f },
        Property{ "Roughness", Kind::Float,  offsetof(MaterialComponent, roughness), 0.0f, 1.0f, 0.01f },
        Property{ "Emissive",  Kind::Color3, offsetof(MaterialComponent, emissiveColor), 0.0f, 0.0f, 0.0f },
    });
}

} // namespace WorldEditor::Properties


