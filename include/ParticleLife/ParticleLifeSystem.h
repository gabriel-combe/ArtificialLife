#pragma once

#include "Cluster.h"
#include <SDL3/SDL.h>
#include <vector>
#include <cmath>

namespace ParticleLife {

/**
 * Interaction rule between two clusters
 */
struct Rule {
    int clusterA;      // Source cluster index
    int clusterB;      // Target cluster index
    float gravity;     // Interaction strength
    
    Rule(int a, int b, float g) : clusterA(a), clusterB(b), gravity(g) {}
};

/**
 * Complete Particle Life system with multi-cluster support
 * Optimized for performance with spatial grid
 */
class ParticleLifeSystem {
private:
    std::vector<Cluster> clusters;
    std::vector<Rule> rules;
    
    // Simulation parameters (editable via GUI)
    // CRITICAL: Original OpenGL used maxDistance=10 in normalized space (-2 to 2)
    // In pixel coordinates, we need much larger values!
    // Screen is ~1820 pixels wide, so maxDistance ~300 allows medium-range interactions
    float maxDistance = 300.0f;  // Scaled for pixel coordinates (was 10.0f)
    float particleSize = 3.0f;
    
    // Simulation bounds
    float marginX = 50.0f;
    float marginY = 50.0f;
    int screenWidth = 1920;
    int screenHeight = 1080;
    
    int totalParticles = 0;
    
    /**
     * Helper to draw filled circle
     */
    void drawFilledCircle(SDL_Renderer* renderer, int cx, int cy, int radius) const {
        for (int y = -radius; y <= radius; ++y) {
            int x = static_cast<int>(std::sqrt(std::max(0, radius * radius - y * y)));
            SDL_RenderLine(renderer, cx - x, cy + y, cx + x, cy + y);
        }
    }
    
public:
    ParticleLifeSystem() = default;
    
    // Screen size setters
    void setScreenSize(int width, int height) {
        screenWidth = width;
        screenHeight = height;
    }
    
    // Parameter getters
    float getMaxDistance() const { return maxDistance; }
    float getParticleSize() const { return particleSize; }
    
    // Parameter setters
    void setMaxDistance(float d) { maxDistance = d; }
    void setParticleSize(float s) { particleSize = s; }
    
    /**
     * Add a cluster to the system
     * @return Index of the added cluster
     */
    int addCluster(int count, const Color& color = Color::Random()) {
        Cluster cluster(count, color);
        cluster.resize(count, marginX, marginY, screenWidth - marginX, screenHeight - marginY);
        clusters.push_back(cluster);
        totalParticles += count;
        return static_cast<int>(clusters.size()) - 1;
    }
    
    /**
     * Remove a cluster
     */
    void removeCluster(int index) {
        if (index >= 0 && index < clusters.size()) {
            totalParticles -= clusters[index].size();
            clusters.erase(clusters.begin() + index);
            
            // Clean up associated rules
            rules.erase(
                std::remove_if(rules.begin(), rules.end(),
                    [index](const Rule& r) {
                        return r.clusterA == index || r.clusterB == index;
                    }),
                rules.end()
            );
        }
    }
    
    /**
     * Resize a cluster (regenerate particles)
     */
    void resizeCluster(int index, int newSize) {
        if (index >= 0 && index < clusters.size()) {
            totalParticles -= clusters[index].size();
            clusters[index].resize(newSize, marginX, marginY, screenWidth - marginX, screenHeight - marginY);
            totalParticles += newSize;
        }
    }
    
    /**
     * Change cluster color
     */
    void setClusterColor(int index, const Color& color) {
        if (index >= 0 && index < clusters.size()) {
            clusters[index].setColor(color);
        }
    }
    
    /**
     * Add interaction rule
     */
    void addRule(int a, int b, float gravity) {
        rules.emplace_back(a, b, gravity);
    }
    
    /**
     * Modify existing rule
     */
    void setRule(int index, float gravity) {
        if (index >= 0 && index < rules.size()) {
            rules[index].gravity = gravity;
        }
    }
    
