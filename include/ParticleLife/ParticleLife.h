#pragma once

#include <cstdint>
#include <random>

namespace ParticleLife {

/**
 * RGBA color for a particle cluster.
 * Self-contained — no SDL, no Eigen, no heavy headers.
 */
struct Color {
    uint8_t r, g, b, a;

    Color() : r(255), g(255), b(255), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    // Named constructors
    static Color Red()     { return Color(255,   0,   0); }
    static Color Green()   { return Color(  0, 255,   0); }
    static Color Blue()    { return Color(  0,   0, 255); }
    static Color Yellow()  { return Color(255, 255,   0); }
    static Color Cyan()    { return Color(  0, 255, 255); }
    static Color Magenta() { return Color(255,   0, 255); }
    static Color White()   { return Color(255, 255, 255); }

    static Color Random() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dist(100, 255);
        return Color(
            (uint8_t)dist(gen),
            (uint8_t)dist(gen),
            (uint8_t)dist(gen)
        );
    }
};

} // namespace ParticleLife