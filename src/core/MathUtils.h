#pragma once

#include "Types.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace WorldEditor {
namespace Math {

// Vector operations
inline f32 length(const Vec2& v) { return glm::length(v); }
inline f32 length(const Vec3& v) { return glm::length(v); }

inline Vec2 normalize(const Vec2& v) { return glm::normalize(v); }
inline Vec3 normalize(const Vec3& v) { return glm::normalize(v); }

inline f32 dot(const Vec2& a, const Vec2& b) { return glm::dot(a, b); }
inline f32 dot(const Vec3& a, const Vec3& b) { return glm::dot(a, b); }

inline Vec3 cross(const Vec3& a, const Vec3& b) { return glm::cross(a, b); }

// Matrix operations
inline Mat4 translate(const Mat4& m, const Vec3& v) { return glm::translate(m, v); }
inline Mat4 rotate(const Mat4& m, f32 angle, const Vec3& axis) { return glm::rotate(m, angle, axis); }
inline Mat4 scaleMatrix(const Mat4& m, const Vec3& v) { return glm::scale(m, v); }

// Quaternion operations
inline Quat angleAxis(f32 angle, const Vec3& axis) { return glm::angleAxis(angle, axis); }
inline Mat4 toMat4(const Quat& q) { return glm::toMat4(q); }

// Transform utilities
struct Transform {
    Vec3 position = Vec3(0.0f);
    Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
    Vec3 scale = Vec3(1.0f);

    Mat4 getMatrix() const {
        Mat4 matrix = Mat4(1.0f);
        matrix = translate(matrix, position);
        matrix = matrix * toMat4(rotation);
        matrix = scaleMatrix(matrix, scale);
        return matrix;
    }

    Vec3 transformPoint(const Vec3& point) const {
        return Vec3(getMatrix() * Vec4(point, 1.0f));
    }

    Vec3 transformVector(const Vec3& vector) const {
        return Vec3(getMatrix() * Vec4(vector, 0.0f));
    }
};

// Interpolation
inline f32 lerp(f32 a, f32 b, f32 t) { return glm::mix(a, b, t); }
inline Vec2 lerp(const Vec2& a, const Vec2& b, f32 t) { return glm::mix(a, b, t); }
inline Vec3 lerp(const Vec3& a, const Vec3& b, f32 t) { return glm::mix(a, b, t); }

// Clamping
template<typename T>
inline T clamp(T value, T min, T max) {
    return std::max(min, std::min(max, value));
}

inline f32 clamp(f32 value, f32 min = 0.0f, f32 max = 1.0f) {
    return clamp<f32>(value, min, max);
}

// Trigonometric functions
inline f32 cos(f32 angle) { return std::cos(angle); }
inline f32 sin(f32 angle) { return std::sin(angle); }
inline f32 tan(f32 angle) { return std::tan(angle); }
inline f32 acos(f32 value) { return std::acos(value); }
inline f32 asin(f32 value) { return std::asin(value); }
inline f32 atan(f32 value) { return std::atan(value); }
inline f32 atan2(f32 y, f32 x) { return std::atan2(y, x); }

// Angle conversions
inline f32 degrees(f32 radians) { return glm::degrees(radians); }
inline f32 radians(f32 degrees) { return glm::radians(degrees); }

// Random utilities
inline f32 random(f32 min = 0.0f, f32 max = 1.0f) {
    return min + (max - min) * (static_cast<f32>(rand()) / RAND_MAX);
}

// Geometry utilities
struct Ray {
    Vec3 origin;
    Vec3 direction;

    Vec3 pointAt(f32 t) const {
        return origin + direction * t;
    }
};

struct Plane {
    Vec3 normal;
    f32 distance;

    f32 distanceToPoint(const Vec3& point) const {
        return dot(normal, point) + distance;
    }
};

struct AABB {
    Vec3 min;
    Vec3 max;

    Vec3 getCenter() const {
        return (min + max) * 0.5f;
    }

    Vec3 getSize() const {
        return max - min;
    }

    bool contains(const Vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool intersects(const AABB& other) const {
        return !(max.x < other.min.x || min.x > other.max.x ||
                 max.y < other.min.y || min.y > other.max.y ||
                 max.z < other.min.z || min.z > other.max.z);
    }
};

// Intersection tests
bool rayPlaneIntersection(const Ray& ray, const Plane& plane, f32& t);
bool rayAABBIntersection(const Ray& ray, const AABB& aabb, f32& tMin, f32& tMax);

// Projection utilities
Vec2 worldToScreen(const Vec3& worldPos, const Mat4& viewProj, const Vec2& screenSize);
Ray screenToWorldRay(const Vec2& screenPos, const Mat4& invViewProj, const Vec2& screenSize);

}
}
