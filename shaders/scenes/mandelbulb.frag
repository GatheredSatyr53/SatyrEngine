#version 330 core
// Mandelbulb fractal (power 8) with orbit-trap colouring.
//
// The distance estimate is only approximate, so STEP_SCALE is lowered and the hit epsilon
// tightened. The material slot of map() carries the orbit-trap value instead of an id.

#define MAX_STEPS 160
#define MAX_DIST 20.0
#define SURF_EPS 0.0004
#define NORMAL_EPS 0.0003
#define STEP_SCALE 0.75

#pragma satyr camera pos=0,0.9,2.9 target=0,0,0 speed=1

#include "common/camera.glsl"
#include "common/ops.glsl"
#include "common/lighting.glsl"

out vec4 fragColor;

const float POWER = 8.0;
const int ITERATIONS = 10;

vec2 map(vec3 p)
{
    p = rotY(0.15 * uTime) * p;

    vec3 z = p;
    float dr = 1.0;
    float r = 0.0;
    float trap = 1e10;
    for (int i = 0; i < ITERATIONS; ++i) {
        r = length(z);
        if (r > 2.0) break;
        r = max(r, 1e-6);

        // Convert to polar coordinates, raise to POWER, convert back.
        float theta = acos(z.z / r) * POWER;
        float phi = atan(z.y, z.x) * POWER;
        dr = pow(r, POWER - 1.0) * POWER * dr + 1.0;
        float zr = pow(r, POWER);
        z = zr * vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)) + p;

        trap = min(trap, length(z - vec3(0.0, 0.0, 0.6)));
    }
    float d = 0.5 * log(r) * r / dr;
    return vec2(d, trap);
}

vec3 palette(float t)
{
    return vec3(0.5, 0.5, 0.5) + vec3(0.5, 0.5, 0.5) * cos(6.2831 * (vec3(1.0, 1.0, 1.0) * t + vec3(0.0, 0.1, 0.2)));
}

void main()
{
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy);

    vec3 bg = mix(vec3(0.02, 0.02, 0.04), vec3(0.08, 0.09, 0.14), 0.5 + 0.5 * rd.y);
    vec3 col = bg;

    Hit h = rayMarch(ro, rd);
    if (h.hit) {
        vec3 p = ro + rd * h.t;
        vec3 n = calcNormal(p);

        vec3 albedo = palette(0.45 + 0.35 * clamp(h.mat, 0.0, 1.0));
        albedo = mix(albedo, vec3(0.9, 0.85, 0.8), smoothstep(0.9, 1.3, h.mat));

        // Two lights plus AO; shadows are skipped because the field is expensive to evaluate.
        vec3 keyDir = normalize(vec3(0.6, 0.8, 0.5));
        vec3 fillDir = normalize(vec3(-0.7, -0.2, -0.4));
        float ao = calcAO(p, n);
        float key = max(dot(n, keyDir), 0.0);
        float fill = max(dot(n, fillDir), 0.0);
        float rim = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
        vec3 hv = normalize(keyDir - rd);
        float spec = pow(max(dot(n, hv), 0.0), 40.0) * key;

        col = albedo * (vec3(1.0, 0.95, 0.9) * key * 1.4 + vec3(0.3, 0.4, 0.6) * fill * 0.6 + vec3(0.25) * ao);
        col += vec3(0.6, 0.7, 1.0) * rim * 0.25 * ao;
        col += vec3(1.0) * spec * 0.4;
        col = applyFog(col, h.t, bg, 0.08);
    }

    if (uMouse.z > 0.5) col = stepHeatmap(h.steps, MAX_STEPS);

    fragColor = vec4(toGamma(tonemapReinhard(col * 1.2)), 1.0);
}
