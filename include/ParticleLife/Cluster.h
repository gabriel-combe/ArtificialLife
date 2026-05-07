#pragma once

#include "ParticleLife.h"
#include "Core/SpatialGrid.h"

#include <SDL3/SDL.h>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

namespace ParticleLife {

class Cluster {
public:
    std::vector<float> posX, posY;
    std::vector<float> velX, velY;

private:
    Color color_;

    mutable SpatialGrid grid_;

    // Scanline half-widths for filled-circle rendering (indexed by dy + radius).
    // Recomputed only when the radius changes.
    mutable std::vector<int> scanlineCache_;
    mutable int              lastRadius_ = -1;

    void ensureScanlineCache(int radius) const {
        if (radius == lastRadius_) return;
        lastRadius_ = radius;
        scanlineCache_.resize(2 * radius + 1);
        for (int dy = -radius; dy <= radius; ++dy)
            scanlineCache_[dy + radius] = (int)sqrtf((float)(radius * radius - dy * dy));
    }

public:
    Cluster() = default;
    explicit Cluster(int /*count*/, const Color& col = Color::Random())
        : color_(col) {}

    void setColor(const Color& c) { color_ = c; }
    const Color& getColor()       const { return color_; }
    int          size()           const { return (int)posX.size(); }

    void clear() {
        posX.clear(); posY.clear();
        velX.clear(); velY.clear();
    }

    void resize(int n, float minX, float minY, float maxX, float maxY) {
        posX.resize(n); posY.resize(n);
        velX.resize(n); velY.resize(n);

        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dX(minX, maxX);
        std::uniform_real_distribution<float> dY(minY, maxY);
        std::uniform_real_distribution<float> dV(-0.5f, 0.5f);

        for (int i = 0; i < n; ++i) {
            posX[i] = dX(gen);  posY[i] = dY(gen);
            velX[i] = dV(gen);  velY[i] = dV(gen);
        }
    }

    // ── Physics ───────────────────────────────────────────────────────────
    void rule(const Cluster& other,
              float gravity,  float radius,
              float viscosity, float worldGravity,
              float screenW,  float screenH,
              bool  wrapping = false,
              float marginX  = 0.f, float marginY = 0.f)
    {
        const int n = (int)posX.size();
        const int m = (int)other.posX.size();
        if (n == 0 || m == 0) return;

        const float g    = gravity / -100.0f;
        const float r2   = radius * radius;
        const float damp = 1.0f - viscosity;

        const float cs   = std::max(radius, 1.0f);

        // Toroidal world dimensions — particles wrap at [marginX, screenW-marginX].
        const float worldW = screenW - 2.f * marginX;
        const float worldH = screenH - 2.f * marginY;
        const float halfW  = worldW * 0.5f;
        const float halfH  = worldH * 0.5f;

        // When wrapping, build the grid in world-space ([0,worldW] x [0,worldH])
        // so forEachNeighborWrapped correctly connects opposite edges.
        int   cols, rows;
        float offX = 0.f, offY = 0.f;
        if (wrapping) {
            cols = std::max(1, (int)(worldW / cs) + 1);
            rows = std::max(1, (int)(worldH / cs) + 1);
            offX = marginX;
            offY = marginY;
        } else {
            cols = std::max(1, (int)(screenW / cs) + 2);
            rows = std::max(1, (int)(screenH / cs) + 2);
        }

        other.grid_.build(other.posX.data(), other.posY.data(), m, cs, cols, rows, offX, offY);

        const float* opx = other.posX.data();
        const float* opy = other.posY.data();

        #pragma omp parallel for schedule(static)
        for (int i = 0; i < n; ++i) {
            float fx = 0.f, fy = 0.f;
            const float px = posX[i];
            const float py = posY[i];

            const int cx0 = std::clamp((int)((px - offX) / cs), 0, cols - 1);
            const int cy0 = std::clamp((int)((py - offY) / cs), 0, rows - 1);

            auto process = [&](int j) {
                float ddx = px - opx[j];
                float ddy = py - opy[j];
                if (wrapping) {
                    // Shortest-path (toroidal) distance
                    if      (ddx >  halfW) ddx -= worldW;
                    else if (ddx < -halfW) ddx += worldW;
                    if      (ddy >  halfH) ddy -= worldH;
                    else if (ddy < -halfH) ddy += worldH;
                }
                const float d2 = ddx * ddx + ddy * ddy;
                if (d2 > 0.f && d2 < r2) {
                    const float inv_d = 1.f / sqrtf(d2);
                    fx += ddx * inv_d;
                    fy += ddy * inv_d;
                }
            };

            if (wrapping)
                other.grid_.forEachNeighborWrapped(cx0, cy0, cols, rows, process);
            else
                other.grid_.forEachNeighbor(cx0, cy0, cols, rows, process);

            velX[i] = (velX[i] + fx * g) * damp;
            velY[i] = (velY[i] + fy * g) * damp + worldGravity;
            posX[i] += velX[i];
            posY[i] += velY[i];
        }
    }

