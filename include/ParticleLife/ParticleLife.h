#pragma once

#include <Eigen/Dense>
#include <random>

namespace ParticleLife {

/**
 * 2D Particle with full physics (position, velocity, acceleration)
 * Screen coordinates (pixels) for performance
 */
struct Particle {
    Eigen::Vector2f position;
    Eigen::Vector2f velocity;
    Eigen::Vector2f acceleration;
    
    // Default constructor - random initialization
    Particle() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> distPos(-1.0f, 1.0f);
        static std::uniform_real_distribution<float> distVel(-0.5f, 0.5f);
        static std::uniform_real_distribution<float> distAcc(-0.05f, 0.05f);
        
        position = Eigen::Vector2f(distPos(gen), distPos(gen));
        velocity = Eigen::Vector2f(distVel(gen), distVel(gen));
        acceleration = Eigen::Vector2f(distAcc(gen), distAcc(gen));
    }
    
    // Constructor with position only
    Particle(float x, float y) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> distVel(-0.5f, 0.5f);
        static std::uniform_real_distribution<float> distAcc(-0.05f, 0.05f);
        
        position = Eigen::Vector2f(x, y);
        velocity = Eigen::Vector2f(distVel(gen), distVel(gen));
        acceleration = Eigen::Vector2f(distAcc(gen), distAcc(gen));
    }
    
    // Constructor with position and velocity
    Particle(const Eigen::Vector2f& pos, const Eigen::Vector2f& vel) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> distAcc(-0.05f, 0.05f);
        
        position = pos;
        velocity = vel;
        acceleration = Eigen::Vector2f(distAcc(gen), distAcc(gen));
    }
    
    // Full constructor
    Particle(const Eigen::Vector2f& pos, const Eigen::Vector2f& vel, const Eigen::Vector2f& acc)
        : position(pos), velocity(vel), acceleration(acc) {}
    
    /**
     * Euler integration for stability
     * Same as original OpenGL system
     */
    inline void update(float dt) {
        velocity += acceleration * dt;
	    position += 0.5f * acceleration * dt * dt + velocity * dt;
    }
    
    /**
     * Apply boundary constraints with bounce
     */
    inline void applyBoundaryConstraints(float minX, float minY, float maxX, float maxY) {
        // X boundaries
        if (position.x() <= minX) {
            position.x() = minX;
        } else if (position.x() >= maxX) {
            position.x() = maxX;
        }
        
        // Y boundaries
        if (position.y() <= minY) {
            position.y() = minY;
        } else if (position.y() >= maxY) {
            position.y() = maxY;
        }
    }

    /**
     * Apply Warp constraints
     */
    inline void applyWarpConstraints(float minX, float minY, float maxX, float maxY) {
        // X boundaries
        if (position.x() <= minX) {
            position.x() = maxX;
        } else if (position.x() >= maxX) {
            position.x() = minX;
        }
        
        // Y boundaries
        if (position.y() <= minY) {
            position.y() = maxY;
        } else if (position.y() >= maxY) {
            position.y() = minY;
        }
    }

    /**
     * Apply Reverse constraints
     */
    inline void applyReverseConstraints(float minX, float minY, float maxX, float maxY) {
        // X boundaries
        if (position.x() <= minX || position.x() >= maxX) {
            velocity.x() *= -1.f;
        }
        
        // Y boundaries
        if (position.y() <= minY || position.y() >= maxY) {
            velocity.y() *= -1.f;
        }
    }
    
    /**
     * Create particle with random position in bounds and random velocity/acceleration
     */
    static Particle randomInBounds(float minX, float minY, float maxX, float maxY) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> distX(minX, maxX);
        std::uniform_real_distribution<float> distY(minY, maxY);
        std::uniform_real_distribution<float> distVel(-0.5f, 0.5f);
        std::uniform_real_distribution<float> distAcc(-0.05f, 0.05f);
        
        Eigen::Vector2f pos(distX(gen), distY(gen));
        Eigen::Vector2f vel(distVel(gen), distVel(gen));
        Eigen::Vector2f acc(distAcc(gen), distAcc(gen));
        
        return Particle(pos, vel, acc);
    }
};

} // namespace ParticleLife