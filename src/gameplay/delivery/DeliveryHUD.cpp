#include "DeliveryHUD.h"
#include <iostream>
#include <cstring>
#include <cmath>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../../../third_party/stb/stb_easy_font.h"

// Shader compilation helper
static unsigned int compileShader(const char* vsSrc, const char* fsSrc)
{
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, NULL); glCompileShader(vs);
    int ok; glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { char info[512]; glGetShaderInfoLog(vs, 512, NULL, info); std::cerr << "VS: " << info << std::endl; }

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, NULL); glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { char info[512]; glGetShaderInfoLog(fs, 512, NULL, info); std::cerr << "FS: " << info << std::endl; }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs); glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

// Convert quads to triangles helper
static void convertQuadsToTrianglesClean(float* quads, int num, float* tris)
{
    for (int i = 0; i < num; i++)
    {
        float* q = quads + i * 16;
        memcpy(tris + i * 24 + 0,  q,      12 * sizeof(float));
        memcpy(tris + i * 24 + 12, q,       4 * sizeof(float));
        memcpy(tris + i * 24 + 16, q + 8,   4 * sizeof(float));
        memcpy(tris + i * 24 + 20, q + 12,  4 * sizeof(float));
    }
}

// Get text width helper
static float GetTextWidth(const char* text, float scale)
{
    float buf[60000];
    int n = stb_easy_font_print(0, 0, (char*)text, NULL, buf, sizeof(buf));
    if (n <= 0) return 0;
    float mn = buf[0], mx = buf[0];
    for (int i = 0; i < n; i++)
    {
        float* q = buf + i * 16;
        for (int j = 0; j < 4; j++) { float x = q[j * 4]; if (x < mn) mn = x; if (x > mx) mx = x; }
    }
    return (mx - mn) * scale;
}

DeliveryHUD::DeliveryHUD(GLFWwindow* w, int sw, int sh)
    : window(w), width(sw), height(sh),
      quadVAO(0), quadVBO(0), hudProgram(0),
      textVAO(0), textVBO(0), textProgram(0)
{
    SetupGraphics();
    SetupTextRendering();
}

DeliveryHUD::~DeliveryHUD()
{
    glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &textVAO); glDeleteBuffers(1, &textVBO);
    if (hudProgram) glDeleteProgram(hudProgram);
    if (textProgram) glDeleteProgram(textProgram);
}

void DeliveryHUD::SetupGraphics()
{
    const char* vs = R"(#version 330 core
        layout(location=0) in vec2 aPos; layout(location=1) in vec2 aTex;
        out vec2 Tex; uniform mat4 proj, model;
        void main(){ gl_Position = proj * model * vec4(aPos,0,1); Tex = aTex; })";
    const char* fs = R"(#version 330 core
        out vec4 frag; in vec2 Tex;
        uniform sampler2D uTex; uniform int uSolid; uniform vec4 uColor;
        void main(){ frag = uSolid==1 ? uColor : texture(uTex,Tex); })";
    hudProgram = compileShader(vs, fs);

    float quad[] = { 0,0,0,1, 0,1,0,0, 1,1,1,0, 0,0,0,1, 1,1,1,0, 1,0,1,1 };
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);
}

void DeliveryHUD::SetupTextRendering()
{
    const char* vs = R"(#version 330 core
        layout(location=0) in vec2 aPos; uniform mat4 proj, model;
        void main(){ gl_Position = proj * model * vec4(aPos,0,1); })";
    const char* fs = R"(#version 330 core
        out vec4 frag; uniform vec4 uColor; void main(){ frag = uColor; })";
    textProgram = compileShader(vs, fs);
    glGenVertexArrays(1, &textVAO); glGenBuffers(1, &textVBO);
}