    // ── Mouse force ───────────────────────────────────────────────────────
    // strength > 0 → repulsion   (right-click)
    // strength < 0 → attraction  (left-click)
    void applyMouseForce(float mouseX, float mouseY,
                         float strength, float radius,
                         float screenW,  float screenH)
    {
        const int n = (int)posX.size();
        if (n == 0) return;

        const float r2   = radius * radius;
        const float cs   = std::max(radius, 1.0f);
        const int   cols = std::max(1, (int)(screenW / cs) + 2);
        const int   rows = std::max(1, (int)(screenH / cs) + 2);

        grid_.build(posX.data(), posY.data(), n, cs, cols, rows);

        const int cx0 = std::clamp((int)((mouseX - radius) / cs), 0, cols - 1);
        const int cx1 = std::clamp((int)((mouseX + radius) / cs), 0, cols - 1);
        const int cy0 = std::clamp((int)((mouseY - radius) / cs), 0, rows - 1);
        const int cy1 = std::clamp((int)((mouseY + radius) / cs), 0, rows - 1);

        for (int gy = cy0; gy <= cy1; ++gy) {
            for (int gx = cx0; gx <= cx1; ++gx) {
                const int cell = gy * cols + gx;
                for (int k = grid_.start[cell], end = grid_.start[cell + 1]; k < end; ++k) {
                    const int   i   = grid_.idx[k];
                    const float dx  = posX[i] - mouseX;
                    const float dy  = posY[i] - mouseY;
                    const float d2  = dx * dx + dy * dy;
                    if (d2 > 0.f && d2 < r2) {
                        const float inv_d = 1.f / sqrtf(d2);
                        velX[i] += dx * inv_d * strength;
                        velY[i] += dy * inv_d * strength;
                    }
                }
            }
        }
    }

    // ── Boundaries ────────────────────────────────────────────────────────
    void applyBoundariesWrapping(float minX, float minY, float maxX, float maxY) {
        const int n = (int)posX.size();
        for (int i = 0; i < n; ++i) {
            if      (posX[i] <= minX) posX[i] = maxX;
            else if (posX[i] >= maxX) posX[i] = minX;
            if      (posY[i] <= minY) posY[i] = maxY;
            else if (posY[i] >= maxY) posY[i] = minY;
        }
    }

    void applyBoundariesClamping(float minX, float minY, float maxX, float maxY) {
        const int n = (int)posX.size();
        for (int i = 0; i < n; ++i) {
            if      (posX[i] <= minX) posX[i] = minX;
            else if (posX[i] >= maxX) posX[i] = maxX;
            if      (posY[i] <= minY) posY[i] = minY;
            else if (posY[i] >= maxY) posY[i] = maxY;
        }
    }

    // ── Rendering ─────────────────────────────────────────────────────────
    void draw(SDL_Renderer* renderer, int radius) const {
        SDL_SetRenderDrawColor(renderer, color_.r, color_.g, color_.b, color_.a);
        const int n = (int)posX.size();

        if (radius <= 1) {
            for (int i = 0; i < n; ++i)
                SDL_RenderPoint(renderer, posX[i], posY[i]);
        } else {
            ensureScanlineCache(radius);
            for (int i = 0; i < n; ++i) {
                const int cx = (int)posX[i];
                const int cy = (int)posY[i];
                for (int dy = -radius; dy <= radius; ++dy)
                    SDL_RenderLine(renderer,
                                   cx - scanlineCache_[dy + radius], cy + dy,
                                   cx + scanlineCache_[dy + radius], cy + dy);
            }
        }
    }

    // ── Connection lines (same-cluster, spatial grid) ─────────────────────
    // Draws lines between particles within connectionRadius at ~40% opacity.
    void drawConnections(SDL_Renderer* renderer,
                         float screenW, float screenH,
                         float connectionRadius,
                         int   maxConnections) const
    {
        const int n = (int)posX.size();
        if (n < 2) return;

        const float cs   = std::max(connectionRadius, 1.0f);
        const int   cols = std::max(1, (int)(screenW / cs) + 2);
        const int   rows = std::max(1, (int)(screenH / cs) + 2);

        grid_.build(posX.data(), posY.data(), n, cs, cols, rows);

        const float r2 = connectionRadius * connectionRadius;

        SDL_SetRenderDrawColor(renderer, color_.r, color_.g, color_.b, 100);

        for (int i = 0; i < n; ++i) {
            const float px = posX[i];
            const float py = posY[i];

            const int cx0 = std::clamp((int)(px / cs), 0, cols - 1);
            const int cy0 = std::clamp((int)(py / cs), 0, rows - 1);

            int connCount = 0;

            grid_.forEachNeighbor(cx0, cy0, cols, rows, [&](int j) {
                if (j <= i || connCount >= maxConnections) return;

                const float ddx = px - posX[j];
                const float ddy = py - posY[j];
                if (ddx * ddx + ddy * ddy < r2) {
                    SDL_RenderLine(renderer,
                                   (int)px,      (int)py,
                                   (int)posX[j], (int)posY[j]);
                    ++connCount;
                }
            });
        }
    }
};

} // namespace ParticleLife
