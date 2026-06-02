#include "City.h"
 
#include <glm/gtc/matrix_transform.hpp>
 
City::City(const std::string& modelPath, float scale, float yOffset, float xOffset, float zOffset, bool autoAlign)
    : mModel(modelPath.c_str())
    , mScale(scale)
    , mYOffset(yOffset)
    , mXOffset(xOffset)
    , mZOffset(zOffset)
{
    if (autoAlign)
    {
        // Use the untransformed model bounds to align the lowest vertex with the ground.
        mPhysics.Initialize(mModel, glm::mat4(1.0f));
        glm::vec3 boundsMin = mPhysics.GetWorldMinBounds();
        mYOffset = -boundsMin.y * mScale + 0.01f;
    }

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

float City::GetHeightAt(float x, float z, float currentY, bool* outFound, float snapDownMax, float snapUpMax) const
{
    return mPhysics.GetHeightAt(mModel, GetMatrix(), x, z, currentY, outFound, snapDownMax, snapUpMax);
}

bool City::GetGroundSample(const glm::vec3& worldPos, float currentY, game::GroundSample& outSample, float snapDownMax, float snapUpMax) const
{
    return mPhysics.GetGroundSample(worldPos, currentY, outSample, snapDownMax, snapUpMax);
}

bool City::CheckCollision(const glm::vec3& pos, float radius) const
{
    return mPhysics.CheckCollision(pos, radius);
}

glm::vec3 City::GetBestRoadSpawn(const glm::vec3& preferred, float maxDistance) const
{
    return mPhysics.GetBestRoadSpawn(preferred, maxDistance);
}

glm::vec3 City::GetWorldMinBounds() const
{
    return mPhysics.GetWorldMinBounds();
}

glm::vec3 City::GetWorldMaxBounds() const
{
    return mPhysics.GetWorldMaxBounds();
}
 