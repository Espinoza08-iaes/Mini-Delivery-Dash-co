#include "Game.h"
#include "../city/City.h"
#include "../scene/streetLamp.h"
#include "../helpers/MainMenu.h"
#include "../delivery/DeliverySystem.h"
#include "../delivery/DeliveryHUD.h"
#include "../shop/ShopUI.h"
#include "../shop/ShopManager.h"
#include "../../engine/graphics/Frustum.h"
#include "../../engine/audio/AudioEngine.h"

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

// ============================================================================
// Game Configuration Constants
// ============================================================================

// Global pointer for GLFW callbacks (needed because GLFW callbacks must be static)
static ShopUI* g_shopUI = nullptr;

namespace GameConstants
{
    // Camera settings
    constexpr float CAMERA_FOV_NORMAL = 45.0f;
    constexpr float CAMERA_FOV_BOOSTING = 58.0f;
    constexpr float CAMERA_FOV_SMOOTHING = 0.08f;
    constexpr float CAMERA_NEAR_PLANE = 0.1f;
    constexpr float CAMERA_FAR_PLANE = 20000.0f;

    // Camera shake during boost
    constexpr float SHAKE_MAGNITUDE = 0.035f;

    // Headlights
    constexpr float HEADLIGHT_VERTICAL_OFFSET = 0.55f;
    constexpr float HEADLIGHT_FORWARD_OFFSET = 1.8f;
    constexpr float HEADLIGHT_SIDE_OFFSET = 0.7f;
    constexpr float HEADLIGHT_DOWN_ANGLE = 0.12f;
    static const glm::vec3 HEADLIGHT_COLOR(2.2f, 2.2f, 1.8f);

    // Ground collision
    constexpr float GROUND_SNAP_DOWN = 15.0f;
    constexpr float GROUND_SNAP_UP = 2.0f;
    constexpr float GROUND_CLEARANCE_TOLERANCE = 0.5f;
    constexpr float VOID_THRESHOLD_FORCE = -10.0f;
    constexpr float VOID_THRESHOLD_RESPAWN = -3.5f;

    // Respawn
    constexpr float RESPAWN_SEARCH_DISTANCE = 500.0f;
    constexpr float RESPAWN_HEIGHT_OFFSET = 0.5f;

    // Spawn
    constexpr float SPAWN_SEARCH_DISTANCE = 1200.0f;
    constexpr float SPAWN_HEIGHT_OFFSET = 2.0f;
    constexpr float SPAWN_SNAP_DOWN = 12.0f;
    constexpr float SPAWN_SNAP_UP = 0.45f;

    // Car jump
    constexpr float JUMP_DISTANCE_BOOST = 1.4f;
    constexpr float JUMP_VERTICAL_SPEED = 3.2f;

    // Ocean rendering
    constexpr float OCEAN_FOG_START = 60.0f;
    constexpr float OCEAN_FOG_END = 350.0f;

    // Shadow mapping
    constexpr unsigned int SHADOW_MAP_WIDTH = 2048;
    constexpr unsigned int SHADOW_MAP_HEIGHT = 2048;
    constexpr float LIGHT_OFFSET_DISTANCE = 220.0f;
    constexpr float ORTHO_SIZE = 90.0f;
    constexpr float ORTHO_NEAR = 0.1f;
    constexpr float ORTHO_FAR = 450.0f;
    constexpr float POLYGON_OFFSET_FACTOR = 2.5f;
    constexpr float POLYGON_OFFSET_UNITS = 10.0f;

    // Sky sphere
    constexpr float SKY_ROTATION_SPEED = 15.0f;

    // Time
    constexpr float MIN_DELTA_TIME = 0.1f;
    constexpr float SECONDS_PER_MINUTE = 60.0f;
}

// ============================================================================
// Game Implementation
// ============================================================================

