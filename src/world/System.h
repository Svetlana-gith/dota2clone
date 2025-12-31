#pragma once

#include "core/Types.h"

namespace WorldEditor {

class System {
public:
    virtual ~System() = default;
    virtual void update(f32 deltaTime) = 0;
    virtual String getName() const = 0;
};

} // namespace WorldEditor
