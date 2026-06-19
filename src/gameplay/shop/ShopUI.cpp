#include "ShopUI.h"
#include "../vehicle/CarController.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <cmath>

// NO definir STB_EASY_FONT_IMPLEMENTATION aquí - ya está definido en DeliveryHUD.cpp
#include "../../../third_party/stb/stb_easy_font.h"

// Shader compilation helper
static unsigned int compileShader(const char* vsSrc, const char* fsSrc) {
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

// Convert quads to triangles
static void convertQuadsToTrianglesClean(float* quads, int num, float* tris) {
    for (int i = 0; i < num; i++) {
        float* q = quads + i * 16;
        memcpy(tris + i * 24 + 0,  q,      12 * sizeof(float));
        memcpy(tris + i * 24 + 12, q,       4 * sizeof(float));
        memcpy(tris + i * 24 + 16, q + 8,   4 * sizeof(float));
        memcpy(tris + i * 24 + 20, q + 12,  4 * sizeof(float));
    }
}

// Get text width
static float GetTextWidth(const char* text, float scale) {
    float buf[60000];
    int n = stb_easy_font_print(0, 0, (char*)text, NULL, buf, sizeof(buf));
    if (n <= 0) return 0;
    float mn = buf[0], mx = buf[0];
    for (int i = 0; i < n; i++) {
        float* q = buf + i * 16;
        for (int j = 0; j < 4; j++) { float x = q[j * 4]; if (x < mn) mn = x; if (x > mx) mx = x; }
    }
    return (mx - mn) * scale;
}

ShopUI::ShopUI(GLFWwindow* w) 
    : window(w), isVisible(false), carState(nullptr), repairButton(nullptr), quadVAO(0), quadVBO(0), textVAO(0), textVBO(0), hudProgram(0), textProgram(0) {
    shopManager = ShopManager::GetInstance();
    SetupGraphics();
    SetupTextRendering();
    BuildButtons();
}

ShopUI::~ShopUI() {
    glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &textVAO); glDeleteBuffers(1, &textVBO);
    if (hudProgram) glDeleteProgram(hudProgram);
    if (textProgram) glDeleteProgram(textProgram);
    delete repairButton;
}

void ShopUI::SetupGraphics() {
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

void ShopUI::SetupTextRendering() {
    const char* vs = R"(#version 330 core
        layout(location=0) in vec2 aPos; uniform mat4 proj, model;
        void main(){ gl_Position = proj * model * vec4(aPos,0,1); })";
    const char* fs = R"(#version 330 core
        out vec4 frag; uniform vec4 uColor; void main(){ frag = uColor; })";
    textProgram = compileShader(vs, fs);
    glGenVertexArrays(1, &textVAO); glGenBuffers(1, &textVBO);
}

void ShopUI::BuildButtons() {
    buttons.clear();
    delete repairButton;
    repairButton = nullptr;
    
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    float startX = fbW / 2.0f - 400.0f;
    float startY = 180.0f;
    float btnWidth = 350.0f;
    float btnHeight = 60.0f;
    float spacing = 70.0f;
    
    // Upgrades
    buttons.push_back(Button(startX, startY, btnWidth, btnHeight, UpgradeType::Speed));
    buttons.push_back(Button(startX, startY + spacing, btnWidth, btnHeight, UpgradeType::PayPerDelivery));
    buttons.push_back(Button(startX, startY + spacing * 2, btnWidth, btnHeight, UpgradeType::Acceleration));
    buttons.push_back(Button(startX, startY + spacing * 3, btnWidth, btnHeight, UpgradeType::Handling));
    buttons.push_back(Button(startX, startY + spacing * 4, btnWidth, btnHeight, UpgradeType::FuelEfficiency));
    
    // Abilities (right column)
    float rightX = fbW / 2.0f + 50.0f;
    buttons.push_back(Button(rightX, startY, btnWidth, btnHeight, AbilityType::Turbo));
    buttons.push_back(Button(rightX, startY + spacing, btnWidth, btnHeight, AbilityType::Jump));
    buttons.push_back(Button(rightX, startY + spacing * 2, btnWidth, btnHeight, AbilityType::Teleport));
    
    // Repair button (bottom center)
    float repairY = startY + spacing * 4;
    repairButton = new Button(rightX, repairY, btnWidth, btnHeight);
}

