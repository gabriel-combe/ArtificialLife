#pragma once
#include "AudioCPPN/AudioBands.h"
#include "AudioCPPN/SimpleMLP.h"
#include <string>
#include <vector>
#include <filesystem>

namespace ParticleLife { class ParticleLifeSystem; }

namespace AudioCPPN {

// Drives ParticleLife attraction/repulsion values in real-time from audio.
//
// Usage:
//   Call update(&system)  in OnUpdate  — before particleSystem.update()
//   Call renderGUI(&system) in OnGUI
class AudioCPPN {
public:
    AudioCPPN();
    ~AudioCPPN() = default;

    AudioCPPN(const AudioCPPN&)            = delete;
    AudioCPPN& operator=(const AudioCPPN&) = delete;

    void update(ParticleLife::ParticleLifeSystem* system);
    void renderGUI(ParticleLife::ParticleLifeSystem* system);

    AudioBands& audioBands() { return bands_; }
    const std::vector<float>& getLastGravities() const { return lastGravities_; }

    bool isEnabled() const { return enabled_; }

    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);

    enum class Mode { MLP, BandCluster, BandMLP };

private:
    // Scalars first — C++ initialises in declaration order; mlp_ reads these.
    bool  enabled_     = false;
    Mode  mode_        = Mode::MLP;
    int   mlpHidden_   = 32;
    int   mlpLayers_   = 8;
    float weightScale_ = 2.f;
    int   weightSeed_  = 0;

    // Cached cluster/rule count — used to detect topology changes and resize buffers.
    int cachedClusterCount_ = 0;

    // Output range controls (tanh ∈ [-1,1] mapped linearly to these ranges)
    float gravityMin_ = -100.f;
    float gravityMax_ =  100.f;
    float radiusMin_  =   10.f;
    float radiusMax_  =  500.f;

    // EMA on outputs before applying to system (0=off, 0.99=very slow).
    float outputAlpha_ = 0.85f;

    // Per-band perceptual weights (A-weighting defaults).
    std::array<float, NUM_BANDS> bandWeights_ = {
        0.02f,  // Sub-bass   0-60 Hz
        0.15f,  // Bass       60-250 Hz
        0.40f,  // Low-mid    250-500 Hz
        0.80f,  // Mid        500-2k Hz
        1.00f,  // Upper-mid  2-4k Hz   — peak sensitivity
        0.90f,  // Presence   4-6k Hz
        0.60f,  // Brilliance 6-20k Hz
        0.02f,  // Ultra      20k+ Hz
    };

    // Band→Cluster specific controls (independent from MLP equivalents)
    float bcSensitivity_ = 2.0f;  // amplifies deviation from median before mapRange
    float bcOutputAlpha_ = 0.5f;  // output EMA — lower = more reactive than MLP default

    // Band→Cluster: which band index drives each cluster.
    // Resized dynamically; default assignment: cluster i → band (i % NUM_BANDS).
    std::vector<int> clusterBand_;

    AudioBands bands_;
    SimpleMLP  mlp_;      // Mode::MLP      — NUM_BANDS inputs, nc*nc*2 outputs
    SimpleMLP  bandMlp_;  // Mode::BandMLP  — 1 input (single band energy), nc*2 outputs

    // Raw and smoothed outputs — sized to current rule count, resized in syncToSystem().
    std::vector<float> lastGravities_;
    std::vector<float> lastRadii_;
    std::vector<float> smoothGravities_;
    std::vector<float> smoothRadii_;
    bool hasValidOutput_ = false;

    // GUI transient state
    char   filePathBuf_[512] = "";
    bool   showBrowser_      = false;
    std::filesystem::path browserDir_;
    struct BrowserEntry { std::filesystem::path path; bool isDir; };
    std::vector<BrowserEntry> browserEntries_;
    bool   browserDirty_ = true;

    // Called at the start of update() — resizes buffers and rebuilds MLP when
    // the system's cluster count changes.
    void syncToSystem(ParticleLife::ParticleLifeSystem* system);

    void applyToSystem(ParticleLife::ParticleLifeSystem* system);
    void runMLP();
    void runBandCluster(ParticleLife::ParticleLifeSystem* system);
    void runBandMLP(ParticleLife::ParticleLifeSystem* system);

    void drawAudioPanel();
    void drawBandViz();
    void drawMLPPanel();
    void drawBandClusterPanel(ParticleLife::ParticleLifeSystem* system);
    void drawBandMLPPanel(ParticleLife::ParticleLifeSystem* system);
    void refreshBrowser(const std::filesystem::path& dir);
    void drawFileBrowser();
};

} // namespace AudioCPPN
