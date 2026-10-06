#include "engine/Physics.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace satyr {

namespace {

bool finite(const vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

constexpr float sgn(float v) { return v < 0.0f ? -1.0f : 1.0f; }

// Effective inverse mass of a body for an impulse along `dir` applied at lever arm `r`.
float effectiveInvMass(const Body& b, const mat3& invInertia, const vec3& r, const vec3& dir)
{
    const vec3 rxd = cross(r, dir);
    return 1.0f / b.mass + dot(dir, cross(invInertia * rxd, r));
}

void applyImpulse(Body& b, const mat3& invInertia, const vec3& r, const vec3& impulse)
{
    b.velocity += impulse / b.mass;
    b.angularVelocity += invInertia * cross(r, impulse);
}

} // namespace

// ---- Body ------------------------------------------------------------------------------------

Body Body::makeSphere(const vec3& position, float radius, float density)
{
    Body b;
    b.shape = Shape::Sphere;
    b.position = position;
    b.radius = radius;
    b.mass = density * (4.0f / 3.0f) * kPi * radius * radius * radius;
    const float inertia = 0.4f * b.mass * radius * radius;
    b.invInertiaLocal = vec3(1.0f / inertia);
    b.sampleOffset = -1;
    return b;
}

Body Body::makeBox(const vec3& position, const vec3& halfExtents, Shape shape, float rounding, float density)
{
    Body b;
    b.shape = shape == Shape::Sphere ? Shape::Box : shape;
    b.position = position;
    b.halfExtents = halfExtents;
    const float smallest = std::min(halfExtents.x, std::min(halfExtents.y, halfExtents.z));
    b.radius = clamp(rounding, 0.0f, 0.9f * smallest);
    const float ax = 2.0f * halfExtents.x, ay = 2.0f * halfExtents.y, az = 2.0f * halfExtents.z;
    b.mass = density * ax * ay * az;
    const float k = b.mass / 12.0f;
    b.invInertiaLocal = {1.0f / (k * (ay * ay + az * az)),
                         1.0f / (k * (ax * ax + az * az)),
                         1.0f / (k * (ax * ax + ay * ay))};
    b.restitution = 0.3f;
    b.friction = 0.6f;
    b.sampleOffset = -1;
    return b;
}

float Body::boundingRadius() const
{
    return shape == Shape::Sphere ? radius : length(halfExtents);
}

mat3 Body::invInertiaWorld() const
{
    const mat3 R = rotation();
    return R * diagonal(invInertiaLocal) * transpose(R);
}

float Body::distance(const vec3& p, vec3& normal) const
{
    if (shape == Shape::Sphere) {
        const vec3 d = p - position;
        const float len = length(d);
        normal = len > 1e-6f ? d / len : vec3(0.0f, 1.0f, 0.0f);
        return len - radius;
    }

    // Rounded box: a core box shrunk by the rounding, offset outwards by it.
    const vec3 local = rotate(conjugate(orientation), p - position);
    const vec3 core = halfExtents - vec3(radius);
    const vec3 q = vabs(local) - core;
    const vec3 outside = vmax(q, vec3(0.0f));
    const float dOut = length(outside);

    vec3 nLocal;
    if (dOut > 1e-6f) {
        nLocal = normalize(vmul(outside, vec3(sgn(local.x), sgn(local.y), sgn(local.z))));
    } else {
        // Inside the core: leave through the nearest face.
        if (q.x > q.y && q.x > q.z)      nLocal = vec3(sgn(local.x), 0.0f, 0.0f);
        else if (q.y > q.z)              nLocal = vec3(0.0f, sgn(local.y), 0.0f);
        else                             nLocal = vec3(0.0f, 0.0f, sgn(local.z));
    }
    normal = rotate(orientation, nLocal);
    const float dIn = std::min(std::max(q.x, std::max(q.y, q.z)), 0.0f);
    return dOut + dIn - radius;
}

// ---- Physics ---------------------------------------------------------------------------------

bool Physics::add(const Body& body)
{
    if (m_bodies.size() >= static_cast<size_t>(kMaxBodies)) return false;
    m_bodies.push_back(body);
    m_bodies.back().sampleOffset = -1; // no field samples until the next samplePoints()
    ++m_version;
    return true;
}

void Physics::contactPoints(const Body& body, vec3* out, float* radii)
{
    const int count = body.sampleCount();
    for (int i = 0; i < count; ++i) radii[i] = body.radius;
    if (body.shape == Shape::Sphere) {
        out[0] = body.position;
        return;
    }
    const vec3 core = body.halfExtents - vec3(body.radius);
    const mat3 R = body.rotation();
    int k = 0;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sz = -1; sz <= 1; sz += 2)
                out[k++] = body.position + R * vec3(sx * core.x, sy * core.y, sz * core.z);
    if (body.shape == Shape::Box) return;

