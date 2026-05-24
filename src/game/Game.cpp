#include "Game.h"
#include "City.h"

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

    // ---------------------------------------------------------------------------
    // Car state
    // ---------------------------------------------------------------------------
    struct CarState
    {
        glm::vec3 position = glm::vec3(2.0f, 0.0f, 0.0f);
        float yaw = 0.0f;
        float speed = 0.0f;
        float steering = 0.0f;
        float wheelSpin = 0.0f;
    };

    // ---------------------------------------------------------------------------
    // Camera orbit globals
    // ---------------------------------------------------------------------------
    float orbitYaw = 0.0f;
    float orbitPitch = 0.0f;
    bool isOrbiting = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;

    // ---------------------------------------------------------------------------
    // Car constants
    // ---------------------------------------------------------------------------
    const float kCarModelScale = 0.42f;
    const float kCarGroundYOffset = 0.02f;

    // ---------------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------------
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

    // ---------------------------------------------------------------------------
    // Ground / marker meshes
    // ---------------------------------------------------------------------------
    Mesh CreateGroundMesh()
    {
        const float halfSize = 500.0f;
        const float uvScale = 100.0f;
        std::vector<Vertex> vertices =
            {
                {{-halfSize, 0.0f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
                {{halfSize, 0.0f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {uvScale, 0.0f}},
                {{halfSize, 0.0f, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {uvScale, uvScale}},
                {{-halfSize, 0.0f, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, uvScale}}};
        std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};

        const int texWidth = 64, texHeight = 64;
        std::vector<unsigned char> checkerData(texWidth * texHeight * 4);
        for (int y = 0; y < texHeight; ++y)
        {
            for (int x = 0; x < texWidth; ++x)
            {
                int idx = (y * texWidth + x) * 4;
                bool isDark = ((x / 8) + (y / 8)) % 2 == 0;
                checkerData[idx + 0] = isDark ? 34 : 46;
                checkerData[idx + 1] = isDark ? 68 : 92;
                checkerData[idx + 2] = isDark ? 42 : 56;
                checkerData[idx + 3] = 255;
            }
        }

        static const unsigned char blackPixel[] = {0, 0, 0, 255};
        std::vector<Texture> textures;
        textures.emplace_back(checkerData.data(), texWidth, texHeight, GL_RGBA, "diffuse", 0);
        textures.emplace_back(blackPixel, 1, 1, GL_RGBA, "specular", 1);
        return Mesh(vertices, indices, textures);
    }

    Mesh CreateOriginMarker()
    {
        const float halfSize = 4.0f;
        std::vector<Vertex> vertices =
            {
                {{-halfSize, 0.005f, -halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {0.0f, 0.0f}},
                {{halfSize, 0.005f, -halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {1.0f, 0.0f}},
                {{halfSize, 0.005f, halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {1.0f, 1.0f}},
                {{-halfSize, 0.005f, halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {0.0f, 1.0f}}};
        std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};

        static const unsigned char whitePixel[] = {255, 255, 255, 255};
        static const unsigned char blackPixel[] = {0, 0, 0, 255};
        std::vector<Texture> textures;
        textures.emplace_back(whitePixel, 1, 1, GL_RGBA, "diffuse", 0);
        textures.emplace_back(blackPixel, 1, 1, GL_RGBA, "specular", 1);
        return Mesh(vertices, indices, textures);
    }

    // ---------------------------------------------------------------------------
    // Car matrix
    // ---------------------------------------------------------------------------
    glm::mat4 BuildCarMatrix(const CarState &car)
    {
        glm::mat4 t = glm::mat4(1.0f);
        t = glm::translate(t, car.position + glm::vec3(0.0f, kCarGroundYOffset, 0.0f));
        t = glm::rotate(t, car.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        t = glm::scale(t, glm::vec3(kCarModelScale));
        return t;
    }

    // ---------------------------------------------------------------------------
    // Camera follow
    // ---------------------------------------------------------------------------
    void UpdateFollowCamera(Camera &camera, const CarState &car, float dt)
    {
        float yaw = car.yaw + orbitYaw;
        float pitch = 0.32f + orbitPitch;

        glm::vec3 dir(
            -std::sin(yaw) * std::cos(pitch),
            std::sin(pitch),
            -std::cos(yaw) * std::cos(pitch));

        glm::vec3 desiredPosition = car.position + dir * 6.0f + glm::vec3(0.0f, 0.25f, 0.0f);
        glm::vec3 lookTarget = car.position + glm::vec3(0.0f, kCarGroundYOffset + 0.45f, 0.0f);

        float t = isOrbiting ? 1.0f : glm::clamp(1.0f - std::pow(0.005f, dt), 0.0f, 1.0f);

        camera.Position = glm::mix(camera.Position, desiredPosition, t);
        camera.Orientation = glm::normalize(glm::mix(
            camera.Orientation,
            glm::normalize(lookTarget - camera.Position),
            t));
    }

    // ---------------------------------------------------------------------------
    // Car update  (city passed in so world-clamp uses its scale)
    // ---------------------------------------------------------------------------
    void UpdateCar(GLFWwindow *window, CarState &car, float dt, const City &city)
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
        const float wheelRadius = 0.38f * kCarModelScale;

        // --- Throttle ---
        float throttle = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            throttle += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            throttle -= 1.0f;

        // --- Steering ---
        float steerInput = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            steerInput += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            steerInput -= 1.0f;

        // --- Speed ---
        if (throttle > 0.0f)
            car.speed += acceleration * dt;
        else if (throttle < 0.0f)
            car.speed -= brakePower * dt;
        else
        {
            if (car.speed > 0.0f)
                car.speed = std::max(0.0f, car.speed - friction * dt);
            if (car.speed < 0.0f)
                car.speed = std::min(0.0f, car.speed + friction * dt);
        }
        car.speed = glm::clamp(car.speed, -maxReverseSpeed, maxForwardSpeed);

        // --- Steering ---
        if (steerInput != 0.0f)
            car.steering += steerInput * steeringResponse * dt;
        else
        {
            if (car.steering > 0.0f)
                car.steering = std::max(0.0f, car.steering - steeringReturn * dt);
            if (car.steering < 0.0f)
                car.steering = std::min(0.0f, car.steering + steeringReturn * dt);
        }
        car.steering = glm::clamp(car.steering, -maxSteering, maxSteering);

        // --- Rotation ---
        float turnFactor = (car.speed < 0.0f)
                               ? car.speed / maxReverseSpeed
                               : car.speed / maxForwardSpeed;
        car.yaw += car.steering * turnFactor * turnRate * dt;

        // --- Movement ---
        glm::vec3 forward = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
        car.position += forward * car.speed * dt;

        // --- World clamping (uses city scale) ---
        float cityScale = city.GetScale();
        glm::vec3 unclamped = car.position;
        car.position.x = glm::clamp(car.position.x, -760.0f * cityScale, 660.0f * cityScale);
        car.position.z = glm::clamp(car.position.z, -740.0f * cityScale, 2200.0f * cityScale);
        if (car.position.x != unclamped.x || car.position.z != unclamped.z)
            car.speed *= 0.3f;

        // --- Wheel spin ---
        car.wheelSpin += (car.speed * dt) / wheelRadius;
    }

} // namespace

// ---------------------------------------------------------------------------
// Game::Run
// ---------------------------------------------------------------------------
int Game::Run()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(width, height, "Mini Delivery Dash", NULL, NULL);
    if (!window)
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
            glad_glBlendFunc = (PFNGLBLENDFUNCPROC)GetProcAddress(openGL, "glBlendFunc");
    }
#endif

    int fbW = static_cast<int>(width), fbH = static_cast<int>(height);
    GetFramebufferSize(window, fbW, fbH);
    glViewport(0, 0, fbW, fbH);

    Shader shaderProgram("res/shaders/default.vert", "res/shaders/default.frag");

    glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);
    shaderProgram.Activate();
    glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Camera camera(fbW, fbH, glm::vec3(0.0f, 2.4f, -7.2f));
    CarState car;

    const bool drawLegacyGround = false;
    Mesh ground = CreateGroundMesh();
    Mesh originMarker = CreateOriginMarker();

    // --- Car model ---
    Model carModel("res/models/mclaren/source/McLaren F1 1993 By Alex.Ka/McLaren F1 1993 by Alex.Ka..obj");

    // --- City  (cambia solo esta ruta para usar otra ciudad) ---
    City city("res/models/hongkong_modificado/scene.gltf",
              7.0f,   // escala
              -0.60f, // offset Y (altura)
              10.0f,  // offset X (izquierda/derecha)
              25.0f   // offset Z (adelante/atrás)
    );

    float lastFrame = static_cast<float>(glfwGetTime());
    bool headlightsOn = false;
    bool lightsPressed = false;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        float dt = std::min(currentFrame - lastFrame, 0.1f);
        lastFrame = currentFrame;

        SyncCameraToFramebuffer(window, camera);

        // --- Headlights toggle (L) ---
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        {
            if (!lightsPressed)
            {
                headlightsOn = !headlightsOn;
                lightsPressed = true;
            }
        }
        else
            lightsPressed = false;

        // --- Braking light ---
        bool braking = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && car.speed > 0.1f);

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

        // --- Update ---
        UpdateCar(window, car, dt, city);
        UpdateFollowCamera(camera, car, dt);
        camera.updateMatrix(45.0f, 0.1f, 20000.0f);

        // --- Headlight uniforms ---
        shaderProgram.Activate();
        glUniform1i(glGetUniformLocation(shaderProgram.ID, "uHeadlightsLightOn"), headlightsOn ? 1 : 0);
        if (headlightsOn)
        {
            glm::vec3 fwd = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
            glm::vec3 right = glm::vec3(std::cos(car.yaw), 0.0f, -std::sin(car.yaw));
            glm::vec3 base = car.position + glm::vec3(0.0f, kCarGroundYOffset + 0.55f * kCarModelScale, 0.0f);
            glm::vec3 lPos = base + fwd * (1.8f * kCarModelScale) + right * (0.7f * kCarModelScale);
            glm::vec3 rPos = base + fwd * (1.8f * kCarModelScale) - right * (0.7f * kCarModelScale);
            glm::vec3 lDir = glm::normalize(fwd - glm::vec3(0.0f, 0.12f, 0.0f));

            glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightLeftPos"), lPos.x, lPos.y, lPos.z);
            glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightRightPos"), rPos.x, rPos.y, rPos.z);
            glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightDir"), lDir.x, lDir.y, lDir.z);
            glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightColor"), 2.2f, 2.2f, 1.8f);
        }

        // --- Draw ---
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (drawLegacyGround)
        {
            ground.Draw(shaderProgram, camera, glm::mat4(1.0f));
            originMarker.Draw(shaderProgram, camera, glm::mat4(1.0f));
        }

        city.Draw(shaderProgram, camera);
        carModel.Draw(shaderProgram, camera, BuildCarMatrix(car), car.wheelSpin, car.steering, headlightsOn, braking);

        // --- Window title ---
        char title[256];
        std::snprintf(title, sizeof(title),
                      "Mini Delivery Dash | Speed: %.1f km/h | Headlights [L]: %s",
                      std::abs(car.speed) * 3.6f,
                      headlightsOn ? "ON" : "OFF");
        glfwSetWindowTitle(window, title);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    shaderProgram.Delete();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}