#pragma once

#include "ShopManager.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

// Forward declaration
struct CarState;

struct Button {
    float x, y, width, height;
    UpgradeType upgradeType;
    AbilityType abilityType;
    bool isUpgrade;
    bool isRepair;
    bool isBack;
    bool isHovered;
    
    Button(float _x, float _y, float _w, float _h, UpgradeType ut)
        : x(_x), y(_y), width(_w), height(_h), upgradeType(ut), abilityType(), isUpgrade(true), isRepair(false), isBack(false), isHovered(false) {}
    
    Button(float _x, float _y, float _w, float _h, AbilityType at)
        : x(_x), y(_y), width(_w), height(_h), upgradeType(), abilityType(at), isUpgrade(false), isRepair(false), isBack(false), isHovered(false) {}
    
    // Constructor para botón de reparación o back
    Button(float _x, float _y, float _w, float _h, bool repair, bool back)
        : x(_x), y(_y), width(_w), height(_h), upgradeType(), abilityType(), isUpgrade(false), isRepair(repair), isBack(back), isHovered(false) {}
    
    bool Contains(double mx, double my) const {
        return mx >= x && mx <= x + width && my >= y && my <= y + height;
    }
};

class ShopUI {
private:
    GLFWwindow* window;
    ShopManager* shopManager;
    CarState* carState;  // Referencia al estado del carro
    bool isVisible;
    unsigned int backgroundTexture = 0;
    
    unsigned int quadVAO, quadVBO;
    unsigned int textVAO, textVBO;
    unsigned int hudProgram;
    unsigned int textProgram;
    
    std::vector<Button> buttons;
    Button* repairButton;
    
    void SetupGraphics();
    void SetupTextRendering();
    void BuildButtons();
    
    void RenderText(const char* text, float x, float y, float r, float g, float b, float scale = 1.0f);
    void RenderTextCentered(const char* text, float cx, float y, float r, float g, float b, float scale = 1.0f);
    void RenderColoredQuad(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
    
    void RenderButton(const Button& btn);
    void RenderUpgradeButton(const Button& btn);
    void RenderAbilityButton(const Button& btn);
    void RenderRepairButton(const Button& btn);
    void RenderBackButton(const Button& btn);
    
    std::string FormatUpgradeInfo(UpgradeType type) const;
    std::string FormatAbilityInfo(AbilityType type) const;
    
public:
    ShopUI(GLFWwindow* w);
    ~ShopUI();
    
    void SetCarState(CarState* car) { carState = car; }
    
    void Show();
    void Hide();
    void Toggle();
    void Update();
    bool IsVisible() const { return isVisible; }
    void SetBackgroundTexture(unsigned int tex) { backgroundTexture = tex; }

    void Render();
    void ProcessMouseClick(double mouseX, double mouseY);
    void ProcessMouseMove(double mouseX, double mouseY);
};
