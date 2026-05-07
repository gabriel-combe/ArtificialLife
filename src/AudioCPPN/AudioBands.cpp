// Prevent windows.h (pulled in by miniaudio) from defining min/max macros
// that break std::min, std::max, std::clamp.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

// MINIAUDIO_IMPLEMENTATION must be defined in exactly one .cpp file.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

// Undefine any residual min/max macros so subsequent STL usage is clean
#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

#include "AudioCPPN/AudioBands.h"
#include <unsupported/Eigen/FFT>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace AudioCPPN {

// ── PcmRingBuffer ─────────────────────────────────────────────────────────────

PcmRingBuffer::PcmRingBuffer(int capacity)
    : capacity_(capacity), buf_(capacity, 0.f) {}

void PcmRingBuffer::write(const float* pcm, int frameCount, int channels, float volume) {
    int wp = writePos_.load(std::memory_order_relaxed);
    for (int f = 0; f < frameCount; ++f) {
        float mono = 0.f;
        for (int c = 0; c < channels; ++c)
            mono += pcm[f * channels + c];
        mono = (mono / channels) * volume;
        buf_[wp] = mono;
        wp = (wp + 1) % capacity_;
    }
    writePos_.store(wp, std::memory_order_release);
    writeCount_.fetch_add(frameCount, std::memory_order_release);
}

bool PcmRingBuffer::readLatest(float* dst, int count) const {
    if (writeCount_.load(std::memory_order_acquire) < count)
        return false;
    int wp   = writePos_.load(std::memory_order_acquire);
    int start = ((wp - count) % capacity_ + capacity_) % capacity_;
    for (int i = 0; i < count; ++i)
        dst[i] = buf_[(start + i) % capacity_];
    return true;
}

// ── AudioBands ────────────────────────────────────────────────────────────────

AudioBands::AudioBands() {
    buildHannWindow();
    pcmScratch_.resize(FFT_SIZE);
    spectrum_.resize(FFT_SIZE / 2 + 1);
}

AudioBands::~AudioBands() {
    stop();
}

void AudioBands::buildHannWindow() {
    hannWindow_.resize(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; ++i)
        hannWindow_[i] = 0.5f * (1.f - std::cos(2.f * 3.14159265358979f * i / (FFT_SIZE - 1)));
}

bool AudioBands::loadFile(const std::string& path) {
    stop();

    auto* dec = new ma_decoder();
    ma_decoder_config decCfg = ma_decoder_config_init(ma_format_f32, 0, 0);

    // Use wide-string API on Windows so non-ASCII (e.g. Japanese) paths work.
    // path is stored as UTF-8; std::filesystem::path converts it to wstring correctly.
#if defined(_WIN32)
    std::wstring wpath = std::filesystem::path(
        reinterpret_cast<const char8_t*>(path.c_str())).wstring();
    ma_result decResult = ma_decoder_init_file_w(wpath.c_str(), &decCfg, dec);
#else
    ma_result decResult = ma_decoder_init_file(path.c_str(), &decCfg, dec);
#endif
    if (decResult != MA_SUCCESS) {
        delete dec;
        return false;
    }

    // Retrieve sample rate and duration
    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(dec, &totalFrames);
    sampleRate_  = (float)dec->outputSampleRate;
    int channels = (int)dec->outputChannels;
    durationSec_ = (sampleRate_ > 0 && totalFrames > 0)
                   ? (double)totalFrames / sampleRate_ : 0.0;
    decoder_ = dec;

    ma_device_config devCfg          = ma_device_config_init(ma_device_type_playback);
    devCfg.playback.format           = ma_format_f32;
    devCfg.playback.channels         = (ma_uint32)channels;
    devCfg.sampleRate                = (ma_uint32)sampleRate_;
    devCfg.dataCallback              = &AudioBands::audioCallback;
    devCfg.pUserData                 = this;

    auto* dev = new ma_device();
    if (ma_device_init(nullptr, &devCfg, dev) != MA_SUCCESS) {
        ma_decoder_uninit(dec);
        delete dec;
        delete dev;
        decoder_ = nullptr;
        return false;
    }
    device_ = dev;

    if (ma_device_start(dev) != MA_SUCCESS) {
        ma_device_uninit(dev);
        ma_decoder_uninit(dec);
        delete dev;
        delete dec;
        device_  = nullptr;
        decoder_ = nullptr;
        return false;
    }

    filePath_     = path;
    deviceRunning_ = true;
    paused_.store(false);
    // Reset band state
    smoothed_ = {};
    raw_      = {};
    for (auto& d : medianHistory_) d.clear();
    return true;
}

