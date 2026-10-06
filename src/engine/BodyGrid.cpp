#include "engine/BodyGrid.h"

#include <algorithm>
#include <cmath>

namespace satyr {

void BodyGrid::cellRange(const vec3& center, float radius, int& x0, int& y0, int& z0, int& x1, int& y1, int& z1) const
{
    const vec3 lo = (center - vec3(radius) - m_origin) / m_cellSize;
    const vec3 hi = (center + vec3(radius) - m_origin) / m_cellSize;
    x0 = clamp(static_cast<int>(std::floor(lo.x)), 0, m_nx - 1);
    y0 = clamp(static_cast<int>(std::floor(lo.y)), 0, m_ny - 1);
    z0 = clamp(static_cast<int>(std::floor(lo.z)), 0, m_nz - 1);
    x1 = clamp(static_cast<int>(std::floor(hi.x)), 0, m_nx - 1);
    y1 = clamp(static_cast<int>(std::floor(hi.y)), 0, m_ny - 1);
    z1 = clamp(static_cast<int>(std::floor(hi.z)), 0, m_nz - 1);
}

void BodyGrid::build(const std::vector<Body>& bodies, float minCellSize, float inflate, int maxCells)
{
    m_cellCount = 0;
    m_cellStart.clear();
    m_cellCountPerCell.clear();
    m_indices.clear();
    m_coarseDistance.clear();
    m_inflate = std::max(inflate, 0.0f);
    if (bodies.empty()) return;

    // Bounding box of all bodies, padded by one cell so every body sits strictly inside.
    vec3 lo = bodies[0].position, hi = bodies[0].position;
    for (const Body& b : bodies) {
        const float r = b.boundingRadius() + m_inflate;
        lo = vmin(lo, b.position - vec3(r));
        hi = vmax(hi, b.position + vec3(r));
    }
    const vec3 extent = hi - lo;
    m_cellSize = std::max(minCellSize, 1e-3f);
    // Keep the table bounded: grow the cells if the bodies are spread over a large volume.
    const float volume = std::max(extent.x, m_cellSize) * std::max(extent.y, m_cellSize) * std::max(extent.z, m_cellSize);
    const float minForBudget = std::cbrt(volume / static_cast<float>(std::max(maxCells, 8)));
    m_cellSize = std::max(m_cellSize, minForBudget);

    // Dimensions in whole coarse cells so the two layers line up; one cell of padding around.
    const int coarse = kCoarseFactor;
    auto dim = [&](float e) {
        const int fine = static_cast<int>(std::ceil(e / m_cellSize)) + 2;
        return ((fine + coarse - 1) / coarse) * coarse;
    };
    m_nx = dim(extent.x);
    m_ny = dim(extent.y);
    m_nz = dim(extent.z);
    m_origin = lo - vec3(m_cellSize);
    m_cellCount = m_nx * m_ny * m_nz;
    m_cnx = m_nx / coarse;
    m_cny = m_ny / coarse;
    m_cnz = m_nz / coarse;

    // Counting sort of body indices into the cells their (dilated) bounding spheres overlap.
    m_cellCountPerCell.assign(static_cast<size_t>(m_cellCount), 0);
    m_cellStart.assign(static_cast<size_t>(m_cellCount), 0);
    const float dilation = m_cellSize;
    for (const Body& b : bodies) {
        int x0, y0, z0, x1, y1, z1;
        cellRange(b.position, b.boundingRadius() + m_inflate + dilation, x0, y0, z0, x1, y1, z1);
        for (int z = z0; z <= z1; ++z)
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                    ++m_cellCountPerCell[static_cast<size_t>(cellIndex(x, y, z))];
    }
    int total = 0;
    for (int c = 0; c < m_cellCount; ++c) {
        m_cellStart[static_cast<size_t>(c)] = total;
        total += m_cellCountPerCell[static_cast<size_t>(c)];
    }
    m_indices.assign(static_cast<size_t>(total), 0);
    std::vector<int> fill(m_cellStart);
    for (size_t i = 0; i < bodies.size(); ++i) {
        const Body& b = bodies[i];
        int x0, y0, z0, x1, y1, z1;
        cellRange(b.position, b.boundingRadius() + m_inflate + dilation, x0, y0, z0, x1, y1, z1);
        for (int z = z0; z <= z1; ++z)
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x)
                    m_indices[static_cast<size_t>(fill[static_cast<size_t>(cellIndex(x, y, z))]++)] = static_cast<int>(i);
    }

    // Coarse layer: lower bound of the distance from each coarse cell centre to the nearest body
    // (bounding spheres, so it is cheap and conservative).
    const float coarseSize = coarseCellSize();
    m_coarseDistance.assign(static_cast<size_t>(m_cnx * m_cny * m_cnz), 1e9f);
    for (int z = 0; z < m_cnz; ++z) {
        for (int y = 0; y < m_cny; ++y) {
            for (int x = 0; x < m_cnx; ++x) {
                const vec3 center = m_origin + vec3((x + 0.5f) * coarseSize, (y + 0.5f) * coarseSize, (z + 0.5f) * coarseSize);
                float best = 1e9f;
                for (const Body& b : bodies) {
                    const float d = length(b.position - center) - b.boundingRadius();
                    if (d < best) best = d;
                }
                m_coarseDistance[static_cast<size_t>((z * m_cny + y) * m_cnx + x)] = best;
            }
        }
    }
}

} // namespace satyr
