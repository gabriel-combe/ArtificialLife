#include "AudioCPPN/AudioCPPN.h"
#include "ParticleLife/ParticleLifeSystem.h"

#include <imgui.h>
#include <implot.h>
#include <Eigen/Dense>

#include <cstring>
#include <algorithm>
#include <string>

namespace AudioCPPN {

// ── Construction ──────────────────────────────────────────────────────────────

AudioCPPN::AudioCPPN()
    : mlp_(NUM_BANDS, 4*4*2, mlpHidden_, mlpLayers_)
    , bandMlp_(1, 4*2, mlpHidden_, mlpLayers_)
{
    browserDir_ = std::filesystem::current_path();

    // Default 4-cluster setup — will be re-synced on first update()
    int nr = 4 * 4;
    lastGravities_.assign(nr, 0.f);
    lastRadii_.assign(nr, 0.f);
    smoothGravities_.assign(nr, 0.f);
    smoothRadii_.assign(nr, 0.f);
    clusterBand_ = { 1, 3, 4, 6 };  // bass / mid / hi-mid / brilliance
    cachedClusterCount_ = 4;
}

// ── Sync to system ────────────────────────────────────────────────────────────

void AudioCPPN::syncToSystem(ParticleLife::ParticleLifeSystem* system) {
    int nc = system->getClusterCount();
    int nr = system->getRuleCount();
    if (nc == cachedClusterCount_ && nr == (int)lastGravities_.size()) return;

    // Preserve existing band assignments; fill new clusters with defaults
    int oldSize = (int)clusterBand_.size();
    clusterBand_.resize(nc);
    for (int i = oldSize; i < nc; ++i)
        clusterBand_[i] = i % NUM_BANDS;

    // Resize output buffers
    lastGravities_.assign(nr, 0.f);
    lastRadii_.assign(nr, 0.f);
    smoothGravities_.assign(nr, 0.f);
    smoothRadii_.assign(nr, 0.f);
    hasValidOutput_ = false;

    // Rebuild both MLPs for new cluster count
    mlp_.rebuild(NUM_BANDS, nr * 2, mlpHidden_, mlpLayers_,
                 weightScale_, (unsigned)weightSeed_);
    bandMlp_.rebuild(1, nc * 2, mlpHidden_, mlpLayers_,
                     weightScale_, (unsigned)weightSeed_);

    cachedClusterCount_ = nc;
}

// ── Per-frame logic ───────────────────────────────────────────────────────────

void AudioCPPN::update(ParticleLife::ParticleLifeSystem* system) {
    if (!system) return;
    syncToSystem(system);
    if (!enabled_) return;

    if (bands_.isPlaying()) {
        if (bands_.update()) {
            if (mode_ == Mode::MLP)
                runMLP();
            else if (mode_ == Mode::BandCluster)
                runBandCluster(system);
            else
                runBandMLP(system);
        }
    }

    if (hasValidOutput_)
        applyToSystem(system);
}

// Map tanh output in [-1,1] linearly to [min, max]
static float mapRange(float t, float lo, float hi) {
    return lo + (t + 1.f) * 0.5f * (hi - lo);
}

void AudioCPPN::runMLP() {
    int nr = (int)lastGravities_.size();
    const auto& b = bands_.getBands();
    Eigen::VectorXf input(NUM_BANDS);
    for (int i = 0; i < NUM_BANDS; ++i)
        input[i] = b[i] * bandWeights_[i];
    Eigen::VectorXf out = mlp_.forward(input);
    for (int i = 0; i < nr; ++i) {
        lastGravities_[i] = mapRange(out[i],      gravityMin_, gravityMax_);
        lastRadii_[i]     = mapRange(out[i + nr], radiusMin_,  radiusMax_);
    }
    hasValidOutput_ = true;
}

void AudioCPPN::runBandCluster(ParticleLife::ParticleLifeSystem* system) {
    const auto& b = bands_.getBands();
    // Median-normalised values are ~1.0 on average.
    // Apply perceptual weight, then amplify deviation from average by sensitivity.
    // sensitivity=1 → ±1 at ±1 unit from average; sensitivity=2 → ±1 at ±0.5 unit.
    auto norm = [&](int bandIdx) {
        return std::clamp((b[bandIdx] * bandWeights_[bandIdx] - 1.f) * bcSensitivity_, -1.f, 1.f);
    };
    int nr = system->getRuleCount();
    for (int k = 0; k < nr && k < (int)lastGravities_.size(); ++k) {
        const auto& rule = system->getRule(k);
        int ca = rule.clusterA;
        int cb = rule.clusterB;
        if (ca >= (int)clusterBand_.size() || cb >= (int)clusterBand_.size()) continue;
        float vi = norm(clusterBand_[ca]);  // source cluster drives gravity
        float vj = norm(clusterBand_[cb]);  // target cluster drives radius
        lastGravities_[k] = mapRange(vi, gravityMin_, gravityMax_);
        lastRadii_[k]     = mapRange(vj, radiusMin_,  radiusMax_);
    }
    hasValidOutput_ = true;
}

void AudioCPPN::runBandMLP(ParticleLife::ParticleLifeSystem* system) {
    const auto& b = bands_.getBands();
    int nc = cachedClusterCount_;
    int nr = system->getRuleCount();

    Eigen::VectorXf input(1);
    for (int i = 0; i < nc && i < (int)clusterBand_.size(); ++i) {
        // Single-value input: this cluster's band energy, normalised by sensitivity
        input[0] = std::clamp(
            (b[clusterBand_[i]] * bandWeights_[clusterBand_[i]] - 1.f) * bcSensitivity_,
            -1.f, 1.f);
        Eigen::VectorXf out = bandMlp_.forward(input);  // nc*2 values in (-1,1)

        // Write outputs to every rule whose source is cluster i
        for (int k = 0; k < nr && k < (int)lastGravities_.size(); ++k) {
            const auto& rule = system->getRule(k);
            if (rule.clusterA != i) continue;
            int j = rule.clusterB;
            if (j < nc) {
                lastGravities_[k] = mapRange(out[j],      gravityMin_, gravityMax_);
                lastRadii_[k]     = mapRange(out[j + nc], radiusMin_,  radiusMax_);
            }
        }
    }
    hasValidOutput_ = true;
}

void AudioCPPN::applyToSystem(ParticleLife::ParticleLifeSystem* system) {
    float a = (mode_ == Mode::MLP) ? outputAlpha_ : bcOutputAlpha_;
    int n = std::min(system->getRuleCount(), (int)lastGravities_.size());
    for (int i = 0; i < n; ++i) {
        smoothGravities_[i] = a * smoothGravities_[i] + (1.f - a) * lastGravities_[i];
        smoothRadii_[i]     = a * smoothRadii_[i]     + (1.f - a) * lastRadii_[i];
        system->setRule(i, smoothGravities_[i], smoothRadii_[i]);
    }
}

// ── GUI ───────────────────────────────────────────────────────────────────────

void AudioCPPN::renderGUI(ParticleLife::ParticleLifeSystem* system) {
    if (system) syncToSystem(system);

    ImGui::SetNextWindowSize(ImVec2(380, 580), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("AudioCPPN")) { ImGui::End(); return; }

    ImGui::PushStyleColor(ImGuiCol_CheckMark,
        enabled_ ? ImVec4(0.2f,1.f,0.4f,1.f) : ImVec4(0.8f,0.8f,0.8f,1.f));
    ImGui::Checkbox("##enabled", &enabled_);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted(enabled_ ? "Enabled — overriding cluster rules"
                                    : "Disabled — manual control active");

    ImGui::Separator();

    int modeInt = (int)mode_;
    ImGui::TextUnformatted("Mode:");
    ImGui::SameLine();
    if (ImGui::RadioButton("MLP",          &modeInt, (int)Mode::MLP))         mode_ = Mode::MLP;
    ImGui::SameLine();
    if (ImGui::RadioButton("BandToCluster", &modeInt, (int)Mode::BandCluster)) mode_ = Mode::BandCluster;
    ImGui::SameLine();
    if (ImGui::RadioButton("BandMLP",      &modeInt, (int)Mode::BandMLP))    mode_ = Mode::BandMLP;

    ImGui::Separator();
    drawAudioPanel();
    ImGui::Separator();
    drawBandViz();
    ImGui::Separator();
    if (mode_ == Mode::MLP)
        drawMLPPanel();
    else if (mode_ == Mode::BandCluster)
        drawBandClusterPanel(system);
    else
        drawBandMLPPanel(system);

    ImGui::End();

    if (showBrowser_)
        drawFileBrowser();
}

// ── Audio panel ───────────────────────────────────────────────────────────────

void AudioCPPN::drawAudioPanel() {
    if (!ImGui::CollapsingHeader("Audio File", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.f);
    ImGui::InputText("##filepath", filePathBuf_, sizeof(filePathBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Browse...", ImVec2(74, 0))) {
        showBrowser_  = true;
        browserDirty_ = true;
        try {
            std::filesystem::path p(reinterpret_cast<const char8_t*>(filePathBuf_));
            std::error_code ec2;
            if (std::filesystem::exists(p.parent_path(), ec2) && !ec2)
                browserDir_ = p.parent_path();
        } catch (...) {}
    }

    if (ImGui::Button("Load & Play")) {
        if (bands_.loadFile(std::string(filePathBuf_)))
            enabled_ = true;
    }
    ImGui::SameLine();

    if (bands_.isLoaded()) {
        if (bands_.isPaused()) {
            if (ImGui::Button("Resume")) bands_.setPaused(false);
        } else {
            if (ImGui::Button("Pause"))  bands_.setPaused(true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            bands_.stop();
            enabled_ = false;
        }
    }

    float vol = bands_.getVolume();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderFloat("Volume", &vol, 0.f, 1.f))
        bands_.setVolume(vol);

    if (bands_.isLoaded()) {
        double pos  = bands_.getPositionSec();
        double dur  = bands_.getDurationSec();
        float  frac = (dur > 0.0) ? (float)(pos / dur) : 0.f;
        char   label[64];
        std::snprintf(label, sizeof(label), "%.1fs / %.1fs", pos, dur);
        ImGui::ProgressBar(frac, ImVec2(-1, 0), label);

        const char* status = bands_.isPaused()  ? "[PAUSED]"
                           : bands_.isPlaying() ? "[PLAYING]"
                           : "[STOPPED]";
        ImGui::TextDisabled("%s", status);
    }
}

// ── Band visualiser ───────────────────────────────────────────────────────────

void AudioCPPN::drawBandViz() {
    if (!ImGui::CollapsingHeader("Frequency Bands", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (!ImPlot::BeginPlot("##bands", ImVec2(-1, 100),
                           ImPlotFlags_NoTitle | ImPlotFlags_NoLegend |
                           ImPlotFlags_NoMouseText)) return;

    ImPlot::SetupAxes(nullptr, nullptr,
                      ImPlotAxisFlags_NoDecorations,
                      ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisLimits(ImAxis_X1, -0.5, NUM_BANDS - 0.5, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 3.0, ImGuiCond_Always);

    const auto& sb = bands_.getBands();
    static const ImVec4 colors[NUM_BANDS] = {
        {0.5f,0.1f,1.0f,1.f}, {0.2f,0.3f,1.0f,1.f}, {0.1f,0.7f,1.0f,1.f},
        {0.1f,1.0f,0.5f,1.f}, {0.5f,1.0f,0.1f,1.f}, {1.0f,0.9f,0.1f,1.f},
        {1.0f,0.5f,0.1f,1.f}, {1.0f,0.1f,0.1f,1.f},
    };
    for (int b = 0; b < NUM_BANDS; ++b) {
        double x   = (double)b;
        double val = (double)sb[b];
        ImPlot::SetNextFillStyle(colors[b]);
        ImPlot::PlotBars(BAND_DEFS[b].name, &x, &val, 1, 0.7);
    }
    ImPlot::EndPlot();

    ImGui::Spacing();
    float cellW = ImGui::GetContentRegionAvail().x / NUM_BANDS;
    for (int b = 0; b < NUM_BANDS; ++b) {
        if (b > 0) ImGui::SameLine(b * cellW);
        ImGui::SetNextItemWidth(cellW);
        ImGui::TextDisabled("%s", BAND_DEFS[b].name);
    }
}

// ── MLP controls ─────────────────────────────────────────────────────────────

void AudioCPPN::drawMLPPanel() {
    if (!ImGui::CollapsingHeader("MLP Controls"))
        return;

    bool rebuild = false;
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderInt("Hidden Size", &mlpHidden_, 8, 128)) rebuild = true;
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderInt("Layers",      &mlpLayers_, 2, 16))  rebuild = true;

    int nOut = (int)lastGravities_.size() * 2;
    if (rebuild) {
        mlp_.rebuild(NUM_BANDS, nOut, mlpHidden_, mlpLayers_,
                     weightScale_, (unsigned)weightSeed_);
        lastGravities_.assign(lastGravities_.size(), 0.f);
        lastRadii_.assign(lastRadii_.size(), 0.f);
        smoothGravities_.assign(smoothGravities_.size(), 0.f);
        smoothRadii_.assign(smoothRadii_.size(), 0.f);
        hasValidOutput_ = false;
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("Weight Scale", &weightScale_, 0.1f, 5.f);
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Seed (0=random)", &weightSeed_);
    if (weightSeed_ < 0) weightSeed_ = 0;

    if (ImGui::Button("Randomise Weights")) {
        mlp_.randomiseWeights(weightScale_, (unsigned)weightSeed_);
        lastGravities_.assign(lastGravities_.size(), 0.f);
        lastRadii_.assign(lastRadii_.size(), 0.f);
        smoothGravities_.assign(smoothGravities_.size(), 0.f);
        smoothRadii_.assign(smoothRadii_.size(), 0.f);
        hasValidOutput_ = false;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Output Ranges");
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Grav min", &gravityMin_, 1.f, -500.f, 0.f,           "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Grav max", &gravityMax_, 1.f, 0.f, 500.f,            "%.0f");
    if (gravityMin_ >= gravityMax_) gravityMax_ = gravityMin_ + 1.f;
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Rad  min", &radiusMin_,  1.f, 1.f, radiusMax_ - 1.f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Rad  max", &radiusMax_,  1.f, radiusMin_ + 1.f, 1000.f, "%.0f");

    ImGui::Separator();
    ImGui::TextUnformatted("Output Smoothing");
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("Output EMA", &outputAlpha_, 0.f, 0.99f, "%.2f");
    ImGui::SameLine();
    ImGui::TextDisabled("(0=off, 0.99=very slow)");

    ImGui::Separator();
    ImGui::TextUnformatted("Band Perceptual Weights");
    ImGui::TextDisabled("Scale each band before MLP input (A-weighting default)");
    static const char* kBandNames[NUM_BANDS] = {
        "Sub", "Bass", "Lo-mid", "Mid", "Hi-mid", "Pres", "Brill", "Ultra"
    };
    for (int i = 0; i < NUM_BANDS; ++i) {
        ImGui::PushID(i);
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat(kBandNames[i], &bandWeights_[i], 0.01f, 0.f, 2.f, "%.2f");
        if (i < NUM_BANDS - 1) ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::Spacing();
    if (ImGui::Button("Reset A-weighting"))
        bandWeights_ = { 0.02f, 0.15f, 0.40f, 0.80f, 1.00f, 0.90f, 0.60f, 0.02f };
    ImGui::SameLine();
    if (ImGui::Button("Flat (1.0)"))
        bandWeights_.fill(1.f);

    ImGui::Separator();
    float alpha = bands_.getAlpha();
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderFloat("Band EMA", &alpha, 0.01f, 0.99f))
        bands_.setAlpha(alpha);
    int mw = bands_.getMedianWindow();
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderInt("Median Window (frames)", &mw, 5, 200))
        bands_.setMedianWindow(mw);

    ImGui::Spacing();
    int nr = (int)lastGravities_.size();
    ImGui::TextDisabled("Network: %d in → %d hidden x %d layers → %d out (%d gravity + %d radius)",
                        mlp_.getInputSize(), mlp_.getHiddenSize(),
                        mlp_.getLayerCount(), mlp_.getOutputSize(), nr, nr);
}

// ── Band→Cluster panel ────────────────────────────────────────────────────────

void AudioCPPN::drawBandClusterPanel(ParticleLife::ParticleLifeSystem* system) {
    if (!ImGui::CollapsingHeader("Band → Cluster", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    int nc = system ? system->getClusterCount() : cachedClusterCount_;
    ImGui::TextDisabled("%d clusters  |  gravity[i→j] by i's band, radius[i→j] by j's band", nc);

    // Convenience presets — resize the system's cluster count
    if (system) {
        ImGui::Spacing();
        auto setNclusters = [&](int target) {
            while (system->getClusterCount() < target)
                system->addCluster(500);
            while (system->getClusterCount() > target)
                system->removeCluster(system->getClusterCount() - 1);
            system->generateRandomRules(gravityMin_, gravityMax_, radiusMin_, radiusMax_);
        };
        for (int n : { 4, 6, 8 }) {
            char label[16];
            std::snprintf(label, sizeof(label), "%d clusters", n);
            if (n == NUM_BANDS) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.6f,0.2f,1.f));
                if (ImGui::Button(label)) setNclusters(n);
                ImGui::PopStyleColor();
            } else {
                if (ImGui::Button(label)) setNclusters(n);
            }
            ImGui::SameLine();
        }
        // Auto-assign: cluster i → band i
        if (ImGui::Button("Auto-assign bands")) {
            for (int i = 0; i < nc && i < (int)clusterBand_.size(); ++i)
                clusterBand_[i] = i % NUM_BANDS;
        }
        ImGui::NewLine();
        ImGui::Separator();
    }

    // Per-cluster band assignment + live energy bar
    const auto& sb = bands_.getBands();
    for (int c = 0; c < nc && c < (int)clusterBand_.size(); ++c) {
        ImGui::PushID(c);
        ImGui::Text("Cluster %d", c);
        ImGui::SameLine(80);
        ImGui::SetNextItemWidth(120);
        if (ImGui::BeginCombo("##band", BAND_DEFS[clusterBand_[c]].name)) {
            for (int b = 0; b < NUM_BANDS; ++b) {
                bool sel = (clusterBand_[c] == b);
                if (ImGui::Selectable(BAND_DEFS[b].name, sel))
                    clusterBand_[c] = b;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        float energy = std::clamp(sb[clusterBand_[c]] * bandWeights_[clusterBand_[c]] / 3.f, 0.f, 1.f);
        char overlay[16];
        std::snprintf(overlay, sizeof(overlay), "%.2f", sb[clusterBand_[c]]);
        ImGui::ProgressBar(energy, ImVec2(80, 0), overlay);
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Output Ranges");
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Grav min##bc", &gravityMin_, 1.f, -500.f, 0.f,           "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Grav max##bc", &gravityMax_, 1.f, 0.f, 500.f,            "%.0f");
    if (gravityMin_ >= gravityMax_) gravityMax_ = gravityMin_ + 1.f;
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Rad  min##bc", &radiusMin_,  1.f, 1.f, radiusMax_ - 1.f, "%.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::DragFloat("Rad  max##bc", &radiusMax_,  1.f, radiusMin_ + 1.f, 1000.f, "%.0f");

    ImGui::Separator();
    ImGui::TextUnformatted("Reactivity");
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("Sensitivity##bc", &bcSensitivity_, 0.5f, 10.f, "%.1f");
    ImGui::SameLine();
    ImGui::TextDisabled("amplifies deviation from average band energy");
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("Output EMA##bc", &bcOutputAlpha_, 0.f, 0.99f, "%.2f");
    ImGui::SameLine();
    ImGui::TextDisabled("(0=instant, 0.99=very slow)");

    ImGui::Separator();
    ImGui::TextUnformatted("Band Perceptual Weights");
    ImGui::TextDisabled("Scale each band before output mapping");
    static const char* kBandNames[NUM_BANDS] = {
        "Sub", "Bass", "Lo-mid", "Mid", "Hi-mid", "Pres", "Brill", "Ultra"
    };
    for (int i = 0; i < NUM_BANDS; ++i) {
        ImGui::PushID(100 + i);
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat(kBandNames[i], &bandWeights_[i], 0.01f, 0.f, 2.f, "%.2f");
        if (i < NUM_BANDS - 1) ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::Spacing();
    if (ImGui::Button("Reset A-weighting##bc"))
        bandWeights_ = { 0.02f, 0.15f, 0.40f, 0.80f, 1.00f, 0.90f, 0.60f, 0.02f };
    ImGui::SameLine();
    if (ImGui::Button("Flat (1.0)##bc"))
        bandWeights_.fill(1.f);

    ImGui::Separator();
    float alpha = bands_.getAlpha();
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderFloat("Band EMA##bc", &alpha, 0.01f, 0.99f))
        bands_.setAlpha(alpha);
    int mw = bands_.getMedianWindow();
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderInt("Median Window (frames)##bc", &mw, 5, 200))
        bands_.setMedianWindow(mw);
}

// ── BandMLP panel ─────────────────────────────────────────────────────────────

void AudioCPPN::drawBandMLPPanel(ParticleLife::ParticleLifeSystem* system) {
    // ── Cluster → band assignment ─────────────────────────────────────────
    if (ImGui::CollapsingHeader("Band → Cluster", ImGuiTreeNodeFlags_DefaultOpen)) {
        int nc = system ? system->getClusterCount() : cachedClusterCount_;
        ImGui::TextDisabled("%d clusters  |  each cluster runs the same MLP with its band as input", nc);

        if (system) {
            ImGui::Spacing();
            auto setNclusters = [&](int target) {
                while (system->getClusterCount() < target) system->addCluster(500);
                while (system->getClusterCount() > target) system->removeCluster(system->getClusterCount() - 1);
                system->generateRandomRules(gravityMin_, gravityMax_, radiusMin_, radiusMax_);
            };
            for (int n : { 4, 6, 8 }) {
                char label[16];
                std::snprintf(label, sizeof(label), "%d clusters", n);
                if (n == NUM_BANDS) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.6f,0.2f,1.f));
                if (ImGui::Button(label)) setNclusters(n);
                if (n == NUM_BANDS) ImGui::PopStyleColor();
                ImGui::SameLine();
            }
            if (ImGui::Button("Auto-assign##bm")) {
                for (int i = 0; i < nc && i < (int)clusterBand_.size(); ++i)
                    clusterBand_[i] = i % NUM_BANDS;
            }
            ImGui::NewLine();
        }

        const auto& sb = bands_.getBands();
        for (int c = 0; c < (int)clusterBand_.size(); ++c) {
            ImGui::PushID(200 + c);
            ImGui::Text("Cluster %d", c);
            ImGui::SameLine(80);
            ImGui::SetNextItemWidth(120);
            if (ImGui::BeginCombo("##band", BAND_DEFS[clusterBand_[c]].name)) {
                for (int b = 0; b < NUM_BANDS; ++b) {
                    bool sel = (clusterBand_[c] == b);
                    if (ImGui::Selectable(BAND_DEFS[b].name, sel)) clusterBand_[c] = b;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            float energy = std::clamp(sb[clusterBand_[c]] * bandWeights_[clusterBand_[c]] / 3.f, 0.f, 1.f);
            char overlay[16];
            std::snprintf(overlay, sizeof(overlay), "%.2f", sb[clusterBand_[c]]);
            ImGui::ProgressBar(energy, ImVec2(80, 0), overlay);
            ImGui::PopID();
        }
    }

    // ── Per-cluster MLP controls ──────────────────────────────────────────
    if (ImGui::CollapsingHeader("Per-Cluster MLP", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool rebuild = false;
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderInt("Hidden Size##bm", &mlpHidden_, 8, 128)) rebuild = true;
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderInt("Layers##bm",      &mlpLayers_, 2, 16))  rebuild = true;

        int nc = cachedClusterCount_;
        if (rebuild) {
            bandMlp_.rebuild(1, nc * 2, mlpHidden_, mlpLayers_, weightScale_, (unsigned)weightSeed_);
            lastGravities_.assign(lastGravities_.size(), 0.f);
            lastRadii_.assign(lastRadii_.size(), 0.f);
            smoothGravities_.assign(smoothGravities_.size(), 0.f);
            smoothRadii_.assign(smoothRadii_.size(), 0.f);
            hasValidOutput_ = false;
        }

        ImGui::SetNextItemWidth(120);
        ImGui::SliderFloat("Weight Scale##bm", &weightScale_, 0.1f, 5.f);
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Seed (0=random)##bm", &weightSeed_);
        if (weightSeed_ < 0) weightSeed_ = 0;

        if (ImGui::Button("Randomise Weights##bm")) {
            bandMlp_.randomiseWeights(weightScale_, (unsigned)weightSeed_);
            lastGravities_.assign(lastGravities_.size(), 0.f);
            lastRadii_.assign(lastRadii_.size(), 0.f);
            smoothGravities_.assign(smoothGravities_.size(), 0.f);
            smoothRadii_.assign(smoothRadii_.size(), 0.f);
            hasValidOutput_ = false;
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Output Ranges");
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Grav min##bm", &gravityMin_, 1.f, -500.f, 0.f,           "%.0f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Grav max##bm", &gravityMax_, 1.f, 0.f, 500.f,            "%.0f");
        if (gravityMin_ >= gravityMax_) gravityMax_ = gravityMin_ + 1.f;
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Rad  min##bm", &radiusMin_,  1.f, 1.f, radiusMax_ - 1.f, "%.0f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Rad  max##bm", &radiusMax_,  1.f, radiusMin_ + 1.f, 1000.f, "%.0f");

        ImGui::Separator();
        ImGui::SetNextItemWidth(120);
        ImGui::SliderFloat("Sensitivity##bm", &bcSensitivity_, 0.5f, 10.f, "%.1f");
        ImGui::SetNextItemWidth(120);
        ImGui::SliderFloat("Output EMA##bm",  &bcOutputAlpha_, 0.f, 0.99f,  "%.2f");

        ImGui::Separator();
        ImGui::TextUnformatted("Band Perceptual Weights");
        static const char* kBandNames[NUM_BANDS] = {
            "Sub", "Bass", "Lo-mid", "Mid", "Hi-mid", "Pres", "Brill", "Ultra"
        };
        for (int i = 0; i < NUM_BANDS; ++i) {
            ImGui::PushID(300 + i);
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat(kBandNames[i], &bandWeights_[i], 0.01f, 0.f, 2.f, "%.2f");
            if (i < NUM_BANDS - 1) ImGui::SameLine();
            ImGui::PopID();
        }
        ImGui::Spacing();
        if (ImGui::Button("Reset A-weighting##bm"))
            bandWeights_ = { 0.02f, 0.15f, 0.40f, 0.80f, 1.00f, 0.90f, 0.60f, 0.02f };
        ImGui::SameLine();
        if (ImGui::Button("Flat (1.0)##bm"))
            bandWeights_.fill(1.f);

        ImGui::Separator();
        float alpha = bands_.getAlpha();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderFloat("Band EMA##bm", &alpha, 0.01f, 0.99f))
            bands_.setAlpha(alpha);
        int mw = bands_.getMedianWindow();
        ImGui::SetNextItemWidth(120);
        if (ImGui::SliderInt("Median Window##bm", &mw, 5, 200))
            bands_.setMedianWindow(mw);

        ImGui::Spacing();
        ImGui::TextDisabled("Network: 1 in → %d hidden x %d layers → %d out  (run once per cluster)",
                            bandMlp_.getHiddenSize(), bandMlp_.getLayerCount(),
                            bandMlp_.getOutputSize());
    }
}

// ── File browser ──────────────────────────────────────────────────────────────

static std::string pathToUtf8(const std::filesystem::path& p) {
    try {
        auto u8 = p.u8string();
        return { reinterpret_cast<const char*>(u8.data()), u8.size() };
    } catch (...) { return {}; }
}

void AudioCPPN::refreshBrowser(const std::filesystem::path& dir) {
    browserDir_    = dir;
    browserEntries_.clear();
    browserDirty_  = false;

    std::error_code ec;
    std::filesystem::directory_iterator it(dir,
        std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) return;

    for (; it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        try {
            const auto& e = *it;
            bool isDir = e.is_directory(ec);
            if (ec) { ec.clear(); continue; }
            const auto& p = e.path();
            if (isDir) {
                browserEntries_.push_back({ p, true });
            } else {
                auto ext = pathToUtf8(p.extension());
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg")
                    browserEntries_.push_back({ p, false });
            }
        } catch (...) { continue; }
    }
    std::stable_sort(browserEntries_.begin(), browserEntries_.end(),
        [](const auto& a, const auto& b) {
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return pathToUtf8(a.path.filename()) < pathToUtf8(b.path.filename());
        });
}

void AudioCPPN::drawFileBrowser() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    if (!ImGui::Begin("Select Audio File", &showBrowser_)) { ImGui::End(); return; }

    if (browserDirty_)
        refreshBrowser(browserDir_);

    ImGui::TextDisabled("Dir: %s", pathToUtf8(browserDir_).c_str());
    ImGui::SameLine();
    if (ImGui::Button("^  Up") && browserDir_.has_parent_path()) {
        auto parent = browserDir_.parent_path();
        if (parent != browserDir_)
            refreshBrowser(parent);
    }
    ImGui::Separator();

    std::filesystem::path pendingNav;
    ImGui::BeginChild("##filelist", ImVec2(0, 300), true);
    for (const auto& entry : browserEntries_) {
        std::string name  = pathToUtf8(entry.path.filename());
        std::string label = entry.isDir ? ("[D] " + name) : name;

        if (ImGui::Selectable(label.c_str(), false)) {
            if (entry.isDir) {
                pendingNav = entry.path;
            } else {
                auto utf8 = pathToUtf8(entry.path);
                std::strncpy(filePathBuf_, utf8.c_str(), sizeof(filePathBuf_) - 1);
                showBrowser_ = false;
            }
        }
    }
    ImGui::EndChild();

    if (!pendingNav.empty())
        refreshBrowser(pendingNav);

    ImGui::Separator();
    if (ImGui::Button("Cancel", ImVec2(80, 0)))
        showBrowser_ = false;

    ImGui::End();
}

} // namespace AudioCPPN
