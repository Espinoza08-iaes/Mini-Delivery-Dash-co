#include "Game.h"
#include "../city/City.h"
#include "../helpers/MainMenu.h"

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

#include "../../engine/rendering/Model.h"
#include "../meshes/ProceduralMeshes.h"
#include "../vehicle/CarController.h"
#include "../scene/DayNightCycle.h"

void Game::UpdateCameraEffects(GLFWwindow *window, Camera &camera, CarState &car)
{
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

void Game::UpdateHeadlights(Shader &shaderProgram, const CarState &car, bool headlightsOn)
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
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightLeftPos"), lPos.x, lPos.y, lPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightRightPos"), rPos.x, rPos.y, rPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightDir"), lDir.x, lDir.y, lDir.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightColor"), 2.2f, 2.2f, 1.8f);
    }
}

void Game::ResolveGroundCollision(CarState &car, City &city, float &carVerticalSpeed, bool &isOnGround, float &lastGroundHeight)
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

// --- Water Detection and Respawn ---
void Game::CheckWaterRespawn(CarState& car, City& city, float& carVerticalSpeed, bool& isOnGround, float& lastGroundHeight)
{
    // Variables estáticas para detectar rebotes repetidos
    static int bounceCount = 0;
    static float lastBounceTime = 0.0f;
    static bool wasBouncing = false;
    
    float currentTime = static_cast<float>(glfwGetTime());
    
    // Detectar si el auto está rebotando (velocidad vertical alta y no está en el suelo)
    bool isBouncing = (std::abs(carVerticalSpeed) > 2.0f && !isOnGround);
    
    // Detectar zona de agua por coordenadas (ajusta estos números según tu mapa)
    bool inWaterZone = (car.position.x > -100.0f && car.position.x < 100.0f &&
                        car.position.z > -100.0f && car.position.z < 100.0f);
    
    // Contar rebotes
    if (isBouncing && !wasBouncing)
    {
        bounceCount++;
        lastBounceTime = currentTime;
    }
    
    // Si hay muchos rebotes en poco tiempo o sigue rebotando por mucho tiempo
    if ((bounceCount >= 3 && (currentTime - lastBounceTime) < 1.0f) || 
        (isBouncing && inWaterZone && bounceCount >= 2))
    {
        std::cout << "[GAME] Car stuck in water! Bounce count: " << bounceCount << std::endl;
        
        // Posición segura
        glm::vec3 safeSpawn = glm::vec3(20.5588f, 7.22175f, -1.32232f);
        
        car.position = safeSpawn + glm::vec3(0.0f, 0.5f, 0.0f);
        car.speed = 0.0f;
        car.steering = 0.0f;
        car.yaw = 0.0f;
        car.pitch = 0.0f;
        car.roll = 0.0f;
        carVerticalSpeed = 0.0f;
        isOnGround = true;
        lastGroundHeight = safeSpawn.y;
        
        // Resetear contador
        bounceCount = 0;
        
        std::cout << "[GAME] Respawned at: " << car.position.x << ", " << car.position.y << ", " << car.position.z << std::endl;
    }
    
    // Resetear contador si pasa mucho tiempo sin rebotar
    if (!isBouncing && (currentTime - lastBounceTime) > 2.0f)
    {
        bounceCount = 0;
    }
    
    wasBouncing = isBouncing;
}

void Game::ApplyDayNightLighting(Shader &shaderProgram, DayNightCycle &dayNight)
{
    glm::vec3 lightColor = dayNight.GetLightColor();
    glm::vec3 lightPos = dayNight.GetLightPosition();
    glm::vec3 fogColor = dayNight.GetHorizonColor();

    glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "uFogColor"), fogColor.x, fogColor.y, fogColor.z);
    glUniform1f(glGetUniformLocation(shaderProgram.ID, "uAmbientStrength"), dayNight.GetAmbientStrength());
}

