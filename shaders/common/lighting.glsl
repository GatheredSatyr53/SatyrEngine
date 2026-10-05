// Ready-made shading so a new scene looks decent with one function call.
#pragma once
#include "raymarch.glsl"

// Simple sky gradient with a sun glow; use for the background and as ambient light.
vec3 skyColor(vec3 rd, vec3 sunDir)
{
    float horizon = clamp(rd.y, 0.0, 1.0);
    vec3 sky = mix(vec3(0.55, 0.70, 0.90), vec3(0.15, 0.30, 0.60), horizon);
    float sun = pow(max(dot(rd, sunDir), 0.0), 256.0);
    sky += vec3(1.0, 0.85, 0.6) * sun;
    sky = mix(sky, vec3(0.45, 0.50, 0.55), smoothstep(0.1, -0.2, rd.y)); // ground haze
    return sky;
}

// Key light + sky + bounce, with soft shadows and AO. `albedo` is linear colour.
vec3 shadeStandard(vec3 p, vec3 n, vec3 rd, vec3 albedo, vec3 sunDir, float shininess)
{
    vec3 sunColor = vec3(1.0, 0.9, 0.75) * 2.2;
    vec3 skyAmbient = vec3(0.45, 0.55, 0.75) * 0.7;
    vec3 bounce = vec3(0.35, 0.3, 0.25) * 0.4;

    float ao = calcAO(p, n);
    float shadow = softShadow(p + n * 0.01, sunDir, 0.02, 20.0, 16.0);

    float diffuse = max(dot(n, sunDir), 0.0) * shadow;
    float skyDiffuse = clamp(0.5 + 0.5 * n.y, 0.0, 1.0);
    float bounceDiffuse = clamp(0.5 - 0.5 * n.y, 0.0, 1.0);

    vec3 h = normalize(sunDir - rd);
    float spec = pow(max(dot(n, h), 0.0), shininess) * diffuse;
    float fresnel = pow(clamp(1.0 + dot(n, rd), 0.0, 1.0), 5.0);

    vec3 col = albedo * (sunColor * diffuse + skyAmbient * skyDiffuse * ao + bounce * bounceDiffuse * ao);
    col += sunColor * spec * (0.04 + 0.5 * fresnel);
    return col;
}

// Exponential distance fog towards `fogColor`.
vec3 applyFog(vec3 col, float t, vec3 fogColor, float density)
{
    return mix(fogColor, col, exp(-density * t));
}

// 0/1 checkerboard on a 2D plane with 1-unit squares.
float checker(vec2 p)
{
    vec2 q = floor(p);
    return mod(q.x + q.y, 2.0);
}

vec3 tonemapReinhard(vec3 c) { return c / (1.0 + c); }

// Filmic curve (Hable/Uncharted-style), keeps highlights from clipping harshly.
vec3 tonemapFilmic(vec3 x)
{
    x = max(vec3(0.0), x - 0.004);
    return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06); // already gamma-encoded
}

vec3 toGamma(vec3 c) { return pow(max(c, 0.0), vec3(1.0 / 2.2)); }

// Visualises how many steps the march took: cheap pixels blue, expensive ones red.
vec3 stepHeatmap(int steps, int maxSteps)
{
    float x = clamp(float(steps) / float(maxSteps), 0.0, 1.0);
    return vec3(x, 0.4 * (1.0 - abs(2.0 * x - 1.0)), 1.0 - x);
}
