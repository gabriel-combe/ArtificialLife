#pragma once

#include "ParticleLife.h"
#include <vector>
#include <cstdint>
#include <cmath>
#include <unordered_map>

namespace ParticleLife {

/**
 * RGBA Color structure
 */
struct Color {
    uint8_t r, g, b, a;
    
    Color() : r(255), g(255), b(255), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
    
    static Color Red()     { return Color(255, 0, 0); }
    static Color Green()   { return Color(0, 255, 0); }
    static Color Blue()    { return Color(0, 0, 255); }
    static Color Yellow()  { return Color(255, 255, 0); }
    static Color Cyan()    { return Color(0, 255, 255); }
    static Color Magenta() { return Color(255, 0, 255); }
    static Color White()   { return Color(255, 255, 255); }
    
    static Color Random() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dist(100, 255);
        return Color(dist(gen), dist(gen), dist(gen));
    }
};

/**
 * Spatial Grid for O(n) performance instead of O(n²)
 * Divides space into cells for neighbor search optimization
 */
class SpatialGrid {
private:
    float cellSize;
    int gridWidth;
    int gridHeight;
    std::unordered_map<int, std::vector<int>> cells;
    
public:
    SpatialGrid(float cs = 100.0f, int w = 20, int h = 20) 
        : cellSize(cs), gridWidth(w), gridHeight(h) {}
    
    inline int getHash(int x, int y) const {
        return y * gridWidth + x;
    }
    
    inline void getCellCoords(const Eigen::Vector2f& pos, int& cx, int& cy) const {
        cx = std::max(0, std::min(gridWidth - 1, static_cast<int>(pos.x() / cellSize)));
        cy = std::max(0, std::min(gridHeight - 1, static_cast<int>(pos.y() / cellSize)));
    }
    
    void clear() {
        cells.clear();
    }
    
    void insert(int particleIdx, const Eigen::Vector2f& pos) {
        int cx, cy;
        getCellCoords(pos, cx, cy);
        cells[getHash(cx, cy)].push_back(particleIdx);
    }
    
    /**
     * Get all particles in neighboring cells (3x3 grid around position)
     */
    std::vector<int> getNeighborParticles(const Eigen::Vector2f& pos) const {
        std::vector<int> neighbors;
        int cx, cy;
        getCellCoords(pos, cx, cy);
        
        // Search in cell and 8 neighbors
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight) {
                    int hash = getHash(nx, ny);
                    auto it = cells.find(hash);
                    if (it != cells.end()) {
                        neighbors.insert(neighbors.end(), it->second.begin(), it->second.end());
                    }
                }
            }
        }
        return neighbors;
    }
};

/**
 * Cluster of particles with interaction rules
 * Optimized with spatial grid for performance
 */
class Cluster {
private:
    std::vector<Particle> particles;
    Color color;
    SpatialGrid grid;
    
public:
    Cluster(int count = 100, const Color& col = Color::Random()) 
        : color(col), grid(100.0f, 20, 20) {
        particles.reserve(count);
    }
    
    // Getters/Setters
    void setColor(const Color& col) { color = col; }
    const Color& getColor() const { return color; }
    int size() const { return static_cast<int>(particles.size()); }
    std::vector<Particle>& getParticles() { return particles; }
    const std::vector<Particle>& getParticles() const { return particles; }
    
    void addParticle(const Particle& p) {
        particles.push_back(p);
    }
    
    void clear() {
        particles.clear();
    }
    
    /**
     * Resize cluster with new random positions, velocities and accelerations
     */
    void resize(int newSize, float minX, float minY, float maxX, float maxY) {
        particles.clear();
        particles.reserve(newSize);
        for (int i = 0; i < newSize; ++i) {
            particles.push_back(Particle::randomInBounds(minX, minY, maxX, maxY));
        }
    }
    
    /**
     * Interaction rule - EXACT copy of original OpenGL code
     * 
     * Original formula:
     * force = (gravity / distance) * direction
     * velocity = (velocity + force) * 0.5f  // ← DAMPING!
     * position = position + velocity * dt
     * 
     * @param other The cluster to interact with
     * @param gravity Interaction strength (negative = repulsion, positive = attraction)
     * @param maxDistance Maximum interaction range (original = 10.0f)
     * @param dt Delta time
     */
    void rule(const Cluster& other, float gravity, float maxDistance, float dt) {
        const float maxDist = maxDistance;
        
        // Calculate forces for each particle
        for (auto& particle : particles) {
            Eigen::Vector2f force(0, 0);
            
            // Check all particles in other cluster
            for (const auto& otherParticle : other.particles) {
                Eigen::Vector2f delta = particle.position - otherParticle.position;
                float dist = delta.norm();
                
                // EXACT original conditions: dist > 0 AND dist < maxDistance
                if (dist > 0.0f && dist < maxDist) {
                    // EXACT original formula
                    force += (gravity / dist) * delta;
                }
            }
            
            // EXACT original velocity update with damping
            particle.velocity = (particle.velocity + force) * 0.5f;
            
            // EXACT original position update (NO Verlet term!)
            particle.position += particle.velocity * dt;
        }
    }
    
    /**
     * Apply boundary constraints with bounce
     */
    void applyBoundaries(float minX, float minY, float maxX, float maxY) {
        for (auto& p : particles) {
            // p.applyBoundaryConstraints(minX, minY, maxX, maxY);
            p.applyWarpConstraints(minX, minY, maxX, maxY);
            // p.applyReverseConstraints(minX, minY, maxX, maxY);
        }
    }
};

} // namespace ParticleLife