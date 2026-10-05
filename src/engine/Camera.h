// Free-flying camera: WASD + mouse look. Produces the basis matrix uploaded to shaders.
#pragma once

#include "engine/Math.h"

namespace satyr {

struct InputState;

class Camera {
public:
    vec3  position{0.0f, 1.5f, 5.0f};
    float yaw   = 0.0f;   // radians, 0 = looking down -Z, increases when turning right
    float pitch = 0.0f;   // radians, positive = looking up
    float fov   = radians(60.0f); // vertical field of view
    float moveSpeed = 3.0f;       // units per second
    float lookSensitivity = 0.0025f; // radians per pixel

    // Applies keyboard movement (always) and mouse look (only when `lookActive`).
    void update(const InputState& input, float dt, bool lookActive);

    vec3 forward() const;
    vec3 right() const;
    vec3 up() const;
    // Columns: right, up, forward. Shader ray = basis * vec3(uv, focal).
    mat3 basis() const;

    void lookAt(const vec3& target);
    void reset();
};

} // namespace satyr
