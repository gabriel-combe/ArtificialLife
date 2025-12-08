#pragma once

#include <Eigen/Dense>
#include <vector>
#include <random>
#include <cmath>

// Boid structure representing a creature
struct Boid {
    Eigen::Vector2f position;
    Eigen::Vector2f velocity;
    Eigen::Vector2f acceleration;
    
    float maxSpeed = 150.0f;
    float maxForce = 100.0f;
    float size = 8.0f;
    
    Boid(float x, float y, float vx, float vy) 
        : position(x, y), velocity(vx, vy), acceleration(0.0f, 0.0f) {}
    
    void update(float deltaTime, int screenWidth, int screenHeight) {
        velocity += acceleration * deltaTime;
        
        float speed = velocity.norm();
        if (speed > maxSpeed) {
            velocity = (velocity / speed) * maxSpeed;
        }
        
        position += velocity * deltaTime;
        
        // Wrap around screen
        if (position.x() < 0) position.x() = screenWidth;
        if (position.x() > screenWidth) position.x() = 0;
        if (position.y() < 0) position.y() = screenHeight;
        if (position.y() > screenHeight) position.y() = 0;
        
        acceleration.setZero();
    }
    
    void applyForce(const Eigen::Vector2f& force) {
        acceleration += force;
    }
    
    float getHeading() const {
        return std::atan2(velocity.y(), velocity.x());
    }
    
    void getTrianglePoints(Eigen::Vector2f& p1, Eigen::Vector2f& p2, Eigen::Vector2f& p3) const {
        float angle = getHeading();
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);
        
        p1.x() = position.x() + cos_a * size;
        p1.y() = position.y() + sin_a * size;
        
        float angle_left = angle + 2.5f;
        p2.x() = position.x() + std::cos(angle_left) * (size * 0.5f);
        p2.y() = position.y() + std::sin(angle_left) * (size * 0.5f);
        
        float angle_right = angle - 2.5f;
        p3.x() = position.x() + std::cos(angle_right) * (size * 0.5f);
        p3.y() = position.y() + std::sin(angle_right) * (size * 0.5f);
    }
};

// Boid algorithm parameters
struct BoidParameters {
    float separationRadius = 50.0f;
    float alignmentRadius = 100.0f;
    float cohesionRadius = 100.0f;
    
    float separationWeight = 1.5f;
    float alignmentWeight = 1.0f;
    float cohesionWeight = 1.0f;
    
    // Performance optimization: use squared radius for distance checks
    float separationRadiusSq = 2500.0f;  // 50^2
    float alignmentRadiusSq = 10000.0f;  // 100^2
    float cohesionRadiusSq = 10000.0f;   // 100^2
    
    void updateSquaredRadii() {
        separationRadiusSq = separationRadius * separationRadius;
        alignmentRadiusSq = alignmentRadius * alignmentRadius;
        cohesionRadiusSq = cohesionRadius * cohesionRadius;
    }
};

// Boid algorithm implementation
namespace BoidAlgorithm {
    
    inline Eigen::Vector2f limit(const Eigen::Vector2f& vec, float maxMagnitude) {
        float mag = vec.norm();
        if (mag > maxMagnitude) {
            return (vec / mag) * maxMagnitude;
        }
        return vec;
    }
    
    inline Eigen::Vector2f calculateSeparation(const Boid& boid, const std::vector<Boid>& boids, const BoidParameters& params) {
        Eigen::Vector2f steer(0.0f, 0.0f);
        int count = 0;
        
        for (const auto& other : boids) {
            float distanceSq = (boid.position - other.position).squaredNorm();
            
            // Only separate from boids within separation radius
            if (distanceSq > 0.01f && distanceSq < params.separationRadiusSq) {
                Eigen::Vector2f diff = boid.position - other.position;
                diff.normalize();  // Direction away from other boid
                steer += diff;
                count++;
            }
        }
        
        if (count > 0) {
            steer /= count;
            
            if (steer.squaredNorm() > 0.01f) {
                steer.normalize();
                steer *= boid.maxSpeed;
                steer -= boid.velocity;
                steer = limit(steer, boid.maxForce);
            }
        }
        
        return steer;
    }
    
    inline Eigen::Vector2f calculateAlignment(const Boid& boid, const std::vector<Boid>& boids, const BoidParameters& params) {
        Eigen::Vector2f sum(0.0f, 0.0f);
        int count = 0;
        
        for (const auto& other : boids) {
            float distanceSq = (boid.position - other.position).squaredNorm();
            
            if (distanceSq > 0.01f && distanceSq < params.alignmentRadiusSq) {
                sum += other.velocity;
                count++;
            }
        }
        
        if (count > 0) {
            sum /= count;
            sum.normalize();
            sum *= boid.maxSpeed;
            
            Eigen::Vector2f steer = sum - boid.velocity;
            steer = limit(steer, boid.maxForce);
            return steer;
        }
        
        return Eigen::Vector2f(0.0f, 0.0f);
    }
    
    inline Eigen::Vector2f calculateCohesion(const Boid& boid, const std::vector<Boid>& boids, const BoidParameters& params) {
        Eigen::Vector2f sum(0.0f, 0.0f);
        int count = 0;
        
        for (const auto& other : boids) {
            float distanceSq = (boid.position - other.position).squaredNorm();
            
            if (distanceSq > 0.01f && distanceSq < params.cohesionRadiusSq) {
                sum += other.position;
                count++;
            }
        }
        
        if (count > 0) {
            sum /= count;
            
            Eigen::Vector2f desired = sum - boid.position;
            desired.normalize();
            desired *= boid.maxSpeed;
            
            Eigen::Vector2f steer = desired - boid.velocity;
            steer = limit(steer, boid.maxForce);
            return steer;
        }
        
        return Eigen::Vector2f(0.0f, 0.0f);
    }
    
    inline void applyBoidForces(Boid& boid, const std::vector<Boid>& boids, const BoidParameters& params) {
        Eigen::Vector2f separation = calculateSeparation(boid, boids, params);
        Eigen::Vector2f alignment = calculateAlignment(boid, boids, params);
        Eigen::Vector2f cohesion = calculateCohesion(boid, boids, params);
        
        separation *= params.separationWeight;
        alignment *= params.alignmentWeight;
        cohesion *= params.cohesionWeight;
        
        boid.applyForce(separation);
        boid.applyForce(alignment);
        boid.applyForce(cohesion);
    }
}