#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <entt/entt.hpp>

// Common types
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

// Entity types (using EnTT)
using Entity = entt::entity;
using Registry = entt::registry;
const Entity INVALID_ENTITY = entt::null;

// Component ID type
using ComponentID = entt::id_type;

// String types
using String = std::string;
using WString = std::wstring;

// Vector types
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

using Vec2i = glm::ivec2;
using Vec3i = glm::ivec3;
using Vec4i = glm::ivec4;

using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

using Quat = glm::quat;

// Smart pointers
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Containers
template<typename T>
using Vector = std::vector<T>;

template<typename K, typename V>
using Map = std::unordered_map<K, V>;

// Result type for error handling
template<typename T>
class Result {
public:
    Result(const T& value) : success_(true), value_(value) {}
    Result(T&& value) : success_(true), value_(std::move(value)) {}
    Result(const String& error) : success_(false), error_(error) {}

    bool isSuccess() const { return success_; }
    const T& getValue() const { return value_; }
    const String& getError() const { return error_; }

private:
    bool success_;
    T value_;
    String error_;
};

// Common constants
namespace Constants {
    const f32 PI = 3.14159265359f;
    const f32 TWO_PI = 2.0f * PI;
    const f32 HALF_PI = 0.5f * PI;
    const f32 DEG_TO_RAD = PI / 180.0f;
    const f32 RAD_TO_DEG = 180.0f / PI;
}

// Global logging macros using spdlog
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARNING(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)  // Alias
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

// Logger setup
#define SETUP_LOGGER(name) auto logger = spdlog::get(name); if (!logger) { logger = spdlog::stdout_color_mt(name); } spdlog::set_default_logger(logger);