void DeliveryHUD::Render(const DeliverySystem& deliverySystem, const CarState& car, bool qKeyPressed)
{
    // Only render if there's an active order or waiting order or just delivered
    if (!deliverySystem.HasWaitingOrder() && !deliverySystem.HasActiveOrder() && deliverySystem.GetCurrentOrder().state != OrderState::DELIVERED)
        return;
    
    // --- MAGIC FOR 2D UI ---
    glDisable(GL_DEPTH_TEST); // Disable 3D depth so UI draws on top of everything
    glEnable(GL_BLEND);       // Enable Alpha channel for transparency
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    // Render wallet balance at top right
    char walletText[64];
    snprintf(walletText, sizeof(walletText), "Dinero: $%.0f", deliverySystem.GetWalletBalance());
    RenderText(walletText, fbW - 250.0f, 30.0f, 1.0f, 0.8f, 0.0f, 3.0f);
    
    // Render compass bar
    glm::vec3 objective = deliverySystem.GetObjectivePosition();
    bool isPickup = deliverySystem.HasWaitingOrder();
    
    // No additional markers - only show current mission objective
    std::vector<CompassMarker>* additionalMarkers = nullptr;
    
    RenderCompassBar(fbW / 2.0f, 60.0f, car.yaw, objective, car.position, isPickup, additionalMarkers);
    
    // Render distance below compass (always to current objective)
    float distance = deliverySystem.GetDistanceToObjective(car);
    
    char distanceText[64];
    snprintf(distanceText, sizeof(distanceText), "%.1f m", distance);
    RenderTextCentered(distanceText, fbW / 2.0f, 110.0f, 1.0f, 1.0f, 1.0f, 3.0f);
    
    // Render success screen if delivered
    if (deliverySystem.GetCurrentOrder().state == OrderState::DELIVERED)
    {
        const DeliveryOrder& order = deliverySystem.GetCurrentOrder();
        
        // Green classic background and larger panel
        RenderColoredQuad(fbW / 2.0f - 350.0f, fbH / 2.0f - 250.0f, 700.0f, 500.0f, 0.1f, 0.4f, 0.1f, 0.95f);
        
        RenderTextCentered("¡ENTREGA EXITOSA!", fbW / 2.0f, fbH / 2.0f - 200.0f, 0.2f, 1.0f, 0.2f, 3.0f);
        
        float yOffset = fbH / 2.0f - 130.0f;
        char buf[128];
        
        // Convert Enum to Text
        const char* diffStr = (deliverySystem.GetCurrentOrder().difficulty == OrderDifficulty::EASY) ? "Facil" : 
                              (deliverySystem.GetCurrentOrder().difficulty == OrderDifficulty::MEDIUM) ? "Medio" : "Dificil";
        
        const char* typeStr = (deliverySystem.GetCurrentOrder().type == OrderType::STANDARD) ? "Estandar" : 
                              (deliverySystem.GetCurrentOrder().type == OrderType::FRAGILE) ? "Fragil" : "Especial";
        
        snprintf(buf, sizeof(buf), "Dificultad: %s  |  Tipo: %s", diffStr, typeStr);
        RenderTextCentered(buf, fbW / 2.0f, yOffset, 1.0f, 1.0f, 1.0f, 1.5f);
        yOffset += 30.0f;
        
        snprintf(buf, sizeof(buf), "Tiempo de Entrega: %.1f seg", deliverySystem.finalElapsedTime);
        RenderTextCentered(buf, fbW / 2.0f, yOffset, 1.0f, 1.0f, 1.0f, 1.5f);
        yOffset += 50.0f;
        
        // Breakdown
        snprintf(buf, sizeof(buf), "Pago Base: $%.0f", deliverySystem.GetInitialReward());
        RenderText(buf, fbW / 2.0f - 250.0f, yOffset, 1.0f, 1.0f, 1.0f, 1.5f);
        yOffset += 35.0f;
        
        if (deliverySystem.finalBonusAmount > 0.0f) {
            snprintf(buf, sizeof(buf), "+ Bono Estrellas: $%.0f", deliverySystem.finalBonusAmount);
            RenderText(buf, fbW / 2.0f - 250.0f, yOffset, 1.0f, 0.8f, 0.0f, 1.5f);
            yOffset += 35.0f;
        }
        if (deliverySystem.finalLossCollision > 0.0f) {
            snprintf(buf, sizeof(buf), "- Choques: $%.0f", deliverySystem.finalLossCollision);
            RenderText(buf, fbW / 2.0f - 250.0f, yOffset, 1.0f, 0.3f, 0.3f, 1.5f);
            yOffset += 35.0f;
        }
        if (deliverySystem.finalLossWater > 0.0f) {
            snprintf(buf, sizeof(buf), "- Caidas al Agua: $%.0f", deliverySystem.finalLossWater);
            RenderText(buf, fbW / 2.0f - 250.0f, yOffset, 0.3f, 0.7f, 1.0f, 1.5f);
            yOffset += 35.0f;
        }
        if (deliverySystem.finalLossTime > 0.0f) {
            snprintf(buf, sizeof(buf), "- Tardanza: $%.0f", deliverySystem.finalLossTime);
            RenderText(buf, fbW / 2.0f - 250.0f, yOffset, 1.0f, 0.5f, 0.2f, 1.5f);
            yOffset += 35.0f;
        }
        
        // Total final
        yOffset += 20.0f;
        snprintf(buf, sizeof(buf), "GANANCIA TOTAL: $%.0f", deliverySystem.GetCurrentOrder().reward);
        RenderTextCentered(buf, fbW / 2.0f, yOffset, 0.2f, 1.0f, 0.2f, 2.5f);
        
        // Instruction to dismiss
        RenderTextCentered("[E] Aceptar", fbW / 2.0f, fbH / 2.0f + 200.0f, 0.8f, 0.8f, 0.8f, 1.8f);
    }
    else if (deliverySystem.HasWaitingOrder() && ShouldShowMissionPanel(deliverySystem, car))
    {
        RenderMissionPanel(deliverySystem);
    }
    else if (deliverySystem.HasActiveOrder())
    {
        RenderDeliveryHUD(deliverySystem);
    }
    
    // Show delivery message when near delivery pillar
    if (ShouldShowDeliveryMessage(deliverySystem, car))
    {
        RenderTextCentered("Entregar con E o cancelar con Q", fbW / 2.0f, fbH / 2.0f + 50.0f, 1.0f, 1.0f, 1.0f, 2.5f);
    }
    
    // Re-enable depth test for 3D rendering
    glEnable(GL_DEPTH_TEST);
}

