#pragma once

#include <Eigen/Dense>
#include <vector>
#include <random>

// Simple particle with smooth random movement
struct ParticleKNN {
    Eigen::Vector2f position;
    Eigen::Vector2f velocity;
    Eigen::Vector2f targetVelocity;
    
    float speed = 50.0f;
    float size = 4.0f;
    float smoothness = 2.0f; // How smoothly to change direction
    
    ParticleKNN(float x, float y) : position(x, y), velocity(0, 0), targetVelocity(0, 0) {
        randomizeDirection();
    }
    
    void randomizeDirection() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dist(0.0f, 2.0f * 3.14159f);
        
        float angle = dist(gen);
        targetVelocity = Eigen::Vector2f(std::cos(angle), std::sin(angle)) * speed;
    }
    
    void update(float deltaTime, int screenWidth, int screenHeight) {
        // Smoothly interpolate towards target velocity
        velocity += (targetVelocity - velocity) * smoothness * deltaTime;
        
        // Update position
        position += velocity * deltaTime;
        
        // Wrap around screen
        if (position.x() < 0) position.x() = screenWidth;
        if (position.x() > screenWidth) position.x() = 0;
        if (position.y() < 0) position.y() = screenHeight;
        if (position.y() > screenHeight) position.y() = 0;
        
        // Randomly change direction occasionally
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> changeDist(0.0f, 1.0f);
        
        if (changeDist(gen) < 0.01f) { // 1% chance per frame
            randomizeDirection();
        }
    }
};

// KNN parameters
struct KNNParameters {
    int maxConnections = 5;
    float maxDistance = 200.0f;
    
    // Derived values
    float maxDistanceSq = 40000.0f; // 200^2
    
    void updateSquared() {
        maxDistanceSq = maxDistance * maxDistance;
    }
};

// KNN algorithm for particle connections
namespace KNNAlgorithm {
    
    struct Connection {
        int particleA;
        int particleB;
        float distance;
    };
    
    // Find K nearest neighbors for each particle
    inline std::vector<Connection> findConnections(
        const std::vector<ParticleKNN>& particles, 
        const KNNParameters& params
    ) {
        std::vector<Connection> connections;
        
        for (size_t i = 0; i < particles.size(); ++i) {
            const auto& p1 = particles[i];
            
            // Store neighbors with distances
            std::vector<std::pair<int, float>> neighbors;
            
            for (size_t j = i + 1; j < particles.size(); ++j) {
                const auto& p2 = particles[j];
                
                float distSq = (p1.position - p2.position).squaredNorm();
                
                if (distSq < params.maxDistanceSq) {
                    neighbors.push_back({j, distSq});
                }
            }
            
            // Sort by distance and take top K
            std::sort(neighbors.begin(), neighbors.end(), 
                [](const auto& a, const auto& b) { return a.second < b.second; });
            
            int count = std::min(params.maxConnections, static_cast<int>(neighbors.size()));
            for (int k = 0; k < count; ++k) {
                connections.push_back({
                    static_cast<int>(i), 
                    neighbors[k].first, 
                    std::sqrt(neighbors[k].second)
                });
            }
        }
        
        return connections;
    }
}