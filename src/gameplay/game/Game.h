#ifndef GAME_H
#define GAME_H

#include "../helpers/Helpers.h"
class Game
{
public:
    int Run();

    private:

        

        void ResolveGroundCollision(CarState& car, City& city, float& carVerticalSpeed, bool&isOnGround, float& lastGroundHeight);

        void CheckWaterRespawn (CarState& car, City& city, float& verticalSpeed, bool& isOnGround, float& lastGroundHeight);

        void UpdateCameraEffects(GLFWwindow* window, Camera& camera, CarState& car);

        void UpdateHeadlights(Shader& shaderProgram, const CarState& car, bool headlightsOn);

        void ApplyDayNightLighting(Shader& shaderProgram, DayNightCycle&  dayNight);

        void DrawOcean(Shader& waterShader, Mesh& ocean, Camera& camera, float currentFrame, const glm::vec3& skyTint, DayNightCycle& dayNight);
};

#endif
