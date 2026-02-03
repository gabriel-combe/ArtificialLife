#pragma once

#include "ParticleKNN/ParticleKNN.h"
#include <SDL3/SDL.h>
#include <random>
#include <cmath>

// Particle system manager
class ParticleKNNSystem {
private:
    std::vector<ParticleKNN> particles;
    KNNParameters params;
    std::vector<KNNAlgorithm::Connection> connections;
    
    static std::random_device rd;
    static std::mt19937 gen;
    
    // Helper function to draw a filled circle
    void drawFilledCircle(SDL_Renderer* renderer, float cx, float cy, float radius) {
        int r = static_cast<int>(radius);
        for (int y = -r; y <= r; y++) {
            int x = static_cast<int>(std::sqrt(r * r - y * y));
            SDL_RenderLine(renderer, cx - x, cy + y, cx + x, cy + y);
        }
    }
    
public:
    ParticleKNNSystem() {
        gen.seed(rd());
        params.updateSquared();
    }
    
    // Generate random particles
    void generate(int count, int screenWidth, int screenHeight) {
        particles.clear();
        
        std::uniform_real_distribution<float> posX(0.0f, static_cast<float>(screenWidth));
        std::uniform_real_distribution<float> posY(0.0f, static_cast<float>(screenHeight));
        
        for (int i = 0; i < count; ++i) {
            float x = posX(gen);
            float y = posY(gen);
            particles.emplace_back(x, y);
        }
    }
    
    // Update all particles
    void update(float deltaTime, int screenWidth, int screenHeight) {
        for (auto& particle : particles) {
            particle.update(deltaTime, screenWidth, screenHeight);
        }
        
        // Update connections using KNN
        connections = KNNAlgorithm::findConnections(particles, params);
    }
    
    // Draw particles
    void draw(SDL_Renderer* renderer) {
        // Draw connections first (lines)
        SDL_SetRenderDrawColor(renderer, 100, 100, 150, 100);
        for (const auto& conn : connections) {
            const auto& p1 = particles[conn.particleA].position;
            const auto& p2 = particles[conn.particleB].position;
            
            // Alpha based on distance
            float alpha = 1.0f - (conn.distance / params.maxDistance);
            Uint8 alphaValue = static_cast<Uint8>(alpha * 150.0f);
            
            SDL_SetRenderDrawColor(renderer, 100, 150, 200, alphaValue);
            SDL_RenderLine(renderer, p1.x(), p1.y(), p2.x(), p2.y());
        }
        
        // Draw particles as filled circles
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (const auto& particle : particles) {
            drawFilledCircle(renderer, particle.position.x(), particle.position.y(), particle.size);
        }
    }
    
    // Getters
    int getCount() const { return static_cast<int>(particles.size()); }
    int getConnectionCount() const { return static_cast<int>(connections.size()); }
    KNNParameters& getParameters() { return params; }
};

// Static member initialization
inline std::random_device ParticleKNNSystem::rd;
inline std::mt19937 ParticleKNNSystem::gen(ParticleKNNSystem::rd());