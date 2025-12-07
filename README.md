# ArtificialLife

C++ multiplatform project using SDL2, Eigen, Dear ImGui, ImPlot and nlohmann/json.

## Prerequisites

### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install build-essential cmake git
```

### Linux (Fedora)
```bash
sudo dnf install cmake gcc-c++ git
```

### Windows
- CMake (3.15+)
- C++ Compiler (MSVC, MinGW-w64, or Clang)
- Git

**Note:** SDL2 and Eigen will be downloaded automatically by CMake if not found on your system.

## Installation

1. **Clone with submodules:**
```bash
git clone --recursive https://github.com/your-username/ArtificialLife.git
cd ArtificialLife
```

If already cloned without `--recursive`:
```bash
git submodule update --init --recursive
```

2. **Build:**
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

3. **Run:**
```bash
# Linux/Mac
./ArtificialLife

# Windows
.\Release\ArtificialLife.exe
```

## Project Structure

```
ArtificialLife/
├── CMakeLists.txt          # Main CMake configuration
├── src/                    # Source code
├── include/                # Headers
├── external/               # External dependencies (submodules)
    ├── imgui/              # Dear ImGui
    ├── implot/             # ImPlot
    └── json/               # nlohmann/json
```

## VSCodium Setup

### Recommended Extensions
- C/C++ (Microsoft)
- CMake Tools
- CMake Language Support

### Build & Run
1. Open folder in VSCodium
2. `Ctrl+Shift+P` → "CMake: Configure"
3. Select your compiler
4. `F7` to build
5. `Ctrl+F5` to run

## Features

- SDL2 for rendering and window management
- Eigen for vector/matrix math
- Dear ImGui for immediate mode GUI
- ImPlot for plotting and visualizations
- nlohmann/json for JSON serialization
- Cross-platform build system (Windows/Linux/Mac)

## License

MIT