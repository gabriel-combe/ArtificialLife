#include "Core/Application.h"
#include "Core/Time.h"
#include "Core/Input.h"
#include "Core/GUI.h"
#include <iostream>

bool Application::Initialize(const ApplicationConfig& config) {
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "[Application] SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    window = SDL_CreateWindow(
        config.title.c_str(),
        config.width, config.height,
        config.resizable ? SDL_WINDOW_RESIZABLE : 0
    );

    if (!window) {
        std::cerr << "[Application] Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    // Create renderer
    renderer = SDL_CreateRenderer(window, nullptr);

    if (!renderer) {
        std::cerr << "[Application] Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    screenWidth = config.width;
    screenHeight = config.height;

    // Initialize subsystems
    Time::Initialize();
    Input::Initialize();
    GUI::Initialize(window, renderer);

    initialized = true;
    isRunning = true;

    std::cout << "[Application] Initialized successfully" << std::endl;
    
    // Call user start
    OnStart();
    
    return true;
}

void Application::Run() {
    while (isRunning) {
        // Update time
        Time::Update();
        
        // Process events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            GUI::ProcessEvent(&event);
            Input::ProcessEvent(&event);
            
            if (event.type == SDL_EVENT_QUIT) {
                Quit();
            }
        }
        
        // Update screen size
        UpdateScreenSize();
        
        // User update
        OnUpdate(Time::DeltaTime());
        
        // Begin frame
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);
        
        // User render
        OnRender();
        
        // GUI
        GUI::BeginFrame();
        OnGUI();
        GUI::EndFrame(renderer);
        
        // Present
        SDL_RenderPresent(renderer);
    }
    
    // Cleanup
    OnShutdown();
    Shutdown();
}

void Application::UpdateScreenSize() {
    SDL_GetWindowSize(window, &screenWidth, &screenHeight);
}

void Application::Shutdown() {
    if (!initialized) return;
    
    GUI::Shutdown();
    
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    
    initialized = false;
    std::cout << "[Application] Shutdown complete" << std::endl;
}