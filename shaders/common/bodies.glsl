// Dynamic bodies: spheres simulated on the CPU (src/engine/Physics.cpp) and merged into the
// scene here. Add  res = opU(res, sdBodies(p));  to map() and colour hits with bodyColor().
//
// Throw balls with B, drop a handful with G, clear with X. The physics probe samples map()
// with SATYR_QUERY_PASS defined, where sdBodies() returns nothing so bodies never collide
// with themselves.
#pragma once

#ifndef SATYR_MAX_BODIES
#define SATYR_MAX_BODIES 64
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

    // Distance to the bounding sphere is a safe lower bound: skip the loop when far away.
    float bound = length(p - uBodyBounds.xyz) - uBodyBounds.w;
    if (bound > 0.1) return vec2(bound, MAT_BODY);

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
