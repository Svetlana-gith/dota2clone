#include "MapIO.h"

#include "world/World.h"
#include "world/Components.h"
#include "world/TerrainMesh.h"
#include "world/TerrainTools.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace WorldEditor::MapIO {

namespace {

using json = nlohmann::json;

static constexpr int kMapVersion = 1;

json vec3ToJson(const Vec3& v) { return json::array({ v.x, v.y, v.z }); }
json vec2ToJson(const Vec2& v) { return json::array({ v.x, v.y }); }
json quatToJson(const Quat& q) { return json::array({ q.w, q.x, q.y, q.z }); }

bool jsonToVec3(const json& j, Vec3& out) {
    if (!j.is_array() || j.size() != 3) return false;
    out = Vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    return true;
}
bool jsonToVec2(const json& j, Vec2& out) {
    if (!j.is_array() || j.size() != 2) return false;
    out = Vec2(j[0].get<float>(), j[1].get<float>());
    return true;
}
bool jsonToQuat(const json& j, Quat& out) {
    if (!j.is_array() || j.size() != 4) return false;
    out = Quat(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    return true;
}

json meshToJson(const MeshComponent& m, const std::unordered_map<Entity, int>& ids) {
    json j;
    j["name"] = m.name;
    j["visible"] = m.visible;
    j["material"] = (m.materialEntity != INVALID_ENTITY && ids.count(m.materialEntity)) ? ids.at(m.materialEntity) : -1;

    // Keep it optional: meshes may be huge; this is OK for now for a prototype.
    json verts = json::array();
    for (const auto& v : m.vertices) verts.push_back(vec3ToJson(v));
    j["vertices"] = std::move(verts);

    json norms = json::array();
    for (const auto& n : m.normals) norms.push_back(vec3ToJson(n));
    j["normals"] = std::move(norms);

    json uvs = json::array();
    for (const auto& uv : m.texCoords) uvs.push_back(vec2ToJson(uv));
    j["texCoords"] = std::move(uvs);

    j["indices"] = m.indices;
    return j;
}

bool jsonToMesh(const json& j, MeshComponent& m) {
    if (j.contains("name")) m.name = j["name"].get<String>();
    if (j.contains("visible")) m.visible = j["visible"].get<bool>();

    if (j.contains("vertices") && j["vertices"].is_array()) {
        m.vertices.clear();
        for (auto& v : j["vertices"]) {
            Vec3 out{};
            if (jsonToVec3(v, out)) m.vertices.push_back(out);
        }
    }
    if (j.contains("normals") && j["normals"].is_array()) {
        m.normals.clear();
        for (auto& n : j["normals"]) {
            Vec3 out{};
            if (jsonToVec3(n, out)) m.normals.push_back(out);
        }
    }
    if (j.contains("texCoords") && j["texCoords"].is_array()) {
        m.texCoords.clear();
        for (auto& uv : j["texCoords"]) {
            Vec2 out{};
            if (jsonToVec2(uv, out)) m.texCoords.push_back(out);
        }
    }
    if (j.contains("indices") && j["indices"].is_array()) {
        m.indices = j["indices"].get<Vector<u32>>();
    }
    return true;
}

json materialToJson(const MaterialComponent& m) {
    json j;
    j["name"] = m.name;
    j["baseColor"] = vec3ToJson(m.baseColor);
    j["metallic"] = m.metallic;
    j["roughness"] = m.roughness;
    j["emissiveColor"] = vec3ToJson(m.emissiveColor);
    j["baseColorTexture"] = m.baseColorTexture;
    j["normalTexture"] = m.normalTexture;
    j["metallicRoughnessTexture"] = m.metallicRoughnessTexture;
    j["emissiveTexture"] = m.emissiveTexture;
    return j;
}

bool jsonToMaterial(const json& j, MaterialComponent& m) {
    if (j.contains("name")) m.name = j["name"].get<String>();
    if (j.contains("baseColor")) jsonToVec3(j["baseColor"], m.baseColor);
    if (j.contains("metallic")) m.metallic = j["metallic"].get<float>();
    if (j.contains("roughness")) m.roughness = j["roughness"].get<float>();
    if (j.contains("emissiveColor")) jsonToVec3(j["emissiveColor"], m.emissiveColor);
    if (j.contains("baseColorTexture")) m.baseColorTexture = j["baseColorTexture"].get<String>();
    if (j.contains("normalTexture")) m.normalTexture = j["normalTexture"].get<String>();
    if (j.contains("metallicRoughnessTexture")) m.metallicRoughnessTexture = j["metallicRoughnessTexture"].get<String>();
    if (j.contains("emissiveTexture")) m.emissiveTexture = j["emissiveTexture"].get<String>();
    m.gpuBufferCreated = false;
    return true;
}

json transformToJson(const TransformComponent& t) {
    json j;
    j["position"] = vec3ToJson(t.position);
    j["rotation"] = quatToJson(t.rotation);
    j["scale"] = vec3ToJson(t.scale);
    return j;
}

bool jsonToTransform(const json& j, TransformComponent& t) {
    if (j.contains("position")) jsonToVec3(j["position"], t.position);
    if (j.contains("rotation")) jsonToQuat(j["rotation"], t.rotation);
    if (j.contains("scale")) jsonToVec3(j["scale"], t.scale);
    return true;
}

json terrainToJson(const TerrainComponent& t) {
    json j;
    j["resolution"] = json::array({ t.resolution.x, t.resolution.y });
    j["size"] = t.size;
    j["minHeight"] = t.minHeight;
    j["maxHeight"] = t.maxHeight;

    // Tile terrain (always tile-based now)
    j["tileSize"] = t.tileSize;
    j["heightStep"] = t.heightStep;
    j["tilesX"] = t.tilesX;
    j["tilesZ"] = t.tilesZ;
    if (!t.heightLevels.empty()) {
        j["heightLevels"] = t.heightLevels;
    } else {
        // Fallback for older saves / partially-initialized tile terrain.
        j["heightmap"] = t.heightmap;
    }
    if (!t.rampMask.empty()) {
        j["rampMask"] = t.rampMask;
    }
    return j;
}

bool jsonToTerrain(const json& j, TerrainComponent& t) {
    if (j.contains("resolution") && j["resolution"].is_array() && j["resolution"].size() == 2) {
        t.resolution.x = j["resolution"][0].get<int>();
        t.resolution.y = j["resolution"][1].get<int>();
    }
    if (j.contains("size")) t.size = j["size"].get<float>();
    if (j.contains("minHeight")) t.minHeight = j["minHeight"].get<float>();
    if (j.contains("maxHeight")) t.maxHeight = j["maxHeight"].get<float>();

    // Always tile-based terrain now
    if (j.contains("tileSize")) t.tileSize = j["tileSize"].get<float>();
    if (j.contains("heightStep")) t.heightStep = j["heightStep"].get<float>();
    if (j.contains("tilesX")) t.tilesX = j["tilesX"].get<i32>();
    if (j.contains("tilesZ")) t.tilesZ = j["tilesZ"].get<i32>();

    // If tilesX/Z are present, prefer deriving the terrain dimensions from them.
    if (t.tilesX > 0 && t.tilesZ > 0) {
        t.resolution = Vec2i(t.tilesX + 1, t.tilesZ + 1);
        // Keep square size assumption (matches TerrainMesh).
        t.size = static_cast<f32>(t.tilesX) * std::max(1.0f, t.tileSize);
    }

    // Ensure buffers exist before filling.
    TerrainMesh::ensureHeightmap(t);

    if (j.contains("heightLevels") && j["heightLevels"].is_array()) {
        t.heightLevels = j["heightLevels"].get<Vector<i16>>();
    } else if (j.contains("heightmap") && j["heightmap"].is_array()) {
        // Back-compat: derive levels from float heightmap if only that exists.
        t.heightmap = j["heightmap"].get<Vector<f32>>();
        const int w = std::max(2, t.resolution.x);
        const int h = std::max(2, t.resolution.y);
        const size_t wanted = static_cast<size_t>(w) * static_cast<size_t>(h);
        t.heightLevels.assign(wanted, static_cast<i16>(0));
        const float step = std::max(1.0f, t.heightStep);
        for (size_t i = 0; i < wanted && i < t.heightmap.size(); ++i) {
            t.heightLevels[i] = static_cast<i16>(std::lround(t.heightmap[i] / step));
        }
    } else {
        // Empty tile terrain: initialize to flat.
        const int w = std::max(2, t.resolution.x);
        const int h = std::max(2, t.resolution.y);
        t.heightLevels.assign(static_cast<size_t>(w) * static_cast<size_t>(h), static_cast<i16>(0));
    }

    if (j.contains("rampMask") && j["rampMask"].is_array()) {
        t.rampMask = j["rampMask"].get<Vector<u8>>();
    }

    // Rebuild float heightmap from discrete levels (also clamps to min/max).
    WorldEditor::TerrainTools::syncHeightmapFromLevels(t);
    return true;
}

json healthToJson(const HealthComponent& h) {
    json j;
    j["maxHealth"] = h.maxHealth;
    j["currentHealth"] = h.currentHealth;
    j["armor"] = h.armor;
    j["magicResistance"] = h.magicResistance;
    j["isDead"] = h.isDead;
    return j;
}

bool jsonToHealth(const json& j, HealthComponent& h) {
    if (j.contains("maxHealth")) h.maxHealth = j["maxHealth"].get<f32>();
    if (j.contains("currentHealth")) h.currentHealth = j["currentHealth"].get<f32>();
    if (j.contains("armor")) h.armor = j["armor"].get<f32>();
    if (j.contains("magicResistance")) h.magicResistance = j["magicResistance"].get<f32>();
    if (j.contains("isDead")) h.isDead = j["isDead"].get<bool>();
    return true;
}

json objectToJson(const ObjectComponent& o) {
    json j;
    j["type"] = static_cast<int>(o.type);
    j["assetPath"] = o.assetPath;
    j["layerName"] = o.layerName;
    j["isStatic"] = o.isStatic;
    j["teamId"] = o.teamId;
    j["spawnRadius"] = o.spawnRadius;
    j["maxUnits"] = o.maxUnits;
    j["spawnLane"] = o.spawnLane;
    j["attackRange"] = o.attackRange;
    j["attackDamage"] = o.attackDamage;
    j["attackSpeed"] = o.attackSpeed;
    j["waypointOrder"] = o.waypointOrder;
    j["waypointLane"] = o.waypointLane;
    if (!o.customData.empty()) {
        j["customData"] = o.customData;
    }
    return j;
}

bool jsonToObject(const json& j, ObjectComponent& o) {
    if (j.contains("type")) o.type = static_cast<ObjectType>(j["type"].get<int>());
    if (j.contains("assetPath")) o.assetPath = j["assetPath"].get<String>();
    if (j.contains("layerName")) o.layerName = j["layerName"].get<String>();
    if (j.contains("isStatic")) o.isStatic = j["isStatic"].get<bool>();
    if (j.contains("teamId")) o.teamId = j["teamId"].get<int>();
    if (j.contains("spawnRadius")) o.spawnRadius = j["spawnRadius"].get<float>();
    if (j.contains("maxUnits")) o.maxUnits = j["maxUnits"].get<int>();
    if (j.contains("spawnLane")) o.spawnLane = j["spawnLane"].get<i32>();
    if (j.contains("attackRange")) o.attackRange = j["attackRange"].get<float>();
    if (j.contains("attackDamage")) o.attackDamage = j["attackDamage"].get<float>();
    if (j.contains("attackSpeed")) o.attackSpeed = j["attackSpeed"].get<float>();
    if (j.contains("waypointOrder")) o.waypointOrder = j["waypointOrder"].get<i32>();
    if (j.contains("waypointLane")) o.waypointLane = j["waypointLane"].get<i32>();
    if (j.contains("customData")) o.customData = j["customData"].get<String>();
    return true;
}

} // namespace

bool save(const World& world, const String& path, String* outError) {
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        const auto& em = world.getEntityManager();
        const auto& reg = em.getRegistry();

        // Assign stable map IDs
        std::unordered_map<Entity, int> ids;
        ids.reserve(static_cast<size_t>(reg.alive()));
        int nextId = 0;
        auto viewNames = reg.view<const NameComponent>();
        for (auto e : viewNames) {
            ids[e] = nextId++;
        }

        json root;
        root["version"] = kMapVersion;
        root["entities"] = json::array();

        for (auto e : viewNames) {
            json ent;
            ent["id"] = ids[e];
            ent["name"] = viewNames.get<const NameComponent>(e).name;

            json comps;
            if (reg.any_of<const TransformComponent>(e)) {
                comps["transform"] = transformToJson(reg.get<const TransformComponent>(e));
            }
            if (reg.any_of<const TerrainComponent>(e)) {
                comps["terrain"] = terrainToJson(reg.get<const TerrainComponent>(e));
            }
            if (reg.any_of<const MaterialComponent>(e)) {
                comps["material"] = materialToJson(reg.get<const MaterialComponent>(e));
            }
            if (reg.any_of<const ObjectComponent>(e)) {
                comps["object"] = objectToJson(reg.get<const ObjectComponent>(e));
            }
            if (reg.any_of<const HealthComponent>(e)) {
                comps["health"] = healthToJson(reg.get<const HealthComponent>(e));
            }
            // Terrain mesh is derived from heightmap; don't serialize mesh payload for terrain entities.
            if (!reg.any_of<const TerrainComponent>(e) && reg.any_of<const MeshComponent>(e)) {
                comps["mesh"] = meshToJson(reg.get<const MeshComponent>(e), ids);
            }

            ent["components"] = std::move(comps);
            root["entities"].push_back(std::move(ent));
        }

        std::ofstream f(path, std::ios::binary);
        if (!f) {
            if (outError) *outError = "Failed to open file for writing";
            return false;
        }
        f << root.dump(2);
        return true;
    } catch (const std::exception& e) {
        if (outError) *outError = e.what();
        return false;
    }
}

