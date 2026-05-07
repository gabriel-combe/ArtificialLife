#pragma once

#include <SDL3/SDL.h>
#include "Boids/Boid.h"
#include "Core/SpatialGrid.h"
#include <vector>
#include <random>
#include <algorithm>

class BoidSystem {
private:
    std::vector<Boid> boids;
    BoidParameters    params;

    // SoA position cache for spatial grid (rebuilt each update)
    std::vector<float> soaX_, soaY_;
    SpatialGrid        grid_;

    static std::random_device rd;
    static std::mt19937       gen;

public:
    BoidSystem() {
        gen.seed(rd());
        params.updateSquaredRadii();
    }

    void generate(int count, int screenWidth, int screenHeight) {
        boids.clear();

        std::uniform_real_distribution<float> posX(0.0f, (float)screenWidth);
        std::uniform_real_distribution<float> posY(0.0f, (float)screenHeight);
        std::uniform_real_distribution<float> vel(-80.0f, 80.0f);

        boids.reserve(count);
        for (int i = 0; i < count; ++i)
            boids.emplace_back(posX(gen), posY(gen), vel(gen), vel(gen));
    }

    void update(float deltaTime, int screenWidth, int screenHeight) {
        const int n = (int)boids.size();
        if (n == 0) return;

        // Extract positions to SoA for grid construction
        soaX_.resize(n);
        soaY_.resize(n);
        for (int i = 0; i < n; ++i) {
            soaX_[i] = boids[i].position.x();
            soaY_[i] = boids[i].position.y();
        }

        // Grid cell size = largest perception radius (guarantees all relevant
        // neighbors fit in the 3x3 cell neighborhood)
        const float cs = std::max({params.separationRadius,
                                   params.alignmentRadius,
                                   params.cohesionRadius,
                                   1.0f});
        const int cols = std::max(1, (int)((float)screenWidth  / cs) + 2);
        const int rows = std::max(1, (int)((float)screenHeight / cs) + 2);
        grid_.build(soaX_.data(), soaY_.data(), n, cs, cols, rows);

        // Single-pass force computation: one grid query per boid accumulates
        // separation, alignment, and cohesion simultaneously
        for (int i = 0; i < n; ++i) {
            Eigen::Vector2f sepSteer(0.f, 0.f);
            Eigen::Vector2f alignSum(0.f, 0.f);
            Eigen::Vector2f cohSum  (0.f, 0.f);
            int sepCount = 0, alignCount = 0, cohCount = 0;

            const int cx0 = std::clamp((int)(soaX_[i] / cs), 0, cols - 1);
            const int cy0 = std::clamp((int)(soaY_[i] / cs), 0, rows - 1);

            grid_.forEachNeighbor(cx0, cy0, cols, rows, [&](int j) {
                if (j == i) return;
                const float distSq =
                    (boids[i].position - boids[j].position).squaredNorm();
                if (distSq < 0.01f) return;

                if (distSq < params.separationRadiusSq) {
                    Eigen::Vector2f diff = boids[i].position - boids[j].position;
                    diff.normalize();
                    sepSteer += diff;
                    ++sepCount;
                }
                if (distSq < params.alignmentRadiusSq) {
                    alignSum += boids[j].velocity;
                    ++alignCount;
                }
                if (distSq < params.cohesionRadiusSq) {
                    cohSum += boids[j].position;
                    ++cohCount;
                }
            });

            const Boid& b = boids[i];

            if (sepCount > 0) {
                sepSteer /= (float)sepCount;
                if (sepSteer.squaredNorm() > 0.01f) {
                    sepSteer.normalize();
                    sepSteer *= b.maxSpeed;
                    sepSteer -= b.velocity;
                    sepSteer = BoidAlgorithm::limit(sepSteer, b.maxForce);
                }
                boids[i].applyForce(sepSteer * params.separationWeight);
            }

            if (alignCount > 0) {
                alignSum /= (float)alignCount;
                alignSum.normalize();
                alignSum *= b.maxSpeed;
                Eigen::Vector2f steer =
                    BoidAlgorithm::limit(alignSum - b.velocity, b.maxForce);
                boids[i].applyForce(steer * params.alignmentWeight);
            }

            if (cohCount > 0) {
                cohSum /= (float)cohCount;
                Eigen::Vector2f desired = cohSum - b.position;
                desired.normalize();
                desired *= b.maxSpeed;
                Eigen::Vector2f steer =
                    BoidAlgorithm::limit(desired - b.velocity, b.maxForce);
                boids[i].applyForce(steer * params.cohesionWeight);
            }
        }

        // Physics update — separate pass intentional to prevent same-frame bias
        for (auto& boid : boids)
            boid.update(deltaTime, screenWidth, screenHeight);
    }

    void draw(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);

        for (const auto& boid : boids) {
            Eigen::Vector2f p1, p2, p3;
            boid.getTrianglePoints(p1, p2, p3);

            SDL_RenderLine(renderer, p1.x(), p1.y(), p2.x(), p2.y());
            SDL_RenderLine(renderer, p2.x(), p2.y(), p3.x(), p3.y());
            SDL_RenderLine(renderer, p3.x(), p3.y(), p1.x(), p1.y());
        }
    }

    void drawDirectionVectors(SDL_Renderer* renderer, float length = 30.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

        for (const auto& boid : boids) {
            float angle = boid.getHeading();
            float endX  = boid.position.x() + std::cos(angle) * length;
            float endY  = boid.position.y() + std::sin(angle) * length;
            SDL_RenderLine(renderer,
                           boid.position.x(), boid.position.y(), endX, endY);
        }
    }

    void drawSteeringVectors(SDL_Renderer* renderer, float scale = 5.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);

        for (const auto& boid : boids) {
            if (boid.acceleration.norm() < 0.01f) continue;
            float endX = boid.position.x() + boid.acceleration.x() * scale;
            float endY = boid.position.y() + boid.acceleration.y() * scale;
            SDL_RenderLine(renderer,
                           boid.position.x(), boid.position.y(), endX, endY);
        }
    }

    int              getCount()      const { return (int)boids.size(); }
    BoidParameters&  getParameters()       { return params; }
};

inline std::random_device BoidSystem::rd;
inline std::mt19937       BoidSystem::gen(BoidSystem::rd());
