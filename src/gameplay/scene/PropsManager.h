#ifndef PROPS_MANAGER_H
#define PROPS_MANAGER_H

#include <vector>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../../engine/graphics/Mesh.h"
#include "../../engine/graphics/Camera.h"
#include "../../engine/graphics/Shader.h"
#include "../city/City.h"

struct DestructibleProp {
    glm::vec3 position;
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 angularVelocity = glm::vec3(0.0f);
    float scale = 1.0f;
    int type = 0; // 0 = Cone, 1 = Box
    bool hit = false;
    float hitTime = 0.0f;
    
    glm::vec3 basePosition;
    glm::quat baseRotation;
};

class PropsManager {
public:
    PropsManager();
    ~PropsManager();

    void Initialize(const std::vector<glm::vec3>& lampPositions, const City& city);
    
    void Update(float dt, const glm::vec3& carPos, const glm::vec3& carVelocity, float carRadius, const City& city);
    
    void Draw(Shader& shader, Camera& camera);

    void ResetAll();

private:
    std::vector<DestructibleProp> mProps;
    
    std::unique_ptr<Mesh> mConeMesh;
    std::unique_ptr<Mesh> mBoxMesh;

    void CreateConeMesh();
    void CreateBoxMesh();
};

#endif
