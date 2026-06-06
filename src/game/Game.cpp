#include "Game.h"
#include "City.h"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include <ctime>
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
        glm::vec3 position = glm::vec3(20.0f, 0.0f, 0.0f);
        float yaw = 0.0f;
        float speed = 0.0f;
        float steering = 0.0f;
        float wheelSpin = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
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
    const float kCarModelScale = 0.21875f;
    const float kCarGroundYOffset = 0.0f;
    const float kGroundClearance = 0.01f;
    const float kMaxDownSnap = 0.40f;

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
    Mesh CreateOceanMesh()
    {
        const float halfSize = 4000.0f;
        std::vector<Vertex> vertices = {
            {{-halfSize, -4.5f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{halfSize, -4.5f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{halfSize, -4.5f, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-halfSize, -4.5f, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };
        std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};
        std::vector<Texture> textures;
        textures.emplace_back("res/textures/sunflowers_puresky_4k.hdr", "diffuse", 0);
        return Mesh(vertices, indices, textures);
    }

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

    Mesh CreateSkySphereMesh(const char* texturePath, int sectors = 32, int stacks = 16, float radius = 3000.0f)
    {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;

        const float PI = 3.14159265f;

        for (int i = 0; i <= stacks; ++i)
        {
            float stackAngle = PI / 2.0f - i * PI / stacks; // from pi/2 to -pi/2
            float xy = radius * std::cos(stackAngle);
            float y = radius * std::sin(stackAngle);

            for (int j = 0; j <= sectors; ++j)
            {
                float sectorAngle = j * 2 * PI / sectors; // from 0 to 2pi

                float x = xy * std::cos(sectorAngle);
                float z = xy * std::sin(sectorAngle);

                Vertex v;
                v.position = glm::vec3(x, y, z);
                v.normal = glm::normalize(glm::vec3(-x, -y, -z)); // point inwards
                v.color = glm::vec3(1.0f);
                v.texUV = glm::vec2((float)j / sectors, (float)i / stacks);
                vertices.push_back(v);
            }
        }

        for (int i = 0; i < stacks; ++i)
        {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;

            for (int j = 0; j < sectors; ++j, ++k1, ++k2)
            {
                if (i != 0)
                {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                if (i != (stacks - 1))
                {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }

        std::vector<Texture> textures;
        textures.emplace_back(texturePath, "diffuse", 0);
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
        t = glm::rotate(t, car.pitch, glm::vec3(1.0f, 0.0f, 0.0f)); // pitch around local X
        t = glm::rotate(t, car.roll, glm::vec3(0.0f, 0.0f, 1.0f));  // roll around local Z
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

        // Dynamically scale follow camera distance and heights based on the car model scale
        float followDistance = 6.0f * (kCarModelScale / 0.42f) + 0.2f;
        float cameraHeight = 0.25f * (kCarModelScale / 0.42f) + 0.05f;
        float lookHeight = 0.45f * (kCarModelScale / 0.42f) + 0.02f;

        glm::vec3 desiredPosition = car.position + dir * followDistance + glm::vec3(0.0f, cameraHeight, 0.0f);
        glm::vec3 lookTarget = car.position + glm::vec3(0.0f, kCarGroundYOffset + lookHeight, 0.0f);

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
        bool isBoosting = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && car.speed > 0.0f);
        const float acceleration = isBoosting ? 22.0f : 9.0f;
        const float brakePower = 15.0f;
        const float maxForwardSpeed = isBoosting ? 24.0f : 11.0f;
        const float maxReverseSpeed = 3.5f;
        const float friction = 5.0f;
        const float steeringResponse = isBoosting ? 3.5f : 5.5f; // Stiffer steering during high-speed boost
        const float steeringReturn = 9.0f;
        const float maxSteering = glm::radians(35.0f);
        const float turnRate = glm::radians(125.0f);
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

        // --- Rotation & Movement ---
        float turnFactor = 0.0f;
        if (std::abs(car.speed) > 0.01f)
        {
            float speedRatio = std::abs(car.speed) / maxForwardSpeed;
            float rawFactor = 0.35f + 0.65f * speedRatio;
            if (car.speed < 0.0f)
            {
                speedRatio = std::abs(car.speed) / maxReverseSpeed;
                rawFactor = 0.35f + 0.65f * speedRatio;
                turnFactor = -rawFactor;
            }
            else
            {
                turnFactor = rawFactor;
            }
        }
        car.yaw += car.steering * turnFactor * turnRate * dt;

        // --- Movement & Obstacle Collisions ---
        glm::vec3 forward = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
        glm::vec3 right = glm::vec3(std::cos(car.yaw), 0.0f, -std::sin(car.yaw));
        glm::vec3 prevPosition = car.position;
        glm::vec3 fullMove = forward * car.speed * dt;
        glm::vec3 nextPosition = car.position + fullMove;

        float carCollisionRadius = kCarModelScale * 1.8f;
        // Removed hard bounds clamping - rely on collision detection instead

        if (city.CheckCollision(nextPosition, carCollisionRadius))
        {
            // Intentar deslizamiento en X
            glm::vec3 tryX = glm::vec3(nextPosition.x, prevPosition.y, prevPosition.z);
            // Intentar deslizamiento en Z
            glm::vec3 tryZ = glm::vec3(prevPosition.x, prevPosition.y, nextPosition.z);

            bool xOk = !city.CheckCollision(tryX, carCollisionRadius);
            bool zOk = !city.CheckCollision(tryZ, carCollisionRadius);

            if (xOk && !zOk)
                nextPosition = tryX;
            else if (zOk && !xOk)
                nextPosition = tryZ;
            else if (xOk && zOk)
                nextPosition = tryX; // ambos libres: prioriza X
            else
            {
                // Colisión total — detener
                car.speed = 0.0f;
                nextPosition = prevPosition;
            }
        }
        car.position = nextPosition;

        // --- Terrain heights and normals (for tilt and snap) ---
        float d = 0.5f * (kCarModelScale / 0.28f); // Distancia de las ruedas proporcional a la escala del coche
        glm::vec3 probePoints[4] = {
            car.position + forward * d, // front
            car.position - forward * d, // back
            car.position + right * d,   // right
            car.position - right * d    // left
        };
        glm::vec3 normals[4] = {};
        for (int i = 0; i < 4; ++i)
        {
            game::GroundSample s;
            if (city.GetGroundSample(probePoints[i], car.position.y, s, 2.0f, 0.12f) && s.found)
            {
                normals[i] = s.normal;
            }
            else
            {
                normals[i] = glm::vec3(0, 1, 0);
            }
        }

        // Corregir solo si la altura detectada no es una caída falsa.
        game::GroundSample centerSample;
        if (city.GetGroundSample(car.position, car.position.y, centerSample, 2.0f, 0.12f) && centerSample.found)
        {
            if (centerSample.height >= car.position.y - kMaxDownSnap)
            {
                car.position.y = std::max(car.position.y, centerSample.height + kGroundClearance);
            }
        }

        // Tilt: usar promedio de normales de las 4 ruedas
        glm::vec3 avgNormal = glm::normalize(normals[0] + normals[1] + normals[2] + normals[3]);
        float targetPitch = std::atan2(avgNormal.x, avgNormal.y);
        float targetRoll = -std::atan2(avgNormal.z, avgNormal.y);
        car.pitch = glm::mix(car.pitch, targetPitch, 15.0f * dt);
        car.roll = glm::mix(car.roll, targetRoll, 15.0f * dt);

        // --- Wheel spin ---
        car.wheelSpin += (car.speed * dt) / wheelRadius;
    }

    // --- Car jump and respawn ---
    void HandleCarJumpAndRespawn(GLFWwindow *window, CarState &car, float &carVerticalSpeed, bool &isOnGround, glm::vec3 spawnPoint, float jumpDistanceBoost, float &lastGroundHeight)
    {
        static bool zPressedLast = false;
        static bool rPressedLast = false;

        // --- Jump ---
        bool zPressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
        if (zPressed && !zPressedLast && isOnGround)
        {
            carVerticalSpeed = 3.2f; // Reduce vertical impulse to avoid overshooting geometry
            isOnGround = false;

            glm::vec3 forward = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
            car.position += forward * (jumpDistanceBoost * 0.45f); // Reduced horizontal boost
        }
        zPressedLast = zPressed;

        // --- Respawn ---
        bool rPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if (rPressed && !rPressedLast)
        {
            car.position = spawnPoint;
            car.speed = 0.0f;
            car.steering = 0.0f;
            car.yaw = 0.0f;
            car.pitch = 0.0f;
            car.roll = 0.0f;
            carVerticalSpeed = 0.0f;
            isOnGround = true;
            lastGroundHeight = spawnPoint.y - kGroundClearance;
        }
        rPressedLast = rPressed;
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
    glfwWindowHint(GLFW_SAMPLES, 4); // Enable 4x Multisample Anti-Aliasing (MSAA)

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
    Shader skyShader("res/shaders/sky.vert", "res/shaders/sky.frag");
    Shader waterShader("res/shaders/water.vert", "res/shaders/water.frag");

    struct SkyKeyframe
    {
        float hour;
        glm::vec3 zenithColor;
        glm::vec3 horizonColor;
        glm::vec3 lightColor;
        glm::vec3 lightPos;
        float ambientStrength;
        glm::vec3 skyTint;
    };

    std::vector<SkyKeyframe> keyframes = {
        {0.0f,  glm::vec3(0.015f, 0.015f, 0.05f), glm::vec3(0.02f, 0.02f, 0.08f),   glm::vec3(0.2f, 0.2f, 0.35f),  glm::vec3(0.0f, -1.0f, 0.0f), 0.12f, glm::vec3(0.06f, 0.08f, 0.22f)},
        {5.0f,  glm::vec3(0.015f, 0.015f, 0.05f), glm::vec3(0.02f, 0.02f, 0.08f),   glm::vec3(0.2f, 0.2f, 0.35f),  glm::vec3(0.0f, -1.0f, 0.0f), 0.12f, glm::vec3(0.06f, 0.08f, 0.22f)},
        {6.5f,  glm::vec3(0.1f, 0.15f, 0.35f),    glm::vec3(0.85f, 0.45f, 0.25f),  glm::vec3(0.8f, 0.5f, 0.35f),  glm::vec3(1.0f, 0.2f, 0.0f),  0.15f, glm::vec3(0.9f, 0.65f, 0.5f)},
        {12.0f, glm::vec3(0.12f, 0.32f, 0.72f),   glm::vec3(0.55f, 0.72f, 0.92f),  glm::vec3(1.0f, 1.0f, 0.95f),  glm::vec3(0.2f, 1.0f, 0.2f),  0.22f, glm::vec3(1.0f, 1.0f, 1.0f)},
        {17.5f, glm::vec3(0.12f, 0.32f, 0.72f),   glm::vec3(0.55f, 0.72f, 0.92f),  glm::vec3(1.0f, 1.0f, 0.95f),  glm::vec3(0.2f, 1.0f, 0.2f),  0.22f, glm::vec3(1.0f, 1.0f, 1.0f)},
        {19.0f, glm::vec3(0.08f, 0.08f, 0.25f),   glm::vec3(0.88f, 0.28f, 0.12f),  glm::vec3(0.85f, 0.35f, 0.15f), glm::vec3(-1.0f, 0.15f, 0.0f), 0.15f, glm::vec3(0.95f, 0.5f, 0.3f)},
        {20.5f, glm::vec3(0.03f, 0.03f, 0.12f),   glm::vec3(0.08f, 0.06f, 0.18f),  glm::vec3(0.3f, 0.25f, 0.4f),  glm::vec3(-1.0f, -0.2f, 0.0f), 0.14f, glm::vec3(0.2f, 0.2f, 0.4f)},
        {24.0f, glm::vec3(0.015f, 0.015f, 0.05f), glm::vec3(0.02f, 0.02f, 0.08f),   glm::vec3(0.2f, 0.2f, 0.35f),  glm::vec3(0.0f, -1.0f, 0.0f), 0.12f, glm::vec3(0.06f, 0.08f, 0.22f)}
    };

    float dayTime = 12.0f;       // Start at noon
    const float daySpeed = 0.05f; // hours of game time per real second (a full 24h cycle takes 480 seconds / 8 minutes)

    glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);
    shaderProgram.Activate();
    glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), 1.0f, 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE); // Enable MSAA rendering pipeline

    Camera camera(fbW, fbH, glm::vec3(0.0f, 2.4f, -7.2f));

    CarState car;
    glm::vec3 spawnPoint = car.position; // Save initial spawn point
    float carVerticalSpeed = 0.0f;
    bool isOnGround = true;
    float lastGroundHeight = spawnPoint.y;

    const bool drawLegacyGround = false;
    Mesh ground = CreateGroundMesh();
    Mesh ocean = CreateOceanMesh();
    Mesh originMarker = CreateOriginMarker();
    Mesh skySphere = CreateSkySphereMesh("res/textures/sunflowers_puresky_4k.hdr");

    // --- Car model ---
    Model carModel("res/models/mclaren/source/McLaren F1 1993 By Alex.Ka/McLaren F1 1993 by Alex.Ka..obj");

    // --- City  (cambia solo esta ruta para usar otra ciudad) ---
    City city("res/models/city_3d/scene.gltf",
              1.0f,   // usar escala original del mapa
              0.0f,   // auto-alinear altura con el suelo
              0.0f,   // sin offset X manual
              0.0f,   // sin offset Z manual
              true    // auto-align map to ground
    );

    // --- Snap Y to the street at the default starting position ---
    bool foundDefaultRoad = false;
    glm::vec3 roadSpawn = city.GetBestRoadSpawn(car.position, 1200.0f);
    car.position = roadSpawn + glm::vec3(0.0f, kGroundClearance, 0.0f);
    game::GroundSample spawnSample;
    if (city.GetGroundSample(car.position, car.position.y + 2.0f, spawnSample, 12.0f, 0.45f) && spawnSample.found)
    {
        car.position.y = spawnSample.height + kGroundClearance;
        foundDefaultRoad = true;
        std::cout << "[SPAWN] Spawned car on road at coordinate: ("
                  << car.position.x << ", " << car.position.y << ", " << car.position.z << ")" << std::endl;
    }
    else
    {
        car.position.y = city.GetHeightAt(car.position.x, car.position.z, car.position.y, &foundDefaultRoad, 12.0f, 0.45f);
        std::cout << "[SPAWN] Fallback street height at coordinate: ("
                  << car.position.x << ", " << car.position.y << ", " << car.position.z << ")" << std::endl;
    }
    spawnPoint = car.position;
    lastGroundHeight = car.position.y - kGroundClearance;

    // --- Game initialization ---
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    float lastFrame = static_cast<float>(glfwGetTime());
    bool headlightsOn = false;
    bool lightsPressed = false;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        float dt = std::min(currentFrame - lastFrame, 0.1f);
        lastFrame = currentFrame;

        SyncCameraToFramebuffer(window, camera);

        // --- Advance Day/Night cycle ---
        dayTime += dt * daySpeed;
        if (dayTime >= 24.0f)
            dayTime -= 24.0f;

        // --- Interpolate Sky Keyframes ---
        glm::vec3 currentZenith(0.0f);
        glm::vec3 currentHorizon(0.0f);
        glm::vec3 currentLightColor(0.0f);
        glm::vec3 currentLightPos(0.0f);
        float currentAmbient = 0.20f;
        glm::vec3 currentSkyTint(1.0f);

        for (size_t i = 0; i < keyframes.size() - 1; ++i)
        {
            if (dayTime >= keyframes[i].hour && dayTime <= keyframes[i + 1].hour)
            {
                float t = (dayTime - keyframes[i].hour) / (keyframes[i + 1].hour - keyframes[i].hour);
                currentZenith = glm::mix(keyframes[i].zenithColor, keyframes[i + 1].zenithColor, t);
                currentHorizon = glm::mix(keyframes[i].horizonColor, keyframes[i + 1].horizonColor, t);
                currentLightColor = glm::mix(keyframes[i].lightColor, keyframes[i + 1].lightColor, t);
                currentLightPos = glm::mix(keyframes[i].lightPos, keyframes[i + 1].lightPos, t);
                currentAmbient = glm::mix(keyframes[i].ambientStrength, keyframes[i + 1].ambientStrength, t);
                currentSkyTint = glm::mix(keyframes[i].skyTint, keyframes[i + 1].skyTint, t);
                break;
            }
        }

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
            if (dayTime < 6.5f || dayTime > 19.0f)
                headlightsOn = true;
            else
                headlightsOn = false;
        }

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

        bool accelerating = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
        float jumpDistanceBoost = accelerating ? 1.4f : 0.0f;

        // --- Handle jump and respawn ---
        HandleCarJumpAndRespawn(window, car, carVerticalSpeed, isOnGround, spawnPoint, jumpDistanceBoost, lastGroundHeight);

        // --- Gravity and vertical movement ---
        const float gravity = 18.0f;
        if (!isOnGround)
        {
            carVerticalSpeed -= gravity * dt;
            car.position.y += carVerticalSpeed * dt;
        }

        // --- Update car movement (horizontal) ---
        UpdateCar(window, car, dt, city);

        // --- Check if car is on ground ---
        game::GroundSample groundSample;
        if (city.GetGroundSample(car.position, car.position.y, groundSample) && groundSample.found)
        {
            float groundY = groundSample.height;
            lastGroundHeight = groundY;

            float desiredY = groundY + kGroundClearance;

            if (car.position.y < desiredY - 0.02f)
            {
                // Evitar hundir o caer por debajo del suelo real.
                car.position.y = desiredY;
                carVerticalSpeed = 0.0f;
                isOnGround = true;
            }
            else if (car.position.y <= desiredY + 0.12f && carVerticalSpeed <= 0.5f)
            {
                car.position.y = desiredY;
                carVerticalSpeed = 0.0f;
                isOnGround = true;
            }
            else
            {
                isOnGround = false;
            }
        }
        else
        {
            // If no ground detected, attempt an expanded recovery probe before forcing position.
            game::GroundSample recoverySample;
            if (city.GetGroundSample(car.position, car.position.y, recoverySample, 12.0f, 1.0f) && recoverySample.found)
            {
                float desiredY = recoverySample.height + kGroundClearance;
                car.position.y = desiredY;
                carVerticalSpeed = 0.0f;
                isOnGround = true;
                lastGroundHeight = recoverySample.height;
            }
            else if (car.position.y < lastGroundHeight - 0.5f)
            {
                // If the car dropped far below the last known ground, clamp it back to prevent falling through.
                car.position.y = lastGroundHeight + kGroundClearance;
                carVerticalSpeed = 0.0f;
                isOnGround = true;
            }
            else
            {
                isOnGround = false;
            }
        }

        // --- Water Detection and Respawn ---
        if (car.position.y < -2.0f)
        {
            std::cout << "[GAME] Car fell into the ocean! Respawning on the nearest road..." << std::endl;
            glm::vec3 safeSpawn = city.GetBestRoadSpawn(car.position, 1200.0f);
            car.position = safeSpawn + glm::vec3(0.0f, kGroundClearance + 0.1f, 0.0f);
            car.speed = 0.0f;
            car.steering = 0.0f;
            carVerticalSpeed = 0.0f;
            isOnGround = true;
            lastGroundHeight = safeSpawn.y;
        }



        UpdateFollowCamera(camera, car, dt);

        // --- Nitrous Boost Camera Warp (FOV) and Shake ---
        bool isBoosting = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && car.speed > 1.0f);
        static float currentFov = 45.0f;
        float targetFov = isBoosting ? 58.0f : 45.0f;
        currentFov = glm::mix(currentFov, targetFov, 0.08f);

        if (isBoosting)
        {
            float shakeX = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * 0.035f;
            float shakeY = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * 0.035f;
            float shakeZ = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * 0.035f;
            camera.Position += glm::vec3(shakeX, shakeY, shakeZ);
        }

        camera.updateMatrix(currentFov, 0.1f, 20000.0f);

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

        // --- Day/Night Cycle uniforms for main shader ---
        glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), currentLightColor.x, currentLightColor.y, currentLightColor.z, 1.0f);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), currentLightPos.x, currentLightPos.y, currentLightPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "uFogColor"), currentHorizon.x, currentHorizon.y, currentHorizon.z);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "uAmbientStrength"), currentAmbient);

        // --- Draw ---
        glClearColor(currentHorizon.x, currentHorizon.y, currentHorizon.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Draw Sky Sphere first ---
        glDepthMask(GL_FALSE); // Disable depth buffer writing so sky sphere is always in the background
        skyShader.Activate();
        
        // Pass camera matrix
        camera.Matrix(skyShader, "camMatrix");
        
        // Center sky sphere around camera and slowly rotate it over time to simulate moving clouds
        glm::mat4 skyModel = glm::translate(glm::mat4(1.0f), camera.Position);
        skyModel = glm::rotate(skyModel, glm::radians(dayTime * 15.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Pass sky tint uniform
        glUniform3f(glGetUniformLocation(skyShader.ID, "uSkyTint"), currentSkyTint.x, currentSkyTint.y, currentSkyTint.z);
        
        // Draw the sky sphere
        skySphere.Draw(skyShader, camera, skyModel);
        glDepthMask(GL_TRUE); // Re-enable depth writing

        if (drawLegacyGround)
        {
            ground.Draw(shaderProgram, camera, glm::mat4(1.0f));
            originMarker.Draw(shaderProgram, camera, glm::mat4(1.0f));
        }

        city.Draw(shaderProgram, camera);
        carModel.Draw(shaderProgram, camera, BuildCarMatrix(car), car.wheelSpin, car.steering, headlightsOn, braking);

        // --- Draw Ocean ---
        waterShader.Activate();
        glUniform3f(glGetUniformLocation(waterShader.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
        glUniform1f(glGetUniformLocation(waterShader.ID, "uTime"), currentFrame);
        glUniform3f(glGetUniformLocation(waterShader.ID, "uSkyTint"), currentSkyTint.x, currentSkyTint.y, currentSkyTint.z);
        glUniform3f(glGetUniformLocation(waterShader.ID, "uFogColor"), currentHorizon.x, currentHorizon.y, currentHorizon.z);
        glUniform1f(glGetUniformLocation(waterShader.ID, "uFogStart"), 60.0f);
        glUniform1f(glGetUniformLocation(waterShader.ID, "uFogEnd"), 350.0f);
        camera.Matrix(waterShader, "camMatrix");
        ocean.Draw(waterShader, camera, glm::mat4(1.0f));

        // --- Reactivate main shader program ---
        shaderProgram.Activate();

        // --- Window title HUD ---
        int hours = (int)dayTime;
        int minutes = (int)((dayTime - hours) * 60.0f);
        char title[256];
        std::snprintf(title, sizeof(title),
                      "Mini Delivery Dash | X: %.2f Y: %.2f Z: %.2f | Time: %02d:%02d | Speed: %.1f km/h",
                      car.position.x,
                      car.position.y,
                      car.position.z,
                      hours,
                      minutes,
                      std::abs(car.speed) * 3.6f);
        glfwSetWindowTitle(window, title);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    shaderProgram.Delete();
    skyShader.Delete();
    waterShader.Delete();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
