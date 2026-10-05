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

#endif
