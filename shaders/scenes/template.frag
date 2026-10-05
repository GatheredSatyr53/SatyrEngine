#version 330 core
// Starting point for a new experiment: copy this file, edit map(), save, watch it reload.
//
// Contract with the engine:
//   * `vec2 map(vec3 p)` returns (signed distance, material id) for the whole scene.
//   * `out vec4 fragColor` is the pixel colour, gamma-encoded.
//   * uniforms are documented in common/uniforms.glsl; cameraRay() gives the primary ray.
// Tweak march settings by defining MAX_STEPS / MAX_DIST / SURF_EPS / STEP_SCALE before the includes.
// The pragma below sets the starting viewpoint (Home returns to it); fov and speed are optional.

#pragma satyr camera pos=0,1.5,5 target=0,0.5,0 fov=60 speed=3

#include "common/camera.glsl" //! #include "../common/camera.glsl"
#include "common/sdf.glsl"  //! #include "../common/sdf.glsl"
#include "common/ops.glsl" //! #include "../common/ops.glsl"
#include "common/bodies.glsl" //! #include "../common/bodies.glsl"
#include "common/lighting.glsl" //! #include "../common/lighting.glsl"

out vec4 fragColor;

const float MAT_GROUND = 0.0;
const float MAT_OBJECT = 1.0;

vec2 map(vec3 p)
{
    vec2 ground = vec2(sdPlane(p, vec3(0.0, 1.0, 0.0), 0.0), MAT_GROUND);
    vec2 object = vec2(sdSphere(p - vec3(0.0, 1.0, 0.0), 1.0), MAT_OBJECT);
    return opU(opU(ground, object), sdBodies(p));
}

vec3 material(float mat, vec3 p)
{
    if (isBody(mat)) return bodyColor(bodyIndex(mat));
    if (mat < 0.5) return mix(vec3(0.25), vec3(0.7), checker(p.xz));
    return vec3(0.9, 0.3, 0.2);
}

void main()
{
    vec3 ro = uCamPos;
    vec3 rd = cameraRay(gl_FragCoord.xy);
    vec3 sunDir = normalize(vec3(0.6, 0.7, 0.4));

    vec3 sky = skyColor(rd, sunDir);
    vec3 col = sky;

    Hit h = rayMarch(ro, rd);
    if (h.hit) {
        vec3 p = ro + rd * h.t;
        vec3 n = calcNormal(p);
        col = shadeStandard(p, n, rd, material(h.mat, p), sunDir, 32.0);
        col = applyFog(col, h.t, sky, 0.02);
    }

    fragColor = vec4(toGamma(tonemapReinhard(col)), 1.0);
}
