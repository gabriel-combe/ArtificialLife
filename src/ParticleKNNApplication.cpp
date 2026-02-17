#include "Core/Application.h"
#include "Core/Time.h"
#include "Core/Input.h"
#include "Core/GUI.h"
#include "Core/Debug.h"
#include "ParticleKNN/particleKNNSystem.h"

// Particle application
class ParticlesKNNApplication : public Application {
private:
    ParticleKNNSystem particleKNNSystem;
    
    // UI state
    int particleCount = 100;
    bool paused = false;
    
public:
    // Called once at startup
    void OnStart() override {
        Debug::Log("Particles KNN simulation starting...");
        particleKNNSystem.generate(particleCount, GetScreenWidth(), GetScreenHeight());
    }
    
    // Called every frame
    void OnUpdate(float deltaTime) override {
        // Update particles if not paused
        if (!paused) {
            particleKNNSystem.update(deltaTime, GetScreenWidth(), GetScreenHeight());
        }
    }
    
    // Called every frame for rendering
    void OnRender() override {
        particleKNNSystem.draw(GetRenderer());
    }
    
    // Called every frame for GUI
    void OnGUI() override {
        RenderControlPanel();
    }
    
    // Render control panel
    void RenderControlPanel() {
        GUI::BeginWindow("Particle KNN Control Panel");
        
        GUI::Text("Simulation Controls");
        GUI::Separator();
        
        // Particle count slider
        int oldCount = particleCount;
        GUI::SliderInt("Number of Particles", &particleCount, 1, 500);
        
        if (oldCount != particleCount) {
            particleKNNSystem.generate(particleCount, GetScreenWidth(), GetScreenHeight());
        }
        
        if (GUI::Button("Regenerate")) {
            particleKNNSystem.generate(particleCount, GetScreenWidth(), GetScreenHeight());
        }
        
        GUI::SameLine();
        
        if (GUI::Button(paused ? "Resume" : "Pause")) {
            paused = !paused;
        }
        
        GUI::Separator();
        
        // KNN Parameters
        if (GUI::CollapsingHeader("KNN Parameters")) {
            KNNParameters& params = particleKNNSystem.getParameters();
            
            GUI::Text("Connection Settings");
            GUI::SliderInt("Max Connections per Particle", &params.maxConnections, 1, 20);
            
            bool distChanged = GUI::SliderFloat("Max Connection Distance", &params.maxDistance, 50.0f, 500.0f);
            
            if (distChanged) {
                params.updateSquared();
            }
            
            GUI::Separator();
            GUI::Text("Debug Info:");
            char debugText[256];
            sprintf(debugText, "Max Distance Sq: %.0f", params.maxDistanceSq);
            GUI::Text(debugText);
            
            if (GUI::Button("Reset Parameters")) {
                params.maxConnections = 5;
                params.maxDistance = 200.0f;
                params.updateSquared();
            }
        }
        
        GUI::Separator();
        GUI::Text("Statistics");
        char statsText[256];
        sprintf(statsText, "Active Particles: %d", particleKNNSystem.getCount());
        GUI::Text(statsText);
        sprintf(statsText, "Active Connections: %d", particleKNNSystem.getConnectionCount());
        GUI::Text(statsText);
        sprintf(statsText, "FPS: %.1f", GUI::GetIO().Framerate);
        GUI::Text(statsText);
        
        GUI::EndWindow();
    }
    
    // Called on shutdown
    void OnShutdown() override {
        Debug::Log("Particles simulation shutting down...");
    }
};

// Factory function
Application* CreateParticlesKNNApplication() {
    return new ParticlesKNNApplication();
}