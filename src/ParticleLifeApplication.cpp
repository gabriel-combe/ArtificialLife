#include "Core/Application.h"
#include "Core/Time.h"
#include "Core/Input.h"
#include "Core/GUI.h"
#include "Core/Debug.h"
#include "ParticleLife/ParticleLifeSystem.h"
#include "AudioCPPN/AudioCPPN.h"

#include <filesystem>
#include <algorithm>

class ParticleLifeApplication : public Application {
private:
    ParticleLife::ParticleLifeSystem particleSystem;
    AudioCPPN::AudioCPPN             audioCPPN;

    bool paused = false;

    // "Add new rule" form
    int   newRuleFrom    = 0;
    int   newRuleTo      = 0;
    float newRuleGravity = 0.0f;
    float newRuleRadius  = 200.0f;

    // Save / Load state
    char  savePath_[256]    = "my_preset";
    char  saveMessage_[128] = "";
    float saveMessageTimer_ = 0.f;

    std::filesystem::path modelsDir_;

    bool showModelsBrowser_ = false;
    struct ModelEntry { std::string filename; };
    std::vector<ModelEntry> modelEntries_;
    bool modelsDirty_ = true;

public:
    // Called just before recording starts: inject the current audio playback
    // position so ffmpeg can mux the audio in sync with the cluster changes.
    void OnRecordingStart() override {
        const auto& ab = audioCPPN.audioBands();
        if (ab.isLoaded() && !ab.getFilePath().empty())
            GetRecorder().setAudioSource(ab.getFilePath(), ab.getPositionSec());
        else
            GetRecorder().clearAudioSource();
    }

    void OnStart() override {
        {
            const char* base = SDL_GetBasePath();
            modelsDir_ = base
                ? std::filesystem::path(base) / "models"
                : std::filesystem::current_path() / "models";
            std::error_code ec;
            std::filesystem::create_directories(modelsDir_, ec);
            Debug::Log("Models dir: " + modelsDir_.string());
        }
        Debug::Log("Particle Life simulation starting...");
        particleSystem.setScreenSize(GetScreenWidth(), GetScreenHeight());
        particleSystem.setupDefault4Clusters();
        Debug::Log("Initialized with 4 clusters x 1000 particles");
    }

    void OnUpdate(float deltaTime) override {
        particleSystem.setScreenSize(GetScreenWidth(), GetScreenHeight());

        audioCPPN.update(&particleSystem);

        if (!paused)
            particleSystem.update();

        // ── Mouse interaction ─────────────────────────────────────────────
        if (!GUI::GetIO().WantCaptureMouse) {
            const float mx  = Input::GetMouseX();
            const float my  = Input::GetMouseY();
            const float rad = particleSystem.getMouseRadius();
            const float str = particleSystem.getMouseStrength();

            if (Input::GetMouseButton(1))  // Left-click  → attraction
                particleSystem.applyMouseForce(mx, my, -str, rad);
            if (Input::GetMouseButton(3))  // Right-click → repulsion
                particleSystem.applyMouseForce(mx, my,  str, rad);
        }

        if (saveMessageTimer_ > 0.f)
            saveMessageTimer_ -= deltaTime;
    }

    void OnRender() override {
        particleSystem.draw(GetRenderer());
    }

    void OnGUI() override {
        RenderControlPanel();
        audioCPPN.renderGUI(&particleSystem);
        drawModelsBrowser();
    }

