#include "ShopUI.h"
#include "../shop/ShopManager.h"
#include "../vehicle/CarController.h"
#include <iostream>
#include <cstring>
#include <cmath>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../../../third_party/stb/stb_easy_font.h"

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
ShopUI::ShopUI(GLFWwindow* w)
    : window(w), shopManager(ShopManager::GetInstance()), carState(nullptr), isVisible(false),
      repairButton(nullptr),
      quadVAO(0), quadVBO(0), hudProgram(0),
      textVAO(0), textVBO(0), textProgram(0)
{
    SetupGraphics();
    SetupTextRendering();
}

ShopUI::~ShopUI()
{
    glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &textVAO); glDeleteBuffers(1, &textVBO);
    if (hudProgram) glDeleteProgram(hudProgram);
    if (textProgram) glDeleteProgram(textProgram);
}

// ======================================================================
// SETUP
// ======================================================================
void ShopUI::SetupGraphics()
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

void ShopUI::SetupTextRendering()
{
    const char* vs = R"(#version 330 core
        layout(location=0) in vec2 aPos; uniform mat4 proj, model;
        void main(){ gl_Position = proj * model * vec4(aPos,0,1); })";
    const char* fs = R"(#version 330 core
        out vec4 frag; uniform vec4 uColor; void main(){ frag = uColor; })";
    textProgram = compileShader(vs, fs);
    glGenVertexArrays(1, &textVAO); glGenBuffers(1, &textVBO);
}

// ======================================================================
// PUBLIC API
// ======================================================================
void ShopUI::Show() { isVisible = true; }
void ShopUI::Hide() { isVisible = false; }
void ShopUI::Toggle() { isVisible = !isVisible; }

void ShopUI::Update()
{
    if (carState && glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        static bool pWasPressed = false;
        if (!pWasPressed) Toggle();
        pWasPressed = true;
    }
    else { /* reset */ }
}

void ShopUI::ProcessMouseMove(double x, double y)
{
    for (auto& btn : buttons)
        btn.isHovered = btn.Contains(x, y);
}

void ShopUI::ProcessMouseClick(double x, double y)
{
    if (!isVisible) return;

    for (auto& btn : buttons)
    {
        if (btn.Contains(x, y))
        {
            if (btn.isBack)
            {
                Hide();
            }
            else if (btn.isRepair)
            {
                // Repair: restore durability
                if (carState && shopManager->GetBalance() >= 50)
                {
                    shopManager->AddMoney(-50);
                    carState->durability = 100.0f;
                }
            }
            else if (btn.isUpgrade)
            {
                shopManager->PurchaseUpgrade(btn.upgradeType);
            }
            else
            {
                shopManager->PurchaseAbility(btn.abilityType);
            }
        }
    }
}

// ======================================================================
// BUILD BUTTONS
// ======================================================================
void ShopUI::BuildButtons()
{
    buttons.clear();
    
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    
    float panelWidth  = 980.0f;
    float panelHeight = 680.0f;
    float panelX = fbW * 0.5f - panelWidth * 0.5f;
    float panelY = (fbH - panelHeight) * 0.5f;
    
    // UPGRADES (left column)
    float upgradeStartY = panelY + 160.0f;
    float upgradeX = panelX + 40.0f;
    float upgradeW = 420.0f;
    float upgradeH = 70.0f;
    float upgradeSpacing = 12.0f;
    
    UpgradeType upgradeTypes[] = {
        UpgradeType::Speed,
        UpgradeType::PayPerDelivery,
        UpgradeType::FuelEfficiency,
        UpgradeType::Handling,
        UpgradeType::Acceleration
    };
    
    for (int i = 0; i < 5; i++)
    {
        float y = upgradeStartY + i * (upgradeH + upgradeSpacing);
        buttons.push_back(Button(upgradeX, y, upgradeW, upgradeH, upgradeTypes[i]));
    }
    
    // ABILITIES (right column)
    float abilityStartY = panelY + 160.0f;
    float abilityX = panelX + panelWidth - 460.0f;
    float abilityW = 420.0f;
    float abilityH = 70.0f;
    float abilitySpacing = 12.0f;
    
    AbilityType abilityTypes[] = {
        AbilityType::Teleport,
        AbilityType::Jump,
        AbilityType::Turbo
    };
    
    for (int i = 0; i < 3; i++)
    {
        float y = abilityStartY + i * (abilityH + abilitySpacing);
        buttons.push_back(Button(abilityX, y, abilityW, abilityH, abilityTypes[i]));
    }
    
    // BACK BUTTON
    float backW = 200.0f, backH = 50.0f;
    float backX = panelX + (panelWidth - backW) * 0.5f;
    float backY = panelY + panelHeight - 80.0f;
    buttons.push_back(Button(backX, backY, backW, backH, false, true));
}

