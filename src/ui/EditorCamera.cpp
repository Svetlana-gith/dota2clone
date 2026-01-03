#include "EditorCamera.h"

#include "core/MathUtils.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace WorldEditor {

namespace {
POINT clientCenterScreen(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    POINT c{};
    c.x = (rc.left + rc.right) / 2;
    c.y = (rc.top + rc.bottom) / 2;
    ClientToScreen(hwnd, &c);
    return c;
}

RECT clientRectScreen(HWND hwnd) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    POINT tl{ rc.left, rc.top };
    POINT br{ rc.right, rc.bottom };
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &br);
    RECT out{};
    out.left = tl.x;
    out.top = tl.y;
    out.right = br.x;
    out.bottom = br.y;
    return out;
}
} // namespace

void EditorCamera::reset() {
    position = Vec3(0.0f, 50.0f, -100.0f); // Поднимаем камеру выше и дальше от terrain
    yawDeg = 0.0f;
    pitchDeg = -15.0f; // Небольшой наклон вниз для лучшего обзора terrain
    moveSpeed = 35.0f;
    fastMultiplier = 4.0f;
    mouseSensitivity = 0.15f;
    fovDeg = 60.0f;
    nearPlane = 0.1f;
    farPlane = 50000.0f; // Increased for large tile terrains (up to 16384 units)
    orthographic = false;
    orthoHalfHeight = 50.0f;
    lockTopDown = false;
    rmbWasDown_ = false;
    lastMousePos_ = {};
    savedMousePos_ = {};
    cursorCaptured_ = false;
    cursorHidden_ = false;
}

Vec3 EditorCamera::getForwardLH() const {
    // Yaw around +Y, pitch around +X. Left-handed: forward is +Z at yaw=pitch=0.
    const float yaw = glm::radians(yawDeg);
    const float pitch = glm::radians(pitchDeg);

    Vec3 f;
    f.x = std::sin(yaw) * std::cos(pitch);
    f.y = std::sin(pitch);
    f.z = std::cos(yaw) * std::cos(pitch);
    return glm::normalize(f);
}

Vec3 EditorCamera::getRightLH() const {
    // Right-handed cross for a left-handed basis: right = normalize(cross(up, forward)).
    const Vec3 up(0.0f, 1.0f, 0.0f);
    return glm::normalize(glm::cross(up, getForwardLH()));
}

Vec3 EditorCamera::getUpLH() const {
    // Up = cross(forward, right) (keeps LH basis consistent).
    return glm::normalize(glm::cross(getForwardLH(), getRightLH()));
}

Mat4 EditorCamera::getViewMatrixLH() const {
    // Map View: top-down (no skew). Even in orthographic projection, tilting the camera
    // turns a square into a parallelogram, which is undesirable for a map editor.
    if (orthographic && lockTopDown) {
        const float yaw = glm::radians(yawDeg);
        const Mat4 rotY = glm::rotate(Mat4(1.0f), yaw, Vec3(0.0f, 1.0f, 0.0f));
        const Vec3 up = Vec3(rotY * Vec4(0.0f, 0.0f, 1.0f, 0.0f)); // screen-up along +Z (rotated by yaw)
        const Vec3 fwd = Vec3(0.0f, -1.0f, 0.0f);                  // look straight down
        return glm::lookAtLH(position, position + fwd, up);
    }

    const Vec3 fwd = getForwardLH();
    return glm::lookAtLH(position, position + fwd, Vec3(0.0f, 1.0f, 0.0f));
}

Mat4 EditorCamera::getProjMatrixLH_ZO(float aspect) const {
    if (orthographic) {
        const float halfH = std::max(0.01f, orthoHalfHeight);
        const float halfW = halfH * std::max(0.01f, aspect);
        return glm::orthoLH_ZO(-halfW, halfW, -halfH, halfH, nearPlane, farPlane);
    }
    return glm::perspectiveLH_ZO(glm::radians(fovDeg), aspect, nearPlane, farPlane);
}

Mat4 EditorCamera::getViewProjLH_ZO(float aspect) const {
    return getProjMatrixLH_ZO(aspect) * getViewMatrixLH();
}

