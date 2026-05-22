# Mini Delivery Dash Co.

## Overview

This repository contains a C++11/OpenGL 3.3 game prototype built as a modular 3D rendering project. The codebase is organized as a lightweight engine layer plus a gameplay layer, with separate folders for assets, models, shaders, and runtime dependencies.

The current architecture focuses on:
- real-time rendering with GLFW, GLEW, and GLM
- model importing through Assimp
- texture loading through SOIL2-based helpers
- a minimal application bootstrap in `src/main.cpp`
- gameplay scaffolding in `src/game`, `src/core`, `src/entities`, and `src/scene`

## Build

Recommended build script:

```bat
MASTER_BUILD.bat
```

Alternative build path:

```bash
make
```

Both paths use the local dependency layout under `Dependencies/` and generate `main.exe` in the project root.

## Runtime Dependencies

- `Dependencies/include` for headers
- `Dependencies/lib` for static/import libraries
- `libassimp-6.dll` next to the executable for runtime loading

Key third-party libraries:
- OpenGL 3.3 core profile
- GLFW3 for windowing and input
- GLEW for OpenGL function loading
- GLM for linear algebra
- Assimp for mesh import
- SOIL2 for image assets

## Current Structure

- `src/main.cpp` — application bootstrap
- `src/game/Game.cpp` — game loop, window initialization, and scene startup
- `src/engine/` — rendering and asset-loading layer
  - `Model.cpp` / `Model.h` — mesh import and material handling
  - `Mesh.cpp` / `Mesh.h` — mesh abstraction
  - `Texture.cpp` / `Texture.h` — texture upload and lifecycle
  - `Shader.h`, `Camera.h`, `VAO.h`, `VBO.h`, `EBO.h`, `Vertex.h`
- `src/core/` — core data types such as transforms
- `src/entities/` — entity-level gameplay primitives
- `src/scene/` — scene composition and entity grouping
- `res/models/` — imported 3D assets
- `res/shaders/` — GLSL shader programs
- `res/levels/`, `res/audio/`, `res/ui/`, `res/skyboxes/` — reserved content folders

## Asset Notes

The main vehicle asset is organized under `res/models/mclaren/` with separate `source/` and `textures/` subfolders. Shader assets are loaded from `res/shaders/`.

## Development Notes

- The project is compiled with `-O3` for release-oriented builds.
- Asset paths are expected to be relative to the executable working directory.
- The codebase is being expanded from a rendering demo into a game-oriented architecture.

## Troubleshooting

- If the executable fails to start, verify that `libassimp-6.dll` is present beside `main.exe`.
- If a model appears untextured, confirm the texture paths under `res/models/mclaren/textures/`.
- If the build fails, confirm that the MSYS2 UCRT64 toolchain is installed and available in `PATH`.

## Status

This is an active game prototype, not a finished product. The current goal is to evolve the rendering base into a functional delivery-style game with structured gameplay systems.
