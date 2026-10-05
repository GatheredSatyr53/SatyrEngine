// Hashes and noise for terrain, displacement and procedural texturing.
#pragma once

float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    return fract(p * (p + p));
}

float hash21(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash31(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

vec2 hash22(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

// Smooth value noise in [0, 1].
float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i + vec2(0, 0)), hash21(i + vec2(1, 0)), u.x),
               mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), u.x), u.y);
}

float valueNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash31(i + vec3(0, 0, 0)), hash31(i + vec3(1, 0, 0)), u.x),
                   mix(hash31(i + vec3(0, 1, 0)), hash31(i + vec3(1, 1, 0)), u.x), u.y),
               mix(mix(hash31(i + vec3(0, 0, 1)), hash31(i + vec3(1, 0, 1)), u.x),
                   mix(hash31(i + vec3(0, 1, 1)), hash31(i + vec3(1, 1, 1)), u.x), u.y), u.z);
}

// Fractal Brownian motion, `octaves` layers of value noise.
float fbm(vec2 p, int octaves)
{
    float v = 0.0;
    float a = 0.5;
    const mat2 rot = mat2(0.8, 0.6, -0.6, 0.8);
    for (int i = 0; i < octaves; ++i) {
        v += a * valueNoise(p);
        p = rot * p * 2.03;
        a *= 0.5;
    }
    return v;
}

float fbm(vec3 p, int octaves)
{
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < octaves; ++i) {
        v += a * valueNoise(p);
        p = p * 2.03 + vec3(1.7, 9.2, 3.1);
        a *= 0.5;
    }
    return v;
}