bool load(World& world, const String& path, String* outError) {
    try {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            if (outError) *outError = "Failed to open file for reading";
            return false;
        }

        json root;
        f >> root;

        const int version = root.value("version", 0);
        if (version != kMapVersion) {
            if (outError) *outError = "Unsupported map version";
            return false;
        }

        if (!root.contains("entities") || !root["entities"].is_array()) {
            if (outError) *outError = "Invalid map format (missing entities)";
            return false;
        }

        world.clearEntities();

        // Create entities first and build id->Entity map
        std::unordered_map<int, Entity> idToEntity;
        for (auto& ent : root["entities"]) {
            const int id = ent.value("id", -1);
            const String name = ent.value("name", "Entity");
            Entity e = world.createEntity(name);
            idToEntity[id] = e;
        }

        // Populate components
        for (auto& ent : root["entities"]) {
            const int id = ent.value("id", -1);
            auto itE = idToEntity.find(id);
            if (itE == idToEntity.end()) continue;
            Entity e = itE->second;

            if (!ent.contains("components")) continue;
            auto& comps = ent["components"];

            if (comps.contains("transform")) {
                auto& t = world.addComponent<TransformComponent>(e);
                jsonToTransform(comps["transform"], t);
            }
            if (comps.contains("terrain")) {
                auto& t = world.addComponent<TerrainComponent>(e);
                jsonToTerrain(comps["terrain"], t);
            }
            if (comps.contains("material")) {
                auto& m = world.addComponent<MaterialComponent>(e);
                jsonToMaterial(comps["material"], m);
            }
            if (comps.contains("object")) {
                auto& o = world.addComponent<ObjectComponent>(e);
                jsonToObject(comps["object"], o);
            }
            if (comps.contains("health")) {
                auto& h = world.addComponent<HealthComponent>(e);
                jsonToHealth(comps["health"], h);
            }
            if (comps.contains("mesh")) {
                auto& m = world.addComponent<MeshComponent>(e);
                jsonToMesh(comps["mesh"], m);
                const int matId = comps["mesh"].value("material", -1);
                if (matId >= 0 && idToEntity.count(matId)) {
                    m.materialEntity = idToEntity[matId];
                }
            }

            // Rebuild terrain mesh if needed.
            if (world.hasComponent<TerrainComponent>(e)) {
                if (!world.hasComponent<MeshComponent>(e)) {
                    auto& m = world.addComponent<MeshComponent>(e);
                    m.name = "Terrain";
                }
                auto& t = world.getComponent<TerrainComponent>(e);
                auto& m = world.getComponent<MeshComponent>(e);
                TerrainMesh::buildMesh(t, m);

                if (m.materialEntity == INVALID_ENTITY) {
                    Entity matE = world.createEntity("TerrainMaterial");
                    auto& mat = world.addComponent<MaterialComponent>(matE);
                    mat.name = "TerrainMaterial";
                    mat.baseColor = Vec3(0.25f, 0.6f, 0.25f);
                    mat.gpuBufferCreated = false;
                    m.materialEntity = matE;
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        if (outError) *outError = e.what();
        return false;
    }
}

} // namespace WorldEditor::MapIO


