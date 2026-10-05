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

vec2 map(vec3 p); // provided by the scene

struct Hit {
    float t;      // distance along the ray (MAX_DIST when nothing was hit)
    float mat;    // material id from map().y
    int   steps;  // iterations used (handy for debugging / cost heatmaps)
    bool  hit;
};

// ---- Cone-traced anti-aliasing ---------------------------------------------------------------
// rayMarchAA() treats the ray as a cone of radius px * t, where px is the radius of one pixel at
// unit distance (pixelRadius()). A surface closer than AA_HIT pixel radii counts as a hit. A
// surface the ray only passes, within AA_WIDTH pixel radii at the closest approach, is recorded
// as an Edge with an estimated coverage so the scene can blend its colour over what lies behind
// (see aa.glsl / renderAA()). With px = 0.0 the function behaves exactly like rayMarch().
//
// Coverage is a linear ramp from 1 at AA_HIT radii to 0 at AA_WIDTH radii. Hits always count as
// full coverage, so silhouettes come out somewhat fatter than they are: with the defaults about
// half a pixel per side, and thin geometry gains roughly a third of its width. A wider band is
// smoother but bolder (AA_HIT 1.0 / AA_WIDTH 3.0 roughly doubles the width of 2-pixel features);
// AA_HIT = 1.0 is the classic pixel-size epsilon that stops the march earliest.
#ifndef AA_LAYERS
#define AA_LAYERS 4        // near-miss surfaces remembered per ray
#endif
#ifndef AA_HIT
#define AA_HIT 0.5         // hit radius in pixel radii
#endif
#ifndef AA_WIDTH
#define AA_WIDTH 1.5       // outer edge of the coverage band in pixel radii (> AA_HIT)
#endif

struct Edge {
    float t;    // closest approach along the ray
    float mat;  // material of the surface passed
    float a;    // coverage, already attenuated by the layers in front (sum over layers <= 1)
};

// Radius of one pixel at unit distance for the engine camera. While frames are accumulated the
// engine shrinks it (uPxScale) so the cone AA stops fattening silhouettes and jitter takes over.
float pixelRadius()
{
    return tan(0.5 * uCamFov) / uResolution.y * uPxScale;
}

// Refines the closest approach from three consecutive samples (d1 > d2 < d3) by fitting a
// parabola through them; sphere tracing alone only knows dmin to within one step.
void refineClosest(float t1, float d1, float t2, float d2, float t3, float d3, out float tMin, out float dMin)
{
    tMin = t2;
    dMin = d2;
    float a = t1 - t2; // < 0
    float b = t3 - t2; // > 0
    if (a > -1e-7 || b < 1e-7) return;
    float A = ((d3 - d2) - (b / a) * (d1 - d2)) / (b * (b - a));
    if (A <= 0.0) return;
    float B = (d1 - d2 - A * a * a) / a;
    float x = clamp(-B / (2.0 * A), a, b);
    tMin = t2 + x;
    dMin = clamp(A * x * x + B * x + d2, 0.0, d2);
}

Hit rayMarchAA(vec3 ro, vec3 rd, float px, out Edge edges[AA_LAYERS], out int edgeCount)
{
    Hit h;
    h.t = 0.0;
    h.mat = -1.0;
    h.steps = 0;
    h.hit = false;

    edgeCount = 0;
    float cover = 0.0;
    // Last two samples (t, distance, material) and whether the distance was shrinking.
    float t1 = 0.0, d1 = 1e10;
    float t2 = 0.0, d2 = 1e10, m2 = -1.0;
    bool descending = false;

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec2 d = map(ro + rd * h.t);
        h.steps = i + 1;

        if (d.x < max(SURF_EPS, AA_HIT * px * h.t)) {
            h.hit = true;
            h.mat = d.y;
            return h;
        }

        if (d.x < d2) {
            descending = true;
        } else if (descending) {
            // The previous sample was a local minimum: the ray just passed a surface.
            descending = false;
            float tMin, dMin;
            refineClosest(t1, d1, t2, d2, h.t, d.x, tMin, dMin);
            float r = px * tMin;
            if (dMin < AA_WIDTH * r && edgeCount < AA_LAYERS) {
                float a = clamp((AA_WIDTH * r - dMin) / ((AA_WIDTH - AA_HIT) * r), 0.0, 1.0) * (1.0 - cover);
                edges[edgeCount] = Edge(tMin, m2, a);
                ++edgeCount;
                cover += a;
                if (cover > 0.99) break;
            }
        }

        t1 = t2; d1 = d2;
        t2 = h.t; d2 = d.x; m2 = d.y;
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
