#include "Core/Application.h"
#include "Core/Time.h"
#include "Core/Input.h"
#include "Core/GUI.h"
#include "Core/Debug.h"
#include "ParticleLife/ParticleLifeSystem.h"

/**
 * Particle Life Application
 * Artificial life system based on simple attraction/repulsion rules
 * 
 * Features:
 * - Multiple clusters with different colors
 * - Customizable interaction rules (gravity-based)
 * - Real-time parameter editing via ImGui
 * - Spatial grid optimization for performance
 * - Random initialization of position, velocity, and acceleration
 */
class ParticleLifeApplication : public Application {
private:
    ParticleLife::ParticleLifeSystem particleSystem;
    
    // UI state
    bool paused = false;
    bool showBoundaries = true;
    
    // State for adding new rules
    int newRuleFrom = 0;
    int newRuleTo = 0;
    float newRuleGravity = 0.0f;
    
public:
    void OnStart() override {
        Debug::Log("Particle Life simulation starting...");
        
        // Initial configuration: 3 clusters like original OpenGL system
        particleSystem.setScreenSize(GetScreenWidth(), GetScreenHeight());
        particleSystem.setupDefault3Clusters();
        
        Debug::Log("Initialized with 3 clusters (Red, Green, Blue)");
    }
    
    void OnUpdate(float deltaTime) override {
        // Update screen size if changed
        particleSystem.setScreenSize(GetScreenWidth(), GetScreenHeight());
        
        // Update simulation if not paused
        if (!paused) {
            particleSystem.update(deltaTime);
        }
    }
    
    void OnRender() override {
        particleSystem.draw(GetRenderer());
    }
    
    void OnGUI() override {
        RenderControlPanel();
    }
    