void DeliveryHUD::RenderCompassBar(float centerX, float centerY, float carYaw, const glm::vec3& objective, const glm::vec3& carPos, bool isPickup, const std::vector<CompassMarker>* additionalMarkers)
{
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    // Draw compass bar background with Kingdom Come style
    float barWidth = 600.0f;
    float barHeight = 40.0f;
    
    // Main background - lighter, cleaner color
    RenderColoredQuad(centerX - barWidth / 2.0f, centerY - barHeight / 2.0f, barWidth, barHeight, 0.15f, 0.15f, 0.2f, 0.95f);
    
    // Top and bottom borders
    RenderColoredQuad(centerX - barWidth / 2.0f, centerY - barHeight / 2.0f, barWidth, 2.0f, 0.4f, 0.4f, 0.5f, 1.0f);
    RenderColoredQuad(centerX - barWidth / 2.0f, centerY + barHeight / 2.0f - 2.0f, barWidth, 2.0f, 0.4f, 0.4f, 0.5f, 1.0f);
    
    // Center indicator (car direction) - more visible
    RenderColoredQuad(centerX - 3.0f, centerY - barHeight / 2.0f - 5.0f, 6.0f, barHeight + 10.0f, 1.0f, 0.85f, 0.0f, 1.0f);
    
    // Draw cardinal marks that move based on car rotation
    const char* cardinals[] = {"N", "E", "S", "O"};
    float cardinalAngles[] = {0.0f, 1.5708f, 3.14159f, 4.71239f}; // N, E, S, O in radians
    
    for (int i = 0; i < 4; i++)
    {
        float angleOffset = cardinalAngles[i] - carYaw;
        // Normalize to -PI to PI
        while (angleOffset > 3.14159f) angleOffset -= 6.28318f;
        while (angleOffset < -3.14159f) angleOffset += 6.28318f;
        
        // Map angle to horizontal position
        float xPos = centerX + angleOffset * (barWidth / 6.28318f);
        
        // Only render if within bar bounds
        if (xPos > centerX - barWidth / 2.0f + 20.0f && xPos < centerX + barWidth / 2.0f - 20.0f)
        {
            RenderTextCentered(cardinals[i], xPos, centerY + 5.0f, 0.7f, 0.7f, 0.7f, 1.8f);
        }
    }
    
    // Calculate angle to objective
    glm::vec3 toObjective = objective - carPos;
    toObjective.y = 0.0f;
    float distToObj = glm::length(toObjective);
    
    if (distToObj > 0.01f)
    {
        toObjective = glm::normalize(toObjective);
        
        // Depending on camera system, may need negative Z
        float angleToObjective = std::atan2(toObjective.x, toObjective.z);
        float angleOffset = angleToObjective - carYaw;
        
        // Normalize to -PI to PI
        while (angleOffset > 3.14159f) angleOffset -= 6.28318f;
        while (angleOffset < -3.14159f) angleOffset += 6.28318f;
        
        // Map angle to horizontal position
        float objX = centerX + angleOffset * (barWidth / 6.28318f);
        
        // Draw only if strictly within visual limits of the dark bar
        if (objX > centerX - barWidth / 2.0f + 15.0f && objX < centerX + barWidth / 2.0f - 15.0f)
        {
            float dotSize = 14.0f;
            
            // Use green for delivery, yellow for pickup
            if (isPickup)
            {
                RenderColoredQuad(objX - dotSize / 2.0f, centerY - dotSize / 2.0f, dotSize, dotSize, 1.0f, 0.8f, 0.0f, 1.0f); // Yellow for pickup
            }
            else
            {
                RenderColoredQuad(objX - dotSize / 2.0f, centerY - dotSize / 2.0f, dotSize, dotSize, 0.0f, 1.0f, 0.3f, 1.0f); // Green for delivery
            }
            
            const char* marker = isPickup ? "P" : "D";
            RenderTextCentered(marker, objX, centerY + 6.0f, 1.0f, 1.0f, 1.0f, 1.5f);
        }
    }
    
    // Draw additional markers (with labels for pickup zones)
    if (additionalMarkers != nullptr)
    {
        for (const auto& marker : *additionalMarkers)
        {
            glm::vec3 toMarker = marker.position - carPos;
            toMarker.y = 0.0f;
            float distToMarker = glm::length(toMarker);
            
            if (distToMarker > 0.01f)
            {
                toMarker = glm::normalize(toMarker);
                
                float angleToMarker = std::atan2(toMarker.x, toMarker.z);
                float angleOffset = angleToMarker - carYaw;
                
                // Normalize to -PI to PI
                while (angleOffset > 3.14159f) angleOffset -= 6.28318f;
                while (angleOffset < -3.14159f) angleOffset += 6.28318f;
                
                // Map angle to horizontal position
                float markerX = centerX + angleOffset * (barWidth / 6.28318f);
                
                // Draw only if strictly within visual limits of the dark bar
                if (markerX > centerX - barWidth / 2.0f + 15.0f && markerX < centerX + barWidth / 2.0f - 15.0f)
                {
                    float dotSize = 10.0f;
                    RenderColoredQuad(markerX - dotSize / 2.0f, centerY - dotSize / 2.0f, dotSize, dotSize, 1.0f, 0.8f, 0.0f, 1.0f);
                    
                    // Draw label (P1, P2, etc.)
                    RenderTextCentered(marker.label.c_str(), markerX, centerY + 12.0f, 1.0f, 0.8f, 0.0f, 1.2f);
                }
            }
        }
    }
}

