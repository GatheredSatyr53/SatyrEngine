// Basic rigid-sphere physics against the scene's distance field.
//
// Bodies are integrated on the CPU. Contact with the scene uses one SDF sample per body
// (distance + normal at the body's centre, produced by SdfProbe on the GPU, one frame old):
// within a frame the field is extrapolated as the plane  d(p) ≈ d0 + dot(n, p - p0).
// Body-body collisions are plain sphere-sphere impulses.
#pragma once

#include "engine/Math.h"

#include <vector>

namespace satyr {

constexpr int kMaxBodies = 64; // must match SATYR_MAX_BODIES in the shader prelude

struct Body {
    vec3 position;
    vec3 velocity;
    float radius = 0.3f;
    float mass = 1.0f;
    float restitution = 0.5f;
};

// Scene distance field sampled at a body's centre.
struct SurfaceSample {
    vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 1e9f;
    bool valid = false;
};

class Physics {
public:
    vec3 gravity{0.0f, -9.81f, 0.0f};
    float friction = 1.5f;        // tangential velocity decay rate while touching (1/s)
    float bounceThreshold = 1.0f; // impacts slower than this (m/s) do not bounce
    int substeps = 4;
    float killBelowY = -100.0f;   // bodies falling below this are removed

    std::vector<Body>& bodies() { return m_bodies; }
    const std::vector<Body>& bodies() const { return m_bodies; }
    size_t size() const { return m_bodies.size(); }
    bool empty() const { return m_bodies.empty(); }

    bool add(const Body& body); // false when kMaxBodies is reached
    void clear() { m_bodies.clear(); }

    // `field[i]` is the sample at m_bodies[i].position taken before this step (may be shorter
    // than bodies(): bodies without a sample skip scene collision this frame).
    void step(float dt, const std::vector<SurfaceSample>& field);

    void positions(std::vector<vec3>& out) const;
    // Sphere enclosing every body; false when there are none.
    bool boundingSphere(vec3& center, float& radius) const;

private:
    void collideWithScene(Body& body, const vec3& startPos, const SurfaceSample& sample, float h);
    void collideBodies();

    std::vector<Body> m_bodies;
};

} // namespace satyr
