// Uniform grid over the physics bodies, rebuilt every frame.
//
// Two layers share one bounding box:
//   * fine cells (cellSize): each stores the indices of the bodies whose bounding sphere comes
//     within one cell of it (a "dilated" list). A point inside the cell therefore sees every
//     body closer than cellSize exactly, and anything missing is at least cellSize away.
//   * coarse cells (4 x cellSize): each stores a lower bound of the distance from its centre to
//     the nearest body, so far from the bodies the field keeps a large, honest value instead of
//     a flat cellSize cap (which soft shadows would read as a nearby occluder).
// The CPU uses the fine lists as the collision broad phase; the GPU gets both layers as
// textures and evaluates sdBodies() as  min(exact over the cell's list, far-field bound).
#pragma once

#include "engine/Body.h"
#include "engine/Math.h"

#include <vector>

namespace satyr {

class BodyGrid {
public:
    // Builds the grid for `bodies`; `inflate` enlarges every bounding sphere (use the distance
    // a body may travel before the next rebuild so the broad phase stays conservative).
    void build(const std::vector<Body>& bodies, float minCellSize = 0.5f, float inflate = 0.0f, int maxCells = 32768);

    bool empty() const { return m_cellCount == 0; }
    const vec3& origin() const { return m_origin; }
    float cellSize() const { return m_cellSize; }
    float coarseCellSize() const { return m_cellSize * kCoarseFactor; }
    int nx() const { return m_nx; }
    int ny() const { return m_ny; }
    int nz() const { return m_nz; }
    int coarseNx() const { return m_cnx; }
    int coarseNy() const { return m_cny; }
    int coarseNz() const { return m_cnz; }

    // Fine cell table: per cell (start, count) into indices(); coarse distances per coarse cell.
    const std::vector<int>& cellStart() const { return m_cellStart; }
    const std::vector<int>& cellCount() const { return m_cellCountPerCell; }
    const std::vector<int>& indices() const { return m_indices; }
    const std::vector<float>& coarseDistance() const { return m_coarseDistance; }

    // Calls fn(j) for every body j whose bounding sphere may overlap body i's (j may repeat).
    template <typename Fn>
    void forEachCandidate(const std::vector<Body>& bodies, size_t i, Fn&& fn) const
    {
        if (m_cellCount == 0) return;
        const Body& b = bodies[i];
        const float r = b.boundingRadius() + m_inflate;
        int x0, y0, z0, x1, y1, z1;
        cellRange(b.position, r, x0, y0, z0, x1, y1, z1);
        for (int z = z0; z <= z1; ++z)
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    const int c = cellIndex(x, y, z);
                    const int start = m_cellStart[static_cast<size_t>(c)];
                    const int count = m_cellCountPerCell[static_cast<size_t>(c)];
                    for (int k = 0; k < count; ++k) fn(m_indices[static_cast<size_t>(start + k)]);
                }
    }

    static constexpr int kCoarseFactor = 4;

private:
    int cellIndex(int x, int y, int z) const { return (z * m_ny + y) * m_nx + x; }
    void cellRange(const vec3& center, float radius, int& x0, int& y0, int& z0, int& x1, int& y1, int& z1) const;

    vec3 m_origin;
    float m_cellSize = 0.5f;
    float m_inflate = 0.0f;
    int m_nx = 0, m_ny = 0, m_nz = 0, m_cellCount = 0;
    int m_cnx = 0, m_cny = 0, m_cnz = 0;
    std::vector<int> m_cellStart;
    std::vector<int> m_cellCountPerCell;
    std::vector<int> m_indices;
    std::vector<float> m_coarseDistance;
};

} // namespace satyr