    void RenderControlPanel() {
        GUI::BeginWindow("Particle Life Control");
        
        // ===== SIMULATION SECTION =====
        if (GUI::CollapsingHeader("Simulation")) {
            // Pause/Resume
            if (GUI::Button(paused ? "Resume" : "Pause")) {
                paused = !paused;
            }
            
            GUI::SameLine();
            
            if (GUI::Button("Reset Positions")) {
                particleSystem.resetPositions();
            }
            
            GUI::Separator();
            
            // Global parameters
            float maxDist = particleSystem.getMaxDistance();
            if (GUI::SliderFloat("Max Distance", &maxDist, 50.0f, 600.0f)) {
                particleSystem.setMaxDistance(maxDist);
            }
            
            float particleSize = particleSystem.getParticleSize();
            if (GUI::SliderFloat("Particle Size", &particleSize, 1.0f, 10.0f)) {
                particleSystem.setParticleSize(particleSize);
            }
            
            GUI::Checkbox("Show Boundaries", &showBoundaries);
        }
        
        // ===== CLUSTERS SECTION =====
        if (GUI::CollapsingHeader("Clusters")) {
            char clusterText[128];
            sprintf(clusterText, "Total Particles: %d", particleSystem.getTotalParticles());
            GUI::Text(clusterText);
            GUI::Separator();
            
            int clusterCount = particleSystem.getClusterCount();
            
            for (int i = 0; i < clusterCount; ++i) {
                GUI::PushID(i);
                
                auto& cluster = particleSystem.getCluster(i);
                const auto& color = cluster.getColor();
                
                // Display color square
                float col[4] = {
                    color.r / 255.0f,
                    color.g / 255.0f,
                    color.b / 255.0f,
                    color.a / 255.0f
                };
                GUI::ColorButton("##color", ImVec4(col[0], col[1], col[2], col[3]));
                GUI::SameLine();
                
                // Slider to adjust particle count
                int size = cluster.size();
                char label[64];
                sprintf(label, "Cluster %d", i);
                if (GUI::SliderInt(label, &size, 10, 500)) {
                    particleSystem.resizeCluster(i, size);
                }
                
                // Delete button
                GUI::SameLine();
                if (GUI::Button("X")) {
                    particleSystem.removeCluster(i);
                    GUI::PopID();
                    break; // Exit loop as indices change
                }
                
                GUI::PopID();
            }
            
            GUI::Separator();
            
            if (GUI::Button("Add Cluster")) {
                particleSystem.addCluster(100);
            }
        }
        
        // ===== RULES SECTION =====
        if (GUI::CollapsingHeader("Rules")) {
            char ruleText[128];
            sprintf(ruleText, "Active Rules: %d", particleSystem.getRuleCount());
            GUI::Text(ruleText);
            GUI::Separator();
            
            int ruleCount = particleSystem.getRuleCount();
            
            // Display and edit existing rules
            for (int i = 0; i < ruleCount; ++i) {
                GUI::PushID(1000 + i);
                
                auto& rule = particleSystem.getRule(i);
                
                char ruleLabel[128];
                sprintf(ruleLabel, "C%d -> C%d:", rule.clusterA, rule.clusterB);
                GUI::Text(ruleLabel);
                GUI::SameLine();
                
                if (GUI::SliderFloat("##gravity", &rule.gravity, -50.f, 50.f)) {
                    particleSystem.setRule(i, rule.gravity);
                }
                
                GUI::SameLine();
                if (GUI::Button("Del")) {
                    particleSystem.removeRule(i);
                    GUI::PopID();
                    break;
                }
                
                GUI::PopID();
            }
            
            GUI::Separator();
            GUI::Text("Add New Rule:");
            
            int clusterCount = particleSystem.getClusterCount();
            if (clusterCount > 0) {
                GUI::SliderInt("From Cluster", &newRuleFrom, 0, clusterCount - 1);
                GUI::SliderInt("To Cluster", &newRuleTo, 0, clusterCount - 1);
                GUI::SliderFloat("Gravity", &newRuleGravity, -50.f, 50.f);
                
                if (GUI::Button("Add Rule")) {
                    particleSystem.addRule(newRuleFrom, newRuleTo, newRuleGravity);
                }
                
                GUI::SameLine();
                if (GUI::Button("Clear All Rules")) {
                    particleSystem.clearRules();
                }
            } else {
                GUI::Text("(Add clusters first)");
            }
        }
        
        // ===== PRESETS SECTION =====
        if (GUI::CollapsingHeader("Presets")) {
            if (GUI::Button("Default 3 Clusters")) {
                particleSystem.setupDefault3Clusters();
            }
            
            GUI::SameLine();
            
            if (GUI::Button("Random Rules")) {
                particleSystem.generateRandomRules(-3.f, 3.f);
            }
            
            GUI::Separator();
            
            // Specific presets
            if (GUI::Button("Chaotic Orbits")) {
                setupChaoticOrbits();
            }
            
            GUI::SameLine();
            
            if (GUI::Button("Predator-Prey")) {
                setupPredatorPrey();
            }
            
            if (GUI::Button("Liquid Crystal")) {
                setupLiquidCrystal();
            }
            
            GUI::SameLine();
            
            if (GUI::Button("Sorting")) {
                setupSpontaneousSorting();
            }
        }
        
        // ===== STATISTICS SECTION =====
        if (GUI::CollapsingHeader("Statistics")) {
            char statsText[256];
            sprintf(statsText, "Clusters: %d", particleSystem.getClusterCount());
            GUI::Text(statsText);
            sprintf(statsText, "Total Particles: %d", particleSystem.getTotalParticles());
            GUI::Text(statsText);
            sprintf(statsText, "Rules: %d", particleSystem.getRuleCount());
            GUI::Text(statsText);
            sprintf(statsText, "FPS: %.1f", GUI::GetIO().Framerate);
            GUI::Text(statsText);
            
            char statusText[64];
            sprintf(statusText, "Status: %s", paused ? "PAUSED" : "RUNNING");
            GUI::Text(statusText);
        }
        
        GUI::EndWindow();
        
        // Help window
        RenderHelpWindow();
    }
    
    void RenderHelpWindow() {
        GUI::BeginWindow("Help");
        
        GUI::Text("Particle Life - Controls");
        GUI::Separator();
        
        GUI::BulletText("Gravity < 0: Repulsion");
        GUI::BulletText("Gravity > 0: Attraction");
        GUI::BulletText("Max Distance: Interaction range");
        GUI::BulletText("Adjust cluster sizes in real-time");
        GUI::BulletText("Add/remove rules dynamically");
        
        GUI::Separator();
        GUI::Text("Tips:");
        GUI::BulletText("Try different presets!");
        GUI::BulletText("Experiment with gravity values");
        GUI::BulletText("Observe emergent behaviors");
        
        GUI::EndWindow();
    }
    
