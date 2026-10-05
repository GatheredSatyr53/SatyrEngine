// Dynamic bodies: spheres simulated on the CPU (src/engine/Physics.cpp) and merged into the
// scene here. Add  res = opU(res, sdBodies(p));  to map() and colour hits with bodyColor().
//
// Throw balls with B, drop a handful with G, clear with X. The physics probe samples map()
// with SATYR_QUERY_PASS defined, where sdBodies() returns nothing so bodies never collide
// with themselves.
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

uniform vec4 uBodies[SATYR_MAX_BODIES]; // xyz centre, w radius
uniform int  uBodyCount;
uniform vec4 uBodyBounds;               // sphere enclosing all bodies (xyz centre, w radius)

const float MAT_BODY = 1000.0;          // material id = MAT_BODY + body index

bool isBody(float mat) { return mat >= MAT_BODY - 0.5; }
int bodyIndex(float mat) { return int(mat - MAT_BODY + 0.5); }

vec3 bodyColor(int i)
{
    float t = fract(float(i) * 0.618034);
    return 0.55 + 0.45 * cos(6.2831 * (t + vec3(0.0, 0.33, 0.67)));
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
        float d = length(p - uBodies[i].xyz) - uBodies[i].w;
        if (d < best) {
            best = d;
            bestIndex = i;
        }
    }
    return vec2(best, MAT_BODY + float(bestIndex));
#endif
}
