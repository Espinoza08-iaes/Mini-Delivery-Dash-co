#include "Game.h"
#include "../city/City.h"

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

#include "../../engine/resources/Model.h"
#include "../meshes/ProceduralMeshes.h"
#include "../vehicle/CarController.h"
#include "../scene/DayNightCycle.h"

void Game::UpdateCameraEffects(GLFWwindow* window, Camera& camera, CarState& car)
{
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
}

void Game::UpdateHeadlights(Shader& shaderProgram, const CarState& car, bool headlightsOn)
{
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
}

// Check if car is on ground
void Game::ResolveGroundCollision(CarState& car, City& city, float& carVerticalSpeed, bool& isOnGround, float& lastGroundHeight)
{
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
}

void Game::ApplyDayNightLighting(Shader& shaderProgram, DayNightCycle& dayNight)
{
    glm::vec3 lightColor = dayNight.GetLightColor();
    glm::vec3 lightPos = dayNight.GetLightPosition();
    glm::vec3 fogColor = dayNight.GetHorizonColor();

    // --- Day/Night Cycle uniforms for main shader ---
    glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "uFogColor"), fogColor.x, fogColor.y, fogColor.z);
    glUniform1f(glGetUniformLocation(shaderProgram.ID, "uAmbientStrength"), dayNight.GetAmbientStrength());
}

void Game::DrawOcean(Shader& waterShader, Mesh& ocean, Camera& camera, float currentFrame, const glm::vec3& skyTint, DayNightCycle& dayNight)
{

    glm::vec3 fogColor = dayNight.GetHorizonColor();
    // --- Draw Ocean ---
    waterShader.Activate();
    glUniform3f(glGetUniformLocation(waterShader.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uTime"), currentFrame);
    glUniform3f(glGetUniformLocation(waterShader.ID, "uSkyTint"), skyTint.x, skyTint.y, skyTint.z);
    glUniform3f(glGetUniformLocation(waterShader.ID, "uFogColor"), fogColor.x, fogColor.y, fogColor.z);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uFogStart"), 60.0f);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uFogEnd"), 350.0f);
    camera.Matrix(waterShader, "camMatrix");
    ocean.Draw(waterShader, camera, glm::mat4(1.0f));
}
// ---------------------------------------------------------------------------
// Game::Run
// ---------------------------------------------------------------------------
int Game::Run()
{
    
    GLFWwindow* window = nullptr;

    int fbW = 0;
    int fbH = 0;

    if (!InitializeWindow(window, fbW, fbH))
    {
        return -1;
    }

    Shader shaderProgram("res/shaders/default.vert", "res/shaders/default.frag");
    Shader skyShader("res/shaders/sky.vert", "res/shaders/sky.frag");
    Shader waterShader("res/shaders/water.vert", "res/shaders/water.frag");

    SetupOpenGL(shaderProgram); // Configure initial OpenGL state

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

    // --- City ---
    City city("res/models/city_3d/scene.gltf",
              1.0f,   // usar escala original del mapa
              0.0f,   // auto-alinear altura con el suelo
              0.0f,   // sin offset X manual
              0.0f,   // sin offset Z manual
              true    // auto-align map to ground
    );

    DayNightCycle dayNight;

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
        dayNight.Update(dt);
        UpdateGameplay(window, dayNight, headlightsOn, lightsPressed);

        // --- Braking light ---
        bool braking = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && car.speed > 0.1f);

        bool accelerating = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
        float jumpDistanceBoost = accelerating ? 1.4f : 0.0f;

        // --- Handle jump and respawn ---
        HandleCarJumpAndRespawn(window, car, carVerticalSpeed, isOnGround, spawnPoint, jumpDistanceBoost, lastGroundHeight);

        ApplyGravity(car, carVerticalSpeed, isOnGround, dt);

        // --- Update car movement (horizontal) ---
        UpdateCar(window, car, dt, city);

        // --- Check if car is on ground ---
        ResolveGroundCollision(car, city, carVerticalSpeed, isOnGround, lastGroundHeight);
        // Respawn for the water
        CheckWaterRespawn(car, city, carVerticalSpeed, isOnGround, lastGroundHeight);

        UpdateFollowCamera(camera, car, dt);

        UpdateCameraEffects(window, camera, car);

        // --- Headlight uniforms ---
        UpdateHeadlights(shaderProgram, car, headlightsOn);

        ApplyDayNightLighting(shaderProgram, dayNight);

        // --- Draw ---
        glm::vec3 horizon = dayNight.GetHorizonColor();
        glClearColor(horizon.x, horizon.y, horizon.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Draw Sky Sphere first ---
        glDepthMask(GL_FALSE); // Disable depth buffer writing so sky sphere is always in the background
        skyShader.Activate();
        
        // Pass camera matrix
        camera.Matrix(skyShader, "camMatrix");
        
        // Center sky sphere around camera and slowly rotate it over time to simulate moving clouds
        glm::mat4 skyModel = glm::translate(glm::mat4(1.0f), camera.Position);
        skyModel = glm::rotate(skyModel, glm::radians(dayNight.GetTime() * 15.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Pass sky tint uniform
        glm::vec3 skyTint = dayNight.GetSkyTint();
        glUniform3f(glGetUniformLocation(skyShader.ID, "uSkyTint"), skyTint.x, skyTint.y, skyTint.z);
        
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

        DrawOcean(waterShader, ocean, camera, currentFrame, skyTint, dayNight);

        // --- Reactivate main shader program ---
        shaderProgram.Activate();

        float timeOfDay = dayNight.GetTime();
        // --- Window title HUD ---
        int hours = static_cast<int>(timeOfDay);
        int minutes = static_cast<int>((timeOfDay - hours) * 60.0f);
        char title[192];
        std::snprintf(title, sizeof(title),
                      "Mini Delivery Dash | Time: %02d:%02d | Speed: %.1f km/h",
                      hours, minutes,
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
