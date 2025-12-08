#pragma once

#include <imgui.h>

// UI system for simulation controls
class UISystem {
public:
    // Simulation state
    int boidCount = 200;
    bool paused = false;
    bool showDirection = false;
    bool showSteering = false;
    bool resetRequested = false;
    
    // Render control panel
    void render(int activeBoidCount, float fps, BoidParameters& params) {
        ImGui::Begin("Boids Control Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        ImGui::Text("Simulation Controls");
        ImGui::Separator();
        
        ImGui::SliderInt("Number of Boids", &boidCount, 1, 1000);
        
        if (ImGui::Button("Reset Boids")) {
            resetRequested = true;
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button(paused ? "Resume" : "Pause")) {
            paused = !paused;
        }
        
        ImGui::Separator();
        ImGui::Text("Visualization");
        ImGui::Checkbox("Show Direction Vector", &showDirection);
        ImGui::Checkbox("Show Steering Vector", &showSteering);
        
        ImGui::Separator();
        
        // Boid parameters
        if (ImGui::CollapsingHeader("Boid Parameters")) {
            bool radiusChanged = false;
            
            ImGui::Text("Perception Radii");
            ImGui::Text("(Higher = affects more distant boids)");
            
            float oldSep = params.separationRadius;
            radiusChanged |= ImGui::SliderFloat("Separation Radius", &params.separationRadius, 10.0f, 200.0f);
            radiusChanged |= ImGui::SliderFloat("Alignment Radius", &params.alignmentRadius, 20.0f, 300.0f);
            radiusChanged |= ImGui::SliderFloat("Cohesion Radius", &params.cohesionRadius, 20.0f, 300.0f);
            
            ImGui::Separator();
            ImGui::Text("Force Weights");
            ImGui::Text("(Higher = stronger effect)");
            ImGui::SliderFloat("Separation Weight", &params.separationWeight, 0.0f, 5.0f);
            ImGui::SliderFloat("Alignment Weight", &params.alignmentWeight, 0.0f, 5.0f);
            ImGui::SliderFloat("Cohesion Weight", &params.cohesionWeight, 0.0f, 5.0f);
            
            // Update squared radii if any radius changed
            if (radiusChanged) {
                params.updateSquaredRadii();
            }
            
            if (ImGui::Button("Reset Parameters")) {
                params.separationRadius = 50.0f;
                params.alignmentRadius = 100.0f;
                params.cohesionRadius = 100.0f;
                params.separationWeight = 1.5f;
                params.alignmentWeight = 1.0f;
                params.cohesionWeight = 1.0f;
                params.updateSquaredRadii();
            }
        }
        
        ImGui::Separator();
        ImGui::Text("Statistics");
        ImGui::Text("Active Boids: %d", activeBoidCount);
        ImGui::Text("FPS: %.1f", fps);
        
        ImGui::End();
    }
};