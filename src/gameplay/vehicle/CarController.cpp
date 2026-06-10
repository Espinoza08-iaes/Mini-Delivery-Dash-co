#include "CarController.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

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
// Car update  (city passed in so world-clamp uses its scale)
// ---------------------------------------------------------------------------
void UpdateCar(GLFWwindow *window, CarState &car, float dt, const City &city)
{
    bool isBoosting = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && car.speed > 0.0f);
    const float acceleration     = isBoosting ? 25.0f : 9.0f;
    const float brakePower       = 16.0f;
    const float maxForwardSpeed  = isBoosting ? 24.0f : 11.0f;
    const float maxReverseSpeed  = 5.0f;
    const float friction         = 5.0f;
    const float steeringResponse = isBoosting ? 3.5f : 5.5f;
    const float steeringReturn   = 9.0f;
    const float maxSteering      = glm::radians(35.0f);
    const float turnRate         = glm::radians(125.0f);
    const float wheelRadius      = 0.38f * kCarModelScale;

    // --- Throttle ---
    float throttle = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) throttle += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) throttle -= 1.0f;

    // --- Steering ---
    float steerInput = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) steerInput += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) steerInput -= 1.0f;

    // --- Speed ---
    if (throttle > 0.0f)
        car.speed += acceleration * dt;
    else if (throttle < 0.0f)
        car.speed -= brakePower * dt;
    else
    {
        if (car.speed > 0.0f) car.speed = std::max(0.0f, car.speed - friction * dt);
        if (car.speed < 0.0f) car.speed = std::min(0.0f, car.speed + friction * dt);
    }
    car.speed = glm::clamp(car.speed, -maxReverseSpeed, maxForwardSpeed);

    // --- Steering ---
    if (steerInput != 0.0f)
        car.steering += steerInput * steeringResponse * dt;
    else
    {
        if (car.steering > 0.0f) car.steering = std::max(0.0f, car.steering - steeringReturn * dt);
        if (car.steering < 0.0f) car.steering = std::min(0.0f, car.steering + steeringReturn * dt);
    }
    car.steering = glm::clamp(car.steering, -maxSteering, maxSteering);

    // --- Rotation ---
    float turnFactor = 0.0f;
    if (std::abs(car.speed) > 0.01f)
    {
        float speedRatio = std::abs(car.speed) / maxForwardSpeed;
        float rawFactor  = 0.35f + 0.65f * speedRatio;
        if (car.speed < 0.0f)
        {
            speedRatio = std::abs(car.speed) / maxReverseSpeed;
            rawFactor  = 0.35f + 0.65f * speedRatio;
            turnFactor = -rawFactor;
        }
        else
        {
            turnFactor = rawFactor;
        }
    }
    car.yaw += car.steering * turnFactor * turnRate * dt;

    // --- Movement & Obstacle Collisions ---
    glm::vec3 forward      = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
    glm::vec3 right        = glm::vec3(std::cos(car.yaw), 0.0f, -std::sin(car.yaw));
    glm::vec3 prevPosition = car.position;
    glm::vec3 fullMove     = forward * car.speed * dt;
    glm::vec3 nextPosition = car.position + fullMove;

    const float carCollisionRadius = kCarModelScale * 1.3f;
    const float rearOffset         = kCarModelScale * 1.1f;

    // Two-sphere collision: center + rear bumper
    auto collidesAt = [&](const glm::vec3 &pos) -> bool
    {
        if (city.CheckCollision(pos, carCollisionRadius))
            return true;
        glm::vec3 rearPos = pos - forward * rearOffset;
        if (city.CheckCollision(rearPos, carCollisionRadius))
            return true;
        return false;
    };

    if (collidesAt(nextPosition))
    {
        glm::vec3 tryX = glm::vec3(nextPosition.x, prevPosition.y, prevPosition.z);
        glm::vec3 tryZ = glm::vec3(prevPosition.x, prevPosition.y, nextPosition.z);

        bool xOk = !collidesAt(tryX);
        bool zOk = !collidesAt(tryZ);

        if      ( xOk && !zOk) nextPosition = tryX;
        else if ( zOk && !xOk) nextPosition = tryZ;
        else if ( xOk &&  zOk) nextPosition = tryX;
        else
        {
            car.speed    = 0.0f;
            nextPosition = prevPosition;
        }
    }
    car.position = nextPosition;

    // ========================================================================
    // CAR TILT (PITCH AND ROLL)
    // ========================================================================
    const float wheelBase    = 1.0f;
    const float trackWidth   = 0.7f;
    const float tiltSmooth   = 6.0f;
    const float heightSmooth = 8.0f;

    glm::vec3 fwd = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));

    glm::vec3 frontPos = car.position + fwd   * wheelBase;
    glm::vec3 rearPos  = car.position - fwd   * wheelBase;
    glm::vec3 leftPos  = car.position + right * trackWidth;
    glm::vec3 rightPos = car.position - right * trackWidth;

    game::GroundSample sample;
    auto sampleH = [&](const glm::vec3 &pos) -> float
    {
        if (city.GetGroundSample(pos, car.position.y, sample, 3.0f, 0.5f) && sample.found)
            return sample.height;
        return car.position.y - kGroundClearance;
    };

    float frontH = sampleH(frontPos);
    float rearH  = sampleH(rearPos);
    float leftH  = sampleH(leftPos);
    float rightH = sampleH(rightPos);

    // --- Pitch ---
    float pitchDiff  = frontH - rearH;
    float targetPitch = -std::atan2(pitchDiff, wheelBase * 2.0f);
    const float MAX_PITCH = glm::radians(20.0f);
    targetPitch = glm::clamp(targetPitch, -MAX_PITCH, MAX_PITCH);

    // --- Roll ---
    float rollDiff  = leftH - rightH;
    float targetRoll = -std::atan2(rollDiff, trackWidth * 2.0f);
    const float MAX_ROLL = glm::radians(6.0f);
    targetRoll = glm::clamp(targetRoll, -MAX_ROLL, MAX_ROLL);

    // --- Smooth ---
    car.pitch = glm::mix(car.pitch, targetPitch, tiltSmooth   * dt);
    car.roll  = glm::mix(car.roll,  targetRoll,  tiltSmooth   * dt);

    // --- Height ---
    float avgHeight    = (frontH + rearH + leftH + rightH) * 0.25f;
    float desiredHeight = avgHeight + kGroundClearance;
    car.position.y = glm::mix(car.position.y, desiredHeight, heightSmooth * dt);

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