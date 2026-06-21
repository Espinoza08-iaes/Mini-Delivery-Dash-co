#include "CarController.h"
#include "../shop/ShopManager.h"
#include "../delivery/DeliverySystem.h"

#include <algorithm>
#include <cmath>
#include <iostream>

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
void UpdateCar(GLFWwindow *window, CarState &car, float dt, const City &city, int tireMode, bool isRaining)
{
    // Get shop upgrades
    ShopManager* shop = ShopManager::GetInstance();
    float speedMult = shop->GetUpgradeMultiplier(UpgradeType::Speed);
    float accelMult = shop->GetUpgradeMultiplier(UpgradeType::Acceleration);
    float handlingMult = shop->GetUpgradeMultiplier(UpgradeType::Handling);
    float durabilityMult = shop->GetUpgradeMultiplier(UpgradeType::FuelEfficiency); // Usar FuelEfficiency para Durabilidad
    
    // Check if turbo ability is unlocked
    bool canUseTurbo = shop->IsAbilityUnlocked(AbilityType::Turbo);
    bool turboKeyPressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && car.speed > 0.0f);
    bool isBoosting = canUseTurbo && turboKeyPressed;
    
    // Debug: Print when trying to use turbo without unlock
    static bool lastTurboKeyPressed = false;
    if (turboKeyPressed && !canUseTurbo && !lastTurboKeyPressed) {
        std::cout << "[DEBUG] Turbo bloqueado - necesitas comprarlo primero!" << std::endl;
    }
    lastTurboKeyPressed = turboKeyPressed;
    
    // Si el carro está muerto, no puede moverse
    if (car.isDead) {
        car.speed = 0.0f;
        car.steering = 0.0f;
        return;
    }
    
    // Base values (reduced for balance)
    const float baseAcceleration = 6.0f;
    const float baseBrakePower = 12.0f;
    const float baseMaxForwardSpeed = 8.0f;
    const float baseMaxReverseSpeed = 4.0f;
    const float baseFriction = 4.0f;
    const float baseSteeringResponse = 4.0f;
    const float baseSteeringReturn = 8.0f;
    
    // Apply upgrades
    float acceleration = baseAcceleration * accelMult;
    float brakePower = baseBrakePower;
    float maxForwardSpeed = baseMaxForwardSpeed * speedMult;
    float maxReverseSpeed = baseMaxReverseSpeed;
    float friction = baseFriction;
    float steeringResponse = baseSteeringResponse * handlingMult;
    float steeringReturn = baseSteeringReturn;
    float maxSteering = glm::radians(35.0f);
    float turnRate = glm::radians(125.0f) * handlingMult;
    float wheelRadius = 0.38f * kCarModelScale;

    // Apply Tires logic
    if (tireMode == 1) { // Grip
        friction *= 1.5f;
        turnRate *= 1.2f;
        steeringResponse *= 1.3f;
    } else if (tireMode == -1) { // Drift
        friction *= 0.4f;
        turnRate *= 1.8f;
        steeringReturn *= 0.5f;
    }

    // Apply Rain logic
    if (isRaining) {
        friction *= 0.6f;
        brakePower *= 0.7f; // 30% worse braking
        steeringResponse *= 0.8f; // Harder to steer
    }

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
    car.speed = glm::clamp(car.speed, -maxReverseSpeed, isBoosting ? maxForwardSpeed * 2.0f : maxForwardSpeed);

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
    float turnFactor = 0.0f;
    if (std::abs(car.speed) > 0.01f)
    {
        float speedRatio = std::abs(car.speed) / (isBoosting ? maxForwardSpeed * 2.0f : maxForwardSpeed);
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

    // Collision detection with a 3-sphere system (center, front, rear) for better coverage, especially at higher speeds
    const float carCollisionRadius = kCarModelScale * 1.3f;
    const float frontOffset = kCarModelScale * 1.1f;
    const float rearOffset = kCarModelScale * 1.1f;

    // System to check collisions at a given position using multiple spheres (center, front, rear)
    auto collidesAt = [&](const glm::vec3 &pos) -> bool
    {
        // Center sphere
        if (city.CheckCollision(pos, carCollisionRadius))
            return true;

        // Front sphere
        glm::vec3 frontPos = pos + forward * frontOffset;
        if (city.CheckCollision(frontPos, carCollisionRadius))
            return true;

        // Back sphere
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

        if (xOk && !zOk)
            nextPosition = tryX;
        else if (zOk && !xOk)
            nextPosition = tryZ;
        else if (xOk && zOk)
            nextPosition = tryX;
        else
        {
            float impactSpeed = std::abs(car.speed);
            car.speed = 0.0f;
            nextPosition = prevPosition;
            
            if (impactSpeed > 4.0f)
            {
                float baseDamage = 4.0f + std::max(0.0f, std::floor((impactSpeed - 4.0f) / 4.0f));
                float collisionDamage = baseDamage / durabilityMult;
                car.durability -= collisionDamage;
                std::cout << "[CAR] Colisión a " << impactSpeed << " m/s! Daño base: " << baseDamage << " → -" << collisionDamage << "% durabilidad" << std::endl;
            }
        }
    }
    car.position = nextPosition;

    // ========================================================================
    // CAR TILT (PITCH AND ROLL)
    // ========================================================================
    const float wheelBase = 1.0f;
    const float trackWidth = 0.7f;
    const float tiltSmooth = 6.0f;
    const float heightSmooth = 8.0f;

    glm::vec3 fwd = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));

    glm::vec3 frontPos = car.position + fwd * wheelBase;
    glm::vec3 rearPos = car.position - fwd * wheelBase;
    glm::vec3 leftPos = car.position + right * trackWidth;
    glm::vec3 rightPos = car.position - right * trackWidth;

    game::GroundSample sample;
    auto sampleH = [&](const glm::vec3 &pos) -> float
    {
        if (city.GetGroundSample(pos, car.position.y, sample, 3.0f, 0.5f) && sample.found)
            return sample.height;
        return car.position.y - kGroundClearance;
    };

    float frontH = sampleH(frontPos);
    float rearH = sampleH(rearPos);
    float leftH = sampleH(leftPos);
    float rightH = sampleH(rightPos);

    // --- Pitch ---
    float pitchDiff = frontH - rearH;
    float targetPitch = -std::atan2(pitchDiff, wheelBase * 2.0f);
    const float MAX_PITCH = glm::radians(20.0f);
    targetPitch = glm::clamp(targetPitch, -MAX_PITCH, MAX_PITCH);

    // --- Roll ---
    float rollDiff = leftH - rightH;
    float targetRoll = -std::atan2(rollDiff, trackWidth * 2.0f);
    const float MAX_ROLL = glm::radians(6.0f);
    targetRoll = glm::clamp(targetRoll, -MAX_ROLL, MAX_ROLL);

    // --- Smooth ---
    car.pitch = glm::mix(car.pitch, targetPitch, tiltSmooth * dt);
    car.roll = glm::mix(car.roll, targetRoll, tiltSmooth * dt);

    // --- Height ---
    float avgHeight = (frontH + rearH + leftH + rightH) * 0.25f;
    float desiredHeight = avgHeight + kGroundClearance;
    car.position.y = glm::mix(car.position.y, desiredHeight, heightSmooth * dt);

    // --- Wheel spin ---
    car.wheelSpin += (car.speed * dt) / wheelRadius;
    
    // --- Durability degradation ---
    // Desgaste por conducir (muy lento)
    if (std::abs(car.speed) > 0.1f) {
        // Desgaste base: 0.5% por minuto conduciendo
        // Con durabilidad Nv.5 (x2.0): 0.25% por minuto
        float degradationRate = 0.5f / 60.0f; // 0.5% por minuto = 0.008% por segundo
        float actualDegradation = degradationRate / durabilityMult; // Dividir para hacer que dure MÁS
        car.durability -= actualDegradation * dt;
    }
    
    // Clamp durability
    car.durability = glm::clamp(car.durability, 0.0f, 100.0f);
    
    // Check if car is dead
    if (car.durability <= 0.0f && !car.isDead) {
        car.isDead = true;
        car.speed = 0.0f;
        std::cout << "[CAR] Carro descompuesto! Necesitas repararlo en la tienda." << std::endl;
    }
}
// --- Car jump and respawn ---
void HandleCarJumpAndRespawn(GLFWwindow *window, CarState &car, float &carVerticalSpeed, bool &isOnGround, glm::vec3 spawnPoint, float jumpDistanceBoost, float &lastGroundHeight, DeliverySystem* deliverySystem)
{
    static bool zPressedLast = false;
    static bool rPressedLast = false;
    static bool oneKeyPressedLast = false;

    // Get shop abilities
    ShopManager* shop = ShopManager::GetInstance();
    bool canJump = shop->IsAbilityUnlocked(AbilityType::Jump);
    bool canTeleport = shop->IsAbilityUnlocked(AbilityType::Teleport);

    // --- Jump (only if ability is unlocked) ---
    bool zPressed = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
    bool wPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    
    if (zPressed && !zPressedLast && isOnGround)
    {
        if (canJump)
        {
            // Salto vertical
            carVerticalSpeed = 3.5f;
            isOnGround = false;
            
            // Si también presiona W, salto hacia adelante
            if (wPressed)
            {
                glm::vec3 forward = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
                car.position += forward * 1.5f;  // Impulso horizontal
                std::cout << "[DEBUG] Salto hacia adelante activado!" << std::endl;
            }
            else
            {
                std::cout << "[DEBUG] Salto vertical activado!" << std::endl;
            }
        }
        else
        {
            std::cout << "[DEBUG] Salto bloqueado - necesitas comprarlo primero!" << std::endl;
        }
    }
    zPressedLast = zPressed;
    
    // --- Teleport (Tecla 1) - Solo funciona si NO hay misión activa ---
    bool oneKeyPressed = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    if (oneKeyPressed && !oneKeyPressedLast)
    {
        if (canTeleport)
        {
            // Solo funciona si hay una misión esperando (no activa)
            if (deliverySystem && deliverySystem->HasWaitingOrder())
            {
                glm::vec3 pickupPos = deliverySystem->GetWaitingOrder().originPosition;
                car.position = pickupPos + glm::vec3(0.0f, 2.0f, 0.0f); // Spawn arriba del pickup
                car.speed = 0.0f;
                std::cout << "[DEBUG] Teletransporte a pickup activado!" << std::endl;
            }
            else
            {
                std::cout << "[DEBUG] Teletransporte: No hay misión esperando o ya tienes una activa" << std::endl;
            }
        }
        else
        {
            std::cout << "[DEBUG] Teletransporte bloqueado - necesitas comprarlo primero!" << std::endl;
        }
    }
    oneKeyPressedLast = oneKeyPressed;

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