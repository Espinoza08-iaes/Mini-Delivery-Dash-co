#ifndef GAME_H
#define GAME_H

#include "../helpers/Helpers.h"
#include "../city/StreetLamp.h"

// Forward declaration
class ShopUI;
struct GameSettings;

class Game
{
public:
    int Run();

private:
    // Shop system
    ShopUI* shopUI;
    
    void ResolveGroundCollision(CarState& car, City& city, float& carVerticalSpeed, bool&isOnGround, float& lastGroundHeight);

    void CheckWaterRespawn (CarState& car, City& city, float& verticalSpeed, bool& isOnGround, float& lastGroundHeight);

    void UpdateCameraEffects(GLFWwindow* window, Camera& camera, CarState& car, GameSettings* settings);

    void UpdateHeadlights(Shader& shaderProgram, const CarState& car, bool headlightsOn);

    void ApplyDayNightLighting(Shader& shaderProgram, DayNightCycle&  dayNight);

    void DrawOcean(Shader& waterShader, Mesh& ocean, Camera& camera, float currentFrame, const glm::vec3& skyTint, DayNightCycle& dayNight);
    
    // Shop callbacks (static para GLFW)
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
};

#endif