void EditorCamera::updateFromInput(HWND hwnd, float dt, bool enableMouseLook, bool enableKeyboardMove) {
    (void)hwnd;

    const bool rmbDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const float speed = moveSpeed * (shiftDown ? fastMultiplier : 1.0f);
    const bool mapView = orthographic && lockTopDown;

    // Mouse look
    if (enableMouseLook && rmbDown) {
        if (!rmbWasDown_) {
            beginMouseLook(hwnd);
            rmbWasDown_ = true;
            lastMousePos_ = clientCenterScreen(hwnd);
            SetCursorPos(lastMousePos_.x, lastMousePos_.y);
        }

        POINT p{};
        GetCursorPos(&p);
        const LONG dx = p.x - lastMousePos_.x;
        const LONG dy = p.y - lastMousePos_.y;

        // Re-center cursor every frame to allow infinite rotation.
        lastMousePos_ = clientCenterScreen(hwnd);
        SetCursorPos(lastMousePos_.x, lastMousePos_.y);

        yawDeg += float(dx) * mouseSensitivity;
        if (!mapView) {
            pitchDeg -= float(dy) * mouseSensitivity;
            pitchDeg = std::clamp(pitchDeg, -89.0f, 89.0f);
        }
    } else {
        if (rmbWasDown_) {
            endMouseLook();
        }
        rmbWasDown_ = false;
    }

    if (!enableKeyboardMove) {
        return;
    }

    if (mapView) {
        // Pan on XZ plane; Q/E zoom (instead of vertical movement).
        const float yaw = glm::radians(yawDeg);
        const Mat4 rotY = glm::rotate(Mat4(1.0f), yaw, Vec3(0.0f, 1.0f, 0.0f));
        const Vec3 panRight = Vec3(rotY * Vec4(1.0f, 0.0f, 0.0f, 0.0f));
        const Vec3 panUp = Vec3(rotY * Vec4(0.0f, 0.0f, 1.0f, 0.0f)); // screen-up on map is +Z

        Vec3 pan(0.0f);
        if (GetAsyncKeyState('W') & 0x8000) pan += panUp;
        if (GetAsyncKeyState('S') & 0x8000) pan -= panUp;
        if (GetAsyncKeyState('D') & 0x8000) pan += panRight;
        if (GetAsyncKeyState('A') & 0x8000) pan -= panRight;

        const float panLen = glm::length(pan);
        if (panLen > 0.0001f) {
            position += (pan / panLen) * speed * dt;
        }

        float zoomDir = 0.0f;
        if (GetAsyncKeyState('E') & 0x8000) zoomDir -= 1.0f; // zoom in
        if (GetAsyncKeyState('Q') & 0x8000) zoomDir += 1.0f; // zoom out
        if (std::abs(zoomDir) > 0.0f) {
            // Zoom speed in world units per second.
            const float zSpeed = speed * 5.0f;
            orthoHalfHeight = (std::max)(1.0f, orthoHalfHeight + zoomDir * zSpeed * dt);
        }
    } else {
        Vec3 move(0.0f);
        if (GetAsyncKeyState('W') & 0x8000) move += getForwardLH();
        if (GetAsyncKeyState('S') & 0x8000) move -= getForwardLH();
        if (GetAsyncKeyState('D') & 0x8000) move += getRightLH();
        if (GetAsyncKeyState('A') & 0x8000) move -= getRightLH();
        if (GetAsyncKeyState('E') & 0x8000) move += Vec3(0.0f, 1.0f, 0.0f);
        if (GetAsyncKeyState('Q') & 0x8000) move -= Vec3(0.0f, 1.0f, 0.0f);

        const float len = glm::length(move);
        if (len > 0.0001f) {
            position += (move / len) * speed * dt;
        }
    }
}

void EditorCamera::beginMouseLook(HWND hwnd) {
    if (cursorCaptured_) {
        return;
    }

    // Save current cursor position so we can restore it when exiting mouse-look.
    GetCursorPos(&savedMousePos_);

    // Capture mouse input and clip cursor to the client rect.
    SetCapture(hwnd);
    cursorCaptured_ = true;
    const RECT clip = clientRectScreen(hwnd);
    ClipCursor(&clip);

    // Hide cursor (ShowCursor is a counter; loop until hidden).
    while (ShowCursor(FALSE) >= 0) {}
    cursorHidden_ = true;
}

void EditorCamera::endMouseLook() {
    if (!cursorCaptured_) {
        return;
    }

    ClipCursor(nullptr);
    ReleaseCapture();
    cursorCaptured_ = false;

    if (cursorHidden_) {
        while (ShowCursor(TRUE) < 0) {}
        cursorHidden_ = false;
    }

    // Restore cursor to where it was before entering mouse-look.
    SetCursorPos(savedMousePos_.x, savedMousePos_.y);
}

} // namespace WorldEditor


