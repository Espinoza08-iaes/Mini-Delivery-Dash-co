#ifndef PAQUETE_H
#define PAQUETE_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../../engine/graphics/Mesh.h"
#include "../../engine/graphics/Shader.h"
#include "../../engine/graphics/Camera.h"

class Paquete
{
public:
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    
    Mesh mesh;
    
    Paquete(const glm::vec3& pos = glm::vec3(0.0f));
    
    void Render(Shader& shader, Camera& camera);
    void SetPosition(const glm::vec3& pos);
    void SetRotation(const glm::quat& rot);
    void SetScale(const glm::vec3& s);
    
private:
    Mesh CreatePackageMesh();
};

#endif
