#ifndef PATHFINDER_H
#define PATHFINDER_H

#include <vector>
#include <glm/glm.hpp>
#include "../city/CityPhysics.h"

class dtNavMesh;
class dtNavMeshQuery;

class Pathfinder {
public:
    Pathfinder();
    ~Pathfinder();

    void Initialize(const std::vector<game::WorldTriangle>& roadTriangles, const glm::vec3& minBounds, const glm::vec3& maxBounds);
    
    std::vector<glm::vec3> FindPath(const glm::vec3& start, const glm::vec3& end) const;
    glm::vec3 GetRandomNavPoint() const;
    glm::vec3 FindNearestPointOnNavMesh(const glm::vec3& pos) const;
    bool IsOnNavMesh(const glm::vec3& pos, float tolerance = 3.0f) const;
    void MarkAreaAsObstacle(const glm::vec3& pos, float radius);
    
    bool IsInitialized() const { return mNavMesh != nullptr; }

private:
    void Cleanup();

    dtNavMesh* mNavMesh;
    dtNavMeshQuery* mNavQuery;
};

#endif // PATHFINDER_H
