#ifdef _WIN32
#include <windows.h>
#endif

#include "Helpers.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

namespace
{
    const unsigned int width = 800;
    const unsigned int height = 800;
    // ---------------------------------------------------------------------------
    // Camera orbit globals
    // ---------------------------------------------------------------------------
    float orbitYaw = 0.0f;
    float orbitPitch = 0.0f;
    bool isOrbiting = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;

}
// ------------------------------------------------
// Window Helpers
// ------------------------------------------------
// Initializes the GLFW window and the OpenGL context.
bool InitializeWindow(GLFWwindow *&window, int &framebufferWidth, int &framebufferHeight) // fbW To save the width and fbH to save the height of the framebuffer (real pixels)
{
#ifdef _WIN32
    DisableProcessWindowsGhosting();
#endif

    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    // Query the primary monitor to adapt to any device and occupy the full screen
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (primaryMonitor)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
        
        // Use the primary monitor's resolution and refresh rate
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
        
        window = glfwCreateWindow(mode->width, mode->height, "Mini Delivery Dash", primaryMonitor, nullptr);
    }
    else
    {
        // Fallback to windowed mode if monitor query fails
        const unsigned int fallbackWidth = 1280;
        const unsigned int fallbackHeight = 720;
        window = glfwCreateWindow(fallbackWidth, fallbackHeight, "Mini Delivery Dash", nullptr, nullptr);
    }

    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate(); // Release GLFW resources if it fails
        return false;
    }

    // ============================================================
    // LOCK THE ASPECT RATIO TO 16:9 (Only for windowed mode fallback)
    // ============================================================
    if (!primaryMonitor)
    {
        glfwSetWindowAspectRatio(window, 16, 9);
        glfwSetWindowSizeLimits(window, 1024, 576, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }

    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
    }
    // Try to recover OpenGL functions from opengl32.dll if GLAD fails
#ifdef _WIN32
    if (glad_glBlendFunc == NULL)
    {
        HMODULE openGL = GetModuleHandleA("opengl32.dll");
        if (openGL)
        {
            glad_glBlendFunc = (PFNGLBLENDFUNCPROC)GetProcAddress(openGL, "glBlendFunc");
        }
    }
#endif

    // Configure the viewport. We use GetFrame buffer Size for Retina/High DPI displays
    GetFramebufferSize(window, framebufferWidth, framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    // Clear window to a premium dark background immediately to prevent white screen flash
    glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);

    return true;
}

void GetFramebufferSize(GLFWwindow *window, int &framebufferWidth, int &framebufferHeight)
{
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0)
        framebufferWidth = static_cast<int>(width);
    if (framebufferHeight <= 0)
        framebufferHeight = static_cast<int>(height);
}

void SyncCameraToFramebuffer(GLFWwindow *window, Camera &camera)
{
    int w, h;
    GetFramebufferSize(window, w, h);
    if (camera.width != w || camera.height != h)
    {
        camera.width = w;
        camera.height = h;
    }
    glViewport(0, 0, w, h);
}

void SetupOpenGL(Shader &shaderProgram)
{
    glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);
    shaderProgram.Activate();
    glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE); // Enable MSAA rendering pipeline
}

// -------------------------------------------------
// Physics Helpers
// -------------------------------------------------

void ApplyGravity(CarState &car, float &carVerticalSpeed, bool isOnGround, float dt)
{
    const float gravity = 18.0f;

    if (!isOnGround)
    {
        carVerticalSpeed -= gravity * dt;
        car.position.y += carVerticalSpeed * dt;
    }
}

// --------------------------------------------------
// Camera
// --------------------------------------------------

