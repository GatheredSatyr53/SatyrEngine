// Sphere tracing core: march, normals, soft shadows and ambient occlusion.
//
// The scene must define  vec2 map(vec3 p)  returning (signed distance, material id).
// Tune the march with #defines placed BEFORE including this file:
//   MAX_STEPS   maximum sphere-tracing iterations          (default 256)
//   MAX_DIST    far clipping distance                      (default 100.0)
//   SURF_EPS    hit threshold                              (default 0.001)
//   STEP_SCALE  step multiplier, <1 for non-Lipschitz fields (default 1.0)
#pragma once
#include "uniforms.glsl"

#ifndef MAX_STEPS
#define MAX_STEPS 256
#endif
#ifndef MAX_DIST
#define MAX_DIST 100.0
#endif
#ifndef SURF_EPS
#define SURF_EPS 0.001
#endif
#ifndef STEP_SCALE
#define STEP_SCALE 1.0
#endif
#ifndef NORMAL_EPS
#define NORMAL_EPS 0.0005
#endif

#define ANTIALIASING

vec2 map(vec3 p); // provided by the scene

struct Hit {
    float t;      // distance along the ray (MAX_DIST when nothing was hit)
    float mat;    // material id from map().y
    int   steps;  // iterations used (handy for debugging / cost heatmaps)
    bool  hit;
};

// px Ч радиус пиксел€ на единичном рассто€нии; px = 0.0 отключает AA
Hit rayMarchAA(vec3 ro, vec3 rd, float px, out Edge edges[AA_LAYERS], out int edgeCount)
{
    Hit h;
    h.t = 0.0;
    h.mat = -1.0;
    h.steps = 0;
    h.hit = false;

    edgeCount = 0;
    vec2  od = vec2(0.0);   // map() на предыдущем шаге
    float ot = 0.0;         // t на предыдущем шаге
    float cover = 0.0;      // накопленное покрытие

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec2 d = map(ro + rd * h.t);
        h.steps = i + 1;

        float th1 = max(SURF_EPS, px * h.t);   // порог попадани€ = радиус пиксел€
        if (d.x < th1) {
            h.hit = true;
            h.mat = d.y;
            return h;
        }

        float th2 = px * h.t * AA_WIDTH;
        if (d.x < th2 && d.x > od.x && edgeCount < AA_LAYERS) {
            float a = clamp(1.0 - (d.x - th1) / (th2 - th1), 0.0, 1.0) * (1.0 - cover);
            edges[edgeCount] = Edge(ot, od.y, a);
            ++edgeCount;
            cover += a;
            if (cover > 0.99) break;
        }

        od = d;
        ot = h.t;
        h.t += d.x * STEP_SCALE;
        if (h.t > MAX_DIST) break;
    }
    h.t = MAX_DIST;
    return h;
}

Hit rayMarch(vec3 ro, vec3 rd)
{
    Hit h;
    h.t = 0.0;
    h.mat = -1.0;
    h.steps = 0;
    h.hit = false;
    for (int i = 0; i < MAX_STEPS; ++i) {
        vec2 d = map(ro + rd * h.t);
        h.steps = i + 1;
        if (d.x < SURF_EPS) {
            h.hit = true;
            h.mat = d.y;
            return h;
        }
        h.t += d.x * STEP_SCALE;
        if (h.t > MAX_DIST) break;
    }
    h.t = MAX_DIST;
    return h;
}

// Tetrahedron-sampled gradient: four map() calls instead of six.
vec3 calcNormal(vec3 p)
{
    const vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * NORMAL_EPS).x +
                     k.yyx * map(p + k.yyx * NORMAL_EPS).x +
                     k.yxy * map(p + k.yxy * NORMAL_EPS).x +
                     k.xxx * map(p + k.xxx * NORMAL_EPS).x);
}

// Penumbra shadow: k controls softness (8 soft .. 64 sharp).
float softShadow(vec3 ro, vec3 rd, float tmin, float tmax, float k)
{
    float res = 1.0;
    float t = tmin;
    for (int i = 0; i < 64 && t < tmax; ++i) {
        float h = map(ro + rd * t).x;
        if (h < 0.0005) return 0.0;
        res = min(res, k * h / t);
        t += clamp(h, 0.01, 0.5);
    }
    return clamp(res, 0.0, 1.0);
}

// Five-tap ambient occlusion along the normal.
float calcAO(vec3 p, vec3 n)
{
    float occ = 0.0;
    float sca = 1.0;
    for (int i = 0; i < 5; ++i) {
        float h = 0.01 + 0.12 * float(i) / 4.0;
        float d = map(p + h * n).x;
        occ += (h - d) * sca;
        sca *= 0.95;
    }
    return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
}
