// Uniforms the engine uploads every frame. Include this (directly or via camera.glsl).
#pragma once
#ifndef COMMON_UNIFORMS_GLSL
#define COMMON_UNIFORMS_GLSL

uniform vec2  uResolution;  // render target size in pixels
uniform float uTime;        // scene time in seconds (P pauses, T resets)
uniform float uDeltaTime;   // seconds since the previous frame (0 while paused)
uniform int   uFrame;       // frame counter
uniform vec4  uMouse;       // xy: cursor in pixels, origin bottom-left; z: LMB held; w: RMB held
uniform vec3  uCamPos;      // camera position (fly camera: WASD + mouse)
uniform mat3  uCamBasis;    // columns: right, up, forward
uniform float uCamFov;      // vertical field of view in radians

// Frame accumulation (active while the scene time is paused and the camera is still, or for
// scenes that never read uTime/uDeltaTime/uFrame). The engine averages consecutive frames.
uniform vec2  uJitter;      // sub-pixel offset of this sample in pixels, [-0.5, 0.5]; zero when live
uniform float uPxScale = 1.0; // scales pixelRadius(); shrinks while accumulating so jitter does the AA
uniform int   uAccumFrame;  // index of the sample being accumulated, 0 when live

#endif
