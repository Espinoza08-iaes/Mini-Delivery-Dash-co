#ifndef STREET_LAMP_H
#define STREET_LAMP_H

#include <glm/glm.hpp>
#include <vector>

struct CarState;

struct LampPoleState
{
    glm::vec3 basePosition;
    glm::vec3 fallAxis;
    float fallAngle;
    float fallVelocity;
    float respawnTimer;
    float shakeAmount;
    bool fallStarted;
};

class Mesh;
class Shader;
class Camera;
class Frustum;

void UpdateStreetLamps(std::vector<glm::vec3>& lampPositions,
                       std::vector<LampPoleState>& lampStates,
                       const CarState& car,
                       const glm::vec3& prevCarPosition,
                       float dt);

void SendStreetLightUniforms(unsigned int shaderProgramId,
                             const std::vector<glm::vec3>& lampPositions,
                             const std::vector<LampPoleState>& lampStates,
                             const CarState& car);

void RenderStreetLamps(Mesh& lampMesh,
                       Shader& lampShader,
                       Camera& camera,
                       const Frustum& cameraFrustum,
                       const std::vector<glm::vec3>& lampPositions,
                       const std::vector<LampPoleState>& lampStates);

#endif
