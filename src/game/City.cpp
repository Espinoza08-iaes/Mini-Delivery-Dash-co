#include "City.h"
 
#include <glm/gtc/matrix_transform.hpp>
 
City::City(const std::string& modelPath, float scale, float yOffset, float xOffset, float zOffset)
    : mModel(modelPath.c_str())
    , mScale(scale)
    , mYOffset(yOffset)
    , mXOffset(xOffset)
    , mZOffset(zOffset)
{
}
 
glm::mat4 City::GetMatrix() const
{
    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, glm::vec3(mXOffset, mYOffset, mZOffset));
    transform = glm::scale(transform, glm::vec3(mScale));
    return transform;
}
 
void City::Draw(Shader& shader, Camera& camera)
{
    mModel.Draw(shader, camera, GetMatrix());
}
 