// Handles dynamic camera FOV transitions and shake effects during car boost.
void Game::UpdateCameraEffects(GLFWwindow *window, Camera &camera, CarState &car)
{
    using namespace GameConstants;
    // Check if turbo is unlocked
    ShopManager* shop = ShopManager::GetInstance();
    bool canUseTurbo = shop->IsAbilityUnlocked(AbilityType::Turbo);
    bool isBoosting = canUseTurbo && (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && car.speed > 1.0f);
    
    static float currentFov = CAMERA_FOV_NORMAL;
    float targetFov = isBoosting ? CAMERA_FOV_BOOSTING : CAMERA_FOV_NORMAL;
    currentFov = glm::mix(currentFov, targetFov, CAMERA_FOV_SMOOTHING);
    
    if (isBoosting)
    {
        float shakeX = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * SHAKE_MAGNITUDE;
        float shakeY = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * SHAKE_MAGNITUDE;
        float shakeZ = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * SHAKE_MAGNITUDE;
        camera.Position += glm::vec3(shakeX, shakeY, shakeZ);
    }
    
    camera.updateMatrix(currentFov, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);
}

// Positions and orientates car headlights in the shader based on vehicle state.
void Game::UpdateHeadlights(Shader &shaderProgram, const CarState &car, bool headlightsOn)
{
    shaderProgram.Activate();
    // Toggle headlight visibility in the shader
    glUniform1i(glGetUniformLocation(shaderProgram.ID, "uHeadlightsLightOn"), headlightsOn ? 1 : 0);

    if (!headlightsOn)
        return;

    // Calculate headlight positions and direction relative to car orientation
    glm::vec3 fwd = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
    glm::vec3 right = glm::vec3(std::cos(car.yaw), 0.0f, -std::sin(car.yaw));
    glm::vec3 base = car.position + glm::vec3(0.0f, kCarGroundYOffset + 0.55f * kCarModelScale, 0.0f);
    glm::vec3 lPos = base + fwd * (1.8f * kCarModelScale) + right * (0.7f * kCarModelScale);
    glm::vec3 rPos = base + fwd * (1.8f * kCarModelScale) - right * (0.7f * kCarModelScale);
    glm::vec3 lDir = glm::normalize(fwd - glm::vec3(0.0f, 0.12f, 0.0f));

    // Pass spatial and color data to the shader
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightLeftPos"), lPos.x, lPos.y, lPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightRightPos"), rPos.x, rPos.y, rPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightDir"), lDir.x, lDir.y, lDir.z);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "uHeadlightColor"), 2.2f, 2.2f, 1.8f);
}

// Manages ground collision, snapping the car to terrain or handling falls.
void Game::ResolveGroundCollision(CarState &car, City &city, float &carVerticalSpeed, bool &isOnGround, float &lastGroundHeight)
{
    using namespace GameConstants;

    game::GroundSample groundSample;
    // Check if there is valid ground geometry beneath or near the car
    if (city.GetGroundSample(car.position, car.position.y, groundSample, GROUND_SNAP_DOWN, GROUND_SNAP_UP))
    {
        float groundY = groundSample.height;
        lastGroundHeight = groundY;
        float desiredY = groundY + kGroundClearance;
        // Snap car to ground if within tolerance and reset vertical velocity
        if (car.position.y <= desiredY + GROUND_CLEARANCE_TOLERANCE)
        {
            car.position.y = desiredY;
            if (carVerticalSpeed < 0.0f)
                carVerticalSpeed = 0.0f;
            isOnGround = true;
        }
        else
        {
            isOnGround = false;
        }
    }
    else
    { // Handle car falling out of bounds (void)
        isOnGround = false;

        if (car.position.y < VOID_THRESHOLD_FORCE)
        {
            car.position.y = VOID_THRESHOLD_RESPAWN;
        }
    }
}

