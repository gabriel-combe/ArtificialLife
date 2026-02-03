#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

// GUI system wrapper - Simplifies ImGui usage
class GUI {
private:
    static bool initialized;
    
public:
    // Initialize GUI system
    static void Initialize(SDL_Window* window, SDL_Renderer* renderer);
    
    // Process SDL events
    static void ProcessEvent(SDL_Event* event);
    
    // Frame management
    static void BeginFrame();
    static void EndFrame(SDL_Renderer* renderer);
    
    // Cleanup
    static void Shutdown();
    
    // Helper functions - Unity-style wrappers
    static void BeginWindow(const char* name, bool* open = nullptr, ImGuiWindowFlags flags = 0);
    static void EndWindow();
    
    static void Text(const char* text);
    static void TextColored(const ImVec4& color, const char* text);
    static void Separator();
    static void Spacing();
    static void SameLine();
    
    static bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
    static bool Checkbox(const char* label, bool* v);
    static bool SliderFloat(const char* label, float* v, float min, float max);
    static bool SliderInt(const char* label, int* v, int min, int max);
    
    static bool CollapsingHeader(const char* label);
    
    // Get IO for advanced features
    static ImGuiIO& GetIO() { return ImGui::GetIO(); }
};