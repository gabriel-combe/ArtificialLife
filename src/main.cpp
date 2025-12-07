#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <implot.h>
#include <Eigen/Dense>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "ArtificialLife",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Variables for demo
    bool show_demo = true;
    bool show_plot = true;
    std::vector<float> data_x, data_y;
    
    // Generate data with Eigen
    Eigen::VectorXd vec = Eigen::VectorXd::LinSpaced(100, 0, 2 * M_PI);
    for (int i = 0; i < vec.size(); ++i) {
        data_x.push_back(vec[i]);
        data_y.push_back(std::sin(vec[i]));
    }

    // JSON example
    json config;
    config["window_width"] = 1280;
    config["window_height"] = 720;
    config["version"] = "1.0.0";
    
    // Save JSON
    std::ofstream config_file("config.json");
    config_file << config.dump(4);
    config_file.close();

    // Main loop
    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        // New ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // User interface
        if (show_demo) {
            ImGui::ShowDemoWindow(&show_demo);
        }

        // Custom window
        ImGui::Begin("Main Window");
        ImGui::Text("Welcome to ArtificialLife!");
        ImGui::Separator();
        
        // Eigen info
        ImGui::Text("Eigen Test:");
        Eigen::Vector3d v(1.0, 2.0, 3.0);
        ImGui::Text("Vector: (%.2f, %.2f, %.2f)", v.x(), v.y(), v.z());
        ImGui::Text("Norm: %.2f", v.norm());
        
        ImGui::Separator();
        ImGui::Checkbox("Show ImGui Demo", &show_demo);
        ImGui::Checkbox("Show Plot", &show_plot);
        
        ImGui::End();

        // Plot with ImPlot
        if (show_plot) {
            ImGui::Begin("Plot Example", &show_plot);
            if (ImPlot::BeginPlot("sin(x)")) {
                ImPlot::PlotLine("y = sin(x)", data_x.data(), data_y.data(), data_x.size());
                ImPlot::EndPlot();
            }
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}