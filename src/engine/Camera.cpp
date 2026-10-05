#include "engine/Camera.h"
#include "engine/Window.h"

#include <GLFW/glfw3.h>

namespace satyr {

namespace {
constexpr vec3 kWorldUp{0.0f, 1.0f, 0.0f};
constexpr float kMaxPitch = radians(89.0f);
} // namespace

void Camera::update(const InputState& input, float dt, bool lookActive)
{
    if (lookActive) {
        yaw   += static_cast<float>(input.mouseDX) * lookSensitivity;
        pitch -= static_cast<float>(input.mouseDY) * lookSensitivity;
        pitch = clamp(pitch, -kMaxPitch, kMaxPitch);
        // Keep yaw in [-pi, pi] so it never grows without bound.
        if (yaw > kPi)  yaw -= 2.0f * kPi;
        if (yaw < -kPi) yaw += 2.0f * kPi;
    }

    // Mouse wheel scales movement speed so you can fly through both huge and tiny scenes.
    if (input.scrollY != 0.0) {
        moveSpeed *= std::pow(1.25f, static_cast<float>(input.scrollY));
        moveSpeed = clamp(moveSpeed, 0.01f, 1000.0f);
    }

    vec3 move{0, 0, 0};
    const vec3 fwd = forward();
    const vec3 rgt = right();
    if (input.key(GLFW_KEY_W)) move += fwd;
    if (input.key(GLFW_KEY_S)) move -= fwd;
    if (input.key(GLFW_KEY_D)) move += rgt;
    if (input.key(GLFW_KEY_A)) move -= rgt;
    if (input.key(GLFW_KEY_E) || input.key(GLFW_KEY_SPACE))        move += kWorldUp;
    if (input.key(GLFW_KEY_Q) || input.key(GLFW_KEY_LEFT_CONTROL)) move -= kWorldUp;

    if (dot(move, move) > 0.0f) {
        float speed = moveSpeed;
        if (input.key(GLFW_KEY_LEFT_SHIFT) || input.key(GLFW_KEY_RIGHT_SHIFT)) speed *= 4.0f;
        if (input.key(GLFW_KEY_LEFT_ALT)) speed *= 0.25f;
        position += normalize(move) * (speed * dt);
    }
}

vec3 Camera::forward() const
{
    const float cp = std::cos(pitch);
    return {std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
}

vec3 Camera::right() const
{
    // Right-handed world: looking down -Z, +X is to the right.
    return normalize(cross(forward(), kWorldUp));
}

vec3 Camera::up() const { return cross(right(), forward()); }

mat3 Camera::basis() const { return mat3(right(), up(), forward()); }

void Camera::lookAt(const vec3& target)
{
    const vec3 d = normalize(target - position);
    pitch = clamp(std::asin(clamp(d.y, -1.0f, 1.0f)), -kMaxPitch, kMaxPitch);
    yaw   = std::atan2(d.x, -d.z);
}

void Camera::reset()
{
    position = {0.0f, 1.5f, 5.0f};
    yaw = 0.0f;
    pitch = 0.0f;
    fov = radians(60.0f);
    lookAt({0.0f, 0.5f, 0.0f});
}

} // namespace satyr
