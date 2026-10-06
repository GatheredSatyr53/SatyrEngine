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

constexpr vec3 vmin(const vec3& a, const vec3& b) { return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z}; }
constexpr vec3 vmax(const vec3& a, const vec3& b) { return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z}; }
constexpr vec3 vabs(const vec3& v) { return {v.x < 0 ? -v.x : v.x, v.y < 0 ? -v.y : v.y, v.z < 0 ? -v.z : v.z}; }
constexpr vec3 vmul(const vec3& a, const vec3& b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }

// Column-major 3x3 matrix, matching GLSL's mat3 memory layout.
struct mat3 {
    float m[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

    constexpr mat3() = default;
    // Build from three column vectors.
    constexpr mat3(const vec3& c0, const vec3& c1, const vec3& c2)
        : m{c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, c2.x, c2.y, c2.z} {}

    const float* data() const { return m; }
    constexpr float at(int row, int col) const { return m[col * 3 + row]; }
    constexpr vec3 column(int c) const { return {m[c * 3], m[c * 3 + 1], m[c * 3 + 2]}; }
};

constexpr vec3 operator*(const mat3& a, const vec3& v)
{
    return {a.m[0] * v.x + a.m[3] * v.y + a.m[6] * v.z,
            a.m[1] * v.x + a.m[4] * v.y + a.m[7] * v.z,
            a.m[2] * v.x + a.m[5] * v.y + a.m[8] * v.z};
}

constexpr mat3 operator*(const mat3& a, const mat3& b)
{
    return mat3(a * b.column(0), a * b.column(1), a * b.column(2));
}

constexpr mat3 transpose(const mat3& a)
{
    return mat3({a.m[0], a.m[3], a.m[6]}, {a.m[1], a.m[4], a.m[7]}, {a.m[2], a.m[5], a.m[8]});
}

constexpr mat3 diagonal(const vec3& d)
{
    return mat3({d.x, 0, 0}, {0, d.y, 0}, {0, 0, d.z});
}

// Unit quaternion (x, y, z, w) for rigid-body orientation.
struct quat {
    float x = 0, y = 0, z = 0, w = 1;
    constexpr quat() = default;
    constexpr quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// Hamilton product: rotation b followed by a.
constexpr quat operator*(const quat& a, const quat& b)
{
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

constexpr quat conjugate(const quat& q) { return {-q.x, -q.y, -q.z, q.w}; }

inline quat normalize(const quat& q)
{
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return len > 1e-8f ? quat{q.x / len, q.y / len, q.z / len, q.w / len} : quat{};
}

inline quat quatFromAxisAngle(const vec3& axis, float angle)
{
    const vec3 a = normalize(axis);
    const float s = std::sin(0.5f * angle);
    return {a.x * s, a.y * s, a.z * s, std::cos(0.5f * angle)};
}

// Rotates v by q (q v q*).
constexpr vec3 rotate(const quat& q, const vec3& v)
{
    const vec3 u{q.x, q.y, q.z};
    const vec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

inline mat3 toMat3(const quat& q)
{
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return mat3({1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy)},
                {2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx)},
                {2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy)});
}

// Advances an orientation by a world-space angular velocity over dt.
inline quat integrate(const quat& q, const vec3& omega, float dt)
{
    const quat dq = quat{omega.x, omega.y, omega.z, 0.0f} * q;
    return normalize(quat{q.x + 0.5f * dt * dq.x, q.y + 0.5f * dt * dq.y, q.z + 0.5f * dt * dq.z, q.w + 0.5f * dt * dq.w});
}

} // namespace satyr
