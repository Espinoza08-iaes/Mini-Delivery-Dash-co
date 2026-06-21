#include "StreetLamp.h"

#include "../vehicle/CarController.h"
#include "../../engine/graphics/Shader.h"
#include "../../engine/graphics/Camera.h"
#include "../../engine/graphics/Frustum.h"
#include "../../engine/graphics/Mesh.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <cstdlib>

// Updates state for all light poles: handles collisions with car and falling/respawning animation.
void UpdateStreetLamps(std::vector<glm::vec3>& lampPositions,
                       std::vector<LampPoleState>& lampStates,
                       const CarState& car,
                       const glm::vec3& prevCarPosition,
                       float dt)
{
    if (lampStates.empty())
    {
        lampStates.resize(lampPositions.size());
        for (size_t i = 0; i < lampStates.size(); ++i)
        {
            lampStates[i].basePosition = lampPositions[i];
            lampStates[i].fallAxis = glm::vec3(0.0f, 0.0f, 1.0f);
            lampStates[i].fallAngle = 0.0f;
            lampStates[i].fallVelocity = 0.0f;
            lampStates[i].respawnTimer = 0.0f;
            lampStates[i].shakeAmount = 0.0f;
            lampStates[i].fallStarted = false;
        }
    }

    if (lampStates.size() != lampPositions.size())
    {
        std::vector<LampPoleState> newStates;
        for (size_t i = 0; i < lampPositions.size(); ++i)
        {
            LampPoleState state;
            if (i < lampStates.size())
            {
                state = lampStates[i];
            }
            else
            {
                state.basePosition = lampPositions[i];
                state.fallAxis = glm::vec3(0.0f, 0.0f, 1.0f);
                state.fallAngle = 0.0f;
                state.fallVelocity = 0.0f;
                state.respawnTimer = 0.0f;
                state.shakeAmount = 0.0f;
                state.fallStarted = false;
            }
            newStates.push_back(state);
        }
        lampStates = std::move(newStates);
    }

    const float poleRadius = 0.13f;
    const float carRadius = kCarModelScale * 1.3f;
    const float frontOffset = kCarModelScale * 1.1f;
    const float rearOffset = kCarModelScale * 1.1f;
    const float hitDistance = poleRadius + carRadius + 0.1f;
    const float maxFallAngle = glm::radians(95.0f);
    const float respawnDelay = 10.0f;

    const glm::vec3 carForward = glm::vec3(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
    const float verticalOffset = kCarGroundYOffset;

    auto sampleSegment = [&](const glm::vec3& segStart, const glm::vec3& segEnd, const glm::vec3& poleBase, int samples) -> float
    {
        float minDistSq = std::numeric_limits<float>::max();
        for (int s = 0; s <= samples; ++s)
        {
            float t = static_cast<float>(s) / static_cast<float>(samples);
            glm::vec3 p = glm::mix(segStart, segEnd, t);
            float dSq = glm::dot(p - poleBase, p - poleBase);
            if (dSq < minDistSq)
                minDistSq = dSq;
        }
        return std::sqrt(minDistSq);
    };

    for (size_t i = 0; i < lampStates.size(); ++i)
    {
        LampPoleState& state = lampStates[i];
        const glm::vec3& base = state.basePosition;

        if (state.respawnTimer > 0.0f)
        {
            state.respawnTimer -= dt;
            if (state.respawnTimer <= 0.0f)
            {
                state.respawnTimer = 0.0f;
                state.fallAngle = 0.0f;
                state.fallVelocity = 0.0f;
                state.shakeAmount = 0.0f;
                state.fallAxis = glm::vec3(0.0f, 0.0f, 1.0f);
                state.fallStarted = false;
                lampPositions[i] = base;
            }
            continue;
        }

        if (state.fallStarted)
        {
            const float angularAcceleration = 8.0f;
            const float angularDamping = 2.2f + (state.fallAngle / maxFallAngle) * 1.8f;
            state.fallVelocity += (angularAcceleration - angularDamping * state.fallVelocity) * dt;
            state.fallAngle += state.fallVelocity * dt;
            if (state.fallAngle >= maxFallAngle - 0.0001f)
            {
                state.fallAngle = maxFallAngle;
                state.fallVelocity = 0.0f;
                state.respawnTimer = respawnDelay;
                state.shakeAmount = 0.0f;
                state.fallStarted = false;
            }
            else if (state.shakeAmount > 0.0f)
            {
                state.shakeAmount *= std::exp(-5.5f * dt);
                if (state.shakeAmount < 0.004f)
                    state.shakeAmount = 0.0f;
            }
            continue;
        }

        if (state.fallAngle >= maxFallAngle - 0.001f && state.respawnTimer <= 0.0f)
        {
            state.respawnTimer = respawnDelay;
            state.fallVelocity = 0.0f;
            state.shakeAmount = 0.0f;
            state.fallStarted = false;
            continue;
        }

        glm::vec3 movement = car.position - prevCarPosition;
        float moveLen = glm::length(movement);
        if (moveLen < 0.0001f)
            continue;

        bool hit = false;
        const int sweepSamples = std::max(4, static_cast<int>(std::ceil(moveLen / 0.15f)));

        const glm::vec3 pointsPrev[] = {
            prevCarPosition + glm::vec3(0.0f, verticalOffset, 0.0f),
            prevCarPosition + carForward * frontOffset + glm::vec3(0.0f, verticalOffset, 0.0f),
            prevCarPosition - carForward * rearOffset + glm::vec3(0.0f, verticalOffset, 0.0f)
        };

        const glm::vec3 pointsCurr[] = {
            car.position + glm::vec3(0.0f, verticalOffset, 0.0f),
            car.position + carForward * frontOffset + glm::vec3(0.0f, verticalOffset, 0.0f),
            car.position - carForward * rearOffset + glm::vec3(0.0f, verticalOffset, 0.0f)
        };

        for (int p = 0; p < 3 && !hit; ++p)
        {
            float d = sampleSegment(pointsPrev[p], pointsCurr[p], base, sweepSamples);
            if (d <= hitDistance)
                hit = true;
        }

        if (!hit)
            continue;

        glm::vec3 carVel = moveLen > 0.0001f ? glm::vec3(movement.x, 0.0f, movement.z) / moveLen : carForward;
        glm::vec3 toPole = glm::vec3(base.x - car.position.x, 0.0f, base.z - car.position.z);
        float forwardDot = glm::dot(carVel, toPole);
        glm::vec3 perp = toPole - carVel * forwardDot;

        if (glm::dot(perp, perp) > 0.0001f)
        {
            state.fallAxis = glm::normalize(perp);
        }
        else
        {
            float angle = static_cast<float>(std::rand()) / RAND_MAX * 6.28318530718f;
            state.fallAxis = glm::normalize(glm::vec3(std::cos(angle), 0.0f, std::sin(angle)));
        }

        state.fallAngle = 0.0f;
        state.fallVelocity = 2.8f + std::min(moveLen, 9.0f) * 0.45f;
        state.shakeAmount = 0.5f;
        state.fallStarted = true;
    }
}

void SendStreetLightUniforms(unsigned int shaderProgramId,
                             const std::vector<glm::vec3>& lampPositions,
                             const std::vector<LampPoleState>& lampStates,
                             const CarState& car)
{
    const int MAX_SL = 8;

    std::vector<std::pair<float, int>> byDist;
    byDist.reserve(lampPositions.size());
    for (int i = 0; i < (int)lampPositions.size(); ++i)
    {
        glm::vec3 diff = lampPositions[i] - car.position;
        diff.y = 0.0f;
        byDist.push_back({glm::dot(diff, diff), i});
    }

    int selectCount = std::min(MAX_SL, (int)byDist.size());
    if (selectCount > 0)
    {
        std::nth_element(byDist.begin(), byDist.begin() + selectCount - 1, byDist.end());
        std::sort(byDist.begin(), byDist.begin() + selectCount);
    }

    auto isLampOff = [&](int idx) -> bool {
        if (idx >= 0 && idx < (int)lampStates.size())
        {
            const auto& s = lampStates[idx];
            return s.fallStarted || s.respawnTimer > 0.0f;
        }
        return false;
    };

    for (int i = 0; i < MAX_SL; ++i)
    {
        glm::vec3 lp;
        if (i < selectCount)
        {
            int idx = byDist[i].second;
            if (isLampOff(idx))
                lp = glm::vec3(0.0f, -90000.0f, 0.0f);
            else
                lp = lampPositions[idx] + glm::vec3(0.0f, 4.7f, 0.0f);
        }
        else
        {
            lp = glm::vec3(0.0f, -90000.0f, 0.0f);
        }
        std::string uname = "uStreetLightPos[" + std::to_string(i) + "]";
        glUniform3f(glGetUniformLocation(shaderProgramId, uname.c_str()), lp.x, lp.y, lp.z);
    }
}

void RenderStreetLamps(Mesh& lampMesh,
                       Shader& lampShader,
                       Camera& camera,
                       const Frustum& cameraFrustum,
                       const std::vector<glm::vec3>& lampPositions,
                       const std::vector<LampPoleState>& lampStates)
{
    int offMask = 0;
    for (size_t k = 0; k < lampStates.size() && k < 8; ++k)
    {
        const auto& s = lampStates[k];
        if (s.fallStarted || s.respawnTimer > 0.0f)
            offMask |= (1 << static_cast<int>(k));
    }
    lampShader.Activate();
    glUniform1i(glGetUniformLocation(lampShader.ID, "uOffMask"), offMask);

    for (size_t i = 0; i < lampPositions.size(); ++i)
    {
        const auto& pos = lampPositions[i];
        glm::vec3 diff = pos - camera.Position;
        if (glm::dot(diff, diff) > 300.0f * 300.0f)
            continue;
        if (!cameraFrustum.IsSphereVisible(pos + glm::vec3(0.0f, 2.5f, 0.0f), 3.5f))
            continue;

        glUniform1i(glGetUniformLocation(lampShader.ID, "uLightIdx"), static_cast<int>(i));

        glm::mat4 lampMatrix = glm::translate(glm::mat4(1.0f), pos);

        if (i < lampStates.size())
        {
            const LampPoleState& state = lampStates[i];
            if (state.fallAngle > 0.001f)
            {
                float progress = state.fallAngle / glm::radians(95.0f);
                progress = glm::clamp(progress, 0.0f, 1.0f);
                float eased = progress * progress * (3.0f - 2.0f * progress);
                float angle = eased * state.fallAngle;

                lampMatrix = glm::rotate(lampMatrix, angle, state.fallAxis);

                float billow = 1.0f - 0.15f * std::sin(progress * 3.14159265f);
                lampMatrix = glm::scale(lampMatrix, glm::vec3(billow, 1.0f, billow));
            }

            if (state.shakeAmount > 0.005f && state.fallAngle < glm::radians(90.0f))
            {
                float shakeMagnitude = state.shakeAmount * 0.018f;
                float shakeX = std::sin(state.shakeAmount * 31.0f) * shakeMagnitude * state.fallAxis.x;
                float shakeZ = std::sin(state.shakeAmount * 31.0f) * shakeMagnitude * state.fallAxis.z;
                lampMatrix[3][0] += shakeX;
                lampMatrix[3][2] += shakeZ;
            }
        }

        lampMesh.Draw(lampShader, camera, lampMatrix);
    }
}
