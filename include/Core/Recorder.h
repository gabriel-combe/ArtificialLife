#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

struct RawFrame {
    std::vector<uint8_t> pixels;
    int width, height;
    int index;
};

class Recorder {
private:
    bool        recording_          = false;
    int         frameIndex_         = 0;
    int         captureCounter_     = 0;
    int         width_              = 0;
    int         height_             = 0;
    std::string sessionDir_;            // capture/2025-04-07_14-30-00/
    std::string outputPath_;            // capture/2025-04-07_14-30-00.mp4

    std::thread     ffmpegThread_;
    std::atomic<bool> converting_{ false };

    // Queue producer/consummer
    std::queue<RawFrame>    frameQueue_;
    std::mutex              queueMutex_;
    std::condition_variable queueCV_;
    bool                    writerDone_ = false;
    std::thread             writerThread_;

    // Audio source for muxing (set before toggle() starts recording)
    std::string audioFilePath_;
    double      audioOffsetSec_ = 0.0;  // playback position at recording start

    std::string makeSessionDir();
    void runFfmpeg();
    void writerLoop();

public:
    ~Recorder();

    void toggle(int screenW, int screenH);
    void captureFrame(SDL_Renderer* renderer);
    void screenshot(SDL_Renderer* renderer);
    void finalize();

    // Set the audio file and the playback offset (seconds) at the exact moment
    // recording will start.  Call this immediately before toggle().
    // The offset ensures the muxed audio is in sync with the visual changes.
    // Uses -stream_loop -1 so a short file loops over a long recording.
    void setAudioSource(const std::string& path, double offsetSec) {
        audioFilePath_  = path;
        audioOffsetSec_ = offsetSec;
    }
    void clearAudioSource() { audioFilePath_.clear(); audioOffsetSec_ = 0.0; }

    bool isRecording()  const { return recording_;  }
    bool isConverting() const { return converting_; }
};