// ======================================================================
// FORMAT INFO
// ======================================================================
std::string ShopUI::FormatUpgradeInfo(UpgradeType type) const
{
    const Upgrade* upg = shopManager->GetUpgrade(type);
    if (!upg) return "";
    return "LEVEL " + std::to_string(upg->currentLevel) + "/" + std::to_string(upg->maxLevel);
}

std::string ShopUI::FormatAbilityInfo(AbilityType type) const
{
    return shopManager->IsAbilityUnlocked(type) ? "UNLOCKED" : "LOCKED";
}

// ======================================================================
// RENDER
// ======================================================================
void ShopUI::Render()
{
    if (!isVisible) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);

    BuildButtons();

    glm::mat4 proj = glm::ortho(0.f, (float)fbW, (float)fbH, 0.f);
    glUseProgram(hudProgram);
    glUniformMatrix4fv(glGetUniformLocation(hudProgram, "proj"), 1, GL_FALSE, &proj[0][0]);

    // =====================================================
    // MAIN PANEL
    // =====================================================
    if (backgroundTexture) {
        // Render the captured menu background
        glUseProgram(hudProgram);
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3((float)fbW, (float)fbH, 1.0f));
        glUniformMatrix4fv(glGetUniformLocation(hudProgram, "model"), 1, GL_FALSE, &model[0][0]);
        glUniform1i(glGetUniformLocation(hudProgram, "uSolid"), 0);
        glUniform1i(glGetUniformLocation(hudProgram, "uTex"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, backgroundTexture);
        glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Semi-transparent dark overlay for the fade effect
    RenderColoredQuad(0, 0, (float)fbW, (float)fbH, 0.0f, 0.0f, 0.0f, 0.30f);

    float panelWidth  = 980.0f;
    float panelHeight = 680.0f;
    float panelX = fbW * 0.5f - panelWidth * 0.5f;
    float panelY = (fbH - panelHeight) * 0.5f;

    // Drop shadow
    RenderColoredQuad(panelX - 8.0f, panelY - 8.0f, panelWidth + 16.0f, panelHeight + 16.0f, 0.0f, 0.0f, 0.0f, 0.30f);

    // Border
    RenderColoredQuad(panelX - 2.0f, panelY - 2.0f, panelWidth + 4.0f, panelHeight + 4.0f, 0.3f, 0.3f, 0.35f, 0.9f);

    // Main body (Dark sleek grey)
    RenderColoredQuad(panelX, panelY, panelWidth, panelHeight, 0.08f, 0.08f, 0.10f, 0.95f);

    // Header section
    RenderColoredQuad(panelX, panelY, panelWidth, 80.0f, 0.13f, 0.13f, 0.16f, 0.95f);
    RenderColoredQuad(panelX, panelY + 78.0f, panelWidth, 2.0f, 0.9f, 0.6f, 0.1f, 1.0f); // Orange separator line

    // title
    RenderTextCentered("SHOP", fbW * 0.5f, panelY + 42.0f, 0.90f, 0.90f, 0.90f, 4.0f);

    // balance
    char balanceText[64];
    snprintf(balanceText, sizeof(balanceText), "BALANCE: $%d", shopManager->GetBalance());
    RenderTextCentered(balanceText, fbW * 0.5f, panelY + 92.0f, 1.0f, 0.75f, 0.15f, 2.5f);

    // section titles
    RenderTextCentered("UPGRADES", fbW * 0.5f - 225.0f, panelY + 110.0f, 0.90f, 0.90f, 0.90f, 2.0f);
    RenderTextCentered("ABILITIES", fbW * 0.5f + 225.0f, panelY + 110.0f, 0.90f, 0.90f, 0.90f, 2.0f);

    // =====================================================
    // RENDER ALL BUTTONS
    // =====================================================
    for (const auto& btn : buttons)
    {
        if (btn.isBack)
            RenderBackButton(btn);
        else if (btn.isRepair)
            RenderRepairButton(btn);
        else if (btn.isUpgrade)
            RenderUpgradeButton(btn);
        else
            RenderAbilityButton(btn);
    }

    glEnable(GL_DEPTH_TEST);
}

