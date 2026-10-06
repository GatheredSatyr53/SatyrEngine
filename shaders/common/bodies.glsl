// Dynamic bodies: spheres and boxes simulated on the CPU (src/engine/Physics.cpp) and merged
// into the scene here. Add  res = opU(res, sdBodies(p));  to map() and colour hits with
// bodyColor().
//
// Throw a ball with B, boxes with N / M / K, drop a handful with G, clear with X. The physics
// probe samples map() with SATYR_QUERY_PASS defined, where sdBodies() returns nothing so bodies
// never collide with themselves.
//
// Bodies and their spatial grid arrive as textures (src/engine/BodyTextures.cpp): a point only
// evaluates the bodies listed for its grid cell, which holds everything closer than one cell,
// and a coarse distance layer keeps the field an honest lower bound further away.
#pragma once
#include "camera.glsl"

// How far outside the bodies' bounding sphere the grid is still consulted. Beyond it only the
// distance to the bounding sphere is returned, which is a lower bound and therefore fine for
// marching, but everything that reads map() as "how close is the nearest surface" would see
// the bounding sphere as an object: soft shadows (k * h / t darkens for h < t / k), ambient
// occlusion and the cone AA band. Two units keeps the bound above t / k for the usual k = 16
// and shadow length 20; raise it for softer or longer shadows.
#ifndef BODIES_MARGIN
#define BODIES_MARGIN 2.0
#endif
// Up to this many bodies also arrive as uniform arrays and are evaluated with a plain loop over
// all of them: exact distances everywhere (bigger march steps) and no texture fetches. Beyond
// it the bodies come from textures and the grid takes over.
#ifndef BODIES_UNIFORM_MAX
#define BODIES_UNIFORM_MAX 64
#endif

uniform vec4 uBodiesU[BODIES_UNIFORM_MAX];  // same records as uBodyData, for small body counts
uniform vec4 uBodyRotU[BODIES_UNIFORM_MAX];
uniform vec4 uBodyExtU[BODIES_UNIFORM_MAX];
uniform sampler2D uBodyData;     // row 0 (centre, radius + 1000 * kind), row 1 quaternion, row 2 half extents
uniform sampler2D uBodyCells;    // per fine cell: (start, count) into uBodyIndices
uniform sampler2D uBodyIndices;  // body indices, 256 per row
uniform sampler2D uBodyCoarse;   // per coarse cell: lower bound of the distance to the nearest body
uniform int   uBodyCount;
uniform vec4  uBodyBounds;       // sphere enclosing all bodies (xyz centre, w radius)
uniform vec3  uBodyGridOrigin;
uniform float uBodyGridCell;     // fine cell size; coarse cells are 4x
uniform vec3  uBodyGridDims;     // fine cells per axis
uniform vec3  uBodyCoarseDims;

const float MAT_BODY = 1000.0;   // material id = MAT_BODY + body index

bool isBody(float mat) { return mat >= MAT_BODY - 0.5; }
int bodyIndex(float mat) { return int(mat - MAT_BODY + 0.5); }

vec3 bodyColor(int i)
{
    float t = fract(float(i) * 0.618034);
    return 0.55 + 0.45 * cos(6.2831 * (t + vec3(0.0, 0.33, 0.67)));
}

// Records are read from the uniform arrays while the count fits, from the textures otherwise.
// The two paths are kept apart so the cheap one never touches a sampler.
bool bodiesFromUniforms() { return uBodyCount <= BODIES_UNIFORM_MAX; }

vec4 bodyPos(int i) // xyz centre, w radius + 1000 * kind
{
    if (bodiesFromUniforms()) return uBodiesU[i];
    return texelFetch(uBodyData, ivec2(i, 0), 0);
}

int bodyKind(int i) { return int(bodyPos(i).w * 0.001); } // 0 sphere, 1 box (8 pts), 2 box (20 pts), 3 box (27 pts)
bool isBoxBody(int i) { return bodyKind(i) > 0; }

// Rotates v by the conjugate of q: world space into the body's local frame.
vec3 bodyLocal(vec4 q, vec3 v)
{
    vec3 u = -q.xyz;
    vec3 t = 2.0 * cross(u, v);
    return v + q.w * t + cross(u, t);
}