// Camera follow
void UpdateFollowCamera(Camera &camera, const CarState &car, float dt, const City &city)
{
    float yaw = car.yaw + orbitYaw;
    float pitch = 0.32f + orbitPitch;
    glm::vec3 dir(-std::sin(yaw) * std::cos(pitch),
                   std::sin(pitch),
                  -std::cos(yaw) * std::cos(pitch));

    float followDistance = 6.0f * (kCarModelScale / 0.42f) + 0.2f;
    float cameraHeight   = 0.25f * (kCarModelScale / 0.42f) + 0.05f;
    float lookHeight     = 0.45f * (kCarModelScale / 0.42f) + 0.02f;

    glm::vec3 carEye     = car.position + glm::vec3(0.0f, kCarGroundYOffset + lookHeight, 0.0f);
    glm::vec3 lookTarget = carEye;

    // -----------------------------------------------------------------------
    // CAMERA: intelligent collision (progressive elevation if blocked)
    // -----------------------------------------------------------------------
    float safeDistance = followDistance;
    float elevation = 0.0f;
    const float elevationStep = 1.0f;
    const float maxElevation = 5.0f;
    bool foundClear = false;
    glm::vec3 bestPosition;

    while (elevation <= maxElevation)
    {
        // Candidate position: behind the car with the current elevation
        glm::vec3 candidate = car.position + dir * safeDistance
                            + glm::vec3(0.0f, cameraHeight + elevation, 0.0f);

        // Ray from the car's "eye" toward the candidate position
        glm::vec3 rayDir = glm::normalize(candidate - carEye);
        float distToTarget = glm::length(candidate - carEye);
        float step = 0.5f;
        int steps = static_cast<int>(distToTarget / step);
        bool blocked = false;

        for (int i = 1; i <= steps; ++i)
        {
            glm::vec3 sample = carEye + rayDir * (i * step);
            if (city.CheckCollision(sample, 0.5f))   // generous radius for the camera
            {
                blocked = true;
                break;
            }
        }

        if (!blocked)
        {
            bestPosition = candidate;
            foundClear = true;
            break;
        }

        elevation += elevationStep;
    }

    // If even the maximum elevation is blocked, place the camera very close and high
    if (!foundClear)
    {
        safeDistance = 0.8f;
        bestPosition = car.position + dir * safeDistance
                     + glm::vec3(0.0f, cameraHeight + maxElevation, 0.0f);
    }

    glm::vec3 desiredPosition = bestPosition;

    // -----------------------------------------------------------------------
    // Smooth movement
    // -----------------------------------------------------------------------
    float t = isOrbiting ? 1.0f : glm::clamp(1.0f - std::pow(0.005f, dt), 0.0f, 1.0f);
    camera.Position    = glm::mix(camera.Position, desiredPosition, t);
    camera.Orientation = glm::normalize(glm::mix(
        camera.Orientation,
        glm::normalize(lookTarget - camera.Position), t));
}

void UpdateOrbitCamera(GLFWwindow *window)
{
    // --- Camera orbit (right mouse) ---
    bool rmb = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    if (rmb)
    {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        if (!isOrbiting)
        {
            isOrbiting = true;
            lastMouseX = mx;
            lastMouseY = my;
        }
        else
        {
            const float sensitivity = 0.007f;
            orbitYaw -= static_cast<float>(mx - lastMouseX) * sensitivity;
            orbitPitch += static_cast<float>(my - lastMouseY) * sensitivity;
            orbitPitch = glm::clamp(orbitPitch, -glm::radians(10.0f), glm::radians(75.0f));
            lastMouseX = mx;
            lastMouseY = my;
        }
    }
    else
    {
        isOrbiting = false;
        orbitYaw = glm::mix(orbitYaw, 0.0f, 0.1f);
        orbitPitch = glm::mix(orbitPitch, 0.0f, 0.1f);
    }
}

// --------------------------------------------------
// Gameplay Helpers
// --------------------------------------------------

void UpdateGameplay(GLFWwindow *window, DayNightCycle &dayNight, bool &headlightsOn, bool &lightsPressed)
{
    // --- Headlights toggle (L) or auto-toggle ---
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
    {
        if (!lightsPressed)
        {
            headlightsOn = !headlightsOn;
            lightsPressed = true;
        }
    }
    else
    {
        lightsPressed = false;
        // Auto toggle: ON during night hours (under 6.5 or above 19.0)
        if (dayNight.GetTime() < 6.5f || dayNight.GetTime() > 19.0f)
            headlightsOn = true;
        else
            headlightsOn = false;
    }

    UpdateOrbitCamera(window);
}