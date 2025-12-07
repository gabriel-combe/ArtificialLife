cmake_minimum_required(VERSION 3.15)
project(ArtificialLife VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

include(FetchContent)

# ============================================
# SDL3 - Auto-download if not found
# ============================================
find_package(SDL3 QUIET)

if(NOT SDL3_FOUND)
    message(STATUS "SDL3 not found, downloading...")
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.28
    )
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
    
    # SDL3 creates SDL3::SDL3 target automatically
    set(SDL_LIBRARIES SDL3::SDL3)
    set(SDL_INCLUDE_DIRS ${sdl3_SOURCE_DIR}/include)
else()
    # System SDL3 found
    set(SDL_LIBRARIES SDL3::SDL3)
endif()

# ============================================
# Eigen (header-only) - Auto-download if not found
# ============================================
find_package(Eigen3 3.3 QUIET NO_MODULE)

if(NOT Eigen3_FOUND)
    message(STATUS "Eigen3 not found, downloading...")
    FetchContent_Declare(
        Eigen3
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 5.0.1
    )
    FetchContent_MakeAvailable(Eigen3)
endif()

# ============================================
# Dear ImGui + ImPlot (from submodules)
# ============================================
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/external/implot)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${SDL_INCLUDE_DIRS}
)

target_link_libraries(imgui PUBLIC SDL3::SDL3)

# ImPlot (depends on ImGui)
add_library(implot STATIC
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

target_include_directories(implot PUBLIC
    ${IMPLOT_DIR}
    ${IMGUI_DIR}
)

target_link_libraries(implot PUBLIC imgui)

# ============================================
# JSON for Modern C++ (header-only, from submodule)
# ============================================
set(JSON_DIR ${CMAKE_SOURCE_DIR}/external/json)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${JSON_DIR}/include)

# ============================================
# Main Executable
# ============================================
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    Eigen3::Eigen
    imgui
    implot
    nlohmann_json
)

# Windows: copy required DLLs to build directory
if(WIN32 AND TARGET SDL3::SDL3)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying SDL3 DLL to output directory"
    )
endif()

# ============================================
# Installation
# ============================================
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)