// ======================================================================
// RENDER UPGRADE BUTTON
// ======================================================================
void ShopUI::RenderUpgradeButton(const Button& btn)
{
    const Upgrade* upg = shopManager->GetUpgrade(btn.upgradeType);
    if (!upg) return;

    int currentLevel = upg->currentLevel;
    bool maxLevel = (currentLevel >= upg->maxLevel);
    int cost = upg->GetCost();
    bool canAfford = !maxLevel && (shopManager->GetBalance() >= cost);

    // Background
    if (btn.isHovered && !maxLevel)
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.18f, 0.18f, 0.18f, 1.0f);
    else
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.10f, 0.10f, 0.10f, 1.0f);

    // Border
    float br, bg, bb;
    if (maxLevel)      { br = 0.45f; bg = 0.45f; bb = 0.45f; }
    else if (canAfford) { br = 0.95f; bg = 0.72f; bb = 0.18f; }
    else               { br = 0.55f; bg = 0.25f; bb = 0.25f; }

    RenderColoredQuad(btn.x, btn.y, btn.width, 3.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y + btn.height - 3.0f, btn.width, 3.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y, 3.0f, btn.height, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x + btn.width - 3.0f, btn.y, 3.0f, btn.height, br, bg, bb, 1.0f);

    // Name
    RenderText(upg->name.c_str(), btn.x + 15.0f, btn.y + 15.0f, 1.0f, 1.0f, 1.0f, 1.6f);

    // Level
    char levelText[64];
    snprintf(levelText, sizeof(levelText), "LEVEL %d/%d", currentLevel, upg->maxLevel);
    RenderText(levelText, btn.x + 15.0f, btn.y + 42.0f, 0.7f, 0.7f, 0.7f, 1.2f);

    // Cost or MAX
    char costText[32];
    if (maxLevel) snprintf(costText, sizeof(costText), "MAX");
    else          snprintf(costText, sizeof(costText), "$%d", cost);
    RenderText(costText, btn.x + btn.width - 70.0f, btn.y + 25.0f, 1.0f, 0.75f, 0.15f, 1.6f);
}

// ======================================================================
// RENDER ABILITY BUTTON
// ======================================================================
void ShopUI::RenderAbilityButton(const Button& btn)
{
    const Ability* ab = shopManager->GetAbility(btn.abilityType);
    if (!ab) return;

    bool unlocked = ab->unlocked;
    int cost = ab->cost;
    bool canAfford = !unlocked && (shopManager->GetBalance() >= cost);

    // Background
    if (btn.isHovered && !unlocked)
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.18f, 0.18f, 0.18f, 1.0f);
    else
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.10f, 0.10f, 0.10f, 1.0f);

    // Border
    float br, bg, bb;
    if (unlocked)      { br = 0.45f; bg = 0.45f; bb = 0.45f; }
    else if (canAfford) { br = 0.95f; bg = 0.72f; bb = 0.18f; }
    else               { br = 0.55f; bg = 0.25f; bb = 0.25f; }

    RenderColoredQuad(btn.x, btn.y, btn.width, 3.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y + btn.height - 3.0f, btn.width, 3.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y, 3.0f, btn.height, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x + btn.width - 3.0f, btn.y, 3.0f, btn.height, br, bg, bb, 1.0f);

    // Name
    RenderText(ab->name.c_str(), btn.x + 15.0f, btn.y + 15.0f, 1.0f, 1.0f, 1.0f, 1.6f);

    // Status
    char statusText[32];
    if (unlocked) snprintf(statusText, sizeof(statusText), "UNLOCKED");
    else          snprintf(statusText, sizeof(statusText), "$%d", cost);
    RenderText(statusText, btn.x + btn.width - 110.0f, btn.y + 25.0f, 1.0f, 0.75f, 0.15f, 1.6f);
}

