// Anti-aliased rendering helper on top of rayMarchAA().
//
// The scene defines, after this include:
//   vec3 shadeHit(vec3 ro, vec3 rd, float t, float mat);  // colour of the surface at ro + rd * t
//   vec3 shadeMiss(vec3 ro, vec3 rd);                      // background colour for the ray
// and calls renderAA(ro, rd, pixelRadius(), steps). Near-miss surfaces are shaded at their
// closest-approach point (within a pixel of the surface, so normals and lighting work) and
// blended front-to-back by coverage. Pass px = 0.0 to disable anti-aliasing.
#pragma once
#include "raymarch.glsl"

vec3 shadeHit(vec3 ro, vec3 rd, float t, float mat);
vec3 shadeMiss(vec3 ro, vec3 rd);

vec3 renderAA(vec3 ro, vec3 rd, float px, out int steps)
{
    Edge edges[AA_LAYERS];
    int edgeCount;
    Hit h = rayMarchAA(ro, rd, px, edges, edgeCount);
    steps = h.steps;

    vec3 col = h.hit ? shadeHit(ro, rd, h.t, h.mat) : shadeMiss(ro, rd);

    vec3 acc = vec3(0.0);
    float cover = 0.0;
    for (int i = 0; i < AA_LAYERS; ++i) {
        if (i >= edgeCount) break;
        acc += shadeHit(ro, rd, edges[i].t, edges[i].mat) * edges[i].a;
        cover += edges[i].a;
    }
    return acc + col * (1.0 - cover);
}
