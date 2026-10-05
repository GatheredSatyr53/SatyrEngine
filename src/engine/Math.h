// Tiny vector math: just enough for a fly camera. Shaders do the heavy lifting.
#pragma once

#include <cmath>

namespace satyr {

constexpr float kPi = 3.14159265358979323846f;

constexpr float radians(float degrees) { return degrees * (kPi / 180.0f); }
constexpr float degrees(float rad)     { return rad * (180.0f / kPi); }

template <typename T>
constexpr T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct vec2 {
    float x = 0, y = 0;
    constexpr vec2() = default;
    constexpr vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct vec3 {
    float x = 0, y = 0, z = 0;
    constexpr vec3() = default;
    constexpr vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    constexpr explicit vec3(float s) : x(s), y(s), z(s) {}

    constexpr vec3 operator+(const vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr vec3 operator-(const vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr vec3 operator*(float s) const       { return {x * s, y * s, z * s}; }
    constexpr vec3 operator/(float s) const       { return {x / s, y / s, z / s}; }
    constexpr vec3 operator-() const              { return {-x, -y, -z}; }
    vec3& operator+=(const vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    vec3& operator-=(const vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    vec3& operator*=(float s)       { x *= s; y *= s; z *= s; return *this; }
};

constexpr vec3 operator*(float s, const vec3& v) { return v * s; }

constexpr float dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

constexpr vec3 cross(const vec3& a, const vec3& b)
{
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline float length(const vec3& v) { return std::sqrt(dot(v, v)); }

inline vec3 normalize(const vec3& v)
{
    const float len = length(v);
    return len > 1e-8f ? v / len : vec3{0, 0, 0};
}

// Column-major 3x3 matrix, matching GLSL's mat3 memory layout.
struct mat3 {
    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    constexpr mat3() = default;
    // Build from three column vectors.
    constexpr mat3(const vec3& c0, const vec3& c1, const vec3& c2)
        : m{c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, c2.x, c2.y, c2.z} {}

    const float* data() const { return m; }
};

} // namespace satyr
