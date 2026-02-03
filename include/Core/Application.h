#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <memory>

// Application configuration
struct ApplicationConfig {
    std::string title = "Application";
    int width = 1920;
    int height = 1080;
    bool resizable = true;
    bool vsync = true;
};

// Main Application class - Singleton pattern like Unity
class Application {
private:
    static Application* instance;
    
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool isRunning = false;
    bool initialized = false;
    
    int screenWidth;
    int screenHeight;
    
protected:
    Application() = default;
    
public:
    // Singleton access
    static Application& Instance() {
        if (!instance) {
            instance = new Application();
        }
        return *instance;
    }
    
    // Delete copy constructor and assignment
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    
    // Initialize application
    bool Initialize(const ApplicationConfig& config);
    
    // Main loop control
    void Run();
    void Quit() { isRunning = false; }
    
    // Getters
    SDL_Renderer* GetRenderer() const { return renderer; }
    SDL_Window* GetWindow() const { return window; }
    int GetScreenWidth() const { return screenWidth; }
    int GetScreenHeight() const { return screenHeight; }
    bool IsRunning() const { return isRunning; }
    
    // Update screen size
    void UpdateScreenSize();
    
    // Cleanup
    void Shutdown();
    
    // Virtual methods for game loop (override in derived class)
    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}
    virtual void OnGUI() {}
    virtual void OnShutdown() {}
    
    ~Application() { Shutdown(); }
};

// Global instance pointer
inline Application* Application::instance = nullptr;