    // Edge midpoints: one coordinate zero, the other two at the corners.
    for (int s1 = -1; s1 <= 1; s1 += 2) {
        for (int s2 = -1; s2 <= 1; s2 += 2) {
            out[k++] = body.position + R * vec3(0.0f, s1 * core.y, s2 * core.z);
            out[k++] = body.position + R * vec3(s1 * core.x, 0.0f, s2 * core.z);
            out[k++] = body.position + R * vec3(s1 * core.x, s2 * core.y, 0.0f);
        }
    }
    if (body.shape != Shape::Box27) return;

    // Face centres, then the volume centre as the inscribed sphere (a tunnelling guard).
    for (int s1 = -1; s1 <= 1; s1 += 2) {
        out[k++] = body.position + R * vec3(s1 * core.x, 0.0f, 0.0f);
        out[k++] = body.position + R * vec3(0.0f, s1 * core.y, 0.0f);
        out[k++] = body.position + R * vec3(0.0f, 0.0f, s1 * core.z);
    }
    out[k] = body.position;
    radii[k] = std::min(body.halfExtents.x, std::min(body.halfExtents.y, body.halfExtents.z));
}

void Physics::samplePoints(std::vector<vec3>& out)
{
    out.clear();
    for (Body& b : m_bodies) {
        b.sampleOffset = static_cast<int>(out.size());
        vec3 pts[kMaxSamplesPerBody];
        float radii[kMaxSamplesPerBody];
        contactPoints(b, pts, radii);
        for (int i = 0; i < b.sampleCount(); ++i) out.push_back(pts[i]);
    }
}

void Physics::step(float dt, const std::vector<SurfaceSample>& field)
{
    if (m_bodies.empty() || dt <= 0.0f) return;
    ++m_version;

    const int steps = std::max(1, substeps);
    const float h = dt / static_cast<float>(steps);

    // Contact points at the start of the frame: the field samples were taken there.
    const size_t count = m_bodies.size();
    m_startPoints.resize(count * kMaxSamplesPerBody);
    for (size_t i = 0; i < count; ++i) {
        float radii[kMaxSamplesPerBody];
        contactPoints(m_bodies[i], &m_startPoints[i * kMaxSamplesPerBody], radii);
    }

    for (int s = 0; s < steps; ++s) {
        for (size_t i = 0; i < count; ++i) {
            Body& b = m_bodies[i];
            b.velocity += gravity * h;
            b.velocity *= std::exp(-linearDamping * h);
            b.angularVelocity *= std::exp(-angularDamping * h);
            b.position += b.velocity * h;
            b.orientation = integrate(b.orientation, b.angularVelocity, h);

            const bool hasSamples = b.sampleOffset >= 0
                                 && b.sampleOffset + b.sampleCount() <= static_cast<int>(field.size());
            if (hasSamples) collideWithScene(b, m_startPoints, field, h);
        }
        collideBodies();
    }

    // Drop bodies that fell out of the world or whose state became invalid.
    for (size_t i = 0; i < m_bodies.size(); ++i) m_startPoints[i] = m_bodies[i].position; // reuse as scratch
    m_bodies.erase(std::remove_if(m_bodies.begin(), m_bodies.end(), [this](const Body& b) {
        const bool ok = finite(b.position) && finite(b.velocity) && finite(b.angularVelocity)
                     && std::isfinite(b.orientation.w) && b.position.y >= killBelowY;
        return !ok;
    }), m_bodies.end());
}

void Physics::collideWithScene(Body& body, const std::vector<vec3>& startPoints, const std::vector<SurfaceSample>& field, float h)
{
    vec3 pts[kMaxSamplesPerBody];
    float radii[kMaxSamplesPerBody];
    contactPoints(body, pts, radii);
    const size_t bodyIndex = static_cast<size_t>(&body - m_bodies.data());

    Contact contacts[kMaxSamplesPerBody];
    int contactCount = 0;
    for (int k = 0; k < body.sampleCount(); ++k) {
        const SurfaceSample& s = field[static_cast<size_t>(body.sampleOffset + k)];
        if (!s.valid || !std::isfinite(s.distance)) continue;
        const vec3& start = startPoints[bodyIndex * kMaxSamplesPerBody + static_cast<size_t>(k)];
        // Planar extrapolation of the field around the sample, minus the point's own radius.
        const float gap = s.distance + dot(s.normal, pts[k] - start) - radii[k];
        if (gap < 0.0f) contacts[contactCount++] = {pts[k] - s.normal * radii[k], s.normal, -gap};
    }
    if (contactCount == 0) return;

    // One positional correction by the deepest contact, then impulses at every contact.
    int deepest = 0;
    for (int i = 1; i < contactCount; ++i)
        if (contacts[i].penetration > contacts[deepest].penetration) deepest = i;
    body.position += contacts[deepest].normal * std::max(contacts[deepest].penetration - penetrationSlop, 0.0f);

    for (int i = 0; i < contactCount; ++i) resolveSceneContact(body, contacts[i], h);
    body.angularVelocity *= std::exp(-rollingResistance * h);
}

