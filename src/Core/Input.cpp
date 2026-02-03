#include "Core/Input.h"

// Static member initialization
const Uint8* Input::keyboardState = nullptr;
Uint32 Input::mouseState = 0;
Eigen::Vector2i Input::mousePosition(0, 0);
Eigen::Vector2i Input::mouseDelta(0, 0);
Eigen::Vector2i Input::lastMousePosition(0, 0);

void Input::Initialize() {
    int numKeys;
    keyboardState = reinterpret_cast<const Uint8*>(SDL_GetKeyboardState(&numKeys));
    float mx, my;
    mouseState = SDL_GetMouseState(&mx, &my);
    mousePosition = Eigen::Vector2i(static_cast<int>(mx), static_cast<int>(my));
    lastMousePosition = mousePosition;
    mouseDelta = Eigen::Vector2i(0, 0);
}

void Input::ProcessEvent(SDL_Event* event) {
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        lastMousePosition = mousePosition;
        mousePosition = Eigen::Vector2i(
            static_cast<int>(event->motion.x),
            static_cast<int>(event->motion.y)
        );
        mouseDelta = mousePosition - lastMousePosition;
    }
}

void Input::Update() {
    int numKeys;
    keyboardState = reinterpret_cast<const Uint8*>(SDL_GetKeyboardState(&numKeys));
    float mx, my;
    mouseState = SDL_GetMouseState(&mx, &my);
}

bool Input::GetKey(SDL_Scancode key) {
    return keyboardState && keyboardState[key];
}

bool Input::GetKeyDown(SDL_Scancode key) {
    // TODO: Implement frame-based detection
    return GetKey(key);
}

bool Input::GetKeyUp(SDL_Scancode key) {
    // TODO: Implement frame-based detection
    return false;
}

bool Input::GetMouseButton(int button) {
    // SDL3 uses SDL_BUTTON_MASK macro differently
    Uint32 mask = (1u << (button - 1));
    return (mouseState & mask) != 0;
}

bool Input::GetMouseButtonDown(int button) {
    // TODO: Implement frame-based detection
    return GetMouseButton(button);
}

bool Input::GetMouseButtonUp(int button) {
    // TODO: Implement frame-based detection
    return false;
}