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
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows

        ImGui::StyleColorsDark();
        
        // When viewports are enabled, tweak WindowRounding/WindowBg
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

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
        
        // Create dockspace
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        window_flags |= ImGuiWindowFlags_NoBackground;  // Make background transparent
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);
        
        // DockSpace
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        
        ImGui::End();
    }
    
    // End frame (render ImGui, present)
    void endFrame() {
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        
        // Update and Render additional Platform Windows
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        
        SDL_RenderPresent(renderer);
    }
    
    // Destructor
    ~Engine() {
        shutdown();
    }
};