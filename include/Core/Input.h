#pragma once

#include <SDL3/SDL.h>
#include <Eigen/Dense>

// Input management system - Unity-style
class Input {
private:
    static const Uint8* keyboardState;
    static Uint32 mouseState;
    static Eigen::Vector2i mousePosition;
    static Eigen::Vector2i mouseDelta;
    static Eigen::Vector2i lastMousePosition;
    
public:
    // Initialize input system
    static void Initialize();
    
    // Process SDL events
    static void ProcessEvent(SDL_Event* event);
    
    // Update input state (called after events)
    static void Update();
    
    // Keyboard - Unity-style
    static bool GetKey(SDL_Scancode key);
    static bool GetKeyDown(SDL_Scancode key); // TODO: Implement frame-based detection
    static bool GetKeyUp(SDL_Scancode key);   // TODO: Implement frame-based detection
    
    // Mouse buttons
    static bool GetMouseButton(int button);
    static bool GetMouseButtonDown(int button); // TODO: Implement
    static bool GetMouseButtonUp(int button);   // TODO: Implement
    
    // Mouse position
    static Eigen::Vector2i GetMousePosition() { return mousePosition; }
    static Eigen::Vector2i GetMouseDelta() { return mouseDelta; }
    static float GetMouseX() { return static_cast<float>(mousePosition.x()); }
    static float GetMouseY() { return static_cast<float>(mousePosition.y()); }
};