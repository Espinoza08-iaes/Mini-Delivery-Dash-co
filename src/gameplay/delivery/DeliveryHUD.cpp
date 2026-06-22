#include "DeliveryHUD.h"
#include "../shop/ShopManager.h"
#include <iostream>
#include <cstring>
#include <cmath>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../../../third_party/stb/stb_easy_font.h"
#include "../../../third_party/stb/stb_image.h"

// ======================================================================
// SHADER COMPILATION
// ======================================================================
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

// ======================================================================
// QUAD TO TRIANGLES
// ======================================================================
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

// ======================================================================
// CONSTRUCTOR / DESTRUCTOR
// ======================================================================
DeliveryHUD::DeliveryHUD(GLFWwindow* w, int sw, int sh)
    : window(w), width(sw), height(sh),
      quadVAO(0), quadVBO(0), hudProgram(0),
      textVAO(0), textVBO(0), textProgram(0),
      starVAO(0), starVBO(0), starProgram(0),
      uiTexProgram(0), missionPanelTex(0), successPanelTex(0),
      dashboardTexture(0), dashboardTexW(0), dashboardTexH(0),
      panelsTexture(0), panelsTexW(0), panelsTexH(0)
{
    SetupGraphics();
    SetupTextRendering();
    SetupStarGraphics();
    LoadTextures();
    
    const char* texVs = R"(#version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTex;
        uniform mat4 proj;
        uniform mat4 model;
        out vec2 TexCoords;
        void main() {
            gl_Position = proj * model * vec4(aPos, 0.0, 1.0);
            TexCoords = aTex;
        }
    )";
    const char* texFs = R"(#version 330 core
        in vec2 TexCoords;
        out vec4 FragColor;
        uniform sampler2D image;
        void main() {
            vec2 fixedCoords = vec2(TexCoords.x, 1.0 - TexCoords.y);
            vec4 texColor = texture(image, fixedCoords);
            if(texColor.a < 0.1) discard;
            FragColor = texColor;
        }
    )";
    uiTexProgram = compileShader(texVs, texFs);
    
    missionPanelTex = LoadUITexture("res/textures/MissionPanel.png");
    successPanelTex = LoadUITexture("res/textures/SuccessfullPanel.png");
}

DeliveryHUD::~DeliveryHUD()
{
    glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &textVAO); glDeleteBuffers(1, &textVBO);
    glDeleteVertexArrays(1, &starVAO); glDeleteBuffers(1, &starVBO);
    if (hudProgram) glDeleteProgram(hudProgram);
    if (textProgram) glDeleteProgram(textProgram);
    if (starProgram) glDeleteProgram(starProgram);
    if (uiTexProgram) glDeleteProgram(uiTexProgram);
    if (missionPanelTex) glDeleteTextures(1, &missionPanelTex);
    if (successPanelTex) glDeleteTextures(1, &successPanelTex);
    if (dashboardTexture) glDeleteTextures(1, &dashboardTexture);
    if (panelsTexture) glDeleteTextures(1, &panelsTexture);
}

