#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <implot.h>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <numbers>

using json = nlohmann::json;

// 3D Cube vertices
const std::vector<Eigen::Vector3d> cubeVertices = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},  // Back face
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}    // Front face
};

// Cube edges (pairs of vertex indices)
const std::vector<std::pair<int, int>> cubeEdges = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},  // Back face
    {4, 5}, {5, 6}, {6, 7}, {7, 4},  // Front face
    {0, 4}, {1, 5}, {2, 6}, {3, 7}   // Connecting edges
};

// Project 3D point to 2D screen
Eigen::Vector2d project3D(const Eigen::Vector3d& point, int width, int height, double zoom = 1.0) {
    double scale = 200.0 * zoom;
    double perspective = 1.0 / (4.0 + point.z());
    double x = width / 2.0 + point.x() * scale * perspective;
    double y = height / 2.0 - point.y() * scale * perspective;
    return Eigen::Vector2d(x, y);
}

// Rotation matrices (angle in degrees)
Eigen::Matrix3d rotationX(double angleDeg) {
    return Eigen::AngleAxisd(angleDeg * std::numbers::pi / 180.0, Eigen::Vector3d::UnitX()).toRotationMatrix();
}

Eigen::Matrix3d rotationY(double angleDeg) {
    return Eigen::AngleAxisd(angleDeg * std::numbers::pi / 180.0, Eigen::Vector3d::UnitY()).toRotationMatrix();
}

Eigen::Matrix3d rotationZ(double angleDeg) {
    return Eigen::AngleAxisd(angleDeg * std::numbers::pi / 180.0, Eigen::Vector3d::UnitZ()).toRotationMatrix();
}

// Trackball-style rotation
Eigen::Matrix3d trackballRotation(const Eigen::Matrix3d& currentRotation, int deltaX, int deltaY, float sensitivity = 0.5f) {
    if (deltaX == 0 && deltaY == 0) return currentRotation;
    
    // Create rotation axis perpendicular to mouse movement
    // Negate deltaX for intuitive left/right motion
    Eigen::Vector3d axis(-deltaY, -deltaX, 0);
    axis.normalize();
    
    // Calculate rotation angle based on mouse movement magnitude
    float angle = std::sqrt(deltaX * deltaX + deltaY * deltaY) * sensitivity;
    
    // Create incremental rotation
    Eigen::Matrix3d incrementalRotation = Eigen::AngleAxisd(angle * std::numbers::pi / 180.0, axis).toRotationMatrix();
    
    // Apply incremental rotation to current rotation
    return incrementalRotation * currentRotation;
}

// Extract Euler angles from rotation matrix (in degrees)
Eigen::Vector3f matrixToEulerAngles(const Eigen::Matrix3d& R) {
    // Extract XYZ Euler angles from rotation matrix
    Eigen::Vector3d euler = R.canonicalEulerAngles(0, 1, 2);  // XYZ order
    return Eigen::Vector3f(
        euler.x() * 180.0 / std::numbers::pi,
        euler.y() * 180.0 / std::numbers::pi,
        euler.z() * 180.0 / std::numbers::pi
    );
}

