// Shadertoy compatibility: paste a shader's mainImage() below this include and it runs here.
//
//   #include "common/shadertoy.glsl"
//   void mainImage(out vec4 fragColor, in vec2 fragCoord) { ... }
//
// Not provided: iChannel textures, iDate, iSampleRate, keyboard input.
#pragma once
#include "uniforms.glsl"

#define iResolution vec3(uResolution, 1.0)
#define iTime       uTime
#define iTimeDelta  uDeltaTime
#define iFrame      uFrame
#define iMouse      uMouse

out vec4 satyrFragColor;

void mainImage(out vec4 fragColor, in vec2 fragCoord);

void main()
{
    mainImage(satyrFragColor, gl_FragCoord.xy);
}
