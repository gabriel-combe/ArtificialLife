#pragma once

#include <vector>
#include <algorithm>

// Uniform-cell spatial hash grid for fast neighbor queries.
// Build once per frame with build(), then query with forEachNeighbor().
// Designed for 2D float positions; cell size must be >= query radius.
struct SpatialGrid {
    std::vector<int> count, start, idx, fill;

    // offX/offY shift the coordinate origin (use marginX/marginY for wrapped worlds).
    void build(const float* px, const float* py, int n,
               float cellSize, int cols, int rows,
               float offX = 0.f, float offY = 0.f)
    {
        const int cells = cols * rows;
        count.assign(cells, 0);
        fill.assign(cells, 0);
        start.resize(cells + 1);
        idx.resize(n);

        for (int j = 0; j < n; ++j) {
            const int cx = std::clamp((int)((px[j] - offX) / cellSize), 0, cols - 1);
            const int cy = std::clamp((int)((py[j] - offY) / cellSize), 0, rows - 1);
            ++count[cy * cols + cx];
        }

        start[0] = 0;
        for (int i = 0; i < cells; ++i)
            start[i + 1] = start[i] + count[i];

        for (int j = 0; j < n; ++j) {
            const int cx   = std::clamp((int)((px[j] - offX) / cellSize), 0, cols - 1);
            const int cy   = std::clamp((int)((py[j] - offY) / cellSize), 0, rows - 1);
            const int cell = cy * cols + cx;
            idx[start[cell] + fill[cell]++] = j;
        }
    }

    // 3x3 neighborhood — clamps at borders (non-wrapping worlds).
    template<typename Func>
    void forEachNeighbor(int cx0, int cy0, int cols, int rows,
                         const Func& func) const
    {
        for (int dy = -1; dy <= 1; ++dy) {
            const int ny = cy0 + dy;
            if (ny < 0 || ny >= rows) continue;
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = cx0 + dx;
                if (nx < 0 || nx >= cols) continue;
                const int cell = ny * cols + nx;
                for (int k = start[cell], end = start[cell + 1]; k < end; ++k)
                    func(idx[k]);
            }
        }
    }

    // 3x3 neighborhood with toroidal wrapping — for worlds with wrapped boundaries.
    template<typename Func>
    void forEachNeighborWrapped(int cx0, int cy0, int cols, int rows,
                                const Func& func) const
    {
        for (int dy = -1; dy <= 1; ++dy) {
            const int ny = ((cy0 + dy) % rows + rows) % rows;
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = ((cx0 + dx) % cols + cols) % cols;
                const int cell = ny * cols + nx;
                for (int k = start[cell], end = start[cell + 1]; k < end; ++k)
                    func(idx[k]);
            }
        }
    }
};
