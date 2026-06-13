#ifndef DELIVERY_HUD_H
#define DELIVERY_HUD_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../vehicle/CarController.h"
#include "DeliverySystem.h"

class DeliveryHUD
{
public:
    DeliveryHUD(GLFWwindow* window, int screenWidth, int screenHeight);
    ~DeliveryHUD();
    
    void Render(const DeliverySystem& deliverySystem, const CarState& car, bool qKeyPressed);
    bool ShouldShowMissionPanel(const DeliverySystem& deliverySystem, const CarState& car) const;
    bool TryRejectMission(DeliverySystem& deliverySystem, bool qKeyPressed) const;
    
private:
    void SetupGraphics();
    void SetupTextRendering();
    void RenderCompassBar(float centerX, float centerY, float carYaw, const glm::vec3& objective, const glm::vec3& carPos, bool isPickup);
    void RenderMissionPanel(const DeliverySystem& deliverySystem);
    void RenderDeliveryHUD(const DeliverySystem& deliverySystem);
    void RenderHealthBar(float x, float y, float w, float h, float health);
    void RenderText(const char* text, float x, float y, float r, float g, float b, float scale = 1.0f);
    void RenderTextCentered(const char* text, float cx, float y, float r, float g, float b, float scale = 1.0f);
    void RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
    
    GLFWwindow* window;
    int width, height;
    
    // OpenGL resources
    unsigned int quadVAO;
    unsigned int quadVBO;
    unsigned int hudProgram;
    unsigned int textVAO;
    unsigned int textVBO;
    unsigned int textProgram;
};

#endif
