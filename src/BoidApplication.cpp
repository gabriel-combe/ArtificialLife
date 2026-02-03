#include "Core/Application.h"
#include "Core/Time.h"
#include "Core/Input.h"
#include "Core/GUI.h"
#include "Core/Debug.h"
#include "Boids/BoidRenderer.h"
#include <cstdio>

// Boids application
class BoidsApplication : public Application {
private:
    BoidSystem boidSystem;
    
    // Simulation state
    int boidCount = 200;
    bool paused = false;
    bool showDirection = false;
    bool showSteering = false;
    
public:
    // Called once at startup
    void OnStart() override {
        Debug::Log("Boids simulation starting...");
        boidSystem.generate(boidCount, GetScreenWidth(), GetScreenHeight());
    }
    
    // Called every frame
    void OnUpdate(float deltaTime) override {
        // Update boids if not paused
        if (!paused) {
            boidSystem.update(deltaTime, GetScreenWidth(), GetScreenHeight());
        }
    }
    
    // Called every frame for rendering
    void OnRender() override {
        // Draw boids
        boidSystem.draw(GetRenderer());
        
        // Draw optional visualizations
        if (showDirection) {
            boidSystem.drawDirectionVectors(GetRenderer());
        }
        
        if (showSteering) {
            boidSystem.drawSteeringVectors(GetRenderer());
        }
    }
    
    // Called every frame for GUI
    void OnGUI() override {
        RenderControlPanel();
    }
    
    // Render the control panel
    void RenderControlPanel() {
        GUI::BeginWindow("Boids Control Panel");
        
        GUI::Text("Simulation Controls");
        GUI::Separator();
        
        GUI::SliderInt("Number of Boids", &boidCount, 1, 1000);
        
        if (GUI::Button("Reset Boids")) {
            boidSystem.generate(boidCount, GetScreenWidth(), GetScreenHeight());
        }
        
        GUI::SameLine();
        
        if (GUI::Button(paused ? "Resume" : "Pause")) {
            paused = !paused;
        }
        
        GUI::Separator();
        GUI::Text("Visualization");
        GUI::Checkbox("Show Direction Vector", &showDirection);
        GUI::Checkbox("Show Steering Vector", &showSteering);
        
        GUI::Separator();
        
        // Boid parameters
        if (GUI::CollapsingHeader("Boid Parameters")) {
            BoidParameters& params = boidSystem.getParameters();
            bool radiusChanged = false;
            
            GUI::Text("Perception Radii");
            GUI::Text("(Higher = affects more distant boids)");
            
            radiusChanged |= GUI::SliderFloat("Separation Radius", &params.separationRadius, 10.0f, 200.0f);
            radiusChanged |= GUI::SliderFloat("Alignment Radius", &params.alignmentRadius, 20.0f, 300.0f);
            radiusChanged |= GUI::SliderFloat("Cohesion Radius", &params.cohesionRadius, 20.0f, 300.0f);
            
            GUI::Separator();
            GUI::Text("Force Weights");
            GUI::Text("(Higher = stronger effect)");
            GUI::SliderFloat("Separation Weight", &params.separationWeight, 0.0f, 5.0f);
            GUI::SliderFloat("Alignment Weight", &params.alignmentWeight, 0.0f, 5.0f);
            GUI::SliderFloat("Cohesion Weight", &params.cohesionWeight, 0.0f, 5.0f);
            
            if (radiusChanged) {
                params.updateSquaredRadii();
            }
            
            if (GUI::Button("Reset Parameters")) {
                params.separationRadius = 50.0f;
                params.alignmentRadius = 100.0f;
                params.cohesionRadius = 100.0f;
                params.separationWeight = 1.5f;
                params.alignmentWeight = 1.0f;
                params.cohesionWeight = 1.0f;
                params.updateSquaredRadii();
            }
        }
        
        GUI::Separator();
        GUI::Text("Statistics");
        char statsText[256];
        sprintf(statsText, "Active Boids: %d", boidSystem.getCount());
        GUI::Text(statsText);
        sprintf(statsText, "FPS: %.1f", GUI::GetIO().Framerate);
        GUI::Text(statsText);
        
        GUI::EndWindow();
    }
    
    // Called on shutdown
    void OnShutdown() override {
        Debug::Log("Boids simulation shutting down...");
    }
};

// Factory function
Application* CreateBoidsApplication() {
    return new BoidsApplication();
}