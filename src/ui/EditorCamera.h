#pragma once

#include "core/Types.h"

// Prevent Windows headers from defining min/max macros that break GLM.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace WorldEditor {

// Simple editor camera controller (WASD + RMB look).
// Uses left-handed view/projection (D3D-style) with depth range [0..1].
class EditorCamera {
public:
    Vec3 position = Vec3(0.0f, 0.0f, -2.0f);
    float yawDeg = 0.0f;   // around +Y
    float pitchDeg = 0.0f; // around +X

    float moveSpeed = 35.0f;
    float fastMultiplier = 4.0f;
    float mouseSensitivity = 0.15f; // degrees per pixel

    float fovDeg = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 500.0f;

    // Projection mode. Ortho is useful for map editing (no perspective skew).
    bool orthographic = false;
    // Half-height of the ortho frustum in world units (halfWidth = halfHeight * aspect).
    float orthoHalfHeight = 50.0f;

    // Map editor helper: keep camera in top-down mode to preserve "map look".
    // Even in orthographic projection, tilting the camera will skew the map (affine projection),
    // so for a true 2D map view we lock pitch near -90° and use pan/zoom.
    bool lockTopDown = false;

    void reset();

    // Update camera from Win32 input state. The caller should gate this with ImGui capture flags.
    void updateFromInput(HWND hwnd, float dt, bool enableMouseLook, bool enableKeyboardMove);

    Mat4 getViewMatrixLH() const;
    Mat4 getProjMatrixLH_ZO(float aspect) const;
    Mat4 getViewProjLH_ZO(float aspect) const;
    Vec3 getForwardLH() const;
    Vec3 getRightLH() const;
    Vec3 getUpLH() const;

private:
    bool rmbWasDown_ = false;
    POINT lastMousePos_ = {};
    POINT savedMousePos_ = {};
    bool cursorCaptured_ = false;
    bool cursorHidden_ = false;

    void beginMouseLook(HWND hwnd);
    void endMouseLook();
};

} // namespace WorldEditor