void ShopUI::Show() {
    isVisible = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void ShopUI::Hide() {
    isVisible = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void ShopUI::Toggle() {
    if (isVisible) {
        Hide();
    } else {
        Show();
    }
}

void ShopUI::Update() {
    if (isVisible && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        Hide();
    }
}

void ShopUI::Render() {
    if (!isVisible) return;
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
     // Background overlay
     RenderColoredQuad(0, 0, fbW, fbH, 0.0f, 0.0f, 0.0f, 0.6f);
    
    // Main shop panel
    float panelWidth = 900.0f;
    float panelHeight = 600.0f;
    float panelX = fbW / 2.0f - panelWidth / 2.0f;
    float panelY = 50.0f;
    
     RenderColoredQuad(panelX, panelY, panelWidth, panelHeight, 0.20f, 0.18f, 0.15f, 0.98f);
    
    float border = 3.0f;
    RenderColoredQuad(panelX, panelY, panelWidth, border, 0.20f, 0.16f, 0.13f, 1.0f);
    RenderColoredQuad(panelX, panelY + panelHeight - border, panelWidth, border, 0.20f, 0.16f, 0.13f, 1.0f);
    RenderColoredQuad(panelX, panelY, border, panelHeight, 0.20f, 0.16f, 0.13f, 1.0f);
    RenderColoredQuad(panelX + panelWidth - border, panelY, border, panelHeight, 0.20f, 0.16f, 0.13f, 1.0f);
    
    float innBorder = 1.5f;
    float g = 4.0f;
    RenderColoredQuad(panelX + g, panelY + g, panelWidth - g*2, innBorder, 0.35f, 0.28f, 0.22f, 1.0f);
    RenderColoredQuad(panelX + g, panelY + panelHeight - g - innBorder, panelWidth - g*2, innBorder, 0.35f, 0.28f, 0.22f, 1.0f);
    RenderColoredQuad(panelX + g, panelY + g, innBorder, panelHeight - g*2, 0.35f, 0.28f, 0.22f, 1.0f);
    RenderColoredQuad(panelX + panelWidth - g - innBorder, panelY + g, innBorder, panelHeight - g*2, 0.35f, 0.28f, 0.22f, 1.0f);
    
    // Title background
    RenderColoredQuad(panelX + 5.0f, panelY + 5.0f, panelWidth - 10.0f, 50.0f, 0.15f, 0.12f, 0.10f, 0.95f);
    RenderTextCentered("UPGRADES SHOP", fbW / 2.0f, panelY + 40.0f, 1.0f, 0.8f, 0.0f, 3.0f);
    
    // Balance
    char balanceText[64];
    snprintf(balanceText, sizeof(balanceText), "Balance: $%d", shopManager->GetBalance());
    RenderTextCentered(balanceText, fbW / 2.0f, panelY + 90.0f, 0.2f, 1.0f, 0.2f, 2.5f);
    
    // Section titles
    RenderTextCentered("UPGRADES", fbW / 2.0f - 225.0f, 150.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    RenderTextCentered("ABILITIES", fbW / 2.0f + 225.0f, 150.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    
    // Render all buttons
    for (const auto& btn : buttons) {
        RenderButton(btn);
    }
    
    // Render repair button
    if (repairButton) {
        RenderButton(*repairButton);
    }
    
    // Instructions
    RenderTextCentered("[ESC] Close  |  Click to Purchase", fbW / 2.0f, fbH - 30.0f, 0.8f, 0.8f, 0.8f, 1.8f);
    
    glEnable(GL_DEPTH_TEST);
}

void ShopUI::RenderButton(const Button& btn) {
    if (btn.isRepair) {
        RenderRepairButton(btn);
    } else if (btn.isUpgrade) {
        RenderUpgradeButton(btn);
    } else {
        RenderAbilityButton(btn);
    }
}

void ShopUI::RenderUpgradeButton(const Button& btn) {
    const Upgrade* upgrade = shopManager->GetUpgrade(btn.upgradeType);
    if (!upgrade) return;
    
    bool canAfford = shopManager->GetBalance() >= upgrade->GetCost();
    bool maxLevel = !upgrade->CanUpgrade();
    
    // Button background
    if (btn.isHovered && !maxLevel) {
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.25f, 0.25f, 0.35f, 0.95f);
    } else {
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.15f, 0.15f, 0.2f, 0.9f);
    }
    
    // Button border
    float br = maxLevel ? 0.3f : (canAfford ? 0.2f : 0.6f);
    float bg = maxLevel ? 0.3f : (canAfford ? 1.0f : 0.2f);
    float bb = maxLevel ? 0.3f : (canAfford ? 0.2f : 0.2f);
    RenderColoredQuad(btn.x, btn.y, btn.width, 2.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y + btn.height - 2.0f, btn.width, 2.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y, 2.0f, btn.height, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x + btn.width - 2.0f, btn.y, 2.0f, btn.height, br, bg, bb, 1.0f);
    
    // Name
    RenderText(upgrade->name.c_str(), btn.x + 10.0f, btn.y + 15.0f, 1.0f, 1.0f, 1.0f, 2.0f);
    
    // Level indicator
    char levelText[32];
    snprintf(levelText, sizeof(levelText), "Lv. %d/%d", upgrade->currentLevel, upgrade->maxLevel);
    RenderText(levelText, btn.x + 10.0f, btn.y + 40.0f, 0.7f, 0.7f, 0.7f, 1.5f);
    
    // Cost or MAX
    if (maxLevel) {
        RenderText("MAX", btn.x + btn.width - 80.0f, btn.y + 25.0f, 0.2f, 1.0f, 0.2f, 2.0f);
    } else {
        char costText[32];
        snprintf(costText, sizeof(costText), "$%d", upgrade->GetCost());
        float r = canAfford ? 1.0f : 1.0f;
        float g = canAfford ? 0.8f : 0.3f;
        float b = canAfford ? 0.0f : 0.3f;
        RenderText(costText, btn.x + btn.width - 90.0f, btn.y + 25.0f, r, g, b, 2.0f);
    }
}

void ShopUI::RenderAbilityButton(const Button& btn) {
    const Ability* ability = shopManager->GetAbility(btn.abilityType);
    if (!ability) return;
    
    bool canAfford = shopManager->GetBalance() >= ability->cost;
    bool unlocked = ability->unlocked;
    
    // Button background
    if (btn.isHovered && !unlocked) {
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.25f, 0.25f, 0.35f, 0.95f);
    } else {
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.15f, 0.15f, 0.2f, 0.9f);
    }
    
    // Button border
    float br = unlocked ? 0.3f : (canAfford ? 0.5f : 0.6f);
    float bg = unlocked ? 0.3f : (canAfford ? 0.3f : 0.2f);
    float bb = unlocked ? 0.3f : (canAfford ? 1.0f : 0.2f);
    RenderColoredQuad(btn.x, btn.y, btn.width, 2.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y + btn.height - 2.0f, btn.width, 2.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y, 2.0f, btn.height, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x + btn.width - 2.0f, btn.y, 2.0f, btn.height, br, bg, bb, 1.0f);
    
    // Name
    RenderText(ability->name.c_str(), btn.x + 10.0f, btn.y + 15.0f, 1.0f, 1.0f, 1.0f, 1.8f);
    
    // Status
    if (unlocked) {
        RenderText("UNLOCKED", btn.x + 10.0f, btn.y + 40.0f, 0.2f, 1.0f, 0.2f, 1.5f);
    } else {
        char costText[32];
        snprintf(costText, sizeof(costText), "$%d", ability->cost);
        float r = canAfford ? 0.5f : 1.0f;
        float g = canAfford ? 0.5f : 0.3f;
        float b = canAfford ? 1.0f : 0.3f;
        RenderText(costText, btn.x + btn.width - 90.0f, btn.y + 25.0f, r, g, b, 2.0f);
    }
}

