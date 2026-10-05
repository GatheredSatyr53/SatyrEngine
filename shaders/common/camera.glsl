// Ray generation from the engine's fly camera, plus a look-at helper for scripted cameras.
#pragma once
#include "uniforms.glsl"

// Normalized device-like coordinates: y in [-1, 1], x scaled by the aspect ratio.
// uJitter moves the sample inside the pixel while frames are being accumulated.
vec2 screenUV(vec2 fragCoord)
{
    return (2.0 * (fragCoord + uJitter) - uResolution) / uResolution.y;
}

// Primary ray direction for a pixel using the interactive camera.
vec3 cameraRay(vec2 fragCoord)
{
    vec2 uv = screenUV(fragCoord);
    float focal = 1.0 / tan(0.5 * uCamFov);
    return normalize(uCamBasis * vec3(uv, focal));
}

// Basis (right, up, forward) for a camera at `ro` looking at `target`, with roll in radians.
mat3 lookAtBasis(vec3 ro, vec3 target, float roll)
{
    vec3 forward = normalize(target - ro);
    vec3 refUp = vec3(sin(roll), cos(roll), 0.0);
    vec3 right = normalize(cross(forward, refUp));
    vec3 up = cross(right, forward);
    return mat3(right, up, forward);
}

// Ray direction for a custom basis (e.g. from lookAtBasis) and vertical FOV in radians.
vec3 basisRay(vec2 fragCoord, mat3 basis, float fov)
{
    vec2 uv = screenUV(fragCoord);
    float focal = 1.0 / tan(0.5 * fov);
    return normalize(basis * vec3(uv, focal));
}

// Radius of one pixel at unit distance for the engine camera. While frames are accumulated the
// engine shrinks it (uPxScale) so the cone AA stops fattening silhouettes and jitter takes over.
float pixelRadius()
{
    return tan(0.5 * uCamFov) / uResolution.y * uPxScale;
}

// How far the hit point p (with surface normal n) moves for the pixel to the right (ddx) and
// above (ddy): the neighbouring pixels' rays intersected with the tangent plane at p. Exact for
// planes and good enough elsewhere; unlike dFdx() it stays valid across silhouettes. Use it to
// filter textures by the pixel footprint (see checkerFiltered()).
void surfaceFootprint(vec3 ro, vec2 fragCoord, vec3 p, vec3 n, out vec3 ddx, out vec3 ddy)
{
    vec3 rdx = cameraRay(fragCoord + vec2(1.0, 0.0));
    vec3 rdy = cameraRay(fragCoord + vec2(0.0, 1.0));
    float planeDist = dot(p - ro, n);
    float tx = planeDist / dot(rdx, n);
    float ty = planeDist / dot(rdy, n);
    // A neighbouring ray that never reaches the plane means a grazing view: use a huge footprint.
    if (!(tx > 0.0)) tx = 1e4;
    if (!(ty > 0.0)) ty = 1e4;
    ddx = ro + rdx * tx - p;
    ddy = ro + rdy * ty - p;
}