void Physics::resolveSceneContact(Body& body, const Contact& c, float /*h*/)
{
    const mat3 invInertia = body.invInertiaWorld();
    const vec3 r = c.point - body.position;

    vec3 vc = body.velocity + cross(body.angularVelocity, r);
    const float vn = dot(vc, c.normal);
    if (vn >= 0.0f) return; // separating

    const float bounce = (-vn > bounceThreshold) ? body.restitution : 0.0f;
    const float j = -(1.0f + bounce) * vn / effectiveInvMass(body, invInertia, r, c.normal);
    applyImpulse(body, invInertia, r, c.normal * j);

    // Coulomb friction against the tangential velocity at the contact.
    vc = body.velocity + cross(body.angularVelocity, r);
    const vec3 vt = vc - c.normal * dot(vc, c.normal);
    const float speed = length(vt);
    if (speed > 1e-5f) {
        const vec3 t = vt / speed;
        float jt = -speed / effectiveInvMass(body, invInertia, r, t);
        const float maxFriction = body.friction * j;
        jt = clamp(jt, -maxFriction, maxFriction);
        applyImpulse(body, invInertia, r, t * jt);
    }
}

void Physics::collideBodies()
{
    const size_t count = m_bodies.size();
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            Body& a = m_bodies[i];
            Body& b = m_bodies[j];
            const float reach = a.boundingRadius() + b.boundingRadius();
            const vec3 d = b.position - a.position;
            if (dot(d, d) > reach * reach) continue;

            // Each body's contact points against the other's distance field.
            for (int pass = 0; pass < 2; ++pass) {
                Body& self = pass == 0 ? a : b;
                Body& other = pass == 0 ? b : a;
                vec3 pts[kMaxSamplesPerBody];
                float radii[kMaxSamplesPerBody];
                contactPoints(self, pts, radii);
                for (int k = 0; k < self.sampleCount(); ++k) {
                    vec3 n;
                    const float gap = other.distance(pts[k], n) - radii[k];
                    if (gap < 0.0f) resolvePairContact(self, other, pts[k] - n * radii[k], n, -gap);
                }
            }
        }
    }
}

void Physics::resolvePairContact(Body& a, Body& b, const vec3& point, const vec3& normal, float penetration)
{
    const mat3 invInertiaA = a.invInertiaWorld();
    const mat3 invInertiaB = b.invInertiaWorld();
    const float invMassA = 1.0f / a.mass;
    const float invMassB = 1.0f / b.mass;
    const float invMassSum = invMassA + invMassB;

    // Positional correction split by inverse mass (normal pushes a away from b).
    const float correction = std::max(penetration - penetrationSlop, 0.0f) * 0.8f;
    a.position += normal * (correction * invMassA / invMassSum);
    b.position -= normal * (correction * invMassB / invMassSum);

    const vec3 ra = point - a.position;
    const vec3 rb = point - b.position;
    vec3 vrel = (a.velocity + cross(a.angularVelocity, ra)) - (b.velocity + cross(b.angularVelocity, rb));
    const float vn = dot(vrel, normal);
    if (vn >= 0.0f) return;

    const float e = std::min(a.restitution, b.restitution);
    const float bounce = (-vn > bounceThreshold) ? e : 0.0f;
    const float invMassN = effectiveInvMass(a, invInertiaA, ra, normal) + effectiveInvMass(b, invInertiaB, rb, normal);
    const float j = -(1.0f + bounce) * vn / invMassN;
    applyImpulse(a, invInertiaA, ra, normal * j);
    applyImpulse(b, invInertiaB, rb, normal * -j);

    vrel = (a.velocity + cross(a.angularVelocity, ra)) - (b.velocity + cross(b.angularVelocity, rb));
    const vec3 vt = vrel - normal * dot(vrel, normal);
    const float speed = length(vt);
    if (speed > 1e-5f) {
        const vec3 t = vt / speed;
        const float invMassT = effectiveInvMass(a, invInertiaA, ra, t) + effectiveInvMass(b, invInertiaB, rb, t);
        float jt = -speed / invMassT;
        const float maxFriction = std::sqrt(a.friction * b.friction) * j;
        jt = clamp(jt, -maxFriction, maxFriction);
        applyImpulse(a, invInertiaA, ra, t * jt);
        applyImpulse(b, invInertiaB, rb, t * -jt);
    }
}

bool Physics::boundingSphere(vec3& center, float& radius) const
{
    if (m_bodies.empty()) return false;
    vec3 sum{0.0f, 0.0f, 0.0f};
    for (const Body& b : m_bodies) sum += b.position;
    center = sum / static_cast<float>(m_bodies.size());
    radius = 0.0f;
    for (const Body& b : m_bodies) radius = std::max(radius, length(b.position - center) + b.boundingRadius());
    return true;
}

} // namespace satyr
