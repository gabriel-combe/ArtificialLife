#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <memory>

#include "Recorder.h"

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

    Recorder recorder_;
    bool screenshotRequested_ = false;
    
protected:
    Application() = default;

    // Provides access to the recorder for derived classes (e.g. to set audio source).
    Recorder& GetRecorder() { return recorder_; }


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
    bool IsRecording()  const { return recorder_.isRecording();  }
    bool IsConverting() const { return recorder_.isConverting(); }
    void RequestScreenshot() { screenshotRequested_ = true; }
    
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

    // Called once, just before recording starts (F8 pressed while not recording).
    // Override to inject an audio source via GetRecorder().setAudioSource(...).
    virtual void OnRecordingStart() {}
    
    ~Application() { Shutdown(); }
};

// Global instance pointer
inline Application* Application::instance = nullptr;