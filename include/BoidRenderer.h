#pragma once

#include <SDL3/SDL.h>
#include "Boid.h"
#include <vector>
#include <random>

// Boid system - manages boid creation, update, and rendering
class BoidSystem {
private:
    std::vector<Boid> boids;
    BoidParameters params;
    
    static std::random_device rd;
    static std::mt19937 gen;
    
public:
    BoidSystem() {
        gen.seed(rd());
        params.updateSquaredRadii(); // Initialize squared radii
    }
    
    // Generate random boids
    void generate(int count, int screenWidth, int screenHeight) {
        boids.clear();
        
        std::uniform_real_distribution<float> posX(0.0f, static_cast<float>(screenWidth));
        std::uniform_real_distribution<float> posY(0.0f, static_cast<float>(screenHeight));
        std::uniform_real_distribution<float> vel(-80.0f, 80.0f);
        
        for (int i = 0; i < count; ++i) {
            float x = posX(gen);
            float y = posY(gen);
            float vx = vel(gen);
            float vy = vel(gen);
            
            boids.emplace_back(x, y, vx, vy);
        }
    }
    
    // Update all boids
    void update(float deltaTime, int screenWidth, int screenHeight) {
        // Apply boid forces
        for (auto& boid : boids) {
            BoidAlgorithm::applyBoidForces(boid, boids, params);
        }
        
        // Update physics
        for (auto& boid : boids) {
            boid.update(deltaTime, screenWidth, screenHeight);
        }
    }
    
    // Draw all boids
    void draw(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255); // Cyan
        
        for (const auto& boid : boids) {
            Eigen::Vector2f p1, p2, p3;
            boid.getTrianglePoints(p1, p2, p3);
            
            SDL_RenderLine(renderer, p1.x(), p1.y(), p2.x(), p2.y());
            SDL_RenderLine(renderer, p2.x(), p2.y(), p3.x(), p3.y());
            SDL_RenderLine(renderer, p3.x(), p3.y(), p1.x(), p1.y());
        }
    }
    
    // Draw direction vectors
    void drawDirectionVectors(SDL_Renderer* renderer, float length = 30.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow
        
        for (const auto& boid : boids) {
            float angle = boid.getHeading();
            float endX = boid.position.x() + std::cos(angle) * length;
            float endY = boid.position.y() + std::sin(angle) * length;
            
            SDL_RenderLine(renderer, boid.position.x(), boid.position.y(), endX, endY);
        }
    }
    
    // Draw steering vectors
    void drawSteeringVectors(SDL_Renderer* renderer, float scale = 5.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255); // Magenta
        
        for (const auto& boid : boids) {
            if (boid.acceleration.norm() < 0.01f) continue;
            
            float endX = boid.position.x() + boid.acceleration.x() * scale;
            float endY = boid.position.y() + boid.acceleration.y() * scale;
            
            SDL_RenderLine(renderer, boid.position.x(), boid.position.y(), endX, endY);
        }
    }
    
    // Get boid count
    int getCount() const { return static_cast<int>(boids.size()); }
    
    // Get parameters for modification
    BoidParameters& getParameters() { return params; }
};

// Static member initialization
inline std::random_device BoidSystem::rd;
inline std::mt19937 BoidSystem::gen(BoidSystem::rd());