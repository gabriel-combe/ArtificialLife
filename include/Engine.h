#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <iostream>

// Engine configuration
namespace EngineConfig {
    constexpr int DEFAULT_SCREEN_WIDTH = 1920;
    constexpr int DEFAULT_SCREEN_HEIGHT = 1080;
    constexpr float MAX_DELTA_TIME = 0.1f;  // Cap delta time to avoid physics issues
}

// Engine core - handles SDL and ImGui initialization
class Engine {
private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool initialized = false;
    
public:
    int screenWidth = EngineConfig::DEFAULT_SCREEN_WIDTH;
    int screenHeight = EngineConfig::DEFAULT_SCREEN_HEIGHT;
    
    // Initialize SDL and ImGui
    bool initialize(const char* title = "ArtificialLife") {
        // Initialize SDL3
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
            return false;
        }

        // Create window
        window = SDL_CreateWindow(
            title,
            screenWidth, screenHeight,
            SDL_WINDOW_RESIZABLE
        );

        if (!window) {
            std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return false;
        }

        // Create renderer
        renderer = SDL_CreateRenderer(window, nullptr);

        if (!renderer) {
            std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }

        // Setup ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer3_Init(renderer);

        initialized = true;
        return true;
    }
    
    // Shutdown and cleanup
    void shutdown() {
        if (!initialized) return;
        
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        
        initialized = false;
    }
    
    // Get renderer for drawing
    SDL_Renderer* getRenderer() { return renderer; }
    
    // Get window
    SDL_Window* getWindow() { return window; }
    
    // Update screen dimensions
    void updateScreenSize() {
        SDL_GetWindowSize(window, &screenWidth, &screenHeight);
    }
    
    // Begin frame (clear screen, start ImGui frame)
    void beginFrame() {
        updateScreenSize();
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);
        
        // Start ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }
    
    // End frame (render ImGui, present)
    void endFrame() {
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
    
    // Destructor
    ~Engine() {
        shutdown();
    }
};