void Game::DrawOcean(Shader &waterShader, Mesh &ocean, Camera &camera, float currentFrame, const glm::vec3 &skyTint, DayNightCycle &dayNight)
{
    glm::vec3 fogColor = dayNight.GetHorizonColor();
    waterShader.Activate();
    glUniform3f(glGetUniformLocation(waterShader.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uTime"), currentFrame);
    glUniform3f(glGetUniformLocation(waterShader.ID, "uSkyTint"), skyTint.x, skyTint.y, skyTint.z);
    glUniform3f(glGetUniformLocation(waterShader.ID, "uFogColor"), fogColor.x, fogColor.y, fogColor.z);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uFogStart"), 60.0f);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uFogEnd"), 350.0f);
    glm::vec3 lightPos = dayNight.GetLightPosition();
    glm::vec3 lightColor = dayNight.GetLightColor();
    glUniform3f(glGetUniformLocation(waterShader.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform4f(glGetUniformLocation(waterShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, 1.0f);

    camera.Matrix(waterShader, "camMatrix");
    ocean.Draw(waterShader, camera, glm::mat4(1.0f));
}

// ======================================================================
// Game::Run
// ======================================================================
int Game::Run()
{

    GLFWwindow *window = nullptr;

    int fbW = 0;
    int fbH = 0;


    if (!InitializeWindow(window, fbW, fbH))
        return -1;

    std::cout << "[INFO] Loading assets and compiling shaders... Please wait." << std::endl;

    std::cout << "[INFO] Loading assets and compiling shaders... Please wait." << std::endl;

    // ----- PRECARGA DE RECURSOS -----
    Shader shaderProgram("res/shaders/default.vert", "res/shaders/default.frag");
    Shader skyShader("res/shaders/sky.vert", "res/shaders/sky.frag");
    Shader waterShader("res/shaders/water.vert", "res/shaders/water.frag");
    glfwPollEvents();

    SetupOpenGL(shaderProgram);
    Camera camera(fbW, fbH, glm::vec3(0.0f, 2.4f, -7.2f));

    const bool drawLegacyGround = false;
    Mesh ground = CreateGroundMesh();
    Mesh ocean = CreateOceanMesh();
    Mesh originMarker = CreateOriginMarker();
    Mesh skySphere = CreateSkySphereMesh("res/textures/sunflowers_puresky_4k.hdr");
    glfwPollEvents();

    // --- Car model ---
    Model carModel("res/models/mclaren/source/McLaren F1 1993 By Alex.Ka/McLaren F1 1993 by Alex.Ka..obj");
    glfwPollEvents();

    // --- City ---
    City city("res/models/city_3d/scene.gltf",
              1.0f, // usar escala original del mapa
              0.0f, // auto-alinear altura con el suelo
              0.0f, // sin offset X manual
              0.0f, // sin offset Z manual
              true  // auto-align map to ground
    );
    glfwPollEvents();

    // --- Shadow Map Shader & FBO Setup ---
    Shader shadowShader("res/shaders/shadow.vert", "res/shaders/shadow.frag");
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    GLuint depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    
    GLuint depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- Camera Depth Map FBO Setup for SSAO ---
    GLuint cameraDepthFBO;
    glGenFramebuffers(1, &cameraDepthFBO);
    
    GLuint cameraDepthMap;
    glGenTextures(1, &cameraDepthMap);
    glBindTexture(GL_TEXTURE_2D, cameraDepthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 
                 fbW, fbH, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindFramebuffer(GL_FRAMEBUFFER, cameraDepthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, cameraDepthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int lastCameraWidth = fbW;
    int lastCameraHeight = fbH;

    DayNightCycle dayNight;

    bool volverAlMenu = true;

    // Bucle principal (menú + juego)
    while (volverAlMenu && !glfwWindowShouldClose(window))
    {

        // Restaurar color de fondo a negro para el menú
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        // ----- MENÚ PRINCIPAL -----
        MainMenu menu(window, fbW, fbH);
        MainMenu::Result res = menu.Show();

        if (res == MainMenu::Result::Quit || glfwWindowShouldClose(window))
        {
            volverAlMenu = false;
            break;
        }

        // Restaurar estado OpenGL tras el menú
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_MULTISAMPLE);
        glDepthMask(GL_TRUE);
        shaderProgram.Activate();

        // ----- JUEGO -----
        CarState car;
        float carVerticalSpeed = 0.0f;
        bool isOnGround = true;
        float lastGroundHeight = car.position.y;

        // Spawn
        bool foundDefaultRoad = false;
        glm::vec3 roadSpawn = city.GetBestRoadSpawn(car.position, 1200.0f);
        car.position = roadSpawn + glm::vec3(0.0f, kGroundClearance, 0.0f);
        game::GroundSample spawnSample;
        if (city.GetGroundSample(car.position, car.position.y + 2.0f, spawnSample, 12.0f, 0.45f) && spawnSample.found)
        {
            car.position.y = spawnSample.height + kGroundClearance;
            foundDefaultRoad = true;
            std::cout << "[SPAWN] Road at: (" << car.position.x << ", " << car.position.y << ", " << car.position.z << ")" << std::endl;
        }
        else
        {
            car.position.y = city.GetHeightAt(car.position.x, car.position.z, car.position.y, &foundDefaultRoad, 12.0f, 0.45f);
            std::cout << "[SPAWN] Fallback at: (" << car.position.x << ", " << car.position.y << ", " << car.position.z << ")" << std::endl;
        }
        glm::vec3 spawnPoint = car.position;
        lastGroundHeight = car.position.y - kGroundClearance;

        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        float lastFrame = static_cast<float>(glfwGetTime());
        bool headlightsOn = false;
        bool lightsPressed = false;

        glfwSetWindowTitle(window, "Mini Delivery Dash");

        bool enJuego = true;

        // Bucle del juego
        while (enJuego && !glfwWindowShouldClose(window))
        {
            float currentFrame = static_cast<float>(glfwGetTime());
            float dt = std::min(currentFrame - lastFrame, 0.1f);
            lastFrame = currentFrame;

            SyncCameraToFramebuffer(window, camera);
            dayNight.Update(dt);
            UpdateGameplay(window, dayNight, headlightsOn, lightsPressed);

            bool braking = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && car.speed > 0.1f);
            bool accelerating = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
            float jumpDistanceBoost = accelerating ? 1.4f : 0.0f;

            HandleCarJumpAndRespawn(window, car, carVerticalSpeed, isOnGround, spawnPoint, jumpDistanceBoost, lastGroundHeight);
            ApplyGravity(car, carVerticalSpeed, isOnGround, dt);
            UpdateCar(window, car, dt, city);
            ResolveGroundCollision(car, city, carVerticalSpeed, isOnGround, lastGroundHeight);
            CheckWaterRespawn(car, city, carVerticalSpeed, isOnGround, lastGroundHeight);

            UpdateFollowCamera(camera, car, dt);
            UpdateCameraEffects(window, camera, car);
            UpdateHeadlights(shaderProgram, car, headlightsOn);
            ApplyDayNightLighting(shaderProgram, dayNight);

        // --- 1. Render depth of scene to texture (from light's perspective) ---
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Define light space matrix centered around the car
        glm::vec3 lightDir = glm::normalize(dayNight.GetLightPosition());
        glm::vec3 lightPos = car.position + lightDir * 180.0f;
        glm::vec3 up = glm::abs(lightDir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        glm::mat4 lightView = glm::lookAt(lightPos, car.position, up);
        glm::mat4 lightProjection = glm::ortho(-75.0f, 75.0f, -75.0f, 75.0f, 0.1f, 350.0f);
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        shadowShader.Activate();
        glUniformMatrix4fv(glGetUniformLocation(shadowShader.ID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.5f, 10.0f);

        city.Draw(shadowShader, camera);
        carModel.Draw(shadowShader, camera, BuildCarMatrix(car), car.wheelSpin, car.steering, headlightsOn, braking);

        glDisable(GL_POLYGON_OFFSET_FILL);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // --- 1b. Resize camera depth texture if window size changed ---
        if (camera.width != lastCameraWidth || camera.height != lastCameraHeight)
        {
            lastCameraWidth = camera.width;
            lastCameraHeight = camera.height;
            glBindTexture(GL_TEXTURE_2D, cameraDepthMap);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 
                         camera.width, camera.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }

        // --- 1c. Render depth of scene from camera's perspective (for SSAO) ---
        glBindFramebuffer(GL_FRAMEBUFFER, cameraDepthFBO);
        glViewport(0, 0, camera.width, camera.height);
        glClear(GL_DEPTH_BUFFER_BIT);

        shadowShader.Activate();
        camera.Matrix(shadowShader, "lightSpaceMatrix");

        city.Draw(shadowShader, camera);
        carModel.Draw(shadowShader, camera, BuildCarMatrix(car), car.wheelSpin, car.steering, headlightsOn, braking);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Reset viewport to window size
        SyncCameraToFramebuffer(window, camera);

        // --- 2. Normal Render Pass ---
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

        // Bind skybox texture for reflections on slot 10
        shaderProgram.Activate();
        glActiveTexture(GL_TEXTURE0 + 10);
        glBindTexture(GL_TEXTURE_2D, skySphere.textures[0].ID);
        glUniform1i(glGetUniformLocation(shaderProgram.ID, "uSkyReflectionMap"), 10);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "uSkyRotationAngle"), glm::radians(dayNight.GetTime() * 15.0f));
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "uSkyTint"), skyTint.x, skyTint.y, skyTint.z);

        // Bind shadow map depth texture on slot 11
        glActiveTexture(GL_TEXTURE0 + 11);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glUniform1i(glGetUniformLocation(shaderProgram.ID, "uShadowMap"), 11);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID, "uLightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

        // Bind camera depth map for SSAO on slot 12
        glActiveTexture(GL_TEXTURE0 + 12);
        glBindTexture(GL_TEXTURE_2D, cameraDepthMap);
        glUniform1i(glGetUniformLocation(shaderProgram.ID, "uCameraDepthMap"), 12);

            city.Draw(shaderProgram, camera);
            carModel.Draw(shaderProgram, camera, BuildCarMatrix(car), car.wheelSpin, car.steering, headlightsOn, braking);
            DrawOcean(waterShader, ocean, camera, currentFrame, skyTint, dayNight);

            shaderProgram.Activate();

        float timeOfDay = dayNight.GetTime();
        int hours = static_cast<int>(timeOfDay);
        int minutes = static_cast<int>((timeOfDay - hours) * 60.0f);
        char title[256];
        std::snprintf(title, sizeof(title),
                      "Mini Delivery Dash | Time: %02d:%02d | Speed: %.1f km/h | Pos: (%.2f, %.2f, %.2f)",
                      hours, minutes,
                      std::abs(car.speed) * 3.6f,
                      car.position.x, car.position.y, car.position.z);
        glfwSetWindowTitle(window, title);

            glfwSwapBuffers(window);
            glfwPollEvents();

            // ----- PAUSA CON ESC -----
            static bool escWasPressed = false;
            bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (escPressed && !escWasPressed)
            {

                // Resetear color de fondo para el menú de pausa
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

                // Crear un menú temporal para la pausa (mismo estilo que el principal)
                MainMenu pauseMenu(window, fbW, fbH);
                MainMenu::Result pauseResult = pauseMenu.Show(true);  // true = modo pausa

                if (pauseResult == MainMenu::Result::Quit)
                {
                    // Salir al menú principal
                    enJuego = false;
                }
                // Restaurar estado OpenGL
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_MULTISAMPLE);
                glDepthMask(GL_TRUE);
                shaderProgram.Activate();
                lastFrame = static_cast<float>(glfwGetTime());
            }
            escWasPressed = escPressed;
        }
    }

    // Limpieza final
    shaderProgram.Delete();
    skyShader.Delete();
    waterShader.Delete();
    shadowShader.Delete();

    glDeleteFramebuffers(1, &depthMapFBO);
    glDeleteTextures(1, &depthMap);
    glDeleteFramebuffers(1, &cameraDepthFBO);
    glDeleteTextures(1, &cameraDepthMap);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}