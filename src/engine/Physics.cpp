#include "engine/Physics.h"

#include <algorithm>
#include <cmath>

namespace satyr {

bool Physics::add(const Body& body)
{
    if (m_bodies.size() >= static_cast<size_t>(kMaxBodies)) return false;
    m_bodies.push_back(body);
    ++m_version;
    return true;
}

void Physics::step(float dt, const std::vector<SurfaceSample>& field)
{
    if (m_bodies.empty() || dt <= 0.0f) return;
    ++m_version;

    const int steps = std::max(1, substeps);
    const float h = dt / static_cast<float>(steps);

    std::vector<vec3> startPos(m_bodies.size());
    for (size_t i = 0; i < m_bodies.size(); ++i) startPos[i] = m_bodies[i].position;

    for (int s = 0; s < steps; ++s) {
        for (size_t i = 0; i < m_bodies.size(); ++i) {
            Body& b = m_bodies[i];
            b.velocity += gravity * h;
            b.position += b.velocity * h;
            if (i < field.size() && field[i].valid) collideWithScene(b, startPos[i], field[i], h);
        }
        collideBodies();
    }

    // Drop bodies that fell out of the world or whose state became invalid.
    m_bodies.erase(std::remove_if(m_bodies.begin(), m_bodies.end(), [this](const Body& b) {
        const bool finite = std::isfinite(b.position.x) && std::isfinite(b.position.y) && std::isfinite(b.position.z)
                         && std::isfinite(b.velocity.x) && std::isfinite(b.velocity.y) && std::isfinite(b.velocity.z);
        return !finite || b.position.y < killBelowY;
    }), m_bodies.end());
}

void Physics::collideWithScene(Body& body, const vec3& startPos, const SurfaceSample& sample, float h)
{
    if (!std::isfinite(sample.distance)) return;

    // Signed gap between the sphere surface and the scene, using the planar extrapolation.
    const vec3 n = sample.normal;
    const float gap = sample.distance + dot(n, body.position - startPos) - body.radius;
    if (gap >= 0.0f) return;

    body.position -= n * gap; // push out to the surface

    const float vn = dot(body.velocity, n);
    if (vn >= 0.0f) return;   // already separating

    const vec3 tangential = body.velocity - n * vn;
    const float bounce = (-vn > bounceThreshold) ? body.restitution : 0.0f;
    const float tangentialScale = std::exp(-friction * h);
    body.velocity = tangential * tangentialScale - n * (vn * bounce);
}

void Physics::collideBodies()
{
    const size_t count = m_bodies.size();
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            Body& a = m_bodies[i];
            Body& b = m_bodies[j];
            const vec3 delta = b.position - a.position;
            const float dist2 = dot(delta, delta);
            const float minDist = a.radius + b.radius;
            if (dist2 >= minDist * minDist || dist2 < 1e-12f) continue;

            const float dist = std::sqrt(dist2);
            const vec3 n = delta / dist;
            const float penetration = minDist - dist;
            const float invMassA = 1.0f / a.mass;
            const float invMassB = 1.0f / b.mass;
            const float invMassSum = invMassA + invMassB;

            // Positional correction split by inverse mass.
            a.position -= n * (penetration * invMassA / invMassSum);
            b.position += n * (penetration * invMassB / invMassSum);

            // Impulse along the contact normal when approaching.
            const float relVel = dot(b.velocity - a.velocity, n);
            if (relVel >= 0.0f) continue;
            const float e = std::min(a.restitution, b.restitution);
            const float impulse = -(1.0f + e) * relVel / invMassSum;
            a.velocity -= n * (impulse * invMassA);
            b.velocity += n * (impulse * invMassB);
        }
    }
}

void Physics::positions(std::vector<vec3>& out) const
{
    out.resize(m_bodies.size());
    for (size_t i = 0; i < m_bodies.size(); ++i) out[i] = m_bodies[i].position;
}

bool Physics::boundingSphere(vec3& center, float& radius) const
{
    if (m_bodies.empty()) return false;
    vec3 sum{0.0f, 0.0f, 0.0f};
    for (const Body& b : m_bodies) sum += b.position;
    center = sum / static_cast<float>(m_bodies.size());
    radius = 0.0f;
    for (const Body& b : m_bodies) radius = std::max(radius, length(b.position - center) + b.radius);
    return true;
}

} // namespace satyr
