#pragma once

#include <SDL3/SDL.h>

// Time management system - Similar to Unity's Time class
class Time {
private:
    static Uint64 lastTime;
    static float deltaTime;
    static float timeScale;
    static float unscaledDeltaTime;
    static float fixedDeltaTime;
    static float elapsedTime;
    static int frameCount;
    
public:
    // Initialize time system
    static void Initialize();
    
    // Update time (called once per frame)
    static void Update();
    
    // Getters - Unity-style naming
    static float DeltaTime() { return deltaTime * timeScale; }
    static float UnscaledDeltaTime() { return unscaledDeltaTime; }
    static float FixedDeltaTime() { return fixedDeltaTime; }
    static float ElapsedTime() { return elapsedTime; }
    static int FrameCount() { return frameCount; }
    static float TimeScale() { return timeScale; }
    
    // Setters
    static void SetTimeScale(float scale) { timeScale = scale; }
    static void SetFixedDeltaTime(float dt) { fixedDeltaTime = dt; }
};