    // ===== PRESETS =====
    
    /**
     * Chaotic Orbits preset
     * Two groups rotating around each other
     * (Gravity values adjusted for pixel coordinates)
     */
    void setupChaoticOrbits() {
        particleSystem.clear();
        
        int red = particleSystem.addCluster(150, ParticleLife::Color::Red());
        int blue = particleSystem.addCluster(150, ParticleLife::Color::Blue());
        
        // Original values were very high, scaled down a bit
        particleSystem.addRule(red, red, -2.0f);
        particleSystem.addRule(red, blue, 1.0f);
        particleSystem.addRule(blue, blue, -2.0f);
        particleSystem.addRule(blue, red, 1.0f);
        
        Debug::Log("Loaded preset: Chaotic Orbits");
    }
    
    /**
     * Predator-Prey preset
     * Red chases Green which tries to escape
     * (Gravity values adjusted for pixel coordinates)
     */
    void setupPredatorPrey() {
        particleSystem.clear();
        
        int prey = particleSystem.addCluster(250, ParticleLife::Color::Green());
        int predator = particleSystem.addCluster(80, ParticleLife::Color::Red());
        
        // Scaled down from very high original values
        particleSystem.addRule(prey, prey, 0.8f);
        particleSystem.addRule(prey, predator, -3.0f);
        particleSystem.addRule(predator, predator, -1.0f);
        particleSystem.addRule(predator, prey, 2.5f);
        
        Debug::Log("Loaded preset: Predator-Prey");
    }
    
    /**
     * Liquid Crystal preset
     * Formation of crystalline structures
     * (Gravity values adjusted for pixel coordinates)
     */
    void setupLiquidCrystal() {
        particleSystem.clear();
        
        int blue = particleSystem.addCluster(120, ParticleLife::Color::Blue());
        int cyan = particleSystem.addCluster(120, ParticleLife::Color::Cyan());
        int white = particleSystem.addCluster(120, ParticleLife::Color::White());
        
        // Moderate attraction/repulsion values
        particleSystem.addRule(blue, blue, 1.5f);
        particleSystem.addRule(blue, cyan, -0.8f);
        particleSystem.addRule(blue, white, 0.8f);
        
        particleSystem.addRule(cyan, cyan, 1.5f);
        particleSystem.addRule(cyan, blue, -0.8f);
        particleSystem.addRule(cyan, white, 0.8f);
        
        particleSystem.addRule(white, white, 1.5f);
        particleSystem.addRule(white, blue, 0.8f);
        particleSystem.addRule(white, cyan, 0.8f);
        
        Debug::Log("Loaded preset: Liquid Crystal");
    }
    
    /**
     * Spontaneous Sorting preset
     * Colors separate into distinct zones
     */
    void setupSpontaneousSorting() {
        particleSystem.clear();
        
        int red = particleSystem.addCluster(100, ParticleLife::Color::Red());
        int green = particleSystem.addCluster(100, ParticleLife::Color::Green());
        int blue = particleSystem.addCluster(100, ParticleLife::Color::Blue());
        
        float attract = 0.8f;
        float repel = -1.2f;
        
        particleSystem.addRule(red, red, attract);
        particleSystem.addRule(red, green, repel);
        particleSystem.addRule(red, blue, repel);
        
        particleSystem.addRule(green, green, attract);
        particleSystem.addRule(green, red, repel);
        particleSystem.addRule(green, blue, repel);
        
        particleSystem.addRule(blue, blue, attract);
        particleSystem.addRule(blue, red, repel);
        particleSystem.addRule(blue, green, repel);
        
        Debug::Log("Loaded preset: Spontaneous Sorting");
    }
    
    void OnShutdown() override {
        Debug::Log("Particle Life simulation shutting down...");
    }
};

/**
 * Factory function
 * Called from main.cpp to create the application instance
 */
Application* CreateParticleLifeApplication() {
    return new ParticleLifeApplication();
}