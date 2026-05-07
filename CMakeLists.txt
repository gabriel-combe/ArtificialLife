cmake_minimum_required(VERSION 3.15)
project(ArtificialLife VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
    set(SDL_LIBRARIES SDL3::SDL3)
    set(SDL_INCLUDE_DIRS ${sdl3_SOURCE_DIR}/include)
else()
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
# OpenMP - Parallel outer loop in Cluster::rule()
# ============================================
find_package(OpenMP)

if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found — parallel particle updates enabled")
else()
    message(WARNING "OpenMP not found — simulation will run single-threaded. "
                    "Install libomp-dev (Linux) or Xcode command-line tools (macOS).")
endif()

# ============================================
# Dear ImGui + ImPlot (from submodules)
# ============================================
set(IMGUI_DIR  ${CMAKE_SOURCE_DIR}/external/imgui)
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
# miniaudio (single-header audio library)
# ============================================
set(MINIAUDIO_DIR ${CMAKE_SOURCE_DIR}/external/miniaudio)

# ============================================
# Main Executable
# ============================================
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/Core/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/Core/*.h")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/external
    ${MINIAUDIO_DIR}
)

# Use WASAPI on Windows (best backend); disable legacy DirectSound / WinMM
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        MA_NO_DSOUND
        MA_NO_WINMM
    )
endif()

# ── Optimisation flags ────────────────────────────────────────────────────────
# -O3        : full auto-vectorisation + loop unrolling
# -march=native: use all SIMD extensions available on the build machine (SSE/AVX)
#               Remove this flag if you distribute the binary to other machines.
#
# NOTE: /O2 is incompatible with /RTC1 (MSVC runtime checks added in Debug).
#       We use generator expressions so optimisation flags only apply in
#       Release / RelWithDebInfo builds — never in Debug.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:/O2 /fp:fast>
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:-O3 -march=native -ffast-math>
    )
endif()

# ── Link libraries ────────────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    Eigen3::Eigen
    imgui
    implot
    nlohmann_json
)

# OpenMP (optional — gracefully skipped if not installed)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()

# ── Windows: copy SDL3 DLL ────────────────────────────────────────────────────
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
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)