void ShopUI::ProcessMouseClick(double mouseX, double mouseY) {
    if (!isVisible) return;
    
    // Check repair button first
    if (repairButton && repairButton->Contains(mouseX, mouseY) && carState) {
        float durability = carState->durability;
        if (durability < 100.0f) {
            int balance = shopManager->GetBalance();
            float damagePercent = (100.0f - durability) / 100.0f;
            int repairCost = static_cast<int>(balance * 0.25f * damagePercent);
            
            if (balance >= repairCost) {
                shopManager->AddMoney(-repairCost);
                carState->durability = 100.0f;
                carState->isDead = false;
                std::cout << "[SHOP] Carro reparado! Costo: $" << repairCost << std::endl;
            } else {
                std::cout << "[SHOP] No tienes suficiente dinero para reparar" << std::endl;
            }
        }
        return;
    }
    
    for (const auto& btn : buttons) {
        if (btn.Contains(mouseX, mouseY)) {
            if (btn.isUpgrade) {
                if (shopManager->PurchaseUpgrade(btn.upgradeType)) {
                    std::cout << "Mejora comprada!" << std::endl;
                } else {
                    std::cout << "No se pudo comprar (fondos/nivel)" << std::endl;
                }
            } else {
                if (shopManager->PurchaseAbility(btn.abilityType)) {
                    std::cout << "Habilidad desbloqueada!" << std::endl;
                } else {
                    std::cout << "No se pudo desbloquear" << std::endl;
                }
            }
            break;
        }
    }
}

