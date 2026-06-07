#include "DayNightCycle.h"

#include <glm/gtc/matrix_transform.hpp>

DayNightCycle::DayNightCycle()
{
    dayTime = 12.0f;
    daySpeed = 0.05f;

    keyframes =
    {
        {0.0f,  glm::vec3(0.015f, 0.015f, 0.05f), glm::vec3(0.02f, 0.02f, 0.08f), glm::vec3(0.2f, 0.2f, 0.35f), glm::vec3(0.0f, -1.0f, 0.0f), 0.12f, glm::vec3(0.06f, 0.08f, 0.22f)},
        {5.0f,  glm::vec3(0.015f, 0.015f, 0.05f), glm::vec3(0.02f, 0.02f, 0.08f), glm::vec3(0.2f, 0.2f, 0.35f), glm::vec3(0.0f, -1.0f, 0.0f), 0.12f, glm::vec3(0.06f, 0.08f, 0.22f)},
        {6.5f,  glm::vec3(0.1f, 0.15f, 0.35f), glm::vec3(0.85f, 0.45f, 0.25f), glm::vec3(0.8f, 0.5f, 0.35f), glm::vec3(1.0f, 0.2f, 0.0f), 0.15f, glm::vec3(0.9f, 0.65f, 0.5f)},
        {12.0f, glm::vec3(0.12f, 0.32f, 0.72f), glm::vec3(0.55f, 0.72f, 0.92f), glm::vec3(1.0f, 1.0f, 0.95f), glm::vec3(0.2f, 1.0f, 0.2f), 0.22f, glm::vec3(1.0f, 1.0f, 1.0f)},
        {17.5f, glm::vec3(0.12f, 0.32f, 0.72f), glm::vec3(0.55f, 0.72f, 0.92f), glm::vec3(1.0f, 1.0f, 0.95f), glm::vec3(0.2f, 1.0f, 0.2f), 0.22f, glm::vec3(1.0f, 1.0f, 1.0f)},
        {19.0f, glm::vec3(0.08f, 0.08f, 0.25f), glm::vec3(0.88f, 0.28f, 0.12f), glm::vec3(0.85f, 0.35f, 0.15f), glm::vec3(-1.0f, 0.15f, 0.0f), 0.15f, glm::vec3(0.95f, 0.5f, 0.3f)},
        {20.5f, glm::vec3(0.03f, 0.03f, 0.12f), glm::vec3(0.08f, 0.06f, 0.18f), glm::vec3(0.3f, 0.25f, 0.4f), glm::vec3(-1.0f, -0.2f, 0.0f), 0.14f, glm::vec3(0.2f, 0.2f, 0.4f)},
        {24.0f, glm::vec3(0.015f, 0.015f, 0.05f), glm::vec3(0.02f, 0.02f, 0.08f), glm::vec3(0.2f, 0.2f, 0.35f), glm::vec3(0.0f, -1.0f, 0.0f), 0.12f, glm::vec3(0.06f, 0.08f, 0.22f)}
    };

    UpdateInterpolatedValues();
}

void DayNightCycle::Update(float dt)
{
    dayTime += dt * daySpeed;

    if (dayTime >= 24.0f)
    {
        dayTime -= 24.0f;
    }

    UpdateInterpolatedValues();
}

void DayNightCycle::UpdateInterpolatedValues()
{
    for (size_t i = 0; i < keyframes.size() - 1; ++i)
    {
        if (dayTime >= keyframes[i].hour &&
            dayTime <= keyframes[i + 1].hour)
        {
            float t =
                (dayTime - keyframes[i].hour) /
                (keyframes[i + 1].hour - keyframes[i].hour);

            currentZenith =
                glm::mix(keyframes[i].zenithColor,
                         keyframes[i + 1].zenithColor,
                         t);

            currentHorizon =
                glm::mix(keyframes[i].horizonColor,
                         keyframes[i + 1].horizonColor,
                         t);

            currentLightColor =
                glm::mix(keyframes[i].lightColor,
                         keyframes[i + 1].lightColor,
                         t);

            currentLightPos =
                glm::mix(keyframes[i].lightPos,
                         keyframes[i + 1].lightPos,
                         t);

            currentAmbient =
                glm::mix(keyframes[i].ambientStrength,
                         keyframes[i + 1].ambientStrength,
                         t);

            currentSkyTint =
                glm::mix(keyframes[i].skyTint,
                         keyframes[i + 1].skyTint,
                         t);

            break;
        }
    }
}

float DayNightCycle::GetTime() const
{
    return dayTime;
}

glm::vec3 DayNightCycle::GetZenithColor() const
{
    return currentZenith;
}

glm::vec3 DayNightCycle::GetHorizonColor() const
{
    return currentHorizon;
}

glm::vec3 DayNightCycle::GetLightColor() const
{
    return currentLightColor;
}

glm::vec3 DayNightCycle::GetLightPosition() const
{
    return currentLightPos;
}

float DayNightCycle::GetAmbientStrength() const
{
    return currentAmbient;
}

glm::vec3 DayNightCycle::GetSkyTint() const
{
    return currentSkyTint;
}