# Lab7 - SOIL2 Graphics Project

## Quick Start

Simply run this file to compile and run the project:
```
BUILD.bat
```

This script will:
1. Set up all necessary directories
2. Copy SOIL2 headers and library
3. Compile main.cpp with all dependencies
4. Run the compiled executable

## Project Structure

- **main.cpp** - Main graphics program
- **Shader.h** - Shader management class
- **Camera.h** - Camera control class
- **Dependencies/** - All external libraries and headers
  - **lib/** - Compiled libraries (libsoil2.a, libglfw3.a, etc.)
  - **include/** - Header files
    - **SOIL2/** - SOIL2 image loading library
    - **glm/** - Mathematics library
    - **glfw/** - Window and input library
    - **glad/** - OpenGL loader

- **SOIL2-master/** - SOIL2 source code
  - **src/SOIL2/** - SOIL2 implementation files
  - **build/libsoil2.a** - Compiled SOIL2 library

- **Textures/** - Texture files used by the program
  - image.png
  - image copy.png
  - image copy 2.png
  - etc.

## Textures

The program loads textures from the **Textures/** folder:
- diffuseMap1: Textures/image.png
- specularMap1: Textures/image copy.png
- diffuseMap2: Textures/image copy 2.png
- specularMap2: Textures/image copy 3.png
- diffuseMap3: Textures/image copy 4.png
- specularMap3: Textures/image copy 5.png

## Build Scripts

### BUILD.bat (Recommended)
Complete build script that handles everything:
- Creates directories
- Copies headers and libraries
- Compiles the project
- Runs the executable

### build_soil2.bat
Builds SOIL2 library from source if needed (requires CMake and MinGW Make)

## Compilation Details

The project uses:
- **Compiler:** g++ (MinGW)
- **C++ Standard:** C++11
- **Libraries:**
  - SOIL2 (image loading)
  - GLFW3 (windowing)
  - GLEW (OpenGL extensions)
  - OpenGL 3.3 (graphics)
  - GLM (mathematics)

## Troubleshooting

### "SOIL2 library not found"
- Make sure SOIL2-master/build/libsoil2.a exists
- If not, run build_soil2.bat first
- Or manually compile SOIL2 using CMake

### "Cannot load texture"
- Check that Textures/ folder exists with PNG files
- Ensure texture filenames match those in main.cpp
- Check Textures folder is in the same directory as main.exe

### Compilation errors
- Ensure all dependencies are in Dependencies/include and Dependencies/lib
- Check that g++ and gcc are installed and in PATH
- Verify file paths use backslashes (\) on Windows

## Customization

To add more textures:
1. Place image files in the Textures/ folder
2. Modify main.cpp's loadTexture calls with new paths
3. Rebuild with BUILD.bat

## Features

- Modern OpenGL 3.3 with core profile
- SOIL2 image loading from PNG files
- Multiple textures with diffuse and specular maps
- Camera control system
- Point and directional lighting
- Texture-based material system
