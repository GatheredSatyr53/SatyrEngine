// Uploads the physics bodies and their spatial grid to textures for shaders/common/bodies.glsl.
//
// Layout (all texelFetch'ed, no filtering):
//   uBodyData    kMaxBodies x 3, RGBA32F: row 0 (centre, radius + 1000 * kind), row 1 quaternion,
//                row 2 half extents; the same records go out as uniform arrays while the count
//                is at most BODIES_UNIFORM_MAX (64), where a plain loop beats the grid
//   uBodyCells   256 x rows, RGBA32F: per fine grid cell (start, count) into the index list
//   uBodyIndices 256 x rows, R32F: body indices
//   uBodyCoarse  256 x rows, R32F: per coarse cell, lower bound of the distance to the nearest body
#pragma once

#include "engine/Body.h"
#include "engine/BodyGrid.h"
#include "engine/gl.h"

#include <vector>

namespace satyr {

class Shader;

class BodyTextures {
public:
    BodyTextures() = default;
    ~BodyTextures();
    BodyTextures(const BodyTextures&) = delete;
    BodyTextures& operator=(const BodyTextures&) = delete;

    bool init();

    // Rebuilds the grid for `bodies` and uploads everything.
    void update(const std::vector<Body>& bodies);
    // Binds the textures to units starting at `firstUnit` and sets the sampler and grid uniforms.
    void bind(Shader& shader, int firstUnit) const;

    const BodyGrid& grid() const { return m_grid; }

private:
    static constexpr int kWidth = 256;
    static constexpr int kUniformBodies = 64; // must match BODIES_UNIFORM_MAX in bodies.glsl

    void destroy();
    void ensureRows(GLuint tex, int& rows, int needed, GLenum internalFormat, GLenum format);

    GLuint m_data = 0, m_cells = 0, m_indices = 0, m_coarse = 0;
    int m_cellRows = 0, m_indexRows = 0, m_coarseRows = 0;
    int m_count = 0;
    vec3 m_boundsCenter;
    float m_boundsRadius = 0.0f;
    BodyGrid m_grid;
    std::vector<float> m_scratch;
    std::vector<float> m_records; // packed body rows, also uploaded as uniforms for small counts
};

} // namespace satyr
