// Combination and domain operators for distance fields.
//
// Two flavours are provided: plain floats, and vec2(distance, materialId) which lets the
// scene colour each object (see scenes/basic.frag).
#pragma once

// ---- Boolean operations ----------------------------------------------------------------------

float opUnion(float d1, float d2)     { return min(d1, d2); }
float opSubtract(float d1, float d2)  { return max(-d1, d2); } // d2 minus d1
float opIntersect(float d1, float d2) { return max(d1, d2); }

// Polynomial smooth min; k is the blend radius.
float opSmoothUnion(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float opSmoothSubtract(float d1, float d2, float k)
{
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return mix(d2, -d1, h) + k * h * (1.0 - h);
}

float opSmoothIntersect(float d1, float d2, float k)
{
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}

// Material-aware variants: .x = distance, .y = material id.
vec2 opU(vec2 a, vec2 b) { return a.x < b.x ? a : b; }
vec2 opS(vec2 a, vec2 b) { return -a.x > b.x ? vec2(-a.x, a.y) : b; }
vec2 opI(vec2 a, vec2 b) { return a.x > b.x ? a : b; }

// Smooth union keeping the material of whichever surface is closer.
vec2 opSmoothU(vec2 a, vec2 b, float k)
{
    float d = opSmoothUnion(a.x, b.x, k);
    return vec2(d, a.x < b.x ? a.y : b.y);
}

// ---- Shape modifiers -------------------------------------------------------------------------

float opRound(float d, float r) { return d - r; }          // rounds edges of any primitive
float opOnion(float d, float t) { return abs(d) - t; }     // hollow shell of thickness t

// ---- Domain operations (apply to p before evaluating a primitive) ----------------------------

// Infinite repetition with period c (per axis). Returns the local coordinate.
vec3 opRepeat(vec3 p, vec3 c)
{
    return mod(p + 0.5 * c, c) - 0.5 * c;
}

// Repetition limited to [-l, l] cells per axis.
vec3 opRepeatLimited(vec3 p, float c, vec3 l)
{
    return p - c * clamp(round(p / c), -l, l);
}

// Mirror across the given axes (symmetry).
vec3 opSymX(vec3 p)  { p.x = abs(p.x); return p; }
vec3 opSymXZ(vec3 p) { p.xz = abs(p.xz); return p; }

// Rotation matrices (counter-clockwise, radians). Use as: sdBox(rotY(a) * p, ...)
mat3 rotX(float a) { float s = sin(a), c = cos(a); return mat3(1, 0, 0,  0, c, s,  0, -s, c); }
mat3 rotY(float a) { float s = sin(a), c = cos(a); return mat3(c, 0, -s,  0, 1, 0,  s, 0, c); }
mat3 rotZ(float a) { float s = sin(a), c = cos(a); return mat3(c, s, 0,  -s, c, 0,  0, 0, 1); }

// Twist around Y with k radians per unit height. Not distance-preserving: lower STEP_SCALE.
vec3 opTwist(vec3 p, float k)
{
    float c = cos(k * p.y), s = sin(k * p.y);
    return vec3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);
}

// Bend along X. Also not distance-preserving.
vec3 opBend(vec3 p, float k)
{
    float c = cos(k * p.x), s = sin(k * p.x);
    return vec3(c * p.x - s * p.y, s * p.x + c * p.y, p.z);
}

// Elongate a primitive along h: d = sdPrimitive(opElongate(p, h)).
vec3 opElongate(vec3 p, vec3 h)
{
    return p - clamp(p, -h, h);
}