void ShopUI::ProcessMouseMove(double mouseX, double mouseY) {
    if (!isVisible) return;
    
    for (auto& btn : buttons) {
        btn.isHovered = btn.Contains(mouseX, mouseY);
    }
    
    if (repairButton) {
        repairButton->isHovered = repairButton->Contains(mouseX, mouseY);
    }
}

std::string ShopUI::FormatUpgradeInfo(UpgradeType type) const {
    const Upgrade* upgrade = shopManager->GetUpgrade(type);
    if (!upgrade) return "";
    
    std::ostringstream oss;
    oss << upgrade->name << " [Nv." << upgrade->currentLevel << "/" << upgrade->maxLevel << "]";
    
    if (upgrade->CanUpgrade()) {
        oss << " - $" << upgrade->GetCost();
    } else {
        oss << " - MAX";
    }
    
    return oss.str();
}

std::string ShopUI::FormatAbilityInfo(AbilityType type) const {
    const Ability* ability = shopManager->GetAbility(type);
    if (!ability) return "";
    
    std::ostringstream oss;
    oss << ability->name;
    
    if (ability->unlocked) {
        oss << " - DESBLOQUEADA";
    } else {
        oss << " - $" << ability->cost;
    }
    
    return oss.str();
}

void ShopUI::RenderText(const char* text, float x, float y, float r, float g, float b, float scale) {
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

void ShopUI::RenderTextCentered(const char* text, float cx, float y, float r, float g, float b, float s) {
    float w = GetTextWidth(text, s);
    RenderText(text, cx - w * 0.5f, y, r, g, b, s);
}

void ShopUI::RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b, float a) {
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

void ShopUI::RenderRepairButton(const Button& btn) {
    if (!carState) return;
    
    float durability = carState->durability;
    bool isDead = carState->isDead;
    
    // Calcular costo de reparación: 25% del balance si está a 0%, proporcional si no
    int balance = shopManager->GetBalance();
    float damagePercent = (100.0f - durability) / 100.0f;
    int repairCost = static_cast<int>(balance * 0.25f * damagePercent);
    
    bool canAfford = balance >= repairCost && durability < 100.0f;
    bool needsRepair = durability < 100.0f;
    
    // Botón background
    if (btn.isHovered && needsRepair) {
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.25f, 0.25f, 0.35f, 0.95f);
    } else {
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.15f, 0.15f, 0.2f, 0.9f);
    }
    
    // Border
    float br, bg, bb;
    if (!needsRepair) {
        br = bg = bb = 0.3f; // Gris si no necesita
    } else if (isDead) {
        br = 1.0f; bg = 0.0f; bb = 0.0f; // Rojo si está muerto
    } else if (canAfford) {
        br = 0.0f; bg = 1.0f; bb = 0.5f; // Verde si puede
    } else {
        br = 1.0f; bg = 0.3f; bb = 0.0f; // Naranja si no puede
    }
    
    RenderColoredQuad(btn.x, btn.y, btn.width, 2.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y + btn.height - 2.0f, btn.width, 2.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y, 2.0f, btn.height, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x + btn.width - 2.0f, btn.y, 2.0f, btn.height, br, bg, bb, 1.0f);
    
    // Texto
    if (isDead) {
        RenderText("REPAIR CAR", btn.x + 10.0f, btn.y + 15.0f, 1.0f, 0.2f, 0.2f, 2.2f);
        RenderText("(BROKEN)", btn.x + 10.0f, btn.y + 40.0f, 1.0f, 0.5f, 0.0f, 1.5f);
    } else {
        RenderText("REPAIR", btn.x + 10.0f, btn.y + 15.0f, 1.0f, 1.0f, 1.0f, 2.2f);
        char durText[32];
        snprintf(durText, sizeof(durText), "Health: %.0f%%", durability);
        RenderText(durText, btn.x + 10.0f, btn.y + 40.0f, 0.7f, 0.7f, 0.7f, 1.5f);
    }
    
    // Costo
    if (needsRepair) {
        char costText[32];
        snprintf(costText, sizeof(costText), "$%d", repairCost);
        RenderText(costText, btn.x + btn.width - 90.0f, btn.y + 25.0f, 1.0f, 0.8f, 0.0f, 2.0f);
    } else {
        RenderText("100%", btn.x + btn.width - 80.0f, btn.y + 25.0f, 0.2f, 1.0f, 0.2f, 2.0f);
    }
}