    // =========================================================================
    void RenderControlPanel() {
        GUI::BeginWindow("Particle Life Control");

        // ── Recorder status ────────────────────────────────────────────────
        if (IsRecording()) {
            ImGui::TextColored(ImVec4(1.f, 0.2f, 0.2f, 1.f), "* REC  (F8 to stop)");
        } else if (IsConverting()) {
            ImGui::TextColored(ImVec4(1.f, 0.8f, 0.f, 1.f), "Converting to MP4...");
        } else {
            GUI::Text("F8 : start recording");
        }
        GUI::SameLine();
        if (GUI::Button("Screenshot (F9)"))
            RequestScreenshot();
        GUI::Separator();

        // ===== SIMULATION =====
        if (GUI::CollapsingHeader("Simulation")) {
            if (GUI::Button(paused ? "Resume" : "Pause")) paused = !paused;
            GUI::SameLine();
            if (GUI::Button("Reset Positions")) particleSystem.resetPositions();

            GUI::Separator();

            float visc = particleSystem.getViscosity();
            if (GUI::SliderFloat("Viscosity / Friction", &visc, 0.0f, 1.0f))
                particleSystem.setViscosity(visc);

            float grav = particleSystem.getWorldGravity();
            if (GUI::SliderFloat("World Gravity", &grav, -1.0f, 1.0f))
                particleSystem.setWorldGravity(grav);

            float ps = particleSystem.getParticleSize();
            if (GUI::SliderFloat("Particle Size", &ps, 1.0f, 10.0f))
                particleSystem.setParticleSize(ps);

            GUI::Separator();

            // ── Mouse interaction ──────────────────────────────────────────
            GUI::Text("Mouse Interaction:");

            float mouseRad = particleSystem.getMouseRadius();
            if (GUI::SliderFloat("Mouse Radius",   &mouseRad,  10.f, 400.f))
                particleSystem.setMouseRadius(mouseRad);

            float mouseStr = particleSystem.getMouseStrength();
            if (GUI::SliderFloat("Mouse Strength", &mouseStr,   0.f,  20.f))
                particleSystem.setMouseStrength(mouseStr);

            GUI::Separator();

            // ── Boundary mode ──────────────────────────────────────────────
            GUI::Text("Boundary Mode:");
            GUI::SameLine();

            int mode = (particleSystem.getBoundaryMode()
                        == ParticleLife::BoundaryMode::Wrapping) ? 0 : 1;
            if (ImGui::RadioButton("Wrapping", &mode, 0))
                particleSystem.setBoundaryMode(ParticleLife::BoundaryMode::Wrapping);
            GUI::SameLine();
            if (ImGui::RadioButton("Clamping", &mode, 1))
                particleSystem.setBoundaryMode(ParticleLife::BoundaryMode::Clamping);

            GUI::Separator();

            // ── Connections ────────────────────────────────────────────────
            bool showConn = particleSystem.getShowConnections();
            if (GUI::Checkbox("Show Connections", &showConn))
                particleSystem.setShowConnections(showConn);

            if (showConn) {
                float connRadius = particleSystem.getConnectionRadius();
                if (GUI::SliderFloat("Connection Radius", &connRadius, 5.0f, 300.0f))
                    particleSystem.setConnectionRadius(connRadius);

                int maxConn = particleSystem.getMaxConnections();
                if (GUI::SliderInt("Max Connections", &maxConn, 1, 20))
                    particleSystem.setMaxConnections(maxConn);
            }
        }

        // ===== CLUSTERS =====
        if (GUI::CollapsingHeader("Clusters")) {
            char buf[128];
            sprintf(buf, "Total Particles: %d", particleSystem.getTotalParticles());
            GUI::Text(buf);
            GUI::Separator();

            int clusterCount = particleSystem.getClusterCount();
            for (int i = 0; i < clusterCount; ++i) {
                GUI::PushID(i);
                auto& cluster     = particleSystem.getCluster(i);
                const auto& color = cluster.getColor();

                float col[4] = {
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f
                };
                GUI::ColorButton("##color", ImVec4(col[0], col[1], col[2], col[3]));
                GUI::SameLine();

                int sz = cluster.size();
                char label[64];
                sprintf(label, "Cluster %d", i);
                if (GUI::SliderInt(label, &sz, 10, 2000))
                    particleSystem.resizeCluster(i, sz);

                GUI::SameLine();
                if (GUI::Button("X")) {
                    particleSystem.removeCluster(i);
                    GUI::PopID();
                    break;
                }
                GUI::PopID();
            }
            GUI::Separator();
            if (GUI::Button("Add Cluster"))
                particleSystem.addCluster(100);
        }

        // ===== RULES — grouped by source cluster =====
        if (GUI::CollapsingHeader("Rules")) {
            char buf[128];
            sprintf(buf, "Active Rules: %d", particleSystem.getRuleCount());
            GUI::Text(buf);

            int clusterCount = particleSystem.getClusterCount();
            int ruleCount    = particleSystem.getRuleCount();

            for (int ci = 0; ci < clusterCount; ++ci) {
                bool hasRules = false;
                for (int ri = 0; ri < ruleCount; ++ri)
                    if (particleSystem.getRule(ri).clusterA == ci) { hasRules = true; break; }
                if (!hasRules) continue;

                char sectionLabel[64];
                sprintf(sectionLabel, "  Cluster %d ->", ci);

                GUI::PushID(2000 + ci);
                if (GUI::CollapsingHeader(sectionLabel)) {
                    for (int ri = 0; ri < ruleCount; ++ri) {
                        auto& rule = particleSystem.getRule(ri);
                        if (rule.clusterA != ci) continue;

                        GUI::PushID(3000 + ri);
                        char ruleLabel[32];
                        sprintf(ruleLabel, "-> C%d", rule.clusterB);
                        GUI::Text(ruleLabel);
                        GUI::SameLine();

                        bool changed = false;
                        changed |= GUI::SliderFloat("##g", &rule.gravity, -100.0f, 100.0f);
                        GUI::SameLine();
                        changed |= GUI::SliderFloat("##r", &rule.radius,   10.0f, 500.0f);
                        if (changed)
                            particleSystem.setRule(ri, rule.gravity, rule.radius);

                        GUI::SameLine();
                        if (GUI::Button("Del")) {
                            particleSystem.removeRule(ri);
                            GUI::PopID();
                            ruleCount = particleSystem.getRuleCount();
                            break;
                        }
                        GUI::PopID();
                    }
                }
                GUI::PopID();
            }

            GUI::Separator();
            GUI::Text("Add New Rule:");
            if (clusterCount > 0) {
                GUI::SliderInt ("From Cluster", &newRuleFrom,    0, clusterCount - 1);
                GUI::SliderInt ("To Cluster",   &newRuleTo,      0, clusterCount - 1);
                GUI::SliderFloat("Gravity",     &newRuleGravity, -100.0f, 100.0f);
                GUI::SliderFloat("Radius",      &newRuleRadius,    10.0f, 500.0f);

                if (GUI::Button("Add Rule"))
                    particleSystem.addRule(newRuleFrom, newRuleTo, newRuleGravity, newRuleRadius);
                GUI::SameLine();
                if (GUI::Button("Clear All Rules"))
                    particleSystem.clearRules();
            } else {
                GUI::Text("(Add clusters first)");
            }
        }

        // ===== SAVE / LOAD =====
        if (GUI::CollapsingHeader("Save / Load")) {
            ImGui::TextDisabled("Dir: %s", modelsDir_.string().c_str());

            if (GUI::Button("Browse...")) {
                modelsDirty_       = true;
                showModelsBrowser_ = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50.f);
            ImGui::InputText("##filename", savePath_, sizeof(savePath_));
            ImGui::SameLine();
            GUI::Text(".json");

            GUI::Separator();

            auto makeFullPath = [&]() -> std::string {
                std::string name = savePath_;
                if (name.size() < 5 || name.substr(name.size() - 5) != ".json")
                    name += ".json";
                return (modelsDir_ / name).string();
            };

            if (GUI::Button("  Save  ")) {
                std::string path = makeFullPath();
                if (particleSystem.saveToFile(path)) {
                    sprintf(saveMessage_, "Saved: %s", savePath_);
                    Debug::Log("Preset saved: " + path);
                    modelsDirty_ = true;
                } else {
                    sprintf(saveMessage_, "ERROR: could not write %s", savePath_);
                    Debug::LogError("Could not save preset: " + path);
                }
                saveMessageTimer_ = 3.0f;
            }

            GUI::SameLine();

            if (GUI::Button("  Load  ")) {
                std::string path = makeFullPath();
                if (particleSystem.loadFromFile(path)) {
                    sprintf(saveMessage_, "Loaded: %s", savePath_);
                    Debug::Log("Preset loaded: " + path);
                } else {
                    sprintf(saveMessage_, "ERROR: could not read %s", savePath_);
                    Debug::LogError("Could not load preset: " + path);
                }
                saveMessageTimer_ = 3.0f;
            }

            if (saveMessageTimer_ > 0.f) {
                GUI::Separator();
                bool isError = (saveMessage_[0] == 'E');
                ImVec4 color = isError
                    ? ImVec4(1.f, 0.3f, 0.3f, 1.f)
                    : ImVec4(0.3f, 1.f, 0.3f, 1.f);
                GUI::TextColored(color, saveMessage_);
            }
        }

        // ===== PRESETS =====
        if (GUI::CollapsingHeader("Presets")) {
            if (GUI::Button("Default 4 Clusters (4x1000)"))
                particleSystem.setupDefault4Clusters();
            GUI::SameLine();
            if (GUI::Button("Random Rules"))
                particleSystem.generateRandomRules(-100.f, 100.f, 10.f, 200.f);

            GUI::Separator();

            if (GUI::Button("3 Clusters RGB"))  particleSystem.setupDefault3Clusters();
            GUI::SameLine();
            if (GUI::Button("Chaotic Orbits"))  setupChaoticOrbits();
            if (GUI::Button("Predator-Prey"))   setupPredatorPrey();
            GUI::SameLine();
            if (GUI::Button("Liquid Crystal"))  setupLiquidCrystal();
            if (GUI::Button("Sorting"))         setupSpontaneousSorting();
        }

        // ===== STATISTICS =====
        if (GUI::CollapsingHeader("Statistics")) {
            char buf[256];
            sprintf(buf, "Clusters: %d",        particleSystem.getClusterCount());   GUI::Text(buf);
            sprintf(buf, "Total Particles: %d",  particleSystem.getTotalParticles()); GUI::Text(buf);
            sprintf(buf, "Rules: %d",            particleSystem.getRuleCount());      GUI::Text(buf);
            sprintf(buf, "FPS: %.1f",            GUI::GetIO().Framerate);            GUI::Text(buf);
            sprintf(buf, "Status: %s",           paused ? "PAUSED" : "RUNNING");     GUI::Text(buf);
            const char* modeStr =
                (particleSystem.getBoundaryMode() == ParticleLife::BoundaryMode::Wrapping)
                ? "Wrapping" : "Clamping";
            sprintf(buf, "Boundary: %s", modeStr); GUI::Text(buf);
        }

        GUI::EndWindow();
        RenderHelpWindow();
    }

