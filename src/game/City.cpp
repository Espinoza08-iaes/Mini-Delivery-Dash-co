#include "City.h"
 
#include <glm/gtc/matrix_transform.hpp>
 
City::City(const std::string& modelPath, float scale, float yOffset, float xOffset, float zOffset)
    : mModel(modelPath.c_str())
    , mScale(scale)
    , mYOffset(yOffset)
    , mXOffset(xOffset)
    , mZOffset(zOffset)
{
    mPhysics.Initialize(mModel, GetMatrix());
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

float City::GetHeightAt(float x, float z, float currentY, bool* outFound) const
{
    return mPhysics.GetHeightAt(mModel, GetMatrix(), x, z, currentY, outFound);
}

bool City::GetGroundSample(const glm::vec3& worldPos, float currentY, game::GroundSample& outSample) const
{
    return mPhysics.GetGroundSample(worldPos, currentY, outSample);
}

bool City::CheckCollision(const glm::vec3& pos, float radius) const
{
    return mPhysics.CheckCollision(pos, radius);
}
 