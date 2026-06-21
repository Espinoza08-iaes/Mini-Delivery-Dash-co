#ifndef CAR_CONTROLLER_H
#define CAR_CONTROLLER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "../city/City.h"

// Forward declaration
class DeliverySystem;

// Car const
constexpr float kCarModelScale = 0.21875f;
constexpr float kCarGroundYOffset = 0.0f;
const float kGroundClearance = 0.01f;
const float kMaxDownSnap = 0.40f;

struct CarState
{
    glm::vec3 position = glm::vec3(-47.64f, 1.76f, 56.14f);

    float yaw = 0.0f;

    float speed = 0.0f;
    float steering = 0.0f;

    float wheelSpin = 0.0f;

    float pitch = 0.0f;
    float roll = 0.0f;
    
    // Durability system
    float durability = 100.0f;  // 0-100%
    bool isDead = false;
};

glm::mat4 BuildCarMatrix(const CarState& car);

void UpdateCar(GLFWwindow *window, CarState &car, float dt, const City &city, int tireMode = 0, bool isRaining = false);

void HandleCarJumpAndRespawn(
    GLFWwindow* window,
    CarState& car,
    float& carVerticalSpeed,
    bool& isOnGround,
    glm::vec3 spawnPoint,
    float jumpDistanceBoost,
    float& lastGroundHeight,
    DeliverySystem* deliverySystem);

#endif