    void refreshModelsBrowser() {
        modelEntries_.clear();
        modelsDirty_ = false;

        std::error_code ec;
        std::filesystem::directory_iterator it(modelsDir_,
            std::filesystem::directory_options::skip_permission_denied, ec);
        if (ec) return;

        for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            try {
                const auto& e = *it;
                if (e.is_directory(ec)) continue;
                if (ec) { ec.clear(); continue; }
                auto ext = e.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".json")
                    modelEntries_.push_back({ e.path().filename().string() });
            } catch (...) { continue; }
        }

        std::stable_sort(modelEntries_.begin(), modelEntries_.end(),
            [](const ModelEntry& a, const ModelEntry& b) {
                return a.filename < b.filename;
            });
    }

    void drawModelsBrowser() {
        if (!showModelsBrowser_) return;
        if (modelsDirty_) refreshModelsBrowser();

        ImGui::SetNextWindowSize(ImVec2(360, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(
            ImGui::GetMainViewport()->GetCenter(),
            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        if (!ImGui::Begin("Select Preset", &showModelsBrowser_)) {
            ImGui::End();
            return;
        }

        ImGui::TextDisabled("%d preset(s) in models/", (int)modelEntries_.size());
        ImGui::Separator();

        ImGui::BeginChild("##modellist", ImVec2(0, 300), true);
        for (const auto& entry : modelEntries_) {
            if (ImGui::Selectable(entry.filename.c_str(), false)) {
                std::string name = entry.filename;
                if (name.size() >= 5 && name.substr(name.size() - 5) == ".json")
                    name = name.substr(0, name.size() - 5);
                std::strncpy(savePath_, name.c_str(), sizeof(savePath_) - 1);
                savePath_[sizeof(savePath_) - 1] = '\0';
                showModelsBrowser_ = false;
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
            showModelsBrowser_ = false;

        ImGui::End();
    }

    void RenderHelpWindow() {
        GUI::BeginWindow("Help");
        GUI::Text("Particle Life - Controls");
        GUI::Separator();
        GUI::BulletText("Gravity < 0 : Repulsion");
        GUI::BulletText("Gravity > 0 : Attraction");
        GUI::BulletText("Each rule has its own Gravity AND Radius");
        GUI::BulletText("Viscosity : global damping");
        GUI::BulletText("World Gravity : constant downward pull");
        GUI::Separator();
        GUI::Text("Mouse Interaction:");
        GUI::BulletText("Left-click  : attract particles toward cursor");
        GUI::BulletText("Right-click : repel  particles away from cursor");
        GUI::BulletText("Adjust Mouse Radius & Strength in Simulation panel");
        GUI::Separator();
        GUI::Text("Connections:");
        GUI::BulletText("Connects nearby particles of the same cluster");
        GUI::BulletText("Connection Radius : max distance for a link");
        GUI::BulletText("Max Connections : max links per particle");
        GUI::Separator();
        GUI::Text("Boundary Modes:");
        GUI::BulletText("Wrapping : particles teleport to opposite side");
        GUI::BulletText("Clamping : particles stop at the edge");
        GUI::Separator();
        GUI::Text("Recording:");
        GUI::BulletText("F8 : start / stop recording");
        GUI::BulletText("ImGui windows are NOT recorded");
        GUI::BulletText("MP4 generated automatically after stop");
        GUI::Separator();
        GUI::Text("Save / Load:");
        GUI::BulletText("Type a filename, click Save or Load");
        GUI::EndWindow();
    }

    // =========================================================================
    // Presets
    // =========================================================================
    void setupChaoticOrbits() {
        particleSystem.clear();
        int red  = particleSystem.addCluster(1000, ParticleLife::Color::Red());
        int blue = particleSystem.addCluster(1000, ParticleLife::Color::Blue());
        particleSystem.addRule(red,  red,  -2.f,  80.f);
        particleSystem.addRule(red,  blue,  1.f, 160.f);
        particleSystem.addRule(blue, blue, -2.f,  80.f);
        particleSystem.addRule(blue, red,   1.f, 160.f);
        Debug::Log("Loaded preset: Chaotic Orbits");
    }

    void setupPredatorPrey() {
        particleSystem.clear();
        int prey     = particleSystem.addCluster(1000, ParticleLife::Color::Green());
        int predator = particleSystem.addCluster(800,  ParticleLife::Color::Red());
        particleSystem.addRule(prey,     prey,       80.f, 120.f);
        particleSystem.addRule(prey,     predator, -300.f, 200.f);
        particleSystem.addRule(predator, predator, -100.f,  80.f);
        particleSystem.addRule(predator, prey,      250.f, 250.f);
        Debug::Log("Loaded preset: Predator-Prey");
    }

    void setupLiquidCrystal() {
        particleSystem.clear();
        int blue  = particleSystem.addCluster(700, ParticleLife::Color::Blue());
        int cyan  = particleSystem.addCluster(700, ParticleLife::Color::Cyan());
        int white = particleSystem.addCluster(700, ParticleLife::Color::White());
        particleSystem.addRule(blue,  blue,   150.f, 100.f);
        particleSystem.addRule(blue,  cyan,   -80.f, 150.f);
        particleSystem.addRule(blue,  white,   80.f, 150.f);
        particleSystem.addRule(cyan,  cyan,   150.f, 100.f);
        particleSystem.addRule(cyan,  blue,   -80.f, 150.f);
        particleSystem.addRule(cyan,  white,   80.f, 150.f);
        particleSystem.addRule(white, white,  150.f, 100.f);
        particleSystem.addRule(white, blue,    80.f, 150.f);
        particleSystem.addRule(white, cyan,    80.f, 150.f);
        Debug::Log("Loaded preset: Liquid Crystal");
    }

    void setupSpontaneousSorting() {
        particleSystem.clear();
        int red   = particleSystem.addCluster(700, ParticleLife::Color::Red());
        int green = particleSystem.addCluster(700, ParticleLife::Color::Green());
        int blue  = particleSystem.addCluster(700, ParticleLife::Color::Blue());
        const float attr = 80.f, rep = -120.f, r = 150.f;
        particleSystem.addRule(red,   red,   attr, r);
        particleSystem.addRule(red,   green, rep,  r);
        particleSystem.addRule(red,   blue,  rep,  r);
        particleSystem.addRule(green, green, attr, r);
        particleSystem.addRule(green, red,   rep,  r);
        particleSystem.addRule(green, blue,  rep,  r);
        particleSystem.addRule(blue,  blue,  attr, r);
        particleSystem.addRule(blue,  red,   rep,  r);
        particleSystem.addRule(blue,  green, rep,  r);
        Debug::Log("Loaded preset: Spontaneous Sorting");
    }

    void OnShutdown() override {
        Debug::Log("Particle Life simulation shutting down...");
    }
};

Application* CreateParticleLifeApplication() {
    return new ParticleLifeApplication();
}