void AudioBands::stop() {
    if (device_) {
        ma_device_uninit(device_);
        delete device_;
        device_ = nullptr;
    }
    if (decoder_) {
        ma_decoder_uninit(decoder_);
        delete decoder_;
        decoder_ = nullptr;
    }
    deviceRunning_ = false;
    filePath_.clear();
    durationSec_ = 0.0;
}

void AudioBands::setPaused(bool p) {
    if (!deviceRunning_) return;
    if (p && !paused_.load()) {
        ma_device_stop(device_);
        paused_.store(true);
    } else if (!p && paused_.load()) {
        ma_device_start(device_);
        paused_.store(false);
    }
}

double AudioBands::getPositionSec() const {
    if (!decoder_ || sampleRate_ <= 0.f) return 0.0;
    ma_uint64 cursor = 0;
    ma_decoder_get_cursor_in_pcm_frames(decoder_, &cursor);
    return (double)cursor / sampleRate_;
}

// ── Audio callback (audio thread) ─────────────────────────────────────────────

void AudioBands::audioCallback(ma_device* dev, void* out,
                                const void* /*in*/, uint32_t frameCount) {
    auto* self    = static_cast<AudioBands*>(dev->pUserData);
    auto* outF    = static_cast<float*>(out);
    int   channels = (int)dev->playback.channels;
    int   total    = (int)(frameCount * channels);

    // Decode from file
    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(self->decoder_, outF, frameCount, &framesRead);

    // Loop: if we hit EOF, seek back and fill remainder
    if (framesRead < frameCount) {
        ma_decoder_seek_to_pcm_frame(self->decoder_, 0);
        ma_uint64 remaining = frameCount - framesRead;
        ma_decoder_read_pcm_frames(self->decoder_,
                                    outF + framesRead * channels,
                                    remaining, nullptr);
    }

    // Fill silence for any un-decoded samples (shouldn't happen with loop)
    if (framesRead == 0)
        std::memset(outF, 0, total * sizeof(float));

    // Push to ring buffer for FFT analysis
    self->onAudioData(outF, frameCount, channels);
}

void AudioBands::onAudioData(const float* pcm, uint32_t frameCount, int channels) {
    ringBuffer_.write(pcm, (int)frameCount, channels, volume_.load(std::memory_order_relaxed));
}

// ── Main-thread DSP ───────────────────────────────────────────────────────────

bool AudioBands::update() {
    if (!ringBuffer_.readLatest(pcmScratch_.data(), FFT_SIZE))
        return false;

    // Apply Hann window
    for (int i = 0; i < FFT_SIZE; ++i)
        pcmScratch_[i] *= hannWindow_[i];

    // FFT via Eigen (kissfft backend, twiddle factors cached in fft object)
    static thread_local Eigen::FFT<float> fft;
    fft.SetFlag(Eigen::FFT<float>::HalfSpectrum);
    fft.fwd(spectrum_, pcmScratch_);

    extractBands();
    return true;
}

void AudioBands::extractBands() {
    for (int b = 0; b < NUM_BANDS; ++b) {
        int lo = hzToBin(BAND_DEFS[b].lowHz);
        int hi = hzToBin(BAND_DEFS[b].highHz);
        if (hi <= lo) hi = lo + 1;
        hi = std::min(hi, (int)spectrum_.size());
        lo = std::min(lo, hi - 1);

        float sum = 0.f;
        for (int k = lo; k < hi; ++k)
            sum += std::abs(spectrum_[k]);
        float energy = sum / (float)(hi - lo);

        // Rolling median normalisation
        float norm = medianUpdate(medianHistory_[b], energy, medianWindow_);
        float median = norm; // medianUpdate returns the normalised value

        // Threshold
        float v = (median < 0.1f) ? 0.f : median;
        raw_[b] = v;

        // EMA smoothing
        smoothed_[b] = alpha_ * v + (1.f - alpha_) * smoothed_[b];
    }
}

float AudioBands::medianUpdate(std::deque<float>& hist, float value, int window) {
    hist.push_back(value);
    if ((int)hist.size() > window)
        hist.pop_front();

    // Compute median
    std::vector<float> sorted(hist.begin(), hist.end());
    std::sort(sorted.begin(), sorted.end());
    float med = sorted[sorted.size() / 2];

    // Normalise: divide by median (floor at epsilon)
    static constexpr float EPS = 1e-6f;
    return value / std::max(med, EPS);
}

int AudioBands::hzToBin(float hz) const {
    return std::clamp((int)std::round(hz * FFT_SIZE / sampleRate_),
                      0, FFT_SIZE / 2);
}

} // namespace AudioCPPN
