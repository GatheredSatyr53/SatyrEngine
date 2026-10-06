// Rigid-body state shared by the physics step, the spatial grid and the renderer upload.
#pragma once

#include "engine/Math.h"

namespace satyr {

constexpr int kMaxBodies = 512;         // bodies live in textures, so this is a CPU/probe budget
constexpr int kMaxSamplesPerBody = 27;  // Box27: corners, edge midpoints, face centres, centre

enum class Shape { Sphere, Box, Box20, Box27 };

struct Body {
    Shape shape = Shape::Sphere;
    vec3 position;
    vec3 velocity;
    quat orientation;
    vec3 angularVelocity;              // world space, radians per second
    float radius = 0.3f;               // sphere radius, or corner rounding of a box
    vec3 halfExtents{0.3f, 0.3f, 0.3f}; // box outer half extents (unused for spheres)
    float mass = 1.0f;
    float restitution = 0.5f;
    float friction = 0.5f;             // Coulomb coefficient
    vec3 invInertiaLocal{1, 1, 1};     // 1 / principal moments, body space
    int sampleOffset = 0;              // index of this body's first contact sample (see samplePoints)

    static Body makeSphere(const vec3& position, float radius, float density = 10.0f);
    // `shape` is Box, Box20 or Box27 (8 / 20 / 27 contact points); geometry is the same.
    static Body makeBox(const vec3& position, const vec3& halfExtents, Shape shape = Shape::Box,
                        float rounding = 0.05f, float density = 10.0f);

    bool isBox() const { return shape != Shape::Sphere; }
    int sampleCount() const
    {
        switch (shape) {
            case Shape::Box:   return 8;
            case Shape::Box20: return 20;
            case Shape::Box27: return 27;
            default:           return 1;
        }
    }
    float boundingRadius() const;
    mat3 rotation() const { return toMat3(orientation); }
    mat3 invInertiaWorld() const;
    // Signed distance from p to this body's surface, with the outward normal.
    float distance(const vec3& p, vec3& normal) const;
};

// Scene distance field sampled at a contact point.
struct SurfaceSample {
    vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 1e9f;
    bool valid = false;
};

} // namespace satyr