// ======================================================================
// TEXTURE LOADING
// ======================================================================
void DeliveryHUD::LoadTextures()
{
    stbi_set_flip_vertically_on_load(true);
    
    int comp;
    unsigned char* data = stbi_load("res/textures/DashBoard_STB.png", &dashboardTexW, &dashboardTexH, &comp, 4);
    if (data)
    {
        glGenTextures(1, &dashboardTexture);
        glBindTexture(GL_TEXTURE_2D, dashboardTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dashboardTexW, dashboardTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stbi_image_free(data);
        std::cout << "[HUD] Loaded DashBoard_STB.png (" << dashboardTexW << "x" << dashboardTexH << ")" << std::endl;
    }
    else { std::cerr << "[HUD] Failed to load DashBoard_STB.png" << std::endl; }
    
    data = stbi_load("res/textures/WhilePlayingPanels_STB.png", &panelsTexW, &panelsTexH, &comp, 4);
    if (data)
    {
        glGenTextures(1, &panelsTexture);
        glBindTexture(GL_TEXTURE_2D, panelsTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, panelsTexW, panelsTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stbi_image_free(data);
        std::cout << "[HUD] Loaded WhilePlayingPanels_STB.png (" << panelsTexW << "x" << panelsTexH << ")" << std::endl;
    }
    else { std::cerr << "[HUD] Failed to load WhilePlayingPanels_STB.png" << std::endl; }
}

// ======================================================================
// SETUP
// ======================================================================
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

void DeliveryHUD::SetupStarGraphics()
{
    const char* vs = R"(#version 330 core
        layout(location=0) in vec2 aPos;
        uniform mat4 proj;
        void main(){ gl_Position = proj * vec4(aPos, 0.0f, 1.0f); })";
    const char* fs = R"(#version 330 core
        out vec4 frag; uniform vec4 uColor;
        void main(){ frag = uColor; })";
    starProgram = compileShader(vs, fs);
    glGenVertexArrays(1, &starVAO);
    glGenBuffers(1, &starVBO);
    glBindVertexArray(starVAO);
    glBindBuffer(GL_ARRAY_BUFFER, starVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 200, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glBindVertexArray(0);
}

// ======================================================================
// RENDER (MAIN ENTRY POINT)
// ======================================================================
void DeliveryHUD::Render(const DeliverySystem& deliverySystem, const CarState& car, bool qKeyPressed)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    // ============================================================
    // BOTTOM CENTER: Speedometer inside DashBoard_STB.png
    // ============================================================
    ShopManager* shop = ShopManager::GetInstance();
    float speedMult = shop->GetUpgradeMultiplier(UpgradeType::Speed);
    float baseMaxSpeed = 8.0f;
    float turboMaxSpeed = baseMaxSpeed * 2.0f * speedMult;
    RenderSpeedometer(car, turboMaxSpeed);
    
    // ============================================================
    // TOP RIGHT: Single panel (Health + Earnings)
    // ============================================================
    RenderDurabilityBar(car);
    
    // Earnings text (inside the same panel, positioned for bottom half)
    char earningsText[64];
    snprintf(earningsText, sizeof(earningsText), "$%.0f", deliverySystem.GetWalletBalance());
    RenderText(earningsText, fbW - 180.0f, 68.0f, 0.85f, 0.70f, 0.35f, 1.5f);
    
    // ============================================================
    // Only render delivery HUD if there's an active order
    // ============================================================
    if (!deliverySystem.HasWaitingOrder() && !deliverySystem.HasActiveOrder() 
        && deliverySystem.GetCurrentOrder().state != OrderState::DELIVERED)
    {
        glEnable(GL_DEPTH_TEST);
        return;
    }
    
    // Compass bar
    std::vector<CompassMarker> markers;
    if (deliverySystem.HasWaitingOrder()) {
        markers.push_back({deliverySystem.GetWaitingOrder().originPosition, "P", true, glm::vec3(0.0f, 0.8f, 1.0f)}); // Cyan for pickup
    }
    
    if (deliverySystem.HasActiveOrder()) {
        const auto& activeOrders = deliverySystem.GetActiveOrders();
        std::vector<std::pair<float, glm::vec3>> dists;
        for (const auto& order : activeOrders) {
            dists.push_back({glm::length(car.position - order.destinationPosition), order.destinationPosition});
        }
        std::sort(dists.begin(), dists.end(), [](const std::pair<float, glm::vec3>& a, const std::pair<float, glm::vec3>& b) {
            return a.first < b.first;
        });
        
        for (size_t i = 0; i < dists.size(); ++i) {
            glm::vec3 color;
            if (i == 0) color = glm::vec3(0.0f, 1.0f, 0.3f); // Green (closest)
            else if (i == 1) color = glm::vec3(1.0f, 0.8f, 0.0f); // Yellow (medium)
            else color = glm::vec3(1.0f, 0.2f, 0.2f); // Red (furthest)
            
            markers.push_back({dists[i].second, "D", false, color});
        }
    }
    
    glm::vec3 dummyObj = markers.empty() ? glm::vec3(0) : markers[0].position;
    RenderCompassBar(fbW / 2.0f, 60.0f, car.yaw, dummyObj, car.position, false, &markers);
    
    // Distance text below compass
    float distance = deliverySystem.GetDistanceToObjective(car);
    char distanceText[64];
    snprintf(distanceText, sizeof(distanceText), "%.1f m", distance);
    RenderTextCentered(distanceText, fbW / 2.0f, 110.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    
    // Success screen
    if (deliverySystem.GetCurrentOrder().state == OrderState::DELIVERED)
    {
        const DeliveryOrder& order = deliverySystem.GetCurrentOrder();
        
        float panelW = 1200.0f;
        float panelH = 950.0f;
        float panelX = fbW / 2.0f - panelW / 2.0f;
        float panelY = fbH / 2.0f - panelH / 2.0f;
        
        RenderTexturedQuad(successPanelTex, panelX, panelY, panelW, panelH);
        
        int stars = order.starsEarned;
        float starY = panelY + 350.0f;
        float starRadius = 50.0f;
        float starSpacing = 115.0f;
        float starStartX = fbW / 2.0f - 150.0f;
        
        for (int i = 0; i < 3; ++i)
        {
            glm::vec3 starColor;
            if (i < stars)
            {
                if (stars == 3) starColor = glm::vec3(1.0f, 0.84f, 0.0f);
                else if (stars == 2) starColor = glm::vec3(0.9f, 0.9f, 0.9f);
                else if (stars == 1) starColor = glm::vec3(0.8f, 0.5f, 0.2f);
                else starColor = glm::vec3(1.0f, 0.1f, 0.1f);
            }
            else
            {
                starColor = glm::vec3(0.25f, 0.25f, 0.25f);
            }
            DrawStar(starStartX + i * starSpacing, starY, starRadius, starColor, starProgram, starVAO, starVBO);
        }
        
        float yOffset = panelY + 460.0f;
        char buf[128];
        
        snprintf(buf, sizeof(buf), "Delivery Time: %.1f sec", deliverySystem.finalElapsedTime);
        RenderTextCentered(buf, fbW / 2.0f, yOffset, 0.1f, 0.1f, 0.1f, 2.0f);
        yOffset += 50.0f;
        
        snprintf(buf, sizeof(buf), "Base Pay: $%.0f", order.reward);
        RenderTextCentered(buf, fbW / 2.0f, yOffset, 0.1f, 0.1f, 0.1f, 2.0f);
        yOffset += 55.0f;
        
        if (deliverySystem.finalBonusAmount > 0.0f) {
            snprintf(buf, sizeof(buf), "+ Star Bonus: $%.0f", deliverySystem.finalBonusAmount);
            RenderTextCentered(buf, fbW / 2.0f, yOffset, 0.0f, 0.4f, 0.0f, 2.0f);
            yOffset += 55.0f;
        }
        if (deliverySystem.finalLossCollision > 0.0f) {
            snprintf(buf, sizeof(buf), "- Collisions: $%.0f", deliverySystem.finalLossCollision);
            RenderTextCentered(buf, fbW / 2.0f, yOffset, 0.6f, 0.0f, 0.0f, 2.0f);
            yOffset += 55.0f;
        }
        if (deliverySystem.finalLossWater > 0.0f) {
            snprintf(buf, sizeof(buf), "- Water Falls: $%.0f", deliverySystem.finalLossWater);
            RenderTextCentered(buf, fbW / 2.0f, yOffset, 0.0f, 0.0f, 0.6f, 2.0f);
            yOffset += 55.0f;
        }
        if (deliverySystem.finalLossTime > 0.0f) {
            snprintf(buf, sizeof(buf), "- Late Penalty: $%.0f", deliverySystem.finalLossTime);
            RenderTextCentered(buf, fbW / 2.0f, yOffset, 0.6f, 0.3f, 0.0f, 2.0f);
            yOffset += 55.0f;
        }
        
        yOffset += 40.0f;
        float total = order.reward + deliverySystem.finalBonusAmount - deliverySystem.finalLossCollision - deliverySystem.finalLossWater - deliverySystem.finalLossTime;
        snprintf(buf, sizeof(buf), "TOTAL EARNINGS: $%.0f", total);
        RenderTextCentered(buf, fbW / 2.0f, yOffset, 0.0f, 0.4f, 0.0f, 3.0f);
        
        RenderTextCentered("Press [ENTER] to continue", fbW / 2.0f, panelY + panelH - 80.0f, 0.3f, 0.3f, 0.3f, 1.5f);
    }
    else if (deliverySystem.HasWaitingOrder() && ShouldShowMissionPanel(deliverySystem, car))
    {
        RenderMissionPanel(deliverySystem);
    }
    else if (deliverySystem.HasActiveOrder())
    {
        RenderDeliveryHUD(deliverySystem, car);
    }
    
    if (ShouldShowDeliveryMessage(deliverySystem, car))
    {
        RenderTextCentered("Deliver with E or Cancel with Q", fbW / 2.0f, fbH / 2.0f + 50.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    }
    
    glEnable(GL_DEPTH_TEST);
}

// ======================================================================
// SPEEDOMETER (Bottom Center — inside DashBoard_STB.png)
// ======================================================================
void DeliveryHUD::RenderSpeedometer(const CarState& car, float maxSpeed)
{
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    float dashboardW = 220.0f;
    float dashboardH = 65.0f;
    float centerX = fbW * 0.5f;
    float centerY = fbH - dashboardH * 0.5f - 10.0f;
    
    // Draw the dashboard image
    if (dashboardTexture != 0)
    {
        RenderQuad(centerX - dashboardW * 0.5f, centerY - dashboardH * 0.5f, dashboardW, dashboardH, dashboardTexture);
    }
    else
    {
        RenderBezelPanel(centerX - dashboardW * 0.5f, centerY - dashboardH * 0.5f, dashboardW, dashboardH, 4.0f);
    }
    
    // Calculate physical km/h correctly (1 unit/s ~ 1 m/s = 3.6 km/h)
    float currentSpeedKMH = std::abs(car.speed) * 3.6f;
    float maxSpeedKMH = maxSpeed * 3.6f;
    

    
    // Speed text centered inside the dashboard
    char speedText[32];
    snprintf(speedText, sizeof(speedText), "%.0f", currentSpeedKMH);
    RenderTextCentered(speedText, centerX, centerY - 2.0f, 1.0f, 1.0f, 1.0f, 3.0f);
    
    // "km/h" small text below speed
    RenderTextCentered("KM/H", centerX, centerY - 25.0f, 0.7f, 0.7f, 0.7f, 1.2f);
}

// ======================================================================
// DURABILITY + EARNINGS PANEL (Top Right)
// ======================================================================
void DeliveryHUD::RenderDurabilityBar(const CarState& car)
{
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    float panelW = 260.0f;
    float panelH = 90.0f;
    float panelX = fbW - panelW - 10.0f;
    float panelY = 10.0f;
    
    // Draw the single panel image (covers both health and earnings)
    if (panelsTexture != 0)
    {
        RenderQuad(panelX, panelY, panelW, panelH, panelsTexture);
    }
    else
    {
        RenderBezelPanel(panelX, panelY, panelW, panelH, 3.0f);
    }
    
    // ---- HEALTH SECTION ----
    RenderText("HEALTH", panelX + 14.0f, panelY + 20.0f, 0.85f, 0.70f, 0.35f, 1.3f);
    
    float barX = panelX + 85.0f;
    float barY = panelY + 20.0f;
    float barW = 130.0f;
    float barH = 12.0f;
    
    // Bar background
    RenderColoredQuad(barX, barY, barW, barH, 0.15f, 0.15f, 0.15f, 0.8f);
    
    // Health fill
    float healthRatio = car.durability / 100.0f;
    float healthW = barW * healthRatio;
    float dr, dg, db;
    if (car.durability > 60.0f)      { dr = 0.0f; dg = 0.8f; db = 0.0f; }
    else if (car.durability > 30.0f) { dr = 1.0f; dg = 0.8f; db = 0.0f; }
    else                              { dr = 1.0f; dg = 0.2f; db = 0.0f; }
    
    RenderColoredQuad(barX, barY, healthW, barH, dr, dg, db, 0.95f);
    
    // Health percentage
    char healthText[16];
    snprintf(healthText, sizeof(healthText), "%.0f%%", car.durability);
    RenderText(healthText, barX + barW + 6.0f, barY - 1.0f, 1.0f, 1.0f, 1.0f, 1.1f);
    
    // Warning text
    if (car.isDead) {
        RenderText("WRECKED!", panelX + 14.0f, panelY + 38.0f, 1.0f, 0.0f, 0.0f, 1.2f);
    } else if (car.durability < 20.0f) {
        RenderText("CRITICAL", panelX + 14.0f, panelY + 38.0f, 1.0f, 0.3f, 0.0f, 1.0f);
    }
    
    // ---- EARNINGS SECTION ----
    // Label
    RenderText("EARNINGS", panelX + 14.0f, panelY + 58.0f, 0.85f, 0.70f, 0.35f, 1.3f);
    
    // Value is rendered in Render() function since it needs deliverySystem
}

// ======================================================================
// COMPASS BAR
// ======================================================================
void DeliveryHUD::RenderCompassBar(float centerX, float centerY, float carYaw, const glm::vec3& objective, const glm::vec3& carPos, bool isPickup, const std::vector<CompassMarker>* additionalMarkers)
{
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    float barWidth = 600.0f;
    float barHeight = 40.0f;
    
    RenderBezelPanel(centerX - barWidth / 2.0f - 4.0f, centerY - barHeight / 2.0f - 4.0f, barWidth + 8.0f, barHeight + 8.0f, 3.0f);
    
    // Center indicator
    RenderColoredQuad(centerX - 2.5f, centerY - barHeight / 2.0f - 5.0f, 5.0f, barHeight + 10.0f, 0.85f, 0.7f, 0.35f, 1.0f);
    
    // Cardinal marks
    const char* cardinals[] = {"N", "E", "S", "W"};
    float cardinalAngles[] = {0.0f, 1.5708f, 3.14159f, 4.71239f};
    
    for (int i = 0; i < 4; i++)
    {
        float angleOffset = cardinalAngles[i] - carYaw;
        while (angleOffset > 3.14159f) angleOffset -= 6.28318f;
        while (angleOffset < -3.14159f) angleOffset += 6.28318f;
        
        float xPos = centerX + angleOffset * (barWidth / 6.28318f);
        if (xPos > centerX - barWidth / 2.0f + 20.0f && xPos < centerX + barWidth / 2.0f - 20.0f)
        {
            RenderTextCentered(cardinals[i], xPos, centerY + 5.0f, 0.7f, 0.7f, 0.7f, 1.5f);
        }
    }
    
    // Render all markers
    if (additionalMarkers)
    {
        for (const auto& marker : *additionalMarkers)
        {
            glm::vec3 toObjective = marker.position - carPos;
            toObjective.y = 0.0f;
            float distToObj = glm::length(toObjective);
            
            if (distToObj > 0.01f)
            {
                toObjective = glm::normalize(toObjective);
                float angleToObj = std::atan2(toObjective.x, toObjective.z);
                float angleOffset = angleToObj - carYaw;
                while (angleOffset > 3.14159f) angleOffset -= 6.28318f;
                while (angleOffset < -3.14159f) angleOffset += 6.28318f;
                
                float objX = centerX + angleOffset * (barWidth / 6.28318f);
                
                if (objX > centerX - barWidth / 2.0f + 15.0f && objX < centerX + barWidth / 2.0f - 15.0f)
                {
                    float dotSize = 14.0f;
                    RenderColoredQuad(objX - dotSize / 2.0f, centerY - dotSize / 2.0f, dotSize, dotSize, marker.color.r, marker.color.g, marker.color.b, 1.0f);
                    RenderTextCentered(marker.label.c_str(), objX, centerY + 6.0f, 1.0f, 1.0f, 1.0f, 1.3f);
                }
            }
        }
    }
}

// ======================================================================
// MISSION PANEL
// ======================================================================
void DeliveryHUD::RenderMissionPanel(const DeliverySystem& deliverySystem)
{
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    const DeliveryOrder& order = deliverySystem.GetWaitingOrder();
    
    float panelW = 900.0f;
    float panelH = 1000.0f;
    float panelX = fbW / 2.0f - panelW / 2.0f;
    float panelY = fbH / 2.0f - panelH / 2.0f;
    
    RenderTexturedQuad(missionPanelTex, panelX, panelY, panelW, panelH);

    char buf[256];
    float yOffset = panelY + 300.0f;
    float textX   = panelX + 300.0f;
    float textScale = 1.8f;

    snprintf(buf, sizeof(buf), "Destination: %s", order.destinationPosition.x != 999999.0f ? "Zone" : "None");
    RenderText(buf, textX, yOffset, 0.8f, 0.8f, 1.0f, textScale);
    yOffset += 50.0f;

    float dist = glm::length(order.destinationPosition - order.originPosition);
    snprintf(buf, sizeof(buf), "Distance: %.0f m", dist);
    RenderText(buf, textX, yOffset, 0.9f, 0.9f, 0.9f, textScale);
    yOffset += 50.0f;

    if (order.type == OrderType::FRAGILE) {
        RenderText("Type: FRAGILE!", textX, yOffset, 1.0f, 0.3f, 0.3f, textScale);
    } else {
        RenderText("Type: Normal", textX, yOffset, 0.7f, 1.0f, 0.7f, textScale);
    }
    yOffset += 60.0f;

    RenderText("Target Times:", textX, yOffset, 1.0f, 0.8f, 0.0f, textScale);
    yOffset += 40.0f;

    snprintf(buf, sizeof(buf), "*** %.0fs (Gold)", order.timeGold);
    RenderText(buf, textX + 20.0f, yOffset, 1.0f, 0.84f, 0.0f, textScale);
    yOffset += 38.0f;

    snprintf(buf, sizeof(buf), "** %.0fs (Silver)", order.timeSilver);
    RenderText(buf, textX + 20.0f, yOffset, 0.75f, 0.75f, 0.75f, textScale);
    yOffset += 38.0f;

    snprintf(buf, sizeof(buf), "* %.0fs (Bronze)", order.timeBronze);
    RenderText(buf, textX + 20.0f, yOffset, 0.8f, 0.5f, 0.2f, textScale);
    yOffset += 55.0f;

    float payMult = ShopManager::GetInstance()->GetUpgradeMultiplier(UpgradeType::PayPerDelivery);
    snprintf(buf, sizeof(buf), "Base Pay: $%.0f", order.baseDisplayReward * payMult);
    RenderText(buf, textX, yOffset, 0.4f, 1.0f, 0.4f, 2.5f);
    yOffset += 60.0f;

    snprintf(buf, sizeof(buf), "Max Reward: $%.0f", (order.baseDisplayReward * payMult) * 1.4f);
    RenderText(buf, textX, yOffset, 1.0f, 0.84f, 0.0f, 2.5f);

    RenderTextCentered("[E] Accept  [Q] Reject", fbW / 2.0f, panelY + panelH - 65.0f, 0.8f, 0.8f, 0.8f, 2.0f);
}

// ======================================================================
// DELIVERY HUD (Active Order)
// ======================================================================
void DeliveryHUD::RenderDeliveryHUD(const DeliverySystem& deliverySystem, const CarState& car)
{
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    const DeliveryOrder* orderPtr = deliverySystem.GetClosestActiveOrder(car.position);
    if (!orderPtr) return;
    const DeliveryOrder& order = *orderPtr;
    
    float hudX = 20.0f;
    float hudY = 80.0f;
    
    // Stars display
    int stars = deliverySystem.GetStarsEarned(order);
    float starY = 20.0f;
    float starRadius = 14.0f;
    float starSpacing = 36.0f;
    float starStartX = 20.0f;
    
    for (int i = 0; i < 3; ++i)
    {
        glm::vec3 starColor;
        if (i < stars)
        {
            if (stars == 3) starColor = glm::vec3(1.0f, 0.84f, 0.0f);
            else if (stars == 2) starColor = glm::vec3(0.9f, 0.9f, 0.9f);
            else if (stars == 1) starColor = glm::vec3(0.8f, 0.5f, 0.2f);
            else starColor = glm::vec3(1.0f, 0.1f, 0.1f);
        }
        else
        {
            starColor = glm::vec3(0.25f, 0.25f, 0.25f);
        }
        DrawStar(starStartX + i * starSpacing, starY, starRadius, starColor, starProgram, starVAO, starVBO);
    }
    
    // Tier text
    const char* tierName = "PENALIZACION";
    float nextTarget = order.timeLimit;
    if (order.elapsedTime <= order.timeGold) { tierName = "TIEMPO ORO"; nextTarget = order.timeGold; }
    else if (order.elapsedTime <= order.timeSilver) { tierName = "TIEMPO PLATA"; nextTarget = order.timeSilver; }
    else if (order.elapsedTime <= order.timeBronze) { tierName = "TIEMPO BRONCE"; nextTarget = order.timeBronze; }
    
    float timeLeft = nextTarget - order.elapsedTime;
    if (timeLeft < 0.0f) timeLeft = 0.0f;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "%s: %.1f s", tierName, timeLeft);
    RenderText(buf, starStartX, starY + starRadius + 8.0f, 1.0f, 1.0f, 1.0f, 1.4f);
    
    char buf2[128];
    snprintf(buf2, sizeof(buf2), "Time: %.1f / %.1f sec", order.elapsedTime, order.timeLimit);
    RenderText(buf2, hudX, hudY, 1.0f, 1.0f, 1.0f, 1.8f);
    
    const char* typeStr = (order.type == OrderType::FRAGILE) ? "FRAGILE" : "STANDARD";
    const char* diffStr = (order.difficulty == OrderDifficulty::EASY) ? "EASY" : 
                          (order.difficulty == OrderDifficulty::MEDIUM) ? "MEDIUM" : "HARD";
    snprintf(buf, sizeof(buf), "%s (%s)", typeStr, diffStr);
    RenderText(buf, hudX, hudY + 40.0f, 1.0f, 1.0f, 1.0f, 1.8f);
    
    // Health bar for fragile orders
    if (order.type == OrderType::FRAGILE)
    {
        float health = order.fragileHealth;
        float barX = hudX, barY = hudY + 75.0f, barW = 180.0f, barH = 20.0f;
        RenderColoredQuad(barX, barY, barW, barH, 0.2f, 0.2f, 0.2f, 0.9f);
        
        float hw = barW * (health / 100.0f);
        float hr = (health > 50.0f) ? 0.0f : (health > 25.0f) ? 1.0f : 1.0f;
        float hg = (health > 50.0f) ? 0.8f : (health > 25.0f) ? 0.8f : 0.2f;
        float hb = (health > 50.0f) ? 0.0f : (health > 25.0f) ? 0.0f : 0.2f;
        RenderColoredQuad(barX, barY, hw, barH, hr, hg, hb, 0.95f);
        
        snprintf(buf, sizeof(buf), "%.0f%%", health);
        RenderText(buf, barX + barW + 10.0f, barY + 2.0f, 1.0f, 1.0f, 1.0f, 1.5f);
    }
    
    float b, bon, pC, pW, pT;
    deliverySystem.CalculateDetailedRewards(order, b, bon, pC, pW, pT);
    float payMult = ShopManager::GetInstance()->GetUpgradeMultiplier(UpgradeType::PayPerDelivery);
    
    b *= payMult;
    pC *= payMult;
    pW *= payMult;
    pT *= payMult;
    
    float estimatedGain = b - pC - pW - pT;
    if (estimatedGain < 0.0f) estimatedGain = 0.0f;
    
    snprintf(buf, sizeof(buf), "Earnings: $%.0f", estimatedGain);
    RenderText(buf, hudX, hudY + 110.0f, 0.0f, 0.9f, 0.0f, 2.0f);
    
    float collisionLoss = pC + pW + pT;
    if (collisionLoss > 0.0f)
    {
        snprintf(buf, sizeof(buf), "Loss: -$%.0f", collisionLoss);
        RenderText(buf, hudX, hudY + 145.0f, 1.0f, 0.2f, 0.2f, 1.8f);
    }
}

// ======================================================================
// HELPERS
// ======================================================================
bool DeliveryHUD::ShouldShowMissionPanel(const DeliverySystem& deliverySystem, const CarState& car) const
{
    if (!deliverySystem.HasWaitingOrder()) return false;
    const DeliveryOrder& order = deliverySystem.GetWaitingOrder();
    float distance = glm::length(car.position - order.originPosition);
    return (distance < 2.0f);
}

bool DeliveryHUD::ShouldShowDeliveryMessage(const DeliverySystem& deliverySystem, const CarState& car) const
{
    if (!deliverySystem.HasActiveOrder()) return false;
    const DeliveryOrder* orderPtr = deliverySystem.GetClosestActiveOrder(car.position);
    if (!orderPtr) return false;
    float distance = glm::length(car.position - orderPtr->destinationPosition);
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

void DeliveryHUD::DrawStar(float cx, float cy, float radius, glm::vec3 color, unsigned int program, unsigned int vao, unsigned int vbo)
{
    std::vector<float> vertices;
    vertices.reserve(24);
    vertices.push_back(cx);
    vertices.push_back(cy);
    
    const float PI = 3.14159265359f;
    float innerRadius = radius * 0.382f;
    
    for (int i = 0; i <= 10; ++i)
    {
        float angle = (PI / 2.0f) - (i * PI / 5.0f);
        float r = (i % 2 == 0) ? radius : innerRadius;
        vertices.push_back(cx + std::cos(angle) * r);
        vertices.push_back(cy - std::sin(angle) * r);
    }
    
    glUseProgram(program);
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    glUniformMatrix4fv(glGetUniformLocation(program, "proj"), 1, GL_FALSE, &proj[0][0]);
    glUniform4f(glGetUniformLocation(program, "uColor"), color.r, color.g, color.b, 1.0f);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(vertices.size() / 2));
    glBindVertexArray(0);
}

void DeliveryHUD::RenderHealthBar(float x, float y, float w, float h, float health)
{
    RenderColoredQuad(x, y, w, h, 0.2f, 0.2f, 0.2f, 0.8f);
    float hw = w * (health / 100.0f);
    float hr = (health > 50.0f) ? 0.0f : (health > 25.0f) ? 1.0f : 1.0f;
    float hg = (health > 50.0f) ? 0.8f : (health > 25.0f) ? 0.8f : 0.2f;
    float hb = (health > 50.0f) ? 0.0f : (health > 25.0f) ? 0.0f : 0.2f;
    RenderColoredQuad(x, y, hw, h, hr, hg, hb, 0.9f);
}

// ======================================================================
// LOW-LEVEL RENDERING
// ======================================================================
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

void DeliveryHUD::RenderBezelPanel(float x, float y, float w, float h, float borderPx)
{
    RenderColoredQuad(x, y, w, h, 0.62f, 0.62f, 0.65f, 0.95f);
    RenderColoredQuad(x + borderPx * 0.4f, y + borderPx * 0.4f, w - borderPx * 0.8f, h - borderPx * 0.8f, 0.08f, 0.08f, 0.08f, 1.0f);
    RenderColoredQuad(x + borderPx, y + borderPx, w - borderPx * 2.0f, h - borderPx * 2.0f, 0.04f, 0.04f, 0.045f, 0.92f);
}

void DeliveryHUD::RenderQuad(float x, float y, float w, float h, unsigned int texture)
{
    glUseProgram(hudProgram);
    int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH);
    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(x, y, 0));
    model = glm::scale(model, glm::vec3(w, h, 1));
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 0);
    glUniform1i(glGetUniformLocation(hudProgram, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void DeliveryHUD::RenderArc(float centerX, float centerY, float radius, float startAngle, float endAngle, float r, float g, float b, float a, int segments)
{
    glUseProgram(hudProgram);
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    
    std::vector<float> verts;
    for (int i = 0; i <= segments; i++)
    {
        float angle = startAngle + (endAngle - startAngle) * (float)i / (float)segments;
        verts.push_back(centerX + std::cos(angle) * radius);
        verts.push_back(centerY - std::sin(angle) * radius);
    }
    
    const float thickness = 3.0f;
    for (size_t i = 0; i < verts.size() - 2; i += 2)
    {
        float x1 = verts[i], y1 = verts[i+1], x2 = verts[i+2], y2 = verts[i+3];
        float dx = x2 - x1, dy = y2 - y1;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len < 0.001f) continue;
        dx /= len; dy /= len;
        float px = -dy * thickness * 0.5f, py = dx * thickness * 0.5f;
        
        float qv[] = { x1+px,y1+py, x1-px,y1-py, x2-px,y2-py, x1+px,y1+py, x2-px,y2-py, x2+px,y2+py };
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
        glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 1);
        glUniform4f(glGetUniformLocation(hudProgram, "uColor"), r, g, b, a);
        glBindVertexArray(textVAO);
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(qv), qv, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, (void*)0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

unsigned int DeliveryHUD::LoadUITexture(const char* path)
{
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int w, h, nrChannels;
    unsigned char *data = stbi_load(path, &w, &h, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "[ERROR] Falló al cargar textura UI: " << path << std::endl;
    }
    stbi_image_free(data);
    return tex;
}

void DeliveryHUD::RenderTexturedQuad(unsigned int texID, float x, float y, float w, float h)
{
    glUseProgram(uiTexProgram);
    int fbW, fbH; glfwGetFramebufferSize(window, &fbW, &fbH);
    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f, -1.f, 1.f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(uiTexProgram, "proj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(uiTexProgram, "model"), 1, GL_FALSE, &model[0][0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    glUniform1i(glGetUniformLocation(uiTexProgram, "image"), 0);

    static unsigned int vao = 0, vbo = 0;
    if (vao == 0) {
        float verts[] = {
            0.f, 1.f,   0.f, 1.f,
            1.f, 0.f,   1.f, 0.f,
            0.f, 0.f,   0.f, 0.f,
            0.f, 1.f,   0.f, 1.f,
            1.f, 1.f,   1.f, 1.f,
            1.f, 0.f,   1.f, 0.f
        };
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    glBindVertexArray(vao);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}