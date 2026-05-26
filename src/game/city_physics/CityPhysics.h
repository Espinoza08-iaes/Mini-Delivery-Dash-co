#ifndef CITY_PHYSICS_H
#define CITY_PHYSICS_H

#include <vector>
#include <glm/glm.hpp>
#include "../../engine/Model.h"

namespace game
{
    struct WorldTriangle
    {
        glm::vec3 a;
        glm::vec3 b;
        glm::vec3 c;
        glm::vec3 normal;
        glm::vec3 minBounds;
        glm::vec3 maxBounds;
        bool isRoad;
    };

    struct GroundSample
    {
        float height = 0.0f;
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        bool found = false;
    };

    class CityPhysics
    {
    public:
        // Analyzes all model meshes once at startup and caches road triangles + obstacle triangles.
        void Initialize(const Model& model, const glm::mat4& cityMatrix);

        // Samples the best matching drivable surface at the given XZ coordinate.
        bool GetGroundSample(const glm::vec3& worldPos, float currentY, GroundSample& outSample) const;

        // Samples the closest drivable road height at horizontal coordinate (x, z).
        float GetHeightAt(const Model& model, const glm::mat4& cityMatrix, float x, float z, float currentY, bool* outFound = nullptr) const;

        // Checks if the car's collision sphere intersects any obstacle triangle.
        bool CheckCollision(const glm::vec3& pos, float radius) const;

    private:
        std::vector<WorldTriangle> mRoadTriangles;
        std::vector<WorldTriangle> mObstacleTriangles;
    };
}

#endif
 