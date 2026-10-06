// Basic rigid-body physics (spheres and boxes) against the scene's distance field.
//
// Bodies are integrated on the CPU with orientation and angular velocity. Contact with the scene
// uses one SDF sample per contact point, produced by SdfProbe on the GPU one frame old: within a
// frame the field is extrapolated as the plane  d(p) ≈ d0 + dot(n, p - p0)  around each sample.
// Contacts are resolved with sequential impulses (restitution, Coulomb friction) so boxes tumble
// and spheres roll. Body-body collisions test each body's contact points against the other
// body's analytic SDF.
//
// Contact points per shape, much like element orders in FEA bricks:
//   Sphere  1 point, the centre (with the radius as contact offset)
//   Box     8 points, the rounded corners (the "linear" 8-node brick): cheap, but a ledge
//           narrower than the corner spacing or an edge-on-edge crossing goes unnoticed
//   Box20   8 corners + 12 edge midpoints (the "quadratic" 20-node brick): catches those
#pragma once

#include "engine/Math.h"

#include <vector>

namespace satyr {

constexpr int kMaxBodies = 64;          // must match SATYR_MAX_BODIES in the shader prelude
constexpr int kMaxSamplesPerBody = 20;  // Box20: corners + edge midpoints

enum class Shape { Sphere, Box, Box20 };

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
    // `shape` is Box (8 contact points) or Box20 (20 contact points); geometry is the same.
    static Body makeBox(const vec3& position, const vec3& halfExtents, Shape shape = Shape::Box,
                        float rounding = 0.05f, float density = 10.0f);

    bool isBox() const { return shape != Shape::Sphere; }
    int sampleCount() const { return shape == Shape::Box20 ? 20 : (shape == Shape::Box ? 8 : 1); }
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

class Physics {
public:
    vec3 gravity{0.0f, -9.81f, 0.0f};
    float bounceThreshold = 1.0f;    // impacts slower than this (m/s) do not bounce
    float linearDamping = 0.02f;     // 1/s
    float angularDamping = 0.3f;     // 1/s
    float rollingResistance = 1.0f;  // extra angular damping while touching the scene, 1/s
    float penetrationSlop = 0.003f;  // tolerated overlap before positional correction
    int substeps = 4;
    float killBelowY = -100.0f;      // bodies falling below this are removed

    std::vector<Body>& bodies() { return m_bodies; }
    const std::vector<Body>& bodies() const { return m_bodies; }
    size_t size() const { return m_bodies.size(); }
    bool empty() const { return m_bodies.empty(); }

    bool add(const Body& body); // false when kMaxBodies is reached
    void clear() { m_bodies.clear(); ++m_version; }

    // Increments whenever bodies are added, removed or moved (used to reset frame accumulation).
    unsigned long long version() const { return m_version; }

    // World-space contact points to sample the scene at (sphere centres, box corners), in the
    // order step() expects in `field`; also records each body's sampleOffset.
    void samplePoints(std::vector<vec3>& out);

    // `field[i]` is the sample at the i-th point of the last samplePoints() call, taken before
    // this step. Bodies whose samples are missing skip scene collision this frame.
    void step(float dt, const std::vector<SurfaceSample>& field);

    // Sphere enclosing every body; false when there are none.
    bool boundingSphere(vec3& center, float& radius) const;

private:
    struct Contact {
        vec3 point;        // world-space contact point on the body surface
        vec3 normal;       // pushes the body out
        float penetration; // > 0
    };

    static void contactPoints(const Body& body, vec3* out, float& pointRadius);
    void collideWithScene(Body& body, const std::vector<vec3>& startPoints, const std::vector<SurfaceSample>& field, float h);
    void collideBodies();
    void resolveSceneContact(Body& body, const Contact& c, float h);
    void resolvePairContact(Body& a, Body& b, const vec3& point, const vec3& normal, float penetration);

    std::vector<Body> m_bodies;
    std::vector<vec3> m_startPoints;
    unsigned long long m_version = 0;
};

} // namespace satyr
