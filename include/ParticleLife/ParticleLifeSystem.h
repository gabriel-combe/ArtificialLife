#pragma once

#include "Cluster.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <cmath>
#include <string>

namespace ParticleLife {

using json = nlohmann::json;

enum class BoundaryMode {
    Wrapping,
    Clamping,
};

struct Rule {
    int   clusterA;
    int   clusterB;
    float gravity;
    float radius;

    Rule(int a, int b, float g, float r = 200.0f)
        : clusterA(a), clusterB(b), gravity(g), radius(r) {}
};

class ParticleLifeSystem {
private:
    std::vector<Cluster> clusters_;
    std::vector<Rule>    rules_;

    // Global physics
    float        viscosity_    = 0.5f;
    float        worldGravity_ = 0.0f;
    float        particleSize_ = 3.0f;
    BoundaryMode boundaryMode_ = BoundaryMode::Clamping;

    // Mouse interaction
    float mouseRadius_   = 200.0f;
    float mouseStrength_ = 5.0f;

    // Connection rendering
    bool  showConnections_  = false;
    float connectionRadius_ = 50.0f;
    int   maxConnections_   = 5;

    // Bounds
    float marginX_ = 50.0f;
    float marginY_ = 50.0f;
    int   screenW_ = 1920;
    int   screenH_ = 1080;

    int totalParticles_ = 0;

public:
    ParticleLifeSystem() = default;

    // ── Screen ────────────────────────────────────────────────────────────
    void setScreenSize(int w, int h) { screenW_ = w; screenH_ = h; }

    // ── Getters / Setters ─────────────────────────────────────────────────
    float        getViscosity()        const { return viscosity_;        }
    float        getWorldGravity()     const { return worldGravity_;     }
    float        getParticleSize()     const { return particleSize_;     }
    BoundaryMode getBoundaryMode()     const { return boundaryMode_;     }
    float        getMouseRadius()      const { return mouseRadius_;      }
    float        getMouseStrength()    const { return mouseStrength_;    }
    bool         getShowConnections()  const { return showConnections_;  }
    float        getConnectionRadius() const { return connectionRadius_; }
    int          getMaxConnections()   const { return maxConnections_;   }

    void setViscosity       (float v)        { viscosity_        = v; }
    void setWorldGravity    (float g)        { worldGravity_     = g; }
    void setParticleSize    (float s)        { particleSize_     = s; }
    void setBoundaryMode    (BoundaryMode m) { boundaryMode_     = m; }
    void setMouseRadius     (float r)        { mouseRadius_      = r; }
    void setMouseStrength   (float s)        { mouseStrength_    = s; }
    void setShowConnections (bool b)         { showConnections_  = b; }
    void setConnectionRadius(float r)        { connectionRadius_ = r; }
    void setMaxConnections  (int n)          { maxConnections_   = n; }

    // ── Clusters ──────────────────────────────────────────────────────────
    int addCluster(int count, const Color& color = Color::Random()) {
        Cluster c(count, color);
        c.resize(count,
                 marginX_, marginY_,
                 (float)screenW_ - marginX_,
                 (float)screenH_ - marginY_);
        clusters_.push_back(std::move(c));
        totalParticles_ += count;
        return (int)clusters_.size() - 1;
    }

    void removeCluster(int idx) {
        if (idx < 0 || idx >= (int)clusters_.size()) return;
        totalParticles_ -= clusters_[idx].size();
        clusters_.erase(clusters_.begin() + idx);
        rules_.erase(
            std::remove_if(rules_.begin(), rules_.end(),
                [idx](const Rule& r) {
                    return r.clusterA == idx || r.clusterB == idx;
                }),
            rules_.end());
    }

    void resizeCluster(int idx, int newSize) {
        if (idx < 0 || idx >= (int)clusters_.size()) return;
        totalParticles_ -= clusters_[idx].size();
        clusters_[idx].resize(newSize,
                              marginX_, marginY_,
                              (float)screenW_ - marginX_,
                              (float)screenH_ - marginY_);
        totalParticles_ += newSize;
    }

