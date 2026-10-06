#include "engine/BodyTextures.h"
#include "engine/Shader.h"

#include <algorithm>

namespace satyr {

BodyTextures::~BodyTextures() { destroy(); }

namespace {
GLuint makeTexture()
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}
} // namespace

bool BodyTextures::init()
{
    m_data = makeTexture();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, kMaxBodies, 3, 0, GL_RGBA, GL_FLOAT, nullptr);
    m_cells = makeTexture();
    m_indices = makeTexture();
    m_coarse = makeTexture();
    ensureRows(m_cells, m_cellRows, 16, GL_RGBA32F, GL_RGBA);
    ensureRows(m_indices, m_indexRows, 16, GL_R32F, GL_RED);
    ensureRows(m_coarse, m_coarseRows, 2, GL_R32F, GL_RED);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return gl::checkErrors("BodyTextures::init");
}

void BodyTextures::destroy()
{
    if (!glDeleteTextures) return;
    const GLuint tex[4] = {m_data, m_cells, m_indices, m_coarse};
    for (GLuint t : tex)
        if (t) glDeleteTextures(1, &t);
    m_data = m_cells = m_indices = m_coarse = 0;
}

void BodyTextures::ensureRows(GLuint tex, int& rows, int needed, GLenum internalFormat, GLenum format)
{
    if (needed <= rows) return;
    rows = std::max(needed, rows * 2);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), kWidth, rows, 0, format, GL_FLOAT, nullptr);
}