// Respawns the car at the nearest safe road position if it falls below the void threshold.
void Game::CheckWaterRespawn(CarState &car, City &city, float &carVerticalSpeed, bool &isOnGround, float &lastGroundHeight)
{
    using namespace GameConstants;

    // Check if the car has fallen into the void/water
    if (car.position.y < VOID_THRESHOLD_RESPAWN)
    {
        std::cout << "[GAME] Car fell into void! Respawning..." << std::endl;
        
        // Daño por caer al agua: 8% base
        ShopManager* shop = ShopManager::GetInstance();
        float durabilityMult = shop->GetUpgradeMultiplier(UpgradeType::FuelEfficiency);
        float waterDamage = 8.0f / durabilityMult;
        car.durability -= waterDamage;
        std::cout << "[CAR] Caída al agua! -" << waterDamage << "% durabilidad" << std::endl;
        
        // Check if car died from water damage
        if (car.durability <= 0.0f) {
            car.isDead = true;
        }
        
        // Find the closest safe spawn point on the road
        glm::vec3 safeSpawn = city.GetBestRoadSpawn(car.position, RESPAWN_SEARCH_DISTANCE);

        // Reset car position, physics state, and orientation
        car.position = safeSpawn + glm::vec3(0.0f, RESPAWN_HEIGHT_OFFSET, 0.0f);
        car.speed = 0.0f;
        car.steering = 0.0f;
        car.yaw = 0.0f;
        car.pitch = 0.0f;
        car.roll = 0.0f;
        carVerticalSpeed = 0.0f;
        isOnGround = true;
        lastGroundHeight = safeSpawn.y;

        std::cout << "[GAME] Respawned at: " << car.position.x << ", " << car.position.y << ", " << car.position.z << std::endl;
    }
}

// Updates shader lighting and fog parameters based on the current day-night cycle.
void Game::ApplyDayNightLighting(Shader &shaderProgram, DayNightCycle &dayNight)
{
    glm::vec3 lightColor = dayNight.GetLightColor();
    glm::vec3 lightPos = dayNight.GetLightPosition();
    glm::vec3 fogColor = dayNight.GetHorizonColor();

    // Send lighting, fog, and ambient intensity uniforms to the shader
    glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "uFogColor"), fogColor.x, fogColor.y, fogColor.z);
    glUniform1f(glGetUniformLocation(shaderProgram.ID, "uAmbientStrength"), dayNight.GetAmbientStrength());
}

