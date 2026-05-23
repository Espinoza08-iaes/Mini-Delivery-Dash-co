#include "Game.h"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#define GLM_ENABLE_EXPERIMENTAL

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "../engine/Model.h"

const unsigned int width = 800;
const unsigned int height = 800;


namespace
{
struct CarState
{
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    float yaw = 0.0f;
    float speed = 0.0f;
    float steering = 0.0f;
    float wheelSpin = 0.0f;
};

// Global variables for camera orbit
float orbitYaw = 0.0f;
float orbitPitch = 0.0f;
bool isOrbiting = false;
double lastMouseX = 0.0;
double lastMouseY = 0.0;

Mesh CreateGroundMesh()
{
    // Generate a tiled checkered floor to give an amazing sense of speed and driving
    const float halfSize = 500.0f;
    const float uvScale = 100.0f; // Repeating UV tile coords
    std::vector<Vertex> vertices =
    {
        {{-halfSize, 0.0f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ halfSize, 0.0f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {uvScale, 0.0f}},
        {{ halfSize, 0.0f,  halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {uvScale, uvScale}},
        {{-halfSize, 0.0f,  halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, uvScale}}
    };

    std::vector<GLuint> indices = { 0, 1, 2, 2, 3, 0 };

    // Create checkerboard textures
    const int texWidth = 64;
    const int texHeight = 64;
    std::vector<unsigned char> checkerData(texWidth * texHeight * 4);
    for (int y = 0; y < texHeight; ++y)
    {
        for (int x = 0; x < texWidth; ++x)
        {
            int idx = (y * texWidth + x) * 4;
            bool isDark = ((x / 8) + (y / 8)) % 2 == 0;
            if (isDark)
            {
                // Forest Green
                checkerData[idx + 0] = 34;
                checkerData[idx + 1] = 68;
                checkerData[idx + 2] = 42;
                checkerData[idx + 3] = 255;
            }
            else
            {
                // Grass Green
                checkerData[idx + 0] = 46;
                checkerData[idx + 1] = 92;
                checkerData[idx + 2] = 56;
                checkerData[idx + 3] = 255;
            }
        }
    }

    static const unsigned char blackPixel[] = { 0, 0, 0, 255 };

    std::vector<Texture> textures;
    textures.emplace_back(checkerData.data(), texWidth, texHeight, GL_RGBA, "diffuse", 0);
    textures.emplace_back(blackPixel, 1, 1, GL_RGBA, "specular", 1);

    return Mesh(vertices, indices, textures);
}

Mesh CreateOriginMarker()
{
    // Beautiful starting pad red grid at center
    const float halfSize = 4.0f;
    std::vector<Vertex> vertices =
    {
        {{-halfSize, 0.005f, -halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {0.0f, 0.0f}},
        {{ halfSize, 0.005f, -halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {1.0f, 0.0f}},
        {{ halfSize, 0.005f,  halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {1.0f, 1.0f}},
        {{-halfSize, 0.005f,  halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {0.0f, 1.0f}}
    };

    std::vector<GLuint> indices = { 0, 1, 2, 2, 3, 0 };

    static const unsigned char whitePixel[] = { 255, 255, 255, 255 };
    static const unsigned char blackPixel[] = { 0, 0, 0, 255 };

    std::vector<Texture> textures;
    textures.emplace_back(whitePixel, 1, 1, GL_RGBA, "diffuse", 0);
    textures.emplace_back(blackPixel, 1, 1, GL_RGBA, "specular", 1);

    return Mesh(vertices, indices, textures);
}

glm::mat4 BuildCarMatrix(const CarState& car)
{
    glm::mat4 transform = glm::mat4(1.0f);
    // Draw the car at its authentic scale (1.0f)
    const float modelScale = 1.0f;
    
    // Add Y offset 0.38f to perfectly align the bottom wheels of the model on the ground
    glm::vec3 renderPos = car.position + glm::vec3(0.0f, 0.38f, 0.0f);
    transform = glm::translate(transform, renderPos);
    transform = glm::rotate(transform, car.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::scale(transform, glm::vec3(modelScale));
    return transform;
}

void UpdateFollowCamera(Camera& camera, const CarState& car, float dt)
{
    // The look direction based on car yaw and orbit angles
    float yaw = car.yaw + orbitYaw;
    float pitch = 0.32f + orbitPitch; // Default angle is ~18 degrees up

    // Calculate direction vector from target to camera
    // We want the camera to be behind the car (-sin(yaw) and -cos(yaw)) and above it (sin(pitch))
    glm::vec3 dir(
        -std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        -std::cos(yaw) * std::cos(pitch)
    );
    
    // Position camera at a distance from the car (adding the offset vector places it above ground)
    float distance = 7.2f;
    glm::vec3 desiredPosition = car.position + dir * distance + glm::vec3(0.0f, 0.4f, 0.0f);
    glm::vec3 lookTarget = car.position + glm::vec3(0.0f, 0.8f, 0.0f);

    // Smoothly interpolate camera position
    float t = 1.0f - std::pow(0.005f, dt);
    t = glm::clamp(t, 0.0f, 1.0f);
    
    // Snappy camera tracking during active drag orbiting
    if (isOrbiting)
    {
        t = 1.0f;
    }
    
    camera.Position = glm::mix(camera.Position, desiredPosition, t);
    
    glm::vec3 desiredOrientation = glm::normalize(lookTarget - camera.Position);
    camera.Orientation = glm::normalize(glm::mix(camera.Orientation, desiredOrientation, t));
}

void UpdateCar(GLFWwindow* window, CarState& car, float dt)
{
    const float acceleration = 25.0f;
    const float brakePower = 35.0f;
    const float maxForwardSpeed = 28.0f;
    const float maxReverseSpeed = 9.0f;
    const float friction = 8.0f;
    const float steeringResponse = 3.2f;
    const float steeringReturn = 6.0f;
    const float maxSteering = glm::radians(34.0f);
    const float turnRate = glm::radians(85.0f);
    const float wheelRadius = 0.38f;

    // Inputs: W/S for Throttle
    float throttle = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        throttle += 1.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        throttle -= 1.0f;
    }

    // Inputs: A/D for Steering
    float steerInput = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        steerInput += 1.0f; // Turn Left (A turns left)
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        steerInput -= 1.0f; // Turn Right (D turns right)
    }

    // Speed calculation
    if (throttle > 0.0f)
    {
        car.speed += acceleration * dt;
    }
    else if (throttle < 0.0f)
    {
        car.speed -= brakePower * dt;
    }
    else
    {
        // Smooth deceleration due to friction
        if (car.speed > 0.0f)
        {
            car.speed = std::max(0.0f, car.speed - friction * dt);
        }
        else if (car.speed < 0.0f)
        {
            car.speed = std::min(0.0f, car.speed + friction * dt);
        }
    }

    car.speed = glm::clamp(car.speed, -maxReverseSpeed, maxForwardSpeed);

    // Steering wheel calculation
    if (steerInput != 0.0f)
    {
        car.steering += steerInput * steeringResponse * dt;
    }
    else
    {
        // Smooth return of the steering wheel to center
        if (car.steering > 0.0f)
        {
            car.steering = std::max(0.0f, car.steering - steeringReturn * dt);
        }
        else if (car.steering < 0.0f)
        {
            car.steering = std::min(0.0f, car.steering + steeringReturn * dt);
        }
    }
    car.steering = glm::clamp(car.steering, -maxSteering, maxSteering);

    // Rotate car based on velocity
    float turnFactor = car.speed / maxForwardSpeed;
    if (car.speed < 0.0f)
    {
        turnFactor = car.speed / maxReverseSpeed; // Reverse steering feels correct
    }
    car.yaw += car.steering * turnFactor * turnRate * dt;

    // Movement: The nose of the car points in the local +Z direction
    glm::vec3 forward = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
    car.position += forward * car.speed * dt;

    // World clamping (ground size 500x500, limit at 480)
    const float worldLimit = 480.0f;
    car.position.x = glm::clamp(car.position.x, -worldLimit, worldLimit);
    car.position.z = glm::clamp(car.position.z, -worldLimit, worldLimit);

    if (std::abs(car.position.x) >= worldLimit || std::abs(car.position.z) >= worldLimit)
    {
        car.speed *= 0.3f; // Penalty for hitting the barrier
    }

    car.wheelSpin += (car.speed * dt) / wheelRadius;
}

} // namespace

int Game::Run()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "Mini Delivery Dash", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

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

    glViewport(0, 0, width, height);

    Shader shaderProgram("res/shaders/default.vert", "res/shaders/default.frag");

    glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);
    glm::mat4 lightModel = glm::mat4(1.0f);
    lightModel = glm::translate(lightModel, lightPos);

    shaderProgram.Activate();
    glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Setup initial camera location just behind the car
    Camera camera(width, height, glm::vec3(0.0f, 2.4f, -7.2f));
    CarState car;
    Mesh ground = CreateGroundMesh();
    Mesh originMarker = CreateOriginMarker();

    std::string modelPath = "res/models/mclaren/source/McLaren F1 1993 By Alex.Ka/McLaren F1 1993 by Alex.Ka..obj";
    Model model(modelPath.c_str());

    float lastFrame = static_cast<float>(glfwGetTime());

    bool headlightsOn = false;
    bool lightsKeyPressed = false;
    bool useColorOverride = false;
    glm::vec3 paintColor = glm::vec3(0.92f, 0.92f, 0.92f);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        
        // Prevent huge frame-time spikes on system freezes
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        
        lastFrame = currentFrame;

        // Handle Headlights Toggle key (L)
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        {
            if (!lightsKeyPressed)
            {
                headlightsOn = !headlightsOn;
                lightsKeyPressed = true;
            }
        }
        else
        {
            lightsKeyPressed = false;
        }

        // Handle Body Paint Customization Keys (1, 2, 3, 4, 5)
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        {
            useColorOverride = false;
        }
        else if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        {
            useColorOverride = true;
            paintColor = glm::vec3(0.95f, 0.35f, 0.05f); // Papaya Racing Orange (McLaren classic!)
        }
        else if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        {
            useColorOverride = true;
            paintColor = glm::vec3(0.9f, 0.05f, 0.05f); // Modena Racing Red
        }
        else if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
        {
            useColorOverride = true;
            paintColor = glm::vec3(0.05f, 0.45f, 0.85f); // Yas Marina Blue
        }
        else if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS)
        {
            useColorOverride = true;
            paintColor = glm::vec3(0.12f, 0.12f, 0.12f); // Stealth Carbon Black
        }

        // Handle Braking Light Trigger
        bool braking = false;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            if (car.speed > 0.1f)
            {
                braking = true;
            }
        }

        // Handle right-click mouse camera orbit controls
        bool rightMousePressed = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
        if (rightMousePressed)
        {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            if (!isOrbiting)
            {
                isOrbiting = true;
                lastMouseX = mouseX;
                lastMouseY = mouseY;
            }
            else
            {
                double dx = mouseX - lastMouseX;
                double dy = mouseY - lastMouseY;
                lastMouseX = mouseX;
                lastMouseY = mouseY;
                
                const float sensitivity = 0.007f;
                orbitYaw -= dx * sensitivity;
                orbitPitch += dy * sensitivity;
                
                // Limit pitch to prevent upside down or ground clipping
                orbitPitch = glm::clamp(orbitPitch, -glm::radians(10.0f), glm::radians(75.0f));
            }
        }
        else
        {
            isOrbiting = false;
            // Smoothly return the orbit camera behind the car when released!
            orbitYaw = glm::mix(orbitYaw, 0.0f, 0.1f);
            orbitPitch = glm::mix(orbitPitch, 0.0f, 0.1f);
        }

        UpdateCar(window, car, deltaTime);
        UpdateFollowCamera(camera, car, deltaTime);
        camera.updateMatrix(45.0f, 0.1f, 1000.0f);

        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ground.Draw(shaderProgram, camera, glm::mat4(1.0f));
        originMarker.Draw(shaderProgram, camera, glm::mat4(1.0f));
        
        // Draw the car model with its wheel spins, steering angle, lights, and color overrides!
        model.Draw(shaderProgram, camera, BuildCarMatrix(car), car.wheelSpin, car.steering, headlightsOn, braking, useColorOverride, paintColor);

        char title[256];
        std::snprintf(title, sizeof(title), "Mini Delivery Dash | Speed: %.1f km/h | Lights [L]: %s | Paint [1-5]: %s", 
                      std::abs(car.speed) * 3.6f, 
                      headlightsOn ? "ON" : "OFF", 
                      !useColorOverride ? "Classic Silver" : 
                      (paintColor == glm::vec3(0.95f, 0.35f, 0.05f) ? "Papaya Orange" :
                       (paintColor == glm::vec3(0.9f, 0.05f, 0.05f) ? "Racing Red" :
                        (paintColor == glm::vec3(0.05f, 0.45f, 0.85f) ? "Yas Marina Blue" : "Carbon Black"))));
        glfwSetWindowTitle(window, title);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    shaderProgram.Delete();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