    /**
     * Remove a rule
     */
    void removeRule(int index) {
        if (index >= 0 && index < rules.size()) {
            rules.erase(rules.begin() + index);
        }
    }
    
    /**
     * Clear all rules
     */
    void clearRules() {
        rules.clear();
    }
    
    /**
     * Clear everything (clusters and rules)
     */
    void clear() {
        clusters.clear();
        rules.clear();
        totalParticles = 0;
    }
    
    /**
     * Default configuration: 3 clusters like the original OpenGL system
     * Red, Green, Blue with specific interaction rules
     * 
     * IMPORTANT: Gravity values scaled ~30x from original because:
     * - Original: maxDistance=10 in normalized space (-2,2)
     * - SDL3: maxDistance=300 in pixel space (1920x1080)
     * - Scaling factor ≈ 30
     */
    void setupDefault3Clusters() {
        clear();
        
        addCluster(100, Color::Red());
        addCluster(100, Color::Green());
        addCluster(100, Color::Blue());
        
        // Original values * 30 for pixel coordinate scaling
        addRule(0, 0, -0.96f);   // was -0.032
        addRule(0, 1, -0.51f);   // was -0.017
        addRule(0, 2,  1.02f);   // was  0.034
        addRule(1, 0, -1.02f);   // was -0.034
        addRule(1, 1, -0.30f);   // was -0.01
        addRule(2, 0, -0.60f);   // was -0.02
        addRule(2, 2,  0.45f);   // was  0.015
    }
    
    /**
     * Generate random rules for exploration
     */
    void generateRandomRules(float minGravity = -0.1f, float maxGravity = 0.1f) {
        clearRules();
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(minGravity, maxGravity);
        
        for (int i = 0; i < clusters.size(); ++i) {
            for (int j = 0; j < clusters.size(); ++j) {
                addRule(i, j, dist(gen));
            }
        }
    }
    
    /**
     * Reset particle positions (keep clusters and rules)
     */
    void resetPositions() {
        for (int i = 0; i < clusters.size(); ++i) {
            int count = clusters[i].size();
            resizeCluster(i, count);
        }
    }
    
    /**
     * Update simulation
     */
    void update(float deltaTime) {
        // CRITICAL FIX: Use constant dt = 1.0 like original OpenGL code
        // deltaTime is in seconds (~0.016) making movement 60x too weak!
        const float dt = 1.0f;
        
        // Apply all interaction rules
        for (const auto& rule : rules) {
            if (rule.clusterA < clusters.size() && rule.clusterB < clusters.size()) {
                clusters[rule.clusterA].rule(
                    clusters[rule.clusterB],
                    rule.gravity,
                    maxDistance,
                    dt  // ← FIXED! Was deltaTime (0.016), now 1.0
                );
            }
        }
        
        // Apply boundary constraints
        for (auto& cluster : clusters) {
            cluster.applyBoundaries(
                marginX, marginY,
                screenWidth - marginX, screenHeight - marginY
            );
        }
    }
    
    /**
     * Render the system
     */
    void draw(SDL_Renderer* renderer) {
        int radius = static_cast<int>(particleSize);
        
        for (const auto& cluster : clusters) {
            const Color& col = cluster.getColor();
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
            
            for (const auto& particle : cluster.getParticles()) {
                int x = static_cast<int>(particle.position.x());
                int y = static_cast<int>(particle.position.y());
                drawFilledCircle(renderer, x, y, radius);
            }
        }
        
        // Draw boundaries (optional)
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_FRect boundary = {
            marginX,
            marginY,
            screenWidth - 2 * marginX,
            screenHeight - 2 * marginY
        };
        SDL_RenderRect(renderer, &boundary);
    }
    
    // Statistics getters
    int getClusterCount() const { return static_cast<int>(clusters.size()); }
    int getTotalParticles() const { return totalParticles; }
    int getRuleCount() const { return static_cast<int>(rules.size()); }
    
    Cluster& getCluster(int index) { return clusters[index]; }
    const Cluster& getCluster(int index) const { return clusters[index]; }
    
    Rule& getRule(int index) { return rules[index]; }
    const Rule& getRule(int index) const { return rules[index]; }
};

} // namespace ParticleLife