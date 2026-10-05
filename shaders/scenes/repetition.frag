#version 330 core
// Infinite domain repetition: an endless hall of pillars with a floating orb per cell.
//
// Pillars are identical in every cell, so plain modulo repetition is exact. The orbs differ
// per cell (hashed height, size and colour); for those the four nearest cells are evaluated
// so the distance field stays correct near cell borders.

// Do not move
#define MAX_DIST 150.0
#define STEP_SCALE 0.9

#pragma satyr camera pos=0,1.6,6 target=0,1.1,0 speed=4

#include "common/camera.glsl" //! #include "../common/camera.glsl"
#include "common/sdf.glsl"  //! #include "../common/sdf.glsl"
#include "common/ops.glsl" //! #include "../common/ops.glsl"
#include "common/noise.glsl" //! #include "../common/noise.glsl"
#include "common/bodies.glsl" //! #include "../common/bodies.glsl"
#include "common/lighting.glsl" //! #include "../common/lighting.glsl"
#include "common/aa.glsl" //! #include "../common/aa.glsl"

out vec4 fragColor;

const float CELL = 4.0;
const float MAT_FLOOR  = 0.0;
const float MAT_PILLAR = 1.0;
const float MAT_ORB    = 2.0;   // orbs use ids 2 + cellHash so each gets its own colour

float orbHeight(float h) { return 2.3 + 0.35 * sin(uTime * (0.6 + 0.8 * h) + 6.2831 * h); }
float orbRadius(float h) { return 0.25 + 0.2 * h; }

// Orb of cell `id`; `p` is already relative to the cell centre.
float sdOrb(vec3 p, vec2 id)
{
    float h = hash21(id);
    return sdSphere(p - vec3(0.0, orbHeight(h), 0.0), orbRadius(h));
}

vec2 map(vec3 p)
{
    p.xz += 0.5 * CELL; // pillars sit on cell corners, so x = 0 is an aisle for the camera

    vec2 res = vec2(sdPlane(p, vec3(0.0, 1.0, 0.0), 0.0), MAT_FLOOR);

    // Pillars: modulo repetition in XZ.
    vec3 q = p;
    q.xz = mod(p.xz + 0.5 * CELL, CELL) - 0.5 * CELL;
    float pillar = sdCappedCylinder(q - vec3(0.0, 0.8, 0.0), 0.8, 0.3);
    pillar = min(pillar, sdBox(q - vec3(0.0, 1.65, 0.0), vec3(0.5, 0.06, 0.5)));   // capital
    pillar = min(pillar, sdBox(q - vec3(0.0, 0.05, 0.0), vec3(0.55, 0.05, 0.55)));  // base
    res = opU(res, vec2(pillar, MAT_PILLAR));

    // Orbs: check the 2x2 neighbouring cells on the side of p.
    vec2 id = round(p.xz / CELL);
    vec2 side = sign(p.xz - CELL * id);
    float d = 1e10;
    float matId = MAT_ORB;
    for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
            vec2 rid = id + vec2(i, j) * side;
            vec2 r = p.xz - CELL * rid;
            float od = sdOrb(vec3(r.x, p.y, r.y), rid);
            if (od < d) {
                d = od;
                matId = MAT_ORB + hash21(rid);
            }
        }
    }
    res = opU(res, vec2(d, matId));

    // Physics balls (press B); note the cell offset applied to p above.
    res = opU(res, sdBodies(p - vec3(0.5 * CELL, 0.0, 0.5 * CELL)));
    return res;
}

// Cosine palette (Inigo Quilez).
vec3 palette(float t)
{
    return 0.5 + 0.5 * cos(6.2831 * (t + vec3(0.0, 0.33, 0.67)));
}

const vec3 FOG = vec3(0.05, 0.06, 0.09);

vec3 sunDir() { return normalize(vec3(0.4, 0.5, 0.7)); }

vec3 shadeMiss(vec3 ro, vec3 rd)
{
    return FOG;
}

vec3 shadeHit(vec3 ro, vec3 rd, float t, float mat)
{
    vec3 p = ro + rd * t;
    vec3 n = calcNormal(p);
    vec3 col;

    if (isBody(mat)) {
        col = shadeStandard(p, n, rd, bodyColor(bodyIndex(mat)), sunDir(), 48.0) * 0.6;
    } else if (mat >= MAT_ORB) {
        // Emissive orb: colour from the cell hash, brighter at grazing angles.
        vec3 c = palette(mat - MAT_ORB);
        float rim = pow(1.0 - max(dot(n, -rd), 0.0), 2.0);
        col = c * (0.6 + 1.5 * rim);
    } else {
        vec3 ddx, ddy;
        surfaceFootprint(ro, gl_FragCoord.xy, p, n, ddx, ddy);
        vec3 albedo = mat < 0.5
            ? mix(vec3(0.12), vec3(0.3), checkerFiltered(p.xz * 0.5, ddx.xz * 0.5, ddy.xz * 0.5))
            : vec3(0.55, 0.5, 0.45);
        col = shadeStandard(p, n, rd, albedo, sunDir(), 24.0) * 0.45;

        // Each cell's orb lights its surroundings (same cell offset as in map()).
        vec2 id = round((p.xz + 0.5 * CELL) / CELL);
        float hh = hash21(id);
        vec3 orbPos = vec3(CELL * id.x - 0.5 * CELL, orbHeight(hh), CELL * id.y - 0.5 * CELL);
        vec3 toOrb = orbPos - p;
        float dist2 = dot(toOrb, toOrb);
        float lit = max(dot(n, toOrb * inversesqrt(dist2)), 0.0) / (1.0 + 0.25 * dist2);
        col += albedo * palette(hh) * lit * 2.5;
    }
    return applyFog(col, t, FOG, 0.035);
}

void main()
{
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy);

    int steps;
    vec3 col = renderAA(ro, rd, pixelRadius(), steps);
    if (uMouse.z > 0.5) col = stepHeatmap(steps, MAX_STEPS);

    fragColor = vec4(toGamma(tonemapReinhard(col)), 1.0);
}
