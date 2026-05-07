#pragma once

#include <Eigen/Dense>
#include <vector>
#include <random>

struct ParticleKNN {
    Eigen::Vector2f position;
    Eigen::Vector2f velocity;
    Eigen::Vector2f targetVelocity;

    float speed      = 50.0f;
    float size       = 4.0f;
    float smoothness = 2.0f;

    ParticleKNN(float x, float y) : position(x, y), velocity(0, 0), targetVelocity(0, 0) {
        randomizeDirection();
    }

    void randomizeDirection() {
        static std::uniform_real_distribution<float> angle(0.0f, 2.0f * 3.14159265f);
        float a = angle(rng_);
        targetVelocity = Eigen::Vector2f(std::cos(a), std::sin(a)) * speed;
    }

    void update(float deltaTime, int screenWidth, int screenHeight) {
        velocity += (targetVelocity - velocity) * smoothness * deltaTime;
        position += velocity * deltaTime;

        if (position.x() < 0)           position.x() = (float)screenWidth;
        if (position.x() > screenWidth)  position.x() = 0;
        if (position.y() < 0)           position.y() = (float)screenHeight;
        if (position.y() > screenHeight) position.y() = 0;

        static std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        if (roll(rng_) < 0.01f)
            randomizeDirection();
    }

private:
    // Shared generator across all instances; avoids duplicated static RNGs
    // in randomizeDirection() and update()
    static inline std::mt19937 rng_{std::random_device{}()};
};

struct KNNParameters {
    int   maxConnections = 5;
    float maxDistance    = 200.0f;
    float maxDistanceSq  = 40000.0f;

    void updateSquared() { maxDistanceSq = maxDistance * maxDistance; }
};

// KNN connection (used by ParticleKNNSystem for rendering)
namespace KNNAlgorithm {

struct Connection {
    int   particleA;
    int   particleB;
    float distance;
};

} // namespace KNNAlgorithm
