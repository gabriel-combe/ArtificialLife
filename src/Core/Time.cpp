#include "Core/Time.h"
#include <algorithm>

// Static member initialization
Uint64 Time::lastTime = 0;
float Time::deltaTime = 0.0f;
float Time::timeScale = 1.0f;
float Time::unscaledDeltaTime = 0.0f;
float Time::fixedDeltaTime = 0.02f; // 50 FPS physics
float Time::elapsedTime = 0.0f;
int Time::frameCount = 0;

void Time::Initialize() {
    lastTime = SDL_GetTicks();
    deltaTime = 0.0f;
    unscaledDeltaTime = 0.0f;
    elapsedTime = 0.0f;
    frameCount = 0;
    timeScale = 1.0f;
}

void Time::Update() {
    Uint64 currentTime = SDL_GetTicks();
    unscaledDeltaTime = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;
    
    // Cap delta time to avoid spiral of death
    unscaledDeltaTime = std::min(unscaledDeltaTime, 0.1f);
    
    deltaTime = unscaledDeltaTime;
    elapsedTime += unscaledDeltaTime;
    frameCount++;
}