#include "Core/Application.h"
#include "Core/Time.h"
#include "Core/Input.h"
#include "Core/GUI.h"
#include "Core/Debug.h"
#include <cstdio>

// Forward declarations
Application* CreateBoidsApplication();
Application* CreateParticlesKNNApplication();

// Entry point
int main(int argc, char* argv[]) {
    // Create application instance
    // Application* app = CreateBoidsApplication();
    Application* app = CreateParticlesKNNApplication();
    
    // Configure application
    ApplicationConfig config;
    // config.title = "ArtificialLife - Boids Simulation";
    config.title = "ArtificialLife - Particles KNN Simulation";
    config.width = 1920;
    config.height = 1080;
    config.resizable = true;
    
    // Initialize and run
    if (app->Initialize(config)) {
        app->Run();
    }
    
    // Cleanup
    delete app;
    
    return 0;
}