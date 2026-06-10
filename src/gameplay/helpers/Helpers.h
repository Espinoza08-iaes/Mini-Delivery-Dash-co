#ifndef HELPERS_H
#define HELPERS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../vehicle/CarController.h"
#include "../city/City.h"
#include "../city/CityPhysics.h"
#include "../scene/DayNightCycle.h"

class Camera;
class Shader;

// ------------------------------------------------
// Window Helpers
// ------------------------------------------------

bool InitializeWindow (GLFWwindow*& window, int& framebuferWidht, int&framebufferHeight);

void GetFramebufferSize (GLFWwindow* window, int& framebufferWidth, int&framebufferHeight);

void SyncCameraToFramebuffer (GLFWwindow* window, Camera& camera);

void SetupOpenGL (Shader& shaderProgram);

// -------------------------------------------------
// Physics Helpers
// -------------------------------------------------

void ApplyGravity (CarState& car, float& verticalSpeed, bool isOnGround, float dt);

void CheckWaterRespawn (CarState& car, City& city, float& verticalSpeed, bool& isOnGround, float& lastGroundHeight);

// --------------------------------------------------
// Camera
// --------------------------------------------------

void UpdateFollowCamera (Camera& camera, const CarState& car, float dt);

void UpdateOrbitCamera (GLFWwindow* window);

// --------------------------------------------------
// Gameplay Helpers
// --------------------------------------------------

void UpdateGameplay (GLFWwindow* window, DayNightCycle& dayNight, bool& headlightsOn, bool& lightsPressed);

#endif