// ======================================================================
// RENDER REPAIR BUTTON
// ======================================================================
void ShopUI::RenderRepairButton(const Button& btn)
{
    bool canAfford = (shopManager->GetBalance() >= 50);
    
    if (btn.isHovered && canAfford)
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.18f, 0.18f, 0.18f, 1.0f);
    else
        RenderColoredQuad(btn.x, btn.y, btn.width, btn.height, 0.10f, 0.10f, 0.10f, 1.0f);
    
    float br, bg, bb;
    if (canAfford) { br = 0.95f; bg = 0.72f; bb = 0.18f; }
    else           { br = 0.55f; bg = 0.25f; bb = 0.25f; }
    
    RenderColoredQuad(btn.x, btn.y, btn.width, 3.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y + btn.height - 3.0f, btn.width, 3.0f, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x, btn.y, 3.0f, btn.height, br, bg, bb, 1.0f);
    RenderColoredQuad(btn.x + btn.width - 3.0f, btn.y, 3.0f, btn.height, br, bg, bb, 1.0f);
    
    RenderText("REPAIR VEHICLE", btn.x + 15.0f, btn.y + 20.0f, 1.0f, 1.0f, 1.0f, 1.6f);
    RenderText("$50", btn.x + btn.width - 60.0f, btn.y + 25.0f, 1.0f, 0.75f, 0.15f, 1.6f);
}

// ======================================================================
// RENDER BACK BUTTON
// ======================================================================
void ShopUI::RenderBackButton(const Button& btn)
{
    float s = btn.isHovered ? 0.92f : 1.0f;
    float yOff = btn.isHovered ? 4.0f : 0.0f;
    
    float drawW = btn.width * s, drawH = btn.height * s;
    float drawX = btn.x + (btn.width - drawW) * 0.5f;
    float drawY = btn.y + (btn.height - drawH) * 0.5f + yOff;
    
    RenderColoredQuad(drawX, drawY, drawW, drawH, 0.1f, 0.1f, 0.1f, 1.0f);
    
    float bt = 1.2f * s, bcol = 0.3f;
    RenderColoredQuad(drawX, drawY, drawW, bt, bcol, bcol, bcol, 1.0f);
    RenderColoredQuad(drawX, drawY + drawH - bt, drawW, bt, bcol, bcol, bcol, 1.0f);
    RenderColoredQuad(drawX, drawY, bt, drawH, bcol, bcol, bcol, 1.0f);
    RenderColoredQuad(drawX + drawW - bt, drawY, bt, drawH, bcol, bcol, bcol, 1.0f);
    
    RenderTextCentered("BACK", drawX + drawW * 0.5f, drawY + (drawH - 18.0f * s) * 0.5f, 0.75f, 0.75f, 0.75f, 1.3f * s);
}

// ======================================================================
// LOW-LEVEL RENDERING
// ======================================================================
void ShopUI::RenderText(const char* text, float x, float y, float r, float g, float b, float scale)
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

void ShopUI::RenderTextCentered(const char* text, float cx, float y, float r, float g, float b, float s)
{
    float w = GetTextWidth(text, s);
    RenderText(text, cx - w * 0.5f, y, r, g, b, s);
}

void ShopUI::RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b, float a)
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