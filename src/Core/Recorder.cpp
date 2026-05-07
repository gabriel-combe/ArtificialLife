#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "Core/Recorder.h"
#include <filesystem>
#include <ctime>
#include <iostream>

Recorder::~Recorder() {
    if (recording_) {
        recording_ = false;
        finalize();
    }

    if (ffmpegThread_.joinable())
        ffmpegThread_.join();
}

std::string Recorder::makeSessionDir() {
    // Timestamp format : 2025-04-07_14-30-00
    std::time_t now = std::time(nullptr);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));

    std::string dir = "capture/" + std::string(timeBuf) + "/";
    std::filesystem::create_directories(dir);
    return dir;
}

void Recorder::runFfmpeg() {
    if (writerThread_.joinable())
        writerThread_.join();

    converting_ = true;

    // Build ffmpeg command
    // -framerate 60        : input framerate (match app)
    // -i frame_%05d.png    : séquence numbered from 00001
    // -c:v libx264         : encoding H.264
    // -pix_fmt yuv420p     : compatibility max (VLC, Windows Media Player...)
    // -y                   : override .mp4 if existing
    char cmd[2048];
    if (!audioFilePath_.empty()) {
        // -stream_loop -1  → loop the audio track if shorter than the video
        // -ss {offset}     → seek audio to the exact playback position when
        //                    recording started, so visual and audio stay in sync
        // -c:a aac -shortest → encode audio, stop at the shortest stream
        std::snprintf(cmd, sizeof(cmd),
            "ffmpeg -framerate 24 -start_number 1 -i \"%sframe_%%05d.png\" "
            "-stream_loop -1 -ss %.6f -i \"%s\" "
            "-vf \"crop=trunc(iw/2)*2:trunc(ih/2)*2\" "
            "-c:v libx264 -c:a aac -shortest -pix_fmt yuv420p -y \"%s\"",
            sessionDir_.c_str(),
            audioOffsetSec_,
            audioFilePath_.c_str(),
            outputPath_.c_str());
    } else {
        std::snprintf(cmd, sizeof(cmd),
            "ffmpeg -framerate 24 -start_number 1 -i \"%sframe_%%05d.png\" "
            "-vf \"crop=trunc(iw/2)*2:trunc(ih/2)*2\" "
            "-c:v libx264 -pix_fmt yuv420p -y \"%s\"",
            sessionDir_.c_str(),
            outputPath_.c_str());
    }

    std::cout << "[Recorder] Start Conversion..." << std::endl;
    std::cout << "[Recorder] " << cmd << std::endl;

    int ret = std::system(cmd);

    if (ret == 0) {
        std::cout << "[Recorder] MP4 generated -> " << outputPath_ << std::endl;

        // PNG deleted after conversion success
        for (int i = 1; i <= frameIndex_; ++i) {
            char f[512];
            std::snprintf(f, sizeof(f), "%sframe_%05d.png", sessionDir_.c_str(), i);
            std::filesystem::remove(f);
        }

        // Delete folder if empty
        std::filesystem::remove(sessionDir_);

    } else
        std::cout << "[Recorder] ffmpeg failed (code " << ret << ") " << "— PNG frames stocked in " << sessionDir_ << std::endl;

    converting_ = false;
}

void Recorder::writerLoop() {
    while (true) {
        RawFrame frame;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait(lock, [this] {
                return !frameQueue_.empty() || writerDone_;
            });

            if (frameQueue_.empty() && writerDone_) break;

            frame = std::move(frameQueue_.front());
            frameQueue_.pop();
        }

        char filename[512];
        std::snprintf(filename, sizeof(filename), "%sframe_%05d.png", sessionDir_.c_str(), frame.index);

        stbi_write_png(filename, frame.width, frame.height, 3, frame.pixels.data(), frame.width * 3);
    }
}

void Recorder::toggle(int screenW, int screenH) {
    if (!recording_) {
        // -- Start --
        if (converting_) {
            std::cout << "[Recorder] Converting, please wait..." << std::endl;
            return;
        }

        frameIndex_     = 0;
        captureCounter_ = 0;
        width_      = screenW;
        height_     = screenH;
        frameIndex_ = 0;
        sessionDir_ = makeSessionDir();
        outputPath_ = sessionDir_.substr(0, sessionDir_.size() - 1) + ".mp4";

        writerDone_   = false;
        writerThread_ = std::thread(&Recorder::writerLoop, this);

        recording_ = true;
        std::cout << "[Recorder] Recording started -> " << sessionDir_ << std::endl;
    } else {
        // -- Stop --
        recording_ = false;
        std::cout << "[Recorder] Recording stopped - " << frameIndex_ << " frames captured" << std::endl;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            writerDone_ = true;
        }

        queueCV_.notify_one();
        finalize();
    }
}

void Recorder::captureFrame(SDL_Renderer* renderer) {
    if (!recording_) return;

    if (captureCounter_++ % 2 != 0) return;

    // -- Read pixel from renderer --
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (!surface) {
        std::cout << "[Recorder] SDL_RenderReadPixels failed: " << SDL_GetError() << std::endl;
        return;
    }

    // -- Convert to RGB24 if the format is different --
    SDL_Surface* rgb = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGB24);
    SDL_DestroySurface(surface);
    if (!rgb) {
        std::cout << "[Recorder] SDL_ConvertSurface failed: " << SDL_GetError() << std::endl;
        return;
    }

    // -- Build the path : capture/session/frame_00001.png --
    RawFrame frame;
    frame.width  = rgb->w;
    frame.height = rgb->h;
    frame.index  = ++frameIndex_;
    frame.pixels.assign(
        (uint8_t*)rgb->pixels,
        (uint8_t*)rgb->pixels + rgb->w * rgb->h * 3
    );
    SDL_DestroySurface(rgb);

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        frameQueue_.push(std::move(frame));
    }
    queueCV_.notify_one();
}

void Recorder::screenshot(SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (!surface) return;

    SDL_Surface* rgb = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGB24);
    SDL_DestroySurface(surface);
    if (!rgb) return;

    std::filesystem::create_directories("capture/");
    std::time_t now = std::time(nullptr);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
    std::string path = "capture/screenshot_" + std::string(timeBuf) + ".png";

    std::vector<uint8_t> pixels((uint8_t*)rgb->pixels,
                                (uint8_t*)rgb->pixels + rgb->w * rgb->h * 3);
    int w = rgb->w, h = rgb->h;
    SDL_DestroySurface(rgb);

    std::thread([path, pixels = std::move(pixels), w, h]() {
        stbi_write_png(path.c_str(), w, h, 3, pixels.data(), w * 3);
        std::cout << "[Recorder] Screenshot -> " << path << std::endl;
    }).detach();
}

void Recorder::finalize() {
    if (frameIndex_ == 0) return;

    // Join previous thread if it still exist
    if (ffmpegThread_.joinable())
        ffmpegThread_.join();

    // Start conversion in a separeted thread
    int framesToConvert = frameIndex_;
    ffmpegThread_ = std::thread(&Recorder::runFfmpeg, this);
}