int main(int argc, char* argv[]) {
    // Initialize SDL3
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "ArtificialLife - Rotating Cube",
        1280, 720,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);

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

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Variables for demo
    bool show_demo = false;
    bool show_controls = true;
    bool show_plot = true;
    float angleX = 0.0f;
    float angleY = 0.0f;
    float angleZ = 0.0f;
    float rotationSpeed = 45.0f;  // degrees per second
    bool autoRotate = true;
    float zoom = 1.0f;
    bool usingSliders = false;  // Track if sliders are being used
    bool wasUsingTrackball = false;  // Track if we just finished trackball
    
    // Mouse control variables
    bool isDragging = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    
    // Trackball rotation matrix
    Eigen::Matrix3d manualRotation = Eigen::Matrix3d::Identity();
    
    // Data for plotting
    std::vector<float> data_x, data_sin, data_cos, data_damped;
    
    // Generate complex function data with Eigen
    Eigen::VectorXd vec = Eigen::VectorXd::LinSpaced(200, 0, 4 * std::numbers::pi);
    for (int i = 0; i < vec.size(); ++i) {
        float x = vec[i];
        data_x.push_back(x);
        data_sin.push_back(std::sin(x));
        data_cos.push_back(std::cos(x));
        // Damped oscillation: e^(-x/5) * sin(2x)
        data_damped.push_back(std::exp(-x / 5.0) * std::sin(2 * x));
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
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            
            // Mouse wheel zoom (only if not over ImGui window)
            if (event.type == SDL_EVENT_MOUSE_WHEEL && !io.WantCaptureMouse) {
                zoom += event.wheel.y * 0.1f;
                zoom = std::max(0.1f, std::min(zoom, 5.0f));  // Clamp between 0.1 and 5.0
            }
            
            // Mouse drag rotation (only if not over ImGui window)
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT && !io.WantCaptureMouse) {
                isDragging = true;
                lastMouseX = (int)event.button.x;
                lastMouseY = (int)event.button.y;
                autoRotate = false;
                wasUsingTrackball = true;
            }
            
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
                isDragging = false;
            }
            
            if (event.type == SDL_EVENT_MOUSE_MOTION && isDragging) {
                int deltaX = (int)event.motion.x - lastMouseX;
                int deltaY = (int)event.motion.y - lastMouseY;
                
                // Apply trackball rotation
                manualRotation = trackballRotation(manualRotation, deltaX, deltaY);
                
                lastMouseX = (int)event.motion.x;
                lastMouseY = (int)event.motion.y;
            }
        }

        // Calculate delta time
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        // Auto-rotate cube
        if (autoRotate) {
            angleX += rotationSpeed * deltaTime;
            angleY += rotationSpeed * deltaTime * 0.7f;
            angleZ += rotationSpeed * deltaTime * 0.5f;
            
            // Loop angles after full rotation (360 degrees)
            angleX = std::fmod(angleX, 360.0f);
            angleY = std::fmod(angleY, 360.0f);
            angleZ = std::fmod(angleZ, 360.0f);
            
            // Update manual rotation matrix from angles
            manualRotation = rotationZ(angleZ) * rotationY(angleY) * rotationX(angleX);
            wasUsingTrackball = false;
        } else if (!usingSliders) {
            // Update angle display from trackball rotation
            Eigen::Vector3f currentAngles = matrixToEulerAngles(manualRotation);
            angleX = std::fmod(currentAngles.x(), 360.0f);
            angleY = std::fmod(currentAngles.y(), 360.0f);
            angleZ = std::fmod(currentAngles.z(), 360.0f);
            
            // Ensure positive angles
            if (angleX < 0) angleX += 360.0f;
            if (angleY < 0) angleY += 360.0f;
            if (angleZ < 0) angleZ += 360.0f;
        }

        // New ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // User interface
        if (show_demo) {
            ImGui::ShowDemoWindow(&show_demo);
        }

        // Control window
        if (show_controls) {
            ImGui::Begin("Cube Controls", &show_controls);
            ImGui::Text("Welcome to ArtificialLife!");
            ImGui::Separator();
            
            ImGui::Checkbox("Auto Rotate", &autoRotate);
            ImGui::SliderFloat("Rotation Speed (°/s)", &rotationSpeed, 0.0f, 180.0f);
            ImGui::SliderFloat("Zoom", &zoom, 0.1f, 5.0f);
            
            ImGui::Separator();
            ImGui::Text("Mouse Controls:");
            ImGui::BulletText("Left Click + Drag: Rotate cube");
            ImGui::BulletText("Mouse Wheel: Zoom in/out");
            
            ImGui::Separator();
            ImGui::Text("Manual Rotation:");
            
            bool sliderXChanged = ImGui::SliderFloat("Angle X (°)", &angleX, 0.0f, 360.0f);
            bool sliderYChanged = ImGui::SliderFloat("Angle Y (°)", &angleY, 0.0f, 360.0f);
            bool sliderZChanged = ImGui::SliderFloat("Angle Z (°)", &angleZ, 0.0f, 360.0f);
            
            // Check if any slider is being actively used
            bool anySliderActive = ImGui::IsItemActive();
            
            // When first touching a slider after trackball, sync the matrix to slider angles
            if ((sliderXChanged || sliderYChanged || sliderZChanged) && wasUsingTrackball) {
                // First slider touch after trackball: use the extracted angles to avoid snap
                manualRotation = rotationZ(angleZ) * rotationY(angleY) * rotationX(angleX);
                wasUsingTrackball = false;
            }
            
            // If user is actively modifying angles via sliders
            if (sliderXChanged || sliderYChanged || sliderZChanged) {
                autoRotate = false;
                usingSliders = true;
                manualRotation = rotationZ(angleZ) * rotationY(angleY) * rotationX(angleX);
            } else if (!anySliderActive) {
                usingSliders = false;
            }
            
            if (ImGui::Button("Reset Rotation")) {
                angleX = angleY = angleZ = 0.0f;
                manualRotation = Eigen::Matrix3d::Identity();
            }
            
            ImGui::Separator();
            ImGui::Checkbox("Show ImGui Demo", &show_demo);
            ImGui::Checkbox("Show Plot Window", &show_plot);
            
            ImGui::Separator();
            ImGui::Text("Eigen Test:");
            Eigen::Vector3d v(1.0, 2.0, 3.0);
            ImGui::Text("Vector: (%.2f, %.2f, %.2f)", v.x(), v.y(), v.z());
            ImGui::Text("Norm: %.2f", v.norm());
            
            ImGui::End();
        }

        // Plot window with ImPlot
        if (show_plot) {
            ImGui::Begin("Function Plots", &show_plot, ImGuiWindowFlags_AlwaysAutoResize);
            
            if (ImPlot::BeginPlot("Trigonometric Functions", ImVec2(600, 300))) {
                ImPlot::SetupAxes("x", "y");
                ImPlot::SetupAxisLimits(ImAxis_X1, 0, 4 * std::numbers::pi);
                ImPlot::SetupAxisLimits(ImAxis_Y1, -1.5, 1.5);
                
                ImPlot::PlotLine("sin(x)", data_x.data(), data_sin.data(), data_x.size());
                ImPlot::PlotLine("cos(x)", data_x.data(), data_cos.data(), data_x.size());
                ImPlot::PlotLine("e^(-x/5) * sin(2x)", data_x.data(), data_damped.data(), data_x.size());
                
                ImPlot::EndPlot();
            }
            
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        // Get window size
        int width, height;
        SDL_GetWindowSize(window, &width, &height);

        // Calculate rotation matrix
        Eigen::Matrix3d rotation = manualRotation;

        // Transform and project vertices
        std::vector<Eigen::Vector2d> projectedVertices;
        for (const auto& vertex : cubeVertices) {
            Eigen::Vector3d rotated = rotation * vertex;
            projectedVertices.push_back(project3D(rotated, width, height, zoom));
        }

        // Draw cube edges
        SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
        for (const auto& edge : cubeEdges) {
            const auto& p1 = projectedVertices[edge.first];
            const auto& p2 = projectedVertices[edge.second];
            SDL_RenderLine(renderer, 
                          (float)p1.x(), (float)p1.y(),
                          (float)p2.x(), (float)p2.y());
        }

        // Draw vertices as points
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        for (const auto& vertex : projectedVertices) {
            SDL_FRect rect = {(float)vertex.x() - 3, (float)vertex.y() - 3, 6, 6};
            SDL_RenderFillRect(renderer, &rect);
        }

        // Render ImGui on top
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}