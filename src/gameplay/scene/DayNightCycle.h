#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct SkyKeyframe
{
    float hour;

    glm::vec3 zenithColor;
    glm::vec3 horizonColor;

    glm::vec3 lightColor;
    glm::vec3 lightPos;

    float ambientStrength;

    glm::vec3 skyTint;
};

class DayNightCycle
{
public:

    DayNightCycle();

    void Update(float dt);

    float GetTime() const;

    glm::vec3 GetZenithColor() const;
    glm::vec3 GetHorizonColor() const;

    glm::vec3 GetLightColor() const;
    glm::vec3 GetLightPosition() const;

    float GetAmbientStrength() const;

    glm::vec3 GetSkyTint() const;

private:

    void UpdateInterpolatedValues();

private:

    float dayTime;
    float daySpeed;

    std::vector<SkyKeyframe> keyframes;

    glm::vec3 currentZenith;
    glm::vec3 currentHorizon;

    glm::vec3 currentLightColor;
    glm::vec3 currentLightPos;

    glm::vec3 currentSkyTint;

    float currentAmbient;
};