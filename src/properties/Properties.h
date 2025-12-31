#pragma once

#include "core/Types.h"

#include <cstddef>
#include <unordered_map>

namespace WorldEditor::Properties {

enum class Kind : u8 {
    Float,
    Vec3,
    Color3,
};

struct Property {
    const char* name = "";
    Kind kind = Kind::Float;
    size_t offset = 0; // offsetof(Component, field)
    float minV = 0.0f;
    float maxV = 0.0f;
    float step = 0.01f;
};

struct TypeMeta {
    const char* typeName = "";
    Vector<Property> props;
};

// NOTE: This is a lightweight runtime registry for the editor.
// We keep it intentionally small: only what we need for Transform/Material now.
const TypeMeta* getTypeMeta(size_t typeId);

template<class T>
const TypeMeta* getTypeMeta() {
    return getTypeMeta(typeid(T).hash_code());
}

void registerDefaults(); // safe to call multiple times

// Helpers for reading/writing by offset (used by UI + undo/redo).
inline float* ptrFloat(void* base, size_t offset) {
    return reinterpret_cast<float*>(reinterpret_cast<u8*>(base) + offset);
}
inline Vec3* ptrVec3(void* base, size_t offset) {
    return reinterpret_cast<Vec3*>(reinterpret_cast<u8*>(base) + offset);
}

} // namespace WorldEditor::Properties