void BodyTextures::update(const std::vector<Body>& bodies)
{
    m_count = static_cast<int>(std::min<size_t>(bodies.size(), static_cast<size_t>(kMaxBodies)));
    if (m_count == 0) return;

    // Bounding sphere of everything, for the cheap far-away early-out in the shader.
    vec3 sum{0.0f, 0.0f, 0.0f};
    for (int i = 0; i < m_count; ++i) sum += bodies[static_cast<size_t>(i)].position;
    m_boundsCenter = sum / static_cast<float>(m_count);
    m_boundsRadius = 0.0f;
    for (int i = 0; i < m_count; ++i) {
        const Body& b = bodies[static_cast<size_t>(i)];
        m_boundsRadius = std::max(m_boundsRadius, length(b.position - m_boundsCenter) + b.boundingRadius());
    }

    // Body rows: (centre, radius + 1000 * kind), quaternion, half extents.
    m_records.assign(static_cast<size_t>(m_count) * 4 * 3, 0.0f);
    for (int i = 0; i < m_count; ++i) {
        const Body& b = bodies[static_cast<size_t>(i)];
        float* pos = &m_records[static_cast<size_t>(i) * 4];
        float* rot = &m_records[static_cast<size_t>(m_count + i) * 4];
        float* ext = &m_records[static_cast<size_t>(2 * m_count + i) * 4];
        const float kind = b.shape == Shape::Sphere ? 0.0f : (b.shape == Shape::Box ? 1.0f : (b.shape == Shape::Box20 ? 2.0f : 3.0f));
        pos[0] = b.position.x; pos[1] = b.position.y; pos[2] = b.position.z; pos[3] = b.radius + 1000.0f * kind;
        rot[0] = b.orientation.x; rot[1] = b.orientation.y; rot[2] = b.orientation.z; rot[3] = b.orientation.w;
        ext[0] = b.halfExtents.x; ext[1] = b.halfExtents.y; ext[2] = b.halfExtents.z; ext[3] = 0.0f;
    }
    // Small counts go out as uniform arrays in bind(); the shader never touches the textures
    // then, and leaving them alone also spares software renderers a sync per frame.
    if (m_count <= kUniformBodies) return;

    glBindTexture(GL_TEXTURE_2D, m_data);
    for (int row = 0; row < 3; ++row)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, row, m_count, 1, GL_RGBA, GL_FLOAT, &m_records[static_cast<size_t>(row * m_count) * 4]);

    m_grid.build(bodies, 0.5f, 0.0f);

    // Fine cells: (start, count).
    const std::vector<int>& starts = m_grid.cellStart();
    const std::vector<int>& counts = m_grid.cellCount();
    const int cellCount = static_cast<int>(starts.size());
    const int cellRowsNeeded = (cellCount + kWidth - 1) / kWidth;
    ensureRows(m_cells, m_cellRows, cellRowsNeeded, GL_RGBA32F, GL_RGBA);
    m_scratch.assign(static_cast<size_t>(cellRowsNeeded) * kWidth * 4, 0.0f);
    for (int c = 0; c < cellCount; ++c) {
        m_scratch[static_cast<size_t>(c) * 4 + 0] = static_cast<float>(starts[static_cast<size_t>(c)]);
        m_scratch[static_cast<size_t>(c) * 4 + 1] = static_cast<float>(counts[static_cast<size_t>(c)]);
    }
    glBindTexture(GL_TEXTURE_2D, m_cells);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kWidth, cellRowsNeeded, GL_RGBA, GL_FLOAT, m_scratch.data());

    // Index list.
    const std::vector<int>& indices = m_grid.indices();
    const int indexCount = static_cast<int>(indices.size());
    const int indexRowsNeeded = std::max(1, (indexCount + kWidth - 1) / kWidth);
    ensureRows(m_indices, m_indexRows, indexRowsNeeded, GL_R32F, GL_RED);
    m_scratch.assign(static_cast<size_t>(indexRowsNeeded) * kWidth, 0.0f);
    for (int e = 0; e < indexCount; ++e) m_scratch[static_cast<size_t>(e)] = static_cast<float>(indices[static_cast<size_t>(e)]);
    glBindTexture(GL_TEXTURE_2D, m_indices);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kWidth, indexRowsNeeded, GL_RED, GL_FLOAT, m_scratch.data());

    // Coarse distances.
    const std::vector<float>& coarse = m_grid.coarseDistance();
    const int coarseCount = static_cast<int>(coarse.size());
    const int coarseRowsNeeded = std::max(1, (coarseCount + kWidth - 1) / kWidth);
    ensureRows(m_coarse, m_coarseRows, coarseRowsNeeded, GL_R32F, GL_RED);
    m_scratch.assign(static_cast<size_t>(coarseRowsNeeded) * kWidth, 0.0f);
    std::copy(coarse.begin(), coarse.end(), m_scratch.begin());
    glBindTexture(GL_TEXTURE_2D, m_coarse);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kWidth, coarseRowsNeeded, GL_RED, GL_FLOAT, m_scratch.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void BodyTextures::bind(Shader& shader, int firstUnit) const
{
    shader.set("uBodyCount", m_count);
    shader.set("uBodyBounds", m_boundsCenter.x, m_boundsCenter.y, m_boundsCenter.z, m_boundsRadius);
    if (m_count == 0) return;

    if (m_count <= kUniformBodies) {
        shader.setVec4Array("uBodiesU[0]", &m_records[0], m_count);
        shader.setVec4Array("uBodyRotU[0]", &m_records[static_cast<size_t>(m_count) * 4], m_count);
        shader.setVec4Array("uBodyExtU[0]", &m_records[static_cast<size_t>(2 * m_count) * 4], m_count);
        return;
    }

    const GLuint tex[4] = {m_data, m_cells, m_indices, m_coarse};
    const char* names[4] = {"uBodyData", "uBodyCells", "uBodyIndices", "uBodyCoarse"};
    for (int i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(firstUnit + i));
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        shader.set(names[i], firstUnit + i);
    }
    glActiveTexture(GL_TEXTURE0);

    shader.set("uBodyGridOrigin", m_grid.origin());
    shader.set("uBodyGridCell", m_grid.cellSize());
    shader.set("uBodyGridDims", vec3(static_cast<float>(m_grid.nx()), static_cast<float>(m_grid.ny()), static_cast<float>(m_grid.nz())));
    shader.set("uBodyCoarseDims", vec3(static_cast<float>(m_grid.coarseNx()), static_cast<float>(m_grid.coarseNy()), static_cast<float>(m_grid.coarseNz())));
}

} // namespace satyr
