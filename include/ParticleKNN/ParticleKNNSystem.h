#pragma once

#include "ParticleKNN/ParticleKNN.h"
#include "Core/SpatialGrid.h"
#include <SDL3/SDL.h>
#include <random>
#include <cmath>
#include <algorithm>

class ParticleKNNSystem {
private:
    std::vector<ParticleKNN>           particles;
    KNNParameters                      params;
    std::vector<KNNAlgorithm::Connection> connections;

    // SoA position cache and spatial grid for O(n) neighbor search
    std::vector<float> soaX_, soaY_;
    SpatialGrid        grid_;

    // Scanline cache for filled-circle rendering (recomputed when size changes)
    std::vector<int> circleScanlines_;
    int              cachedCircleRadius_ = -1;

    static std::random_device rd;
    static std::mt19937       gen;

    void rebuildCircleCache(int r) {
        if (r == cachedCircleRadius_) return;
        cachedCircleRadius_ = r;
        circleScanlines_.resize(2 * r + 1);
        for (int y = -r; y <= r; ++y)
            circleScanlines_[y + r] = (int)std::sqrt((float)(r * r - y * y));
    }

    void drawFilledCircle(SDL_Renderer* renderer, float cx, float cy, float radius) {
        const int r = (int)radius;
        rebuildCircleCache(r);
        for (int y = -r; y <= r; ++y)
            SDL_RenderLine(renderer,
                           cx - circleScanlines_[y + r], cy + y,
                           cx + circleScanlines_[y + r], cy + y);
    }

public:
    ParticleKNNSystem() {
        gen.seed(rd());
        params.updateSquared();
    }

    void generate(int count, int screenWidth, int screenHeight) {
        particles.clear();
        particles.reserve(count);

        std::uniform_real_distribution<float> posX(0.0f, (float)screenWidth);
        std::uniform_real_distribution<float> posY(0.0f, (float)screenHeight);

        for (int i = 0; i < count; ++i)
            particles.emplace_back(posX(gen), posY(gen));
    }

    void update(float deltaTime, int screenWidth, int screenHeight) {
        const int n = (int)particles.size();

        for (auto& p : particles)
            p.update(deltaTime, screenWidth, screenHeight);

        // Extract positions to SoA for grid
        soaX_.resize(n);
        soaY_.resize(n);
        for (int i = 0; i < n; ++i) {
            soaX_[i] = particles[i].position.x();
            soaY_[i] = particles[i].position.y();
        }

        // Build grid: cellSize = maxDistance guarantees all neighbors
        // within range are in the 3x3 cell neighborhood
        const float cs   = std::max(params.maxDistance, 1.0f);
        const int   cols = std::max(1, (int)((float)screenWidth  / cs) + 2);
        const int   rows = std::max(1, (int)((float)screenHeight / cs) + 2);
        grid_.build(soaX_.data(), soaY_.data(), n, cs, cols, rows);

        // Find K-nearest connections using the grid — O(n * avg_cell_pop)
        connections.clear();

        std::vector<std::pair<float, int>> neighbors; // (distSq, index)
        for (int i = 0; i < n; ++i) {
            neighbors.clear();

            const int cx0 = std::clamp((int)(soaX_[i] / cs), 0, cols - 1);
            const int cy0 = std::clamp((int)(soaY_[i] / cs), 0, rows - 1);

            grid_.forEachNeighbor(cx0, cy0, cols, rows, [&](int j) {
                if (j <= i) return; // each undirected edge once
                const float dx = soaX_[i] - soaX_[j];
                const float dy = soaY_[i] - soaY_[j];
                const float d2 = dx * dx + dy * dy;
                if (d2 < params.maxDistanceSq)
                    neighbors.push_back({d2, j});
            });

            if (neighbors.empty()) continue;

            // Partial sort: only the K closest matter
            const int k = std::min(params.maxConnections, (int)neighbors.size());
            std::partial_sort(neighbors.begin(), neighbors.begin() + k,
                              neighbors.end());

            for (int m = 0; m < k; ++m)
                connections.push_back({i, neighbors[m].second,
                                       std::sqrt(neighbors[m].first)});
        }
    }

    void draw(SDL_Renderer* renderer) {
        // Connections first (lines with distance-based alpha)
        for (const auto& conn : connections) {
            const auto& p1 = particles[conn.particleA].position;
            const auto& p2 = particles[conn.particleB].position;

            const float alpha    = 1.0f - (conn.distance / params.maxDistance);
            const Uint8 alphaVal = (Uint8)(alpha * 150.0f);

            SDL_SetRenderDrawColor(renderer, 100, 150, 200, alphaVal);
            SDL_RenderLine(renderer, p1.x(), p1.y(), p2.x(), p2.y());
        }

        // Particles as filled circles
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (const auto& p : particles)
            drawFilledCircle(renderer, p.position.x(), p.position.y(), p.size);
    }

    int            getCount()           const { return (int)particles.size(); }
    int            getConnectionCount() const { return (int)connections.size(); }
    KNNParameters& getParameters()            { return params; }
};

inline std::random_device ParticleKNNSystem::rd;
inline std::mt19937       ParticleKNNSystem::gen(ParticleKNNSystem::rd());
