#include "Core/GUI.h"
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <iostream>

bool GUI::initialized = false;

void GUI::Initialize(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    
    initialized = true;
    std::cout << "[GUI] Initialized" << std::endl;
}

void GUI::ProcessEvent(SDL_Event* event) {
    if (initialized) {
        ImGui_ImplSDL3_ProcessEvent(event);
    }
}

void GUI::BeginFrame() {
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
    window_flags |= ImGuiWindowFlags_NoBackground;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    
    ImGui::End();
}

void GUI::EndFrame(SDL_Renderer* renderer) {
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void GUI::Shutdown() {
    if (!initialized) return;
    
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    
    initialized = false;
    std::cout << "[GUI] Shutdown" << std::endl;
}

// Helper wrappers
void GUI::BeginWindow(const char* name, bool* open, ImGuiWindowFlags flags) {
    ImGui::Begin(name, open, flags);
}

void GUI::EndWindow() {
    ImGui::End();
}

void GUI::Text(const char* text) {
    ImGui::Text("%s", text);
}

void GUI::TextColored(const ImVec4& color, const char* text) {
    ImGui::TextColored(color, "%s", text);
}

void GUI::Separator() {
    ImGui::Separator();
}

void GUI::Spacing() {
    ImGui::Spacing();
}

void GUI::SameLine() {
    ImGui::SameLine();
}

bool GUI::Button(const char* label, const ImVec2& size) {
    return ImGui::Button(label, size);
}

bool GUI::Checkbox(const char* label, bool* v) {
    return ImGui::Checkbox(label, v);
}

bool GUI::SliderFloat(const char* label, float* v, float min, float max) {
    return ImGui::SliderFloat(label, v, min, max);
}

bool GUI::SliderInt(const char* label, int* v, int min, int max) {
    return ImGui::SliderInt(label, v, min, max);
}

bool GUI::CollapsingHeader(const char* label) {
    return ImGui::CollapsingHeader(label);
}