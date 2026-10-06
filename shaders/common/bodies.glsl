// Dynamic bodies: spheres and boxes simulated on the CPU (src/engine/Physics.cpp) and merged
// into the scene here. Add  res = opU(res, sdBodies(p));  to map() and colour hits with
// bodyColor().
//
// Throw a ball with B, a box with N, drop a handful with G, clear with X. The physics probe
// samples map() with SATYR_QUERY_PASS defined, where sdBodies() returns nothing so bodies
// never collide with themselves.
#pragma once
#include "camera.glsl"

#ifndef SATYR_MAX_BODIES
#define SATYR_MAX_BODIES 64
#endif
// How far outside the bodies' bounding sphere the exact per-body distance is still evaluated.
// Beyond it only the distance to the bounding sphere is returned, which is a lower bound and
// therefore fine for marching, but everything that reads map() as "how close is the nearest
// surface" would see the bounding sphere as an object: soft shadows (k * h / t darkens for
// h < t / k), ambient occlusion and the cone AA band. Two units keeps the bound above t / k for
// the usual k = 16 and shadow length 20; raise it for softer or longer shadows.
#ifndef BODIES_MARGIN
#define BODIES_MARGIN 2.0
#endif

uniform vec4 uBodies[SATYR_MAX_BODIES];  // xyz centre, w sphere radius or box corner rounding
uniform vec4 uBodyRot[SATYR_MAX_BODIES]; // orientation quaternion (x, y, z, w)
uniform vec4 uBodyExt[SATYR_MAX_BODIES]; // xyz box outer half extents, w: 0 sphere, 1 box, 2 box with 20 contact points
uniform int  uBodyCount;
uniform vec4 uBodyBounds;                // sphere enclosing all bodies (xyz centre, w radius)

const float MAT_BODY = 1000.0;          // material id = MAT_BODY + body index

bool isBody(float mat) { return mat >= MAT_BODY - 0.5; }
int bodyIndex(float mat) { return int(mat - MAT_BODY + 0.5); }

vec3 bodyColor(int i)
{
    float t = fract(float(i) * 0.618034);
    return 0.55 + 0.45 * cos(6.2831 * (t + vec3(0.0, 0.33, 0.67)));
}

bool isBoxBody(int i) { return uBodyExt[i].w > 0.5; }
int bodyKind(int i) { return int(uBodyExt[i].w + 0.5); } // 0 sphere, 1 box, 2 box with edge contacts

// Rotates v by the conjugate of q: world space into the body's local frame.
vec3 bodyLocal(int i, vec3 v)
{
    vec4 q = uBodyRot[i];
    vec3 u = -q.xyz;
    vec3 t = 2.0 * cross(u, v);
    return v + q.w * t + cross(u, t);
}

// Signed distance to body i: a sphere, or a rounded box in its own orientation.
float sdBody(int i, vec3 p)
{
    vec4 b = uBodies[i];
    if (!isBoxBody(i)) return length(p - b.xyz) - b.w;
    vec3 l = bodyLocal(i, p - b.xyz);
    vec3 q = abs(l) - (uBodyExt[i].xyz - b.w);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - b.w;
}

vec2 sdBodies(vec3 p)
{
#ifdef SATYR_QUERY_PASS
    return vec2(1e10, MAT_BODY);
#else
    if (uBodyCount == 0) return vec2(1e10, MAT_BODY);

    // Distance to the bounding sphere is a safe lower bound: skip the loop when far away. The
    // field jumps where the loop takes over, so the switch must happen well outside anything
    // that reads map() as proximity: BODIES_MARGIN covers shadows and AO, the footprint term
    // covers the cone-AA hit threshold and near-miss band at any distance (see rayMarchAA).
    float bound = length(p - uBodyBounds.xyz) - uBodyBounds.w;
    float margin = max(BODIES_MARGIN, 2.0 * pixelRadius() * length(p - uCamPos));
    if (bound > margin) return vec2(bound, MAT_BODY);

    float best = 1e10;
    int bestIndex = 0;
    for (int i = 0; i < SATYR_MAX_BODIES; ++i) {
        if (i >= uBodyCount) break;
        float d = sdBody(i, p);
        if (d < best) {
            best = d;
            bestIndex = i;
        }
    }
    return vec2(best, MAT_BODY + float(bestIndex));
#endif
}