    void setClusterColor(int idx, const Color& c) {
        if (idx >= 0 && idx < (int)clusters_.size())
            clusters_[idx].setColor(c);
    }

    // ── Rules ─────────────────────────────────────────────────────────────
    void addRule(int a, int b, float gravity, float radius = 200.0f) {
        rules_.emplace_back(a, b, gravity, radius);
    }

    void setRule(int idx, float gravity, float radius) {
        if (idx >= 0 && idx < (int)rules_.size()) {
            rules_[idx].gravity = gravity;
            rules_[idx].radius  = radius;
        }
    }

    void removeRule(int idx) {
        if (idx >= 0 && idx < (int)rules_.size())
            rules_.erase(rules_.begin() + idx);
    }

    void clearRules() { rules_.clear(); }

    void clear() {
        clusters_.clear();
        rules_.clear();
        totalParticles_ = 0;
    }

    // ── Presets ───────────────────────────────────────────────────────────
    void setupDefault4Clusters() {
        clear();
        addCluster(1000, Color::Green());
        addCluster(1000, Color::Red());
        addCluster(1000, Color::White());
        addCluster(1000, Color::Yellow());
        generateRandomRules(-100.f, 100.f, 10.f, 200.f);
    }

    void setupDefault3Clusters() {
        clear();
        addCluster(100, Color::Red());
        addCluster(100, Color::Green());
        addCluster(100, Color::Blue());
        addRule(0, 0, -96.f, 200.f);
        addRule(0, 1, -51.f, 200.f);
        addRule(0, 2, 102.f, 200.f);
        addRule(1, 0,-102.f, 200.f);
        addRule(1, 1, -30.f, 200.f);
        addRule(2, 0, -60.f, 200.f);
        addRule(2, 2,  45.f, 200.f);
    }

    void generateRandomRules(float minG = -100.f, float maxG = 100.f,
                              float minR =   10.f, float maxR = 200.f)
    {
        clearRules();
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dG(minG, maxG);
        std::uniform_real_distribution<float> dR(minR, maxR);
        for (int i = 0; i < (int)clusters_.size(); ++i)
            for (int j = 0; j < (int)clusters_.size(); ++j)
                addRule(i, j, dG(gen), dR(gen));
    }

    void resetPositions() {
        for (int i = 0; i < (int)clusters_.size(); ++i)
            resizeCluster(i, clusters_[i].size());
    }

    // ── Save / Load ───────────────────────────────────────────────────────
    bool saveToFile(const std::string& path) const {
        json j;
        j["viscosity"]      = viscosity_;
        j["worldGravity"]   = worldGravity_;
        j["particleSize"]   = particleSize_;
        j["boundaryMode"]   = (boundaryMode_ == BoundaryMode::Wrapping)
                              ? "wrapping" : "clamping";
        j["mouseRadius"]    = mouseRadius_;
        j["mouseStrength"]  = mouseStrength_;

        j["clusters"] = json::array();
        for (const auto& c : clusters_) {
            const Color& col = c.getColor();
            j["clusters"].push_back({
                { "count", c.size() },
                { "color", { {"r", col.r}, {"g", col.g}, {"b", col.b} } }
            });
        }

        j["rules"] = json::array();
        for (const auto& r : rules_) {
            j["rules"].push_back({
                { "from",    r.clusterA },
                { "to",      r.clusterB },
                { "gravity", r.gravity  },
                { "radius",  r.radius   }
            });
        }

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return file.good();
    }

    bool loadFromFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        json j;
        try { file >> j; }
        catch (const json::exception&) { return false; }

