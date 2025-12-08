#include "Engine.h"
#include "Time.h"
#include "BoidRenderer.h"
#include "UI.h"

int main(int argc, char* argv[]) {
    // Initialize engine
    Engine engine;
    if (!engine.initialize("ArtificialLife - Boids Simulation")) {
        return 1;
    }
    
    // Initialize systems
    Time time;
    time.initialize();
    
    BoidSystem boidSystem;
    UISystem ui;
    
    // Generate initial boids
    boidSystem.generate(ui.boidCount, engine.screenWidth, engine.screenHeight);
    
    // Main loop
    bool running = true;
    SDL_Event event;
    
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        // Update time
        time.update();
        float deltaTime = time.getDeltaTime();
        
        // Handle reset request
        if (ui.resetRequested) {
            boidSystem.generate(ui.boidCount, engine.screenWidth, engine.screenHeight);
            ui.resetRequested = false;
        }
        
        // Update simulation
        if (!ui.paused) {
            boidSystem.update(deltaTime, engine.screenWidth, engine.screenHeight);
        }
        
        // Render
        engine.beginFrame();
        
        // Draw boids
        boidSystem.draw(engine.getRenderer());
        
        if (ui.showDirection) {
            boidSystem.drawDirectionVectors(engine.getRenderer());
        }
        
        if (ui.showSteering) {
            boidSystem.drawSteeringVectors(engine.getRenderer());
        }
        
        // Draw UI
        ui.render(boidSystem.getCount(), ImGui::GetIO().Framerate, boidSystem.getParameters());
        
        engine.endFrame();
    }
    
    return 0;
}