void DeliveryHUD::RenderMissionPanel(const DeliverySystem& deliverySystem)
{
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    const DeliveryOrder& order = deliverySystem.GetCurrentOrder();
    
    // Draw panel background with rounded corners effect
    float panelWidth = 450.0f;
    float panelHeight = 400.0f;
    float panelX = fbW / 2.0f - panelWidth / 2.0f;
    float panelY = fbH / 2.0f - panelHeight / 2.0f;
    
    // Main panel background
    RenderColoredQuad(panelX, panelY, panelWidth, panelHeight, 0.05f, 0.05f, 0.1f, 0.98f);
    
    // Border
    RenderColoredQuad(panelX, panelY, panelWidth, 3.0f, 1.0f, 0.6f, 0.0f, 1.0f); // Top
    RenderColoredQuad(panelX, panelY + panelHeight - 3.0f, panelWidth, 3.0f, 1.0f, 0.6f, 0.0f, 1.0f); // Bottom
    RenderColoredQuad(panelX, panelY, 3.0f, panelHeight, 1.0f, 0.6f, 0.0f, 1.0f); // Left
    RenderColoredQuad(panelX + panelWidth - 3.0f, panelY, 3.0f, panelHeight, 1.0f, 0.6f, 0.0f, 1.0f); // Right
    
    // Title with background
    RenderColoredQuad(panelX + 5.0f, panelY + 5.0f, panelWidth - 10.0f, 40.0f, 0.15f, 0.15f, 0.2f, 0.9f);
    RenderTextCentered("INSPECCIONAR MISION", fbW / 2.0f, panelY + 35.0f, 1.0f, 0.8f, 0.0f, 2.5f);
    
    // Order type
    const char* typeStr = (order.type == OrderType::FRAGILE) ? "FRAGIL" : "ESTANDAR";
    char typeText[128];
    snprintf(typeText, sizeof(typeText), "Tipo: %s", typeStr);
    RenderText(typeText, panelX + 20.0f, panelY + 70.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    
    // Difficulty
    const char* diffStr = (order.difficulty == OrderDifficulty::EASY) ? "FACIL" : 
                          (order.difficulty == OrderDifficulty::MEDIUM) ? "MEDIO" : 
                          (order.difficulty == OrderDifficulty::HARD) ? "DIFICIL" : "ESPECIAL";
    char diffText[128];
    snprintf(diffText, sizeof(diffText), "Dificultad: %s", diffStr);
    RenderText(diffText, panelX + 20.0f, panelY + 110.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    
    // Time limit
    char timeText[128];
    snprintf(timeText, sizeof(timeText), "Tiempo: %.0f seg", order.timeLimit);
    RenderText(timeText, panelX + 20.0f, panelY + 150.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    
    // Star times
    char star3Buf[64], star2Buf[64], star1Buf[64];
    snprintf(star3Buf, sizeof(star3Buf), "*** Oro:    < %.1f seg", deliverySystem.GetTimeStar3());
    snprintf(star2Buf, sizeof(star2Buf), "** Plata:  < %.1f seg", deliverySystem.GetTimeStar2());
    snprintf(star1Buf, sizeof(star1Buf), "* Bronce: < %.1f seg", deliverySystem.GetTimeStar1());
    
    float yOffset = panelY + 190.0f;
    RenderText(star3Buf, panelX + 20.0f, yOffset, 1.0f, 0.8f, 0.0f, 1.2f); yOffset += 25.0f; // Yellow
    RenderText(star2Buf, panelX + 20.0f, yOffset, 0.8f, 0.8f, 0.8f, 1.2f); yOffset += 25.0f; // Gray
    RenderText(star1Buf, panelX + 20.0f, yOffset, 0.8f, 0.5f, 0.2f, 1.2f); yOffset += 25.0f; // Orange
    
    // Distance
    float distance = glm::length(order.destinationPosition - order.originPosition);
    char distText[128];
    snprintf(distText, sizeof(distText), "Distancia: %.0f m", distance);
    RenderText(distText, panelX + 20.0f, yOffset, 1.0f, 1.0f, 1.0f, 2.0f); yOffset += 40.0f;
    
    // Max reward
    char rewardText[128];
    snprintf(rewardText, sizeof(rewardText), "Ganancia max: $%.0f", order.reward);
    RenderText(rewardText, panelX + 20.0f, yOffset, 1.0f, 0.8f, 0.0f, 2.2f); yOffset += 40.0f;
    
    // Potential loss info
    char lossText[128];
    snprintf(lossText, sizeof(lossText), "Perdida max: -%.0f%%", order.reward * 0.3f);
    RenderText(lossText, panelX + 20.0f, yOffset, 1.0f, 0.8f, 0.0f, 2.0f);
    
    // Instructions (below the panel)
    RenderTextCentered("[E] Aceptar  [Q] Rechazar", fbW / 2.0f, panelY + panelHeight + 20.0f, 0.8f, 0.8f, 0.8f, 1.6f);
}

void DeliveryHUD::RenderDeliveryHUD(const DeliverySystem& deliverySystem)
{
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    const DeliveryOrder& order = deliverySystem.GetCurrentOrder();
    
    // Position HUD in upper left corner
    float hudX = 20.0f;
    float hudY = 80.0f;
    
    // Timer in MM:SS format
    float remainingTime = order.timeLimit - deliverySystem.GetElapsedTime();
    if (remainingTime < 0.0f) remainingTime = 0.0f;
    
    // Time and Stars display
    char timeMsg[128];
    snprintf(timeMsg, sizeof(timeMsg), "Tiempo: %.1fs / %.1fs", deliverySystem.GetElapsedTime(), deliverySystem.currentTargetTime);
    RenderText(timeMsg, hudX, hudY, 1.0f, 1.0f, 1.0f, 2.0f);
    
    // Type and difficulty
    const char* typeStr = (order.type == OrderType::FRAGILE) ? "FRAGIL" : "ESTANDAR";
    const char* diffStr = (order.difficulty == OrderDifficulty::EASY) ? "FACIL" : 
                          (order.difficulty == OrderDifficulty::MEDIUM) ? "MEDIO" : 
                          (order.difficulty == OrderDifficulty::HARD) ? "DIFICIL" : "ESPECIAL";
    char typeDiffText[128];
    snprintf(typeDiffText, sizeof(typeDiffText), "%s (%s)", typeStr, diffStr);
    RenderText(typeDiffText, hudX, hudY + 50.0f, 1.0f, 1.0f, 1.0f, 2.2f);
    
    // Health bar
    float health = order.fragileHealth;
    float barX = hudX;
    float barY = hudY + 90.0f;
    float barW = 200.0f;
    float barH = 25.0f;
    
    // Background
    RenderColoredQuad(barX, barY, barW, barH, 0.2f, 0.2f, 0.2f, 0.9f);
    
    // Health fill
    float healthWidth = barW * (health / 100.0f);
    float hr = (health > 50.0f) ? 0.0f : (health > 25.0f) ? 1.0f : 1.0f;
    float hg = (health > 50.0f) ? 0.8f : (health > 25.0f) ? 0.8f : 0.2f;
    float hb = (health > 50.0f) ? 0.0f : (health > 25.0f) ? 0.0f : 0.2f;
    RenderColoredQuad(barX, barY, healthWidth, barH, hr, hg, hb, 0.95f);
    
    // Health percentage text
    char healthText[64];
    snprintf(healthText, sizeof(healthText), "%.0f%%", health);
    RenderText(healthText, barX + barW + 15.0f, barY + 5.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    
    // Show potential loss if health is below 100%
    if (health < 100.0f)
    {
        float healthLost = 100.0f - health;
        char lossText[128];
        snprintf(lossText, sizeof(lossText), "-%.0f%%", healthLost);
        RenderText(lossText, barX + barW + 15.0f, barY + 30.0f, 1.0f, 0.8f, 0.0f, 1.8f);
    }
    
    // Current/Estimated Gain
    float estimatedGain = deliverySystem.GetEstimatedReward();
    char gainText[128];
    snprintf(gainText, sizeof(gainText), "Ganancia: $%.0f", estimatedGain);
    RenderText(gainText, hudX, hudY + 130.0f, 0.0f, 0.9f, 0.0f, 2.5f);
    
    // Loss by Damage
    float collisionLoss = deliverySystem.GetCollisionLoss();
    float starsY = hudY + 170.0f;
    if (collisionLoss > 0.0f)
    {
        char lossText[128];
        snprintf(lossText, sizeof(lossText), "Perdida: -$%.0f", collisionLoss);
        RenderText(lossText, hudX, hudY + 170.0f, 1.0f, 0.2f, 0.2f, 2.5f);
        starsY += 40.0f;
    }
    else
    {
        starsY = hudY + 170.0f;
    }
    
    // Visual stars (below gain/loss)
    RenderText("Calidad:", hudX, starsY, 1.0f, 1.0f, 1.0f, 1.5f);
    for(int i = 0; i < 3; i++) {
        if (i < deliverySystem.currentStars) {
            // Active star (Yellow, bright)
            RenderText("*", hudX + 100.0f + (i * 30.0f), starsY + 5.0f, 1.0f, 0.9f, 0.0f, 3.0f);
        } else {
            // Lost star (Gray, dim)
            RenderText("*", hudX + 100.0f + (i * 30.0f), starsY + 5.0f, 0.3f, 0.3f, 0.3f, 3.0f);
        }
    }
}

bool DeliveryHUD::ShouldShowMissionPanel(const DeliverySystem& deliverySystem, const CarState& car) const
{
    // Show mission panel when car is near the pickup zone (not stopped, just near)
    if (!deliverySystem.HasWaitingOrder())
        return false;
    
    const DeliveryOrder& order = deliverySystem.GetCurrentOrder();
    float distance = glm::length(car.position - order.originPosition);
    
    // Show if near pickup zone (touching distance)
    return (distance < 2.0f);
}

bool DeliveryHUD::ShouldShowDeliveryMessage(const DeliverySystem& deliverySystem, const CarState& car) const
{
    // Show delivery message when car has picked up order and is near delivery zone
    if (!deliverySystem.HasActiveOrder())
        return false;
    
    const DeliveryOrder& order = deliverySystem.GetCurrentOrder();
    float distance = glm::length(car.position - order.destinationPosition);
    
    // Show if near delivery pillar (no need to be stopped)
    return (distance < 2.0f);
}

bool DeliveryHUD::TryRejectMission(DeliverySystem& deliverySystem, bool qKeyPressed, const CarState& car) const
{
    if (qKeyPressed && deliverySystem.HasWaitingOrder())
    {
        deliverySystem.RejectOrder(car);
        return true;
    }
    return false;
}

void DeliveryHUD::RenderHealthBar(float x, float y, float w, float h, float health)
{
    // Background
    RenderColoredQuad(x, y, w, h, 0.2f, 0.2f, 0.2f, 0.8f);
    
    // Health fill
    float healthWidth = w * (health / 100.0f);
    float hr = (health > 50.0f) ? 0.0f : (health > 25.0f) ? 1.0f : 1.0f;
    float hg = (health > 50.0f) ? 0.8f : (health > 25.0f) ? 0.8f : 0.2f;
    float hb = (health > 50.0f) ? 0.0f : (health > 25.0f) ? 0.0f : 0.2f;
    RenderColoredQuad(x, y, healthWidth, h, hr, hg, hb, 0.9f);
}

void DeliveryHUD::RenderText(const char* text, float x, float y, float r, float g, float b, float scale)
{
    static float qb[60000]; int n = stb_easy_font_print(0, 0, (char*)text, NULL, qb, sizeof(qb));
    if (n <= 0) return;
    static float tb[90000]; convertQuadsToTrianglesClean(qb, n, tb);
    glUseProgram(textProgram);
    int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH);
    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(x, y, 0));
    model = glm::scale(model, glm::vec3(scale, scale, 1));
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "proj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform4f(glGetUniformLocation(textProgram, "uColor"), r, g, b, 1);
    glBindVertexArray(textVAO); glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, n * 6 * 16, tb, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, n * 6); glBindVertexArray(0);
}

void DeliveryHUD::RenderTextCentered(const char* text, float cx, float y, float r, float g, float b, float s)
{
    float w = GetTextWidth(text, s);
    RenderText(text, cx - w * 0.5f, y, r, g, b, s);
}

void DeliveryHUD::RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glUseProgram(hudProgram);
    int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH);
    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(x, y, 0));
    model = glm::scale(model, glm::vec3(w, h, 1));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 1);
    glUniform4f(glGetUniformLocation(hudProgram, "uColor"), r, g, b, a);
    glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
}