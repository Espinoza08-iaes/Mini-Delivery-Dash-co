#ifndef CITY_PHYSICS_H
#define CITY_PHYSICS_H

#include <vector>
#include <glm/glm.hpp>
#include "../../engine/resources/Model.h"

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
        bool nonBlockingSurface;
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
        bool GetGroundSample(const glm::vec3& worldPos, float currentY, GroundSample& outSample, float snapDownMax = 8.0f, float snapUpMax = 0.12f) const;

        // Samples the closest drivable road height at horizontal coordinate (x, z).
        float GetHeightAt(const Model& model, const glm::mat4& cityMatrix, float x, float z, float currentY, bool* outFound = nullptr, float snapDownMax = 8.0f, float snapUpMax = 0.12f) const;

        // Checks if the car's collision sphere intersects any obstacle triangle.
        bool CheckCollision(const glm::vec3& pos, float radius) const;

        // Returns the closest road triangle center to the preferred spawn position.
        glm::vec3 GetBestRoadSpawn(const glm::vec3 &preferred, float maxDistance = 1000.0f) const;

        glm::vec3 GetWorldMinBounds() const { return mWorldMinBounds; }
        glm::vec3 GetWorldMaxBounds() const { return mWorldMaxBounds; }

    private:
        struct GridCell
        {
            std::vector<size_t> roadTriangleIndices;
            std::vector<size_t> obstacleTriangleIndices;
        };

        int GetCellCol(float x) const;
        int GetCellRow(float z) const;

        std::vector<WorldTriangle> mRoadTriangles;
        std::vector<WorldTriangle> mObstacleTriangles;
        glm::vec3 mWorldMinBounds;
        glm::vec3 mWorldMaxBounds;

        float mGridCellSize;
        int mGridCols;
        int mGridRows;
        std::vector<GridCell> mGrid;
    };
}

#endif
