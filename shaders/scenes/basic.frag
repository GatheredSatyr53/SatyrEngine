#version 330 core
// Primitive showcase: boolean ops, smooth blending, per-object materials, soft shadows,
// ambient occlusion and fog. Hold the left mouse button to see the step-count heatmap.

#pragma satyr camera pos=0,2.2,7.5 target=0,0.7,0

#include "common/camera.glsl"
#include "common/sdf.glsl"
#include "common/ops.glsl"
#include "common/lighting.glsl"

out vec4 fragColor;

const float MAT_GROUND  = 0.0;
const float MAT_BLOB    = 1.0;
const float MAT_TORUS   = 2.0;
const float MAT_DIE     = 3.0;
const float MAT_CAPSULE = 4.0;
const float MAT_OCTA    = 5.0;
const float MAT_CUP     = 6.0;

vec2 map(vec3 p)
{
    vec2 res = vec2(sdPlane(p, vec3(0.0, 1.0, 0.0), 0.0), MAT_GROUND);

    // Sphere and box blended with a smooth union; the sphere slides back and forth.
    {
        vec3 q = p - vec3(0.0, 1.0, 0.0);
        float s = sdSphere(q - vec3(0.6 * sin(uTime), 0.0, 0.0), 0.7);
        float b = sdBox(rotY(0.5 * uTime) * q, vec3(0.5));
        res = opU(res, vec2(opSmoothUnion(s, b, 0.4), MAT_BLOB));
    }

    // Tumbling torus.
    {
        vec3 q = rotX(0.7 * uTime) * (p - vec3(-3.0, 1.2, 0.0));
        res = opU(res, vec2(sdTorus(q, vec2(0.8, 0.25)), MAT_TORUS));
    }

    // Rounded die: box intersected with a sphere, then a cylinder bored through it.
    {
        vec3 q = p - vec3(3.0, 1.0, 0.0);
        float d = opIntersect(sdBox(q, vec3(0.8)), sdSphere(q, 1.0));
        d = opSubtract(sdCappedCylinder(q, 2.0, 0.35), d);
        res = opU(res, vec2(d, MAT_DIE));
    }

    // Capsule lying on the ground.
    res = opU(res, vec2(sdCapsule(p, vec3(-1.6, 0.3, 2.5), vec3(-0.6, 1.2, 2.0), 0.3), MAT_CAPSULE));

    // Spinning octahedron.
    {
        vec3 q = rotY(uTime) * (p - vec3(1.5, 0.8, 2.5));
        res = opU(res, vec2(sdOctahedron(q, 0.7), MAT_OCTA));
    }

    // Hollow cup: onion shell of a cylinder with the top cut off.
    {
        vec3 q = p - vec3(-1.5, 0.5, -3.0);
        float shell = opOnion(sdCappedCylinder(q, 0.5, 0.7), 0.05);
        float cup = opSubtract(sdBox(q - vec3(0.0, 1.0, 0.0), vec3(1.0, 0.55, 1.0)), shell);
        res = opU(res, vec2(cup, MAT_CUP));
    }

    return res;
}

vec3 material(int id, vec3 p)
{
    if (id == 0) {
        vec3 c = mix(vec3(0.22, 0.22, 0.24), vec3(0.65, 0.63, 0.6), checker(p.xz));
        // Thin grid lines every unit.
        vec2 g = abs(fract(p.xz + 0.5) - 0.5);
        c *= 1.0 - 0.5 * (1.0 - smoothstep(0.0, 0.03, min(g.x, g.y)));
        return c;
    }
    if (id == 1) return vec3(0.9, 0.35, 0.2);
    if (id == 2) return vec3(0.2, 0.5, 0.9);
    if (id == 3) return vec3(0.95, 0.95, 0.92);
    if (id == 4) return vec3(0.3, 0.8, 0.4);
    if (id == 5) return vec3(0.95, 0.75, 0.2);
    return vec3(0.6, 0.3, 0.75);
}

void main()
{
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy);
    vec3 sunDir = normalize(vec3(0.8, 0.6, 0.4));

    vec3 sky = skyColor(rd, sunDir);
    vec3 col = sky;

    Hit h = rayMarch(ro, rd);
    if (h.hit) {
        vec3 p = ro + rd * h.t;
        vec3 n = calcNormal(p);
        int id = int(h.mat + 0.5);
        col = shadeStandard(p, n, rd, material(id, p), sunDir, id == 3 ? 128.0 : 32.0);
        col = applyFog(col, h.t, sky, 0.015);
    }

    if (uMouse.z > 0.5) col = stepHeatmap(h.steps, MAX_STEPS);

    fragColor = vec4(tonemapFilmic(col), 1.0);
}
