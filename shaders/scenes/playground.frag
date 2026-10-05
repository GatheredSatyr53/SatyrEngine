#version 330 core
// Physics playground: throw balls (B), drop a handful (G), clear them (X).
// Ramps, stairs, a bowl and a spinning bar to bounce off. Hold LMB for the step heatmap.

#pragma satyr camera pos=0,3.5,10 target=0,1,0 speed=5

#include "common/camera.glsl"
#include "common/sdf.glsl"
#include "common/ops.glsl"
#include "common/bodies.glsl"
#include "common/lighting.glsl"

out vec4 fragColor;

const float MAT_FLOOR  = 0.0;
const float MAT_BOWL   = 1.0;
const float MAT_RAMP   = 2.0;
const float MAT_STAIRS = 3.0;
const float MAT_BAR    = 4.0;
const float MAT_WALL   = 5.0;

vec2 map(vec3 p)
{
    vec2 res = vec2(sdPlane(p, vec3(0.0, 1.0, 0.0), 0.0), MAT_FLOOR);

    // Bowl: hollow sphere with its upper half cut away.
    {
        vec3 q = p - vec3(-4.5, 2.0, 0.0);
        float shell = opOnion(sdSphere(q, 2.0), 0.12);
        float bowl = opSubtract(sdBox(q - vec3(0.0, 2.5, 0.0), vec3(3.0, 2.5, 3.0)), shell);
        res = opU(res, vec2(bowl, MAT_BOWL));
    }

    // Tilted ramp with low side walls.
    {
        vec3 q = rotZ(-0.35) * (p - vec3(4.0, 1.0, 0.0));
        float ramp = sdBox(q, vec3(2.8, 0.12, 1.4));
        ramp = min(ramp, sdBox(q - vec3(0.0, 0.2, 1.4), vec3(2.8, 0.2, 0.08)));
        ramp = min(ramp, sdBox(q - vec3(0.0, 0.2, -1.4), vec3(2.8, 0.2, 0.08)));
        res = opU(res, vec2(ramp, MAT_RAMP));
    }

    // Stairs climbing away from the camera.
    {
        float stairs = 1e10;
        for (int k = 0; k < 5; ++k) {
            float h = 0.2 * float(k + 1);
            stairs = min(stairs, sdBox(p - vec3(0.0, h, -4.0 - 0.7 * float(k)), vec3(2.0, h, 0.35)));
        }
        res = opU(res, vec2(stairs, MAT_STAIRS));
    }

    // Spinning bar: moving geometry pushes bodies around.
    {
        vec3 q = rotY(0.8 * uTime) * (p - vec3(0.0, 0.3, 2.5));
        float bar = sdRoundBox(q, vec3(2.2, 0.15, 0.15), 0.08);
        bar = min(bar, sdCappedCylinder(p - vec3(0.0, 0.3, 2.5), 0.3, 0.25));
        res = opU(res, vec2(bar, MAT_BAR));
    }

    // Back wall so balls stay in view.
    res = opU(res, vec2(sdBox(p - vec3(0.0, 1.0, -9.0), vec3(9.0, 1.0, 0.3)), MAT_WALL));

    // Dynamic bodies from the physics system.
    res = opU(res, sdBodies(p));
    return res;
}

vec3 material(float mat, vec3 p)
{
    if (isBody(mat)) return bodyColor(bodyIndex(mat));
    int id = int(mat + 0.5);
    if (id == 0) return mix(vec3(0.28, 0.3, 0.33), vec3(0.6, 0.6, 0.58), checker(p.xz));
    if (id == 1) return vec3(0.85, 0.8, 0.7);
    if (id == 2) return vec3(0.75, 0.45, 0.25);
    if (id == 3) return vec3(0.45, 0.55, 0.7);
    if (id == 4) return vec3(0.9, 0.25, 0.2);
    return vec3(0.5, 0.52, 0.5);
}

void main()
{
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy);
    vec3 sunDir = normalize(vec3(0.5, 0.7, 0.45));

    vec3 sky = skyColor(rd, sunDir);
    vec3 col = sky;

    Hit h = rayMarch(ro, rd);
    if (h.hit) {
        vec3 p = ro + rd * h.t;
        vec3 n = calcNormal(p);
        col = shadeStandard(p, n, rd, material(h.mat, p), sunDir, isBody(h.mat) ? 64.0 : 24.0);
        col = applyFog(col, h.t, sky, 0.012);
    }

    if (uMouse.z > 0.5) col = stepHeatmap(h.steps, MAX_STEPS);

    fragColor = vec4(tonemapFilmic(col), 1.0);
}