        viscosity_    = j.value("viscosity",    0.5f);
        worldGravity_ = j.value("worldGravity", 0.0f);
        particleSize_ = j.value("particleSize", 3.0f);
        mouseRadius_  = j.value("mouseRadius",  200.0f);
        mouseStrength_= j.value("mouseStrength", 5.0f);

        std::string modeStr = j.value("boundaryMode", "wrapping");
        boundaryMode_ = (modeStr == "clamping")
                        ? BoundaryMode::Clamping : BoundaryMode::Wrapping;

        clear();
        if (j.contains("clusters")) {
            for (const auto& jc : j["clusters"]) {
                int count = jc.value("count", 100);
                Color col;
                if (jc.contains("color")) {
                    col.r = (uint8_t)jc["color"].value("r", 255);
                    col.g = (uint8_t)jc["color"].value("g", 255);
                    col.b = (uint8_t)jc["color"].value("b", 255);
                    col.a = 255;
                }
                addCluster(count, col);
            }
        }

        if (j.contains("rules")) {
            for (const auto& jr : j["rules"]) {
                int   from    = jr.value("from",    0);
                int   to      = jr.value("to",      0);
                float gravity = jr.value("gravity", 0.f);
                float radius  = jr.value("radius",  200.f);
                if (from < (int)clusters_.size() && to < (int)clusters_.size())
                    addRule(from, to, gravity, radius);
            }
        }

        return true;
    }

    // ── Update ────────────────────────────────────────────────────────────
    void update() {
        const float sw = (float)screenW_;
        const float sh = (float)screenH_;

        for (const auto& rule : rules_) {
            if (rule.clusterA < (int)clusters_.size() &&
                rule.clusterB < (int)clusters_.size())
            {
                clusters_[rule.clusterA].rule(
                    clusters_[rule.clusterB],
                    rule.gravity, rule.radius,
                    viscosity_, worldGravity_,
                    sw, sh,
                    boundaryMode_ == BoundaryMode::Wrapping,
                    marginX_, marginY_
                );
            }
        }

        for (auto& c : clusters_) {
            if (boundaryMode_ == BoundaryMode::Wrapping)
                c.applyBoundariesWrapping(marginX_, marginY_, sw - marginX_, sh - marginY_);
            else
                c.applyBoundariesClamping(marginX_, marginY_, sw - marginX_, sh - marginY_);
        }
    }

    // ── Mouse interaction ─────────────────────────────────────────────────
    void applyMouseForce(float mouseX, float mouseY, float strength, float radius) {
        const float sw = (float)screenW_;
        const float sh = (float)screenH_;
        for (auto& c : clusters_)
            c.applyMouseForce(mouseX, mouseY, strength, radius, sw, sh);
    }

    // ── Render ────────────────────────────────────────────────────────────
    void draw(SDL_Renderer* renderer) {
        const float sw            = (float)screenW_;
        const float sh            = (float)screenH_;
        const int   particleRadius = std::max(1, (int)particleSize_);

        // Connections underneath particles
        if (showConnections_) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            for (const auto& c : clusters_)
                c.drawConnections(renderer, sw, sh,
                                  connectionRadius_, maxConnections_);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        for (const auto& c : clusters_)
            c.draw(renderer, particleRadius);

        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_FRect boundary = {
            marginX_, marginY_,
            (float)screenW_ - 2.f * marginX_,
            (float)screenH_ - 2.f * marginY_
        };
        SDL_RenderRect(renderer, &boundary);
    }

    // ── Statistics / accessors ────────────────────────────────────────────
    int getClusterCount()   const { return (int)clusters_.size(); }
    int getTotalParticles() const { return totalParticles_; }
    int getRuleCount()      const { return (int)rules_.size(); }

    Cluster&       getCluster(int i)       { return clusters_[i]; }
    const Cluster& getCluster(int i) const { return clusters_[i]; }

    Rule&       getRule(int i)       { return rules_[i]; }
    const Rule& getRule(int i) const { return rules_[i]; }
};

} // namespace ParticleLife