// Renders the ocean mesh using environmental shaders, fog, and day-night lighting.
void Game::DrawOcean(Shader &waterShader, Mesh &ocean, Camera &camera, float currentFrame, const glm::vec3 &skyTint, DayNightCycle &dayNight)
{
    using namespace GameConstants;

    glm::vec3 fogColor = dayNight.GetHorizonColor();
    waterShader.Activate();
    // Update shader uniforms with camera, time, and environment data
    glUniform3f(glGetUniformLocation(waterShader.ID, "camPos"), camera.Position.x, camera.Position.y, camera.Position.z);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uTime"), currentFrame);
    glUniform3f(glGetUniformLocation(waterShader.ID, "uSkyTint"), skyTint.x, skyTint.y, skyTint.z);
    glUniform3f(glGetUniformLocation(waterShader.ID, "uFogColor"), fogColor.x, fogColor.y, fogColor.z);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uFogStart"), OCEAN_FOG_START);
    glUniform1f(glGetUniformLocation(waterShader.ID, "uFogEnd"), OCEAN_FOG_END);

    // Sync lighting properties with the day-night cycle
    glm::vec3 lightPos = dayNight.GetLightPosition();
    glm::vec3 lightColor = dayNight.GetLightColor();
    glUniform3f(glGetUniformLocation(waterShader.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform4f(glGetUniformLocation(waterShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, 1.0f);

    // Set view-projection matrix and render the mesh
    camera.Matrix(waterShader, "camMatrix");
    ocean.Draw(waterShader, camera, glm::mat4(1.0f));
}

// ============================================================================
// Shop System Callbacks
// ============================================================================

void Game::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (g_shopUI && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        g_shopUI->ProcessMouseClick(mouseX, mouseY);
    }
}

void Game::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_shopUI) {
        g_shopUI->ProcessMouseMove(xpos, ypos);
    }
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

    bool backToMenu = true;

    // For the audio in menu and effects
    AudioEngine audioEngine;

    audioEngine.Initialize();
    audioEngine.LoadEngineSound("res/audio/motorAudio.wav");
    audioEngine.LoadMenuMusic("res/audio/menuMusic.wav");
    audioEngine.LoadAmbientMusic("res/audio/ambientMusic.wav");
    audioEngine.LoadDeliveryMusic("res/audio/deliveryMusic.wav");
    
    OrderState lastOrderState = OrderState::WAITING;
    
    // Bucle principal (menú + juego)
    while (backToMenu && !glfwWindowShouldClose(window))
    {

        audioEngine.PlayMenuMusic();

        std::cout << "[AUDIO] Entering main menu\n";
        // Restaurar color de fondo a negro para el menú
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        // ----- MENÚ PRINCIPAL -----
        MainMenu menu(window, fbW, fbH);
        MainMenu::Result res = menu.Show();

        if (res == MainMenu::Result::Quit || glfwWindowShouldClose(window))
        {
            backToMenu = false;
            break;
        }
        audioEngine.StopMenuMusic();
        audioEngine.PlayAmbientMusic();
        lastOrderState = OrderState::FAILED;
        std::cout << "[INFO] Loading assets and compiling shaders... Please wait." << std::endl;

        // ----- PRECARGA DE RECURSOS -----
        menu.RenderLoading(0.10f, "COMPILING SHADERS...");
        Shader shaderProgram("res/shaders/default.vert", "res/shaders/default.frag");
        Shader skyShader("res/shaders/sky.vert", "res/shaders/sky.frag");
        Shader waterShader("res/shaders/water.vert", "res/shaders/water.frag");
        Shader lampShader("res/shaders/default.vert", "res/shaders/streetlamp.frag");
        glfwPollEvents();

        SetupOpenGL(shaderProgram);
        Camera camera(fbW, fbH, glm::vec3(0.0f, 2.4f, -7.2f));

        menu.RenderLoading(0.30f, "GENERATING PROCEDURAL GEOMETRY...");
        const bool drawLegacyGround = false;
        Mesh ground = CreateGroundMesh();
        Mesh ocean = CreateOceanMesh();
        Mesh originMarker = CreateOriginMarker();
        Mesh skySphere = CreateSkySphereMesh("res/textures/sunflowers_puresky_4k.hdr");
        glfwPollEvents();

        // --- Car model ---
        menu.RenderLoading(0.50f, "LOADING VEHICLE (MCLAREN F1)...");
        Model carModel("res/models/mclaren/source/McLaren F1 1993 By Alex.Ka/McLaren F1 1993 by Alex.Ka..obj");
        glfwPollEvents();

        // --- City ---
        menu.RenderLoading(0.70f, "PARSING CITY 3D MAP (GLTF)...");
        City city("res/models/city_3d/scene.gltf",
                  1.0f, // usar escala original del mapa
                  0.0f, // auto-alinear altura con el suelo
                  0.0f, // sin offset X manual
                  0.0f, // sin offset Z manual
                  true  // auto-align map to ground
        );
        glfwPollEvents();

        // Cargar desde archivo si existe, si no usar auto-generación
        menu.RenderLoading(0.90f, "SETTING UP SHADOWS & LIGHTS...");
        std::vector<glm::vec3> lampPositions;
        {
            std::ifstream testFile("res/lamp_positions.txt");
            if (testFile.good())
                lampPositions = city.GetStreetLampPositionsFromFile("res/lamp_positions.txt");
            else
                lampPositions = city.GetStreetLampPositions(18.0f);
        }
        std::cout << "[INFO] Street lamps: " << lampPositions.size() << std::endl;

        Mesh lampMesh = CreateStreetLampMesh();

        // --- Delivery System ---
        DeliverySystem deliverySystem;
        deliverySystem.Initialize();

        // --- Delivery HUD ---
        DeliveryHUD deliveryHUD(window, fbW, fbH);

        // --- Shop System ---
        ShopManager::GetInstance()->LoadShopData();
        g_shopUI = new ShopUI(window);
        glfwSetMouseButtonCallback(window, Game::mouse_button_callback);
        glfwSetCursorPosCallback(window, Game::cursor_position_callback);
        std::cout << "[INFO] Shop system initialized" << std::endl;
        
        // Set car state reference AFTER car is initialized
        // Will be set in game loop

        // --- Shadow Map Shader & FBO Setup ---
        Shader shadowShader("res/shaders/shadow.vert", "res/shaders/shadow.frag");
        // En lugar de SHADOW_WIDTH, SHADOW_HEIGHT usa GameConstants::SHADOW_MAP_WIDTH, etc.
        const unsigned int SHADOW_WIDTH = GameConstants::SHADOW_MAP_WIDTH;
        const unsigned int SHADOW_HEIGHT = GameConstants::SHADOW_MAP_HEIGHT;
        GLuint depthMapFBO;
        glGenFramebuffers(1, &depthMapFBO);

        GLuint depthMap;
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, fbW, fbH, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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
        bool useSSAO = true;

        // Restaurar estado OpenGL tras el menú
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_MULTISAMPLE);
        glDepthMask(GL_TRUE);
        shaderProgram.Activate();

        // ----- JUEGO -----
        CarState car;
        audioEngine.PlayEngine();
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
        bool shopKeyPressed = false;

        glfwSetWindowTitle(window, "Mini Delivery Dash");

        bool enJuego = true;
        
        // Set car state reference to shop UI
        g_shopUI->SetCarState(&car);

        // Bucle del juego
        while (enJuego && !glfwWindowShouldClose(window))
        {
            float currentFrame = static_cast<float>(glfwGetTime());
            float dt = std::min(currentFrame - lastFrame, 0.1f);
            lastFrame = currentFrame;

            SyncCameraToFramebuffer(window, camera);
            dayNight.Update(dt);
            UpdateGameplay(window, dayNight, headlightsOn, lightsPressed);

            // --- Shop Input Handling ---
            if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS && !shopKeyPressed) {
                g_shopUI->Toggle();
                shopKeyPressed = true;
            }
            if (glfwGetKey(window, GLFW_KEY_T) == GLFW_RELEASE) {
                shopKeyPressed = false;
            }
            // Close shop with ESC when visible
            if (g_shopUI->IsVisible() && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                g_shopUI->Hide();
            }
            
            // Auto-open shop if car is dead
            if (car.isDead && !g_shopUI->IsVisible()) {
                g_shopUI->Show();
                std::cout << "[GAME] Carro descompuesto! Abriendo tienda..." << std::endl;
            }

            bool braking = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && car.speed > 0.1f);
            bool accelerating = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
            float jumpDistanceBoost = accelerating ? 1.4f : 0.0f;

            HandleCarJumpAndRespawn(window, car, carVerticalSpeed, isOnGround, spawnPoint, jumpDistanceBoost, lastGroundHeight, &deliverySystem);
            ApplyGravity(car, carVerticalSpeed, isOnGround, dt);
            UpdateCar(window, car, dt, city);
            audioEngine.UpdateEngineRPM(car.speed,24.0f, dt);
            ResolveGroundCollision(car, city, carVerticalSpeed, isOnGround, lastGroundHeight);

            // Check water respawn and notify delivery system
            glm::vec3 oldPos = car.position;
            CheckWaterRespawn(car, city, carVerticalSpeed, isOnGround, lastGroundHeight);
            if (glm::length(car.position - oldPos) > 10.0f)
            {
                // Car was respawned (teleported), notify delivery system
                deliverySystem.OnWaterRespawn();
            }

            // Update delivery system with 'E' key state
            bool eKeyPressed = (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS);
            bool qKeyPressed = (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS);
            deliverySystem.Update(dt, car, eKeyPressed);
            OrderState currentState = deliverySystem.GetOrderState();

            // Music change for delivery or waiting for order, depending on status.
            if(currentState != lastOrderState)
            {
                switch(currentState)
                {
                    case OrderState::WAITING:
                    {
                        audioEngine.StopDeliveryMusic();
                        audioEngine.PlayAmbientMusic();
                        break;
                    }
                    case OrderState::PICKED_UP:
                    {
                        audioEngine.StopAmbientMusic();
                        audioEngine.PlayDeliveryMusic();
                        break;
                    }
                    case OrderState::DELIVERED:
                    {
                        audioEngine.StopDeliveryMusic();
                        audioEngine.PlayAmbientMusic();
                        break;
                    }
                    case OrderState::FAILED:
                    {
                        audioEngine.StopDeliveryMusic();
                        audioEngine.PlayAmbientMusic();
                        break;
                    }
                }
                lastOrderState = currentState;
            }

            // Suspense for the order, depending on the time or condition of the box.
            if(currentState == OrderState::PICKED_UP)
            {
                bool danger = false;

                float timeRemaining = deliverySystem.GetTimeRemaining();

                if(timeRemaining <= 10.0f)
                    danger = true;
                if(deliverySystem.GetFragileHealth() <= 20.0f)
                    danger = true;
                if(danger)
                    audioEngine.SetDeliveryPitch(1.25f);
                else
                    audioEngine.SetDeliveryPitch(1.0f);
            }

            // Handle mission rejection
           deliveryHUD.TryRejectMission(deliverySystem, qKeyPressed, car);

            UpdateFollowCamera(camera, car, dt, city);
            UpdateCameraEffects(window, camera, car);
            UpdateHeadlights(shaderProgram, car, headlightsOn);
            ApplyDayNightLighting(shaderProgram, dayNight);

            // Update camera frustum
            Frustum cameraFrustum;
            cameraFrustum.Update(camera.GetCameraMatrix());

            // --- 1. Render depth of scene to texture (from light's perspective) ---
            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);

            // Define light space matrix centered around the car
            glm::vec3 lightDir = glm::normalize(dayNight.GetLightPosition());
            glm::vec3 lightPos = car.position + lightDir * GameConstants::LIGHT_OFFSET_DISTANCE;
            glm::vec3 up = glm::abs(lightDir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            glm::mat4 lightView = glm::lookAt(lightPos, car.position, up);
            glm::mat4 lightProjection = glm::ortho(-GameConstants::ORTHO_SIZE, GameConstants::ORTHO_SIZE, -GameConstants::ORTHO_SIZE, GameConstants::ORTHO_SIZE, GameConstants::ORTHO_NEAR, GameConstants::ORTHO_FAR);
            glm::mat4 lightSpaceMatrix = lightProjection * lightView;

            // Stabilize directional shadow map: snap world space origin projection to shadow map texels
            glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            shadowOrigin = lightSpaceMatrix * shadowOrigin;

            float shadowMapX = shadowOrigin.x * (static_cast<float>(SHADOW_WIDTH) / 2.0f);
            float shadowMapY = shadowOrigin.y * (static_cast<float>(SHADOW_HEIGHT) / 2.0f);

            float dx = (std::round(shadowMapX) - shadowMapX) * (2.0f / static_cast<float>(SHADOW_WIDTH));
            float dy = (std::round(shadowMapY) - shadowMapY) * (2.0f / static_cast<float>(SHADOW_HEIGHT));

            lightProjection[3][0] += dx;
            lightProjection[3][1] += dy;
            lightSpaceMatrix = lightProjection * lightView;

            Frustum lightFrustum;
            lightFrustum.Update(lightSpaceMatrix);

            shadowShader.Activate();
            glUniformMatrix4fv(glGetUniformLocation(shadowShader.ID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(2.5f, 10.0f);

            city.Draw(shadowShader, camera, &lightFrustum);
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
            if (useSSAO)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, cameraDepthFBO);
                glViewport(0, 0, camera.width, camera.height);
                glClear(GL_DEPTH_BUFFER_BIT);

                shadowShader.Activate();
                camera.Matrix(shadowShader, "lightSpaceMatrix");

                city.Draw(shadowShader, camera, &cameraFrustum);
                carModel.Draw(shadowShader, camera, BuildCarMatrix(car), car.wheelSpin, car.steering, headlightsOn, braking);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

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
            glUniform1i(glGetUniformLocation(shaderProgram.ID, "uUseSSAO"), useSSAO ? 1 : 0);

            // --- Street Lamps: encender de noche ---
            float timeOfDay = dayNight.GetTime();
            bool isNight = (timeOfDay >= 18.0f || timeOfDay < 6.0f);

            shaderProgram.Activate();
            glUniform1i(glGetUniformLocation(shaderProgram.ID, "uStreetLightsOn"), isNight ? 1 : 0);

            // Encontrar las 8 mas cercanas al carro (usando std::nth_element para O(N))
            const int MAX_SL = 8;
            std::vector<std::pair<float, int>> byDist;
            byDist.reserve(lampPositions.size());
            for (int i = 0; i < (int)lampPositions.size(); ++i)
            {
                glm::vec3 diff = lampPositions[i] - car.position;
                diff.y = 0.0f;
                byDist.push_back({glm::dot(diff, diff), i});
            }

            int selectCount = std::min(MAX_SL, (int)byDist.size());
            if (selectCount > 0)
            {
                std::nth_element(byDist.begin(), byDist.begin() + selectCount - 1, byDist.end());
                std::sort(byDist.begin(), byDist.begin() + selectCount); // Sort only the closest 8
            }

            for (int i = 0; i < MAX_SL; ++i)
            {
                glm::vec3 lp = (i < selectCount)
                                   ? lampPositions[byDist[i].second] + glm::vec3(0.0f, 4.7f, 0.0f)
                                   : glm::vec3(0.0f, 0.0f, 0.0f);
                std::string uname = "uStreetLightPos[" + std::to_string(i) + "]";
                glUniform3f(glGetUniformLocation(shaderProgram.ID, uname.c_str()),
                            lp.x, lp.y, lp.z);
            }

            lampShader.Activate();
            glUniform1i(glGetUniformLocation(lampShader.ID, "uStreetLightsOn"), isNight ? 1 : 0);
            glUniform1f(glGetUniformLocation(lampShader.ID, "uAmbientStrength"), dayNight.GetAmbientStrength());
            glUniform4f(glGetUniformLocation(lampShader.ID, "lightColor"),
                        dayNight.GetLightColor().x, dayNight.GetLightColor().y, dayNight.GetLightColor().z, 1.0f);
            glUniform3f(glGetUniformLocation(lampShader.ID, "lightPos"),
                        dayNight.GetLightPosition().x, dayNight.GetLightPosition().y, dayNight.GetLightPosition().z);
            glm::vec3 fc = dayNight.GetHorizonColor();
            glUniform3f(glGetUniformLocation(lampShader.ID, "uFogColor"), fc.x, fc.y, fc.z);

            // Dibujar postes visibles (con test de distancia + frustum culling)
            for (const auto &pos : lampPositions)
            {
                glm::vec3 diff = pos - camera.Position;
                if (glm::dot(diff, diff) > 300.0f * 300.0f)
                    continue;
                if (!cameraFrustum.IsSphereVisible(pos + glm::vec3(0.0f, 2.5f, 0.0f), 3.5f))
                    continue;
                glm::mat4 lampMatrix = glm::translate(glm::mat4(1.0f), pos);
                lampMesh.Draw(lampShader, camera, lampMatrix);
            }

            city.Draw(shaderProgram, camera, &cameraFrustum);
            carModel.Draw(shaderProgram, camera, BuildCarMatrix(car), car.wheelSpin, car.steering, headlightsOn, braking);

            // Render delivery system
            deliverySystem.Render(shaderProgram, camera);

            DrawOcean(waterShader, ocean, camera, currentFrame, skyTint, dayNight);

            // Render delivery HUD (2D overlay)
            deliveryHUD.Render(deliverySystem, car, qKeyPressed);

            // Render shop UI (2D overlay)
            g_shopUI->Render();

            shaderProgram.Activate();

            static float fpsTime = 0.0f;
            static int fpsFrames = 0;
            static float currentFps = 0.0f;
            fpsTime += dt;
            fpsFrames++;
            if (fpsTime >= 1.0f)
            {
                currentFps = static_cast<float>(fpsFrames) / fpsTime;
                fpsTime = 0.0f;
                fpsFrames = 0;
            }

            int hours = static_cast<int>(timeOfDay);
            int minutes = static_cast<int>((timeOfDay - hours) * 60.0f);
            char title[256];
            std::snprintf(title, sizeof(title),
                          "Mini Delivery Dash | FPS: %.0f | Time: %02d:%02d | Speed: %.1f km/h | Pos: (%.2f, %.2f, %.2f)",
                          currentFps, hours, minutes,
                          std::abs(car.speed) * 3.6f,
                          car.position.x, car.position.y, car.position.z);
            glfwSetWindowTitle(window, title);

            glfwSwapBuffers(window);
            glfwPollEvents();

            // ----- PAUSA CON ESC -----
            static bool escWasPressed = false;
            bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            // Only pause if shop is not visible
            if (escPressed && !escWasPressed && !g_shopUI->IsVisible())
            {
                // Capture the current frame buffer contents into a texture to display behind the pause menu
                GLuint pauseTex = 0;
                glGenTextures(1, &pauseTex);
                glBindTexture(GL_TEXTURE_2D, pauseTex);
                glReadBuffer(GL_FRONT);
                glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, fbW, fbH, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                MainMenu pauseMenu(window, fbW, fbH);
                pauseMenu.SetPauseBackground(pauseTex);
                MainMenu::Result pauseResult = pauseMenu.Show(true);

                glDeleteTextures(1, &pauseTex);

                if (pauseResult == MainMenu::Result::Quit)
                {
                    audioEngine.StopEngine();
                    audioEngine.StopDeliveryMusic();
                    audioEngine.StopAmbientMusic();
                    enJuego = false;
                }

                // Restore OpenGL state after the pause menu loop
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_MULTISAMPLE);
                glDepthMask(GL_TRUE);
                shaderProgram.Activate();
                lastFrame = static_cast<float>(glfwGetTime());
            }
            escWasPressed = escPressed;

            // --- F8: guardar posición de poste ---
            static bool f8WasPressed = false;
            bool f8Pressed = glfwGetKey(window, GLFW_KEY_F8) == GLFW_PRESS;
            if (f8Pressed && !f8WasPressed)
            {
                lampPositions.push_back(car.position);
                std::ofstream lf("res/lamp_positions.txt", std::ios::app);
                lf << car.position.x << " " << car.position.y << " " << car.position.z << "\n";
                std::cout << "[LAMP] Agregado poste #" << lampPositions.size()
                          << " en (" << car.position.x << ", " << car.position.y
                          << ", " << car.position.z << ")" << std::endl;
            }
            f8WasPressed = f8Pressed;

            // --- F9: borrar el último poste ---
            static bool f9WasPressed = false;
            bool f9Pressed = glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS;
            if (f9Pressed && !f9WasPressed)
            {
                if (!lampPositions.empty())
                {
                    lampPositions.pop_back();
                    // Reescribir el archivo completo sin el último
                    std::ofstream lf("res/lamp_positions.txt", std::ios::trunc);
                    for (const auto &p : lampPositions)
                        lf << p.x << " " << p.y << " " << p.z << "\n";
                    std::cout << "[LAMP] Borrado último poste. Quedan: "
                              << lampPositions.size() << std::endl;
                }
                else
                {
                    std::cout << "[LAMP] No hay postes que borrar." << std::endl;
                }
            }
            f9WasPressed = f9Pressed;

            // --- O: toggle SSAO ---
            static bool oWasPressed = false;
            bool oPressed = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
            if (oPressed && !oWasPressed)
            {
                useSSAO = !useSSAO;
                std::cout << "[GAME] SSAO " << (useSSAO ? "ENABLED" : "DISABLED") << std::endl;
            }
            oWasPressed = oPressed;
        }

        // Limpieza de recursos al salir del gameplay
        audioEngine.StopEngine();
        audioEngine.StopAmbientMusic();
        audioEngine.StopDeliveryMusic();
        audioEngine.StopMenuMusic();
        shaderProgram.Delete();
        skyShader.Delete();
        waterShader.Delete();
        shadowShader.Delete();
        lampShader.Delete();

        glDeleteFramebuffers(1, &depthMapFBO);
        glDeleteTextures(1, &depthMap);
        glDeleteFramebuffers(1, &cameraDepthFBO);
        glDeleteTextures(1, &cameraDepthMap);
        
        // Shop cleanup
        if (g_shopUI) {
            ShopManager::GetInstance()->SaveShopData();
            delete g_shopUI;
            g_shopUI = nullptr;
        }
    }

    // Limpieza final
    ShopManager::Destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}