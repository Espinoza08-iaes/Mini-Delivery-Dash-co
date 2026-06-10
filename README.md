# Mini Delivery Dash Co.

A high-fidelity 3D driving simulation prototype built using **C++11** and the **OpenGL 3.3 Core Profile**. The application features an interactive, physics-driven McLaren F1 vehicle model navigating a detailed urban environment surrounded by a dynamic, reflective ocean with a real-time day/night lighting cycle.

---

## Technical Specifications

| Component | Library / API | Version | Description |
|:---|:---|:---|:---|
| **Graphics API** | OpenGL | 3.3 Core Profile | Core rendering pipeline, shaders, and framebuffers. |
| **Windowing & Input** | GLFW | 3.x | Window lifecycle management and keyboard/mouse polling. |
| **Loader** | GLAD | Modern | OpenGL extension loader. |
| **Math Engine** | GLM (OpenGL Mathematics) | 0.9.x | Matrix calculations, quaternions, and vector operations. |
| **Mesh Parsing** | Assimp | 5.x | Import and processing of 3D formats (OBJ, FBX, Collada). |
| **Texture Loader** | SOIL2 | Modern | Loading raw textures into OpenGL texture units. |

---

## Gameplay Controls

| Key / Input | Action |
|:---|:---|
| **W / S** | Throttle (Accelerate / Reverse) |
| **A / D** | Steering (Turn Left / Right) |
| **LEFT SHIFT** | **Nitrous Boost (NOS)** — Instantly double acceleration and top speed (approx. 86 km/h) with dynamic camera shake and FOV warping. |
| **L** | Manual Headlight Toggle (Note: Headlights also auto-activate at night). |
| **Right Mouse Button (RMB) + Drag** | Free Orbit Camera around the vehicle. |

---

## Build and Execution Workflow

Compiling and running the application requires **CMake** and a modern C++ compiler (such as GCC/MinGW via MSYS2, or MSVC on Windows).

### 1. Project Generation (CMake Configure)
This command reads the project configuration in `CMakeLists.txt`, configures compiler flags, maps static libraries under the `Dependencies/` folder, and generates the native build files inside the `cmake-build` directory:
```bash
cmake -B cmake-build
```

### 2. Compilation Phase (Build Binary)
This command triggers the active compiler to compile all C++ source files (`.cpp`) and link them with the GLFW, GLAD, and Assimp static libraries to generate the final executable file:
```bash
cmake --build cmake-build
```

### 3. Run Executable
To launch the game, run the compiled binary directly from the build output directory:
```powershell
.\cmake-build\MiniDeliveryDash.exe
```

---

## Detailed Directory Layout

```text
├── Dependencies/                      # Project libraries and headers
│   ├── include/                       # Third-party header files (GLFW, glad, glm, Assimp)
│   └── lib/                           # Pre-compiled static libraries (.lib/.a)
├── res/                               # Asset directory
│   ├── models/                        # 3D assets
│   │   ├── mclaren/                   # McLaren F1 model (OBJ mesh and textures)
│   │   └── city/                      # Urban grid map layout and building geometries
│   └── shaders/                       # GLSL shader source code
│       ├── default.vert / .frag       # Core mesh rendering with directional lighting and headlights
│       ├── sky.vert / .frag           # Equirectangular dynamic sky sphere rendering
│       └── water.vert / .frag         # Procedural multi-octave ocean wave simulation with Fresnel
├── src/                               # Application source code
│   ├── engine/                        # Lightweight OpenGL rendering wrappers
│   │   ├── Camera.h                   # View/Projection matrices and follow physics calculations
│   │   ├── Mesh.h / Mesh.cpp          # VAO, VBO, EBO abstractions and draws
│   │   ├── Model.h / Model.cpp        # Assimp layout parser and material loader
│   │   ├── Shader.h                   # GLSL compiler, linker, and uniform manager
│   │   ├── Texture.h / Texture.cpp    # Texture parameters, mipmaps, and loader wrappers
│   │   └── VAO.h / VBO.h / EBO.h      # Individual buffer state abstractions
│   ├── game/                          # Gameplay mechanics and physics systems
│   │   ├── City.h / City.cpp          # City map scaling and model instance placement
│   │   ├── Game.h / Game.cpp          # Primary game loop, keyframe interpolation, and inputs
│   │   └── city_physics/              # Raycasting and spatial partitioning
│   │       ├── CityPhysics.h / .cpp   # AABB collision checks and ground height profiling
│   └── main.cpp                       # Program entry point and window context setup
├── CMakeLists.txt                     # CMake build configuration script
└── README.md                          # Project documentation
```

