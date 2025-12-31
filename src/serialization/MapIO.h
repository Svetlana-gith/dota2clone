#pragma once

#include "core/Types.h"

namespace WorldEditor {

class World;

namespace MapIO {

// Saves current world to JSON.
bool save(const World& world, const String& path, String* outError = nullptr);

// Loads world from JSON (clears entities first, preserves systems).
bool load(World& world, const String& path, String* outError = nullptr);

} // namespace MapIO
} // namespace WorldEditor


