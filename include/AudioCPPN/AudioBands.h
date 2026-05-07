#pragma once
#include <array>
#include <algorithm>
#include <deque>
#include <vector>
#include <atomic>
#include <string>
#include <cstdint>
#include <complex>

// Forward-declare miniaudio types to avoid pulling in the full 96K-line header
// into every translation unit that includes this header.
struct ma_device;
struct ma_decoder;

namespace AudioCPPN {

static constexpr int FFT_SIZE      = 2048;
static constexpr int NUM_BANDS     = 8;
static constexpr int RING_CAPACITY = FFT_SIZE * 8; // ~370ms buffer at 44100Hz

struct BandDef { float lowHz; float highHz; const char* name; };

static constexpr std::array<BandDef, NUM_BANDS> BAND_DEFS = {{
    {     0.f,    60.f, "Sub-bass"  },
    {    60.f,   250.f, "Bass"      },
    {   250.f,   500.f, "Low-mid"   },
    {   500.f,  2000.f, "Mid"       },
    {  2000.f,  4000.f, "Upper-mid" },
    {  4000.f,  6000.f, "Presence"  },
    {  6000.f, 20000.f, "Brilliance"},
    { 20000.f, 24000.f, "Ultra"     },
}};

// Lock-free single-producer / single-consumer ring buffer for mono float PCM.
// Audio thread writes; main thread reads.  No mutex, no blocking.
class PcmRingBuffer {
public:
    explicit PcmRingBuffer(int capacity = RING_CAPACITY);

    // Called from audio thread. Mixes stereo→mono, applies volume, writes to ring.
    // Overwrites oldest data when full (never blocks).
    void write(const float* pcm, int frameCount, int channels, float volume);

    // Called from main thread. Copies the most recent `count` mono samples into dst.
    // Returns false if fewer than `count` samples have been written so far.
    bool readLatest(float* dst, int count) const;

    int totalWritten() const { return writeCount_.load(std::memory_order_acquire); }

private:
    std::vector<float>   buf_;
    int                  capacity_;
    std::atomic<int>     writePos_{ 0 };   // producer index (mod capacity_)
    std::atomic<int>     writeCount_{ 0 }; // total samples ever written
};

// Loads an audio file (MP3/WAV/FLAC) via miniaudio, streams it into a ring buffer
// from the audio callback thread, and computes 8 normalised frequency bands each
// frame from the main thread using Eigen FFT.
class AudioBands {
public:
    AudioBands();
    ~AudioBands();

    AudioBands(const AudioBands&)            = delete;
    AudioBands& operator=(const AudioBands&) = delete;

    // Load a file and start looping playback immediately.
    // Stops any previously loaded file first.  Returns false on failure.
    bool loadFile(const std::string& path);

    // Stop playback and release miniaudio resources.
    void stop();

    // Pause / resume without re-loading.
    void setPaused(bool paused);
    bool isPaused()  const { return paused_.load(); }
    bool isLoaded()  const { return deviceRunning_; }
    bool isPlaying() const { return deviceRunning_ && !paused_.load(); }

    // Volume in [0, 1].
    void  setVolume(float v) { volume_.store(std::clamp(v, 0.f, 1.f)); }
    float getVolume()  const { return volume_.load(); }

    // Call once per frame from the main thread.
    // Reads the latest FFT_SIZE mono samples, runs the FFT, extracts and normalises
    // the 8 frequency bands.
    // Returns false if not enough audio data has been received yet.
    bool update();

    // Most recently computed smoothed bands (after update()).
    const std::array<float, NUM_BANDS>& getBands()    const { return smoothed_; }
    const std::array<float, NUM_BANDS>& getRawBands() const { return raw_; }

    // Approximate playback position (seconds).
    double getPositionSec() const;
    double getDurationSec() const { return durationSec_; }
    const std::string& getFilePath() const { return filePath_; }

    // EMA smoothing coefficient [0,1]. Higher = smoother / slower response.
    float getAlpha()      const { return alpha_; }
    void  setAlpha(float a)     { alpha_ = std::clamp(a, 0.001f, 0.999f); }

    // Rolling median window in frames (~frames before audio normalises).
    int  getMedianWindow()    const { return medianWindow_; }
    void setMedianWindow(int n)     { medianWindow_ = std::max(1, n); }

private:
    // miniaudio objects (heap-allocated to keep header clean)
    ma_device*  device_  = nullptr;
    ma_decoder* decoder_ = nullptr;

    bool               deviceRunning_ = false;
    std::atomic<bool>  paused_{ false };
    std::atomic<float> volume_{ 1.f };
    std::string        filePath_;
    double             durationSec_ = 0.0;
    float              sampleRate_  = 44100.f;

    PcmRingBuffer ringBuffer_;

    // FFT scratch buffers (allocated once)
    std::vector<float>                 hannWindow_;
    std::vector<float>                 pcmScratch_;
    std::vector<std::complex<float>>   spectrum_;  // FFT_SIZE/2+1 bins

    // Band state
    std::array<float, NUM_BANDS>               raw_     = {};
    std::array<float, NUM_BANDS>               smoothed_= {};
    std::array<std::deque<float>, NUM_BANDS>   medianHistory_;

    float alpha_        = 0.5f;
    int   medianWindow_ = 43;

    void buildHannWindow();
    void extractBands();
    float medianUpdate(std::deque<float>& hist, float value, int window);
    int   hzToBin(float hz) const;

    // miniaudio callback (static shim + instance handler)
    static void audioCallback(ma_device* dev, void* out,
                               const void* in, uint32_t frameCount);
    void onAudioData(const float* pcm, uint32_t frameCount, int channels);
};

} // namespace AudioCPPN