---

## Game Development Process & Architecture Journey

The development of **Mini Delivery Dash Co.** evolved through five key architectural phases:

### Phase 1: Core Graphics Engine & Asset Pipeline
*   **Windowing & OpenGL Context**: Initialized GLFW windowing, enabled the GLAD loader, and configured depth testing, backface culling, and vertex array state machines.
*   **Assimp Mesh Importing**: Implemented a modular `Model` and `Mesh` class structure to parse and render complex OBJ/FBX files, including the multi-material McLaren F1 and city layouts.
*   **Texturing & Mipmapping**: Configured texture filtering parameters to utilize trilinear mipmapping (`GL_LINEAR_MIPMAP_LINEAR`) to solve high-frequency pixel shimmering (aliasing) at long distances.

### Phase 2: Kinematic Physics & Ground Clamping
*   **Custom Physics Model**: Designed a custom kinematic car model with linear acceleration, braking force, friction models, and steering angular velocity.
*   **Collision Detection**: Integrated Axis-Aligned Bounding Box (AABB) checks against city geometry. Implemented a sliding collision resolver to prevent the vehicle from clipping through buildings.
*   **Terrain Profiler**: Built a ground sampling utility that queries street heights and normal vectors to smoothly align the vehicle's height (`y`), pitch, and roll with uneven roadways and bridges.

### Phase 3: Dynamic Ocean Simulation
*   **Procedural Water Plane**: Rendered a massive `4000x4000` quad positioned at `y = -4.5f` as the sea surface.
*   **Three-Octave Wave Simulation**: Designed a custom GLSL fragment shader (`water.frag`) combining three distinct mathematical sine/cosine waves at different frequencies to simulate large swells, medium ripples, and fine wind-driven wave crests:
    *   *Octave 1 (Swell)*: $f_1(x, z, t) = \sin(x \cdot 0.05 + t \cdot 1.2) \cdot 0.04$
    *   *Octave 2 (Chop)*: $f_2(x, z, t) = \cos(z \cdot 0.15 - t \cdot 2.0) \cdot 0.035$
    *   *Octave 3 (Ripple)*: $f_3(x, z, t) = \sin((x+z) \cdot 0.45 + t \cdot 3.5) \cdot 0.015$
*   **Fresnel Reflectivity**: Implemented physical Fresnel reflection blending. As the camera's angle of view gets shallower, the water smoothly transitions from its deep blue color to a mirror reflection of the skybox.

### Phase 4: Day/Night Keyframe Cycling
*   **Atmospheric keyframes**: Defined 8 procedural keyframes throughout a 24-hour cycle.
*   **Linear Interpolation (Lerp)**: Programmed smooth color blending for ambient strength, zenith/horizon colors, fog density, and light direction to represent dawn, noon, dusk, and midnight.
*   **Auto-Headlights**: Tied the vehicle's emissive headlight texture state to the daytime clock, ensuring lights turn on automatically after dusk (19:00) and off after dawn (06:30).

### Phase 5: Quality of Life & Arcade Mechanics
*   **Water Fall Rescue (Auto-Respawn)**: Added a safety watchdog. If the vehicle falls off a bridge or slips into the ocean (`y < -2.0f`), it is instantly teleported back to the nearest road spawn coordinate with speed reset to zero, ensuring the player never gets stuck.
*   **NOS Warp Effects**: Combined a dynamic FOV zoom-out (smoothly interpolating the camera projection matrix from 45° to 58° FOV) with high-frequency randomized camera translation shake to amplify the sense of raw speed when boosting.