// Signed distance to a body record: a sphere, or a rounded box in its own orientation.
float sdBodyRecord(vec4 b, vec4 rot, vec3 ext, vec3 p)
{
    int kind = int(b.w * 0.001);
    float r = b.w - 1000.0 * float(kind);
    if (kind == 0) return length(p - b.xyz) - r;
    vec3 l = bodyLocal(rot, p - b.xyz);
    vec3 q = abs(l) - (ext - r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdBodyUniform(int i, vec3 p)
{
    vec4 b = uBodiesU[i];
    if (b.w < 1000.0) return length(p - b.xyz) - b.w; // sphere: one record read
    return sdBodyRecord(b, uBodyRotU[i], uBodyExtU[i].xyz, p);
}

float sdBodyTexture(int i, vec3 p)
{
    vec4 b = texelFetch(uBodyData, ivec2(i, 0), 0);
    if (b.w < 1000.0) return length(p - b.xyz) - b.w;
    return sdBodyRecord(b, texelFetch(uBodyData, ivec2(i, 1), 0), texelFetch(uBodyData, ivec2(i, 2), 0).xyz, p);
}

// Signed distance to body i, whichever source the records come from.
float sdBody(int i, vec3 p)
{
    if (bodiesFromUniforms()) return sdBodyUniform(i, p);
    return sdBodyTexture(i, p);
}

vec2 sdBodies(vec3 p)
{
#ifdef SATYR_QUERY_PASS
    return vec2(1e10, MAT_BODY);
#else
    if (uBodyCount == 0) return vec2(1e10, MAT_BODY);

    // Far away: the bounding sphere is a safe lower bound and far cheaper than the grid.
    float bound = length(p - uBodyBounds.xyz) - uBodyBounds.w;
    float margin = max(BODIES_MARGIN, 2.0 * pixelRadius() * length(p - uCamPos));
    if (bound > margin) return vec2(bound, MAT_BODY);

    float best = 1e10;
    int bestIndex = 0;

    if (bodiesFromUniforms()) {
        for (int i = 0; i < BODIES_UNIFORM_MAX; ++i) {
            if (i >= uBodyCount) break;
            float d = sdBodyUniform(i, p);
            if (d < best) {
                best = d;
                bestIndex = i;
            }
        }
        return vec2(best, MAT_BODY + float(bestIndex));
    }

    ivec3 dims = ivec3(uBodyGridDims + 0.5);
    vec3 gridSize = vec3(dims) * uBodyGridCell;
    vec3 g = (p - uBodyGridOrigin) / uBodyGridCell;
    ivec3 cell = clamp(ivec3(floor(g)), ivec3(0), dims - 1);

    // Far field, three lower bounds combined (the largest valid one wins):
    //  * nothing missing from the cell's list is closer than one cell;
    //  * outside the grid box every body is at least a cell inside it, so the distance to the
    //    box plus one cell;
    //  * the coarse layer: the distance bound at each of the 8 surrounding coarse cell centres
    //    carried to p (the field is 1-Lipschitz, so subtracting the offset keeps it valid).
    vec3 outside = max(max(uBodyGridOrigin - p, p - (uBodyGridOrigin + gridSize)), vec3(0.0));
    best = max(uBodyGridCell, length(outside) + uBodyGridCell);

    float coarseSize = 4.0 * uBodyGridCell;
    ivec3 cdims = ivec3(uBodyCoarseDims + 0.5);
    vec3 cg = (p - uBodyGridOrigin) / coarseSize - 0.5;
    ivec3 c0 = clamp(ivec3(floor(cg)), ivec3(0), cdims - 1);
    ivec3 c1 = clamp(c0 + 1, ivec3(0), cdims - 1);
    for (int corner = 0; corner < 8; ++corner) {
        ivec3 cc = ivec3((corner & 1) != 0 ? c1.x : c0.x, (corner & 2) != 0 ? c1.y : c0.y, (corner & 4) != 0 ? c1.z : c0.z);
        int ci = (cc.z * cdims.y + cc.y) * cdims.x + cc.x;
        vec3 ccenter = uBodyGridOrigin + (vec3(cc) + 0.5) * coarseSize;
        float bound = texelFetch(uBodyCoarse, ivec2(ci & 255, ci >> 8), 0).x - length(p - ccenter);
        best = max(best, bound);
    }

    // Exact distances to the bodies listed for this cell.
    int c = (cell.z * dims.y + cell.y) * dims.x + cell.x;
    vec4 entry = texelFetch(uBodyCells, ivec2(c & 255, c >> 8), 0);
    int start = int(entry.x + 0.5);
    int count = int(entry.y + 0.5);
    for (int k = 0; k < count; ++k) {
        int e = start + k;
        int i = int(texelFetch(uBodyIndices, ivec2(e & 255, e >> 8), 0).x + 0.5);
        float d = sdBodyTexture(i, p);
        if (d < best) {
            best = d;
            bestIndex = i;
        }
    }
    return vec2(best, MAT_BODY + float(bestIndex));
#endif
}
