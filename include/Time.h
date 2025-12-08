#pragma once

#include <SDL3/SDL.h>

// Time management for simulation
class Time {
private:
    Uint64 lastTime = 0;
    float deltaTime = 0.0f;
    float maxDeltaTime = 0.1f;
    
public:
    // Initialize time
    void initialize() {
        lastTime = SDL_GetTicks();
        deltaTime = 0.0f;
    }
    
    // Update delta time
    void update() {
        Uint64 currentTime = SDL_GetTicks();
        deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // Cap delta time to avoid large jumps
        if (deltaTime > maxDeltaTime) {
            deltaTime = maxDeltaTime;
        }
    }
    
    // Get delta time in seconds
    float getDeltaTime() const { return deltaTime; }
    
    // Set maximum delta time
    void setMaxDeltaTime(float maxDt) { maxDeltaTime = maxDt; }
};