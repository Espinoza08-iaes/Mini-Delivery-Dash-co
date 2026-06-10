//#include <Windows.h>
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
bool InitializeWindow(GLFWwindow*& window, int& framebufferWidth, int& framebufferHeight) //fbW To save the width and fbH to save the height of the framebuffer (real pixels)
{
#ifdef _WIN32
    DisableProcessWindowsGhosting();
#endif

    // Initialize the GLFW library
    glfwInit();

    // Configure OpenGL versions (Core Profile 3.3)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    // Enable MSAA (Multi-Sample Anti-Aliasing) to smooth edges
    window = glfwCreateWindow(width, height, "Mini Delivery Dash", nullptr, nullptr);

    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;

        glfwDestroyWindow(window);
        glfwTerminate(); // Release GLFW resources if it fails

         return false;
    }

    // Make this window context the current one for OpenGL
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;

        glfwDestroyWindow(window);
        glfwTerminate(); // Release GLFW resources if it fails

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

void SetupOpenGL (Shader& shaderProgram)
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

void ApplyGravity(CarState& car, float& carVerticalSpeed, bool isOnGround, float dt)
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
    float yaw   = car.yaw + orbitYaw;
    float pitch = 0.32f + orbitPitch;
    glm::vec3 dir(-std::sin(yaw) * std::cos(pitch),
                   std::sin(pitch),
                  -std::cos(yaw) * std::cos(pitch));

    float followDistance = 6.0f * (kCarModelScale / 0.42f) + 0.2f;
    float cameraHeight   = 0.25f * (kCarModelScale / 0.42f) + 0.05f;
    float lookHeight     = 0.45f * (kCarModelScale / 0.42f) + 0.02f;

    glm::vec3 carEye      = car.position + glm::vec3(0.0f, kCarGroundYOffset + lookHeight, 0.0f);
    glm::vec3 desiredPosition = car.position + dir * followDistance
                              + glm::vec3(0.0f, cameraHeight, 0.0f);
    glm::vec3 lookTarget  = carEye;

    // -----------------------------------------------------------------------
    // CAMERA COLLISION  (active always, critical when reversing)
    // -----------------------------------------------------------------------
    // Pull the camera to at most hitDistance-margin along the car→camera ray
    // so it never sits inside solid geometry.
    // -----------------------------------------------------------------------
// CAMERA COLLISION - stepcast from car toward desired camera position
// -----------------------------------------------------------------------
    float safeDistance = followDistance;

    glm::vec3 rayDir = glm::normalize(desiredPosition - carEye);
    float stepSize   = 0.5f;
    int   numSteps   = static_cast<int>(followDistance / stepSize);

    for (int i = 1; i <= numSteps; ++i)
    {
        float      dist    = i * stepSize;
        glm::vec3  sample  = carEye + rayDir * dist;

        if (city.CheckCollision(sample, 0.3f))
        {
            const float margin = 0.5f;
            safeDistance = glm::max(dist - margin, 0.8f);
            break;
        }
    }

    if (safeDistance < followDistance)
        desiredPosition = car.position + dir * safeDistance
                        + glm::vec3(0.0f, cameraHeight, 0.0f);
    // -----------------------------------------------------------------------
    // -----------------------------------------------------------------------

    float t = isOrbiting ? 1.0f : glm::clamp(1.0f - std::pow(0.005f, dt), 0.0f, 1.0f);
    camera.Position    = glm::mix(camera.Position, desiredPosition, t);
    camera.Orientation = glm::normalize(glm::mix(
        camera.Orientation,
        glm::normalize(lookTarget - camera.Position), t));
}

void UpdateOrbitCamera(GLFWwindow* window)
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

void UpdateGameplay (GLFWwindow* window, DayNightCycle& dayNight, bool& headlightsOn, bool& lightsPressed)
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