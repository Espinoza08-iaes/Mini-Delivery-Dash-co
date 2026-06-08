#include "CityPhysics.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cctype>
#include <string>

namespace game
{
    namespace
    {
        glm::vec3 ComputeTriangleNormal(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            glm::vec3 n = glm::cross(b - a, c - a);
            float length = glm::length(n);
            if (length < 1e-8f)
            {
                return glm::vec3(0.0f, 1.0f, 0.0f);
            }
            return n / length;
        }

        glm::vec3 ComponentMin(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            return glm::vec3(
                std::min(a.x, std::min(b.x, c.x)),
                std::min(a.y, std::min(b.y, c.y)),
                std::min(a.z, std::min(b.z, c.z)));
        }

        glm::vec3 ComponentMax(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            return glm::vec3(
                std::max(a.x, std::max(b.x, c.x)),
                std::max(a.y, std::max(b.y, c.y)),
                std::max(a.z, std::max(b.z, c.z)));
        }

        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool ContainsAny(const std::string &value, const char *const *needles, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
            {
                if (value.find(needles[i]) != std::string::npos)
                {
                    return true;
                }
            }
            return false;
        }

        bool ContainsExactOrTaggedMaterial(const std::string &value, const std::string &materialTag)
        {
            if (value == materialTag)
            {
                return true;
            }

            std::string paddedValue = " " + value + " ";
            std::string paddedTag = " " + materialTag + " ";
            return paddedValue.find(paddedTag) != std::string::npos;
        }

        bool IsKnownCityGroundMaterial(const std::string &surfaceName)
        {
            std::string name = ToLower(surfaceName);
            static const char *const cityGroundWords[] = {
                "yardground", "parkinglot", "pavement", "street", "concrete", "curbs"
            };
            static const char *const cityGroundMaterials[] = {
                "my_city_0facadetexture_58", // curbs
                "my_city_0facadetexture_59", // pavement
                "my_city_0facadetexture_60", // street
                "my_city_0facadetexture_61"  // concrete / plazas
            };

            if (ContainsAny(name, cityGroundWords, sizeof(cityGroundWords) / sizeof(cityGroundWords[0])))
            {
                return true;
            }

            for (size_t i = 0; i < sizeof(cityGroundMaterials) / sizeof(cityGroundMaterials[0]); ++i)
            {
                if (ContainsExactOrTaggedMaterial(name, cityGroundMaterials[i]))
                {
                    return true;
                }
            }

            return false;
        }

        bool IsWaterMaterial(const std::string &surfaceName)
        {
            std::string name = ToLower(surfaceName);

            static const char *const waterWords[] = {"water", "ocean", "sea", "lake", "river", "pond", "pool", "liquid"};

            for (size_t i = 0; i < sizeof(waterWords) / sizeof(waterWords[0]);++i)
            {
                if (name.find(waterWords[i]) != std::string::npos)
                {
                    return true;
                }
            }
            return false;
        }

        bool IsKnownCityNonBlockingGroundDetail(const std::string &surfaceName)
        {
            std::string name = ToLower(surfaceName);

            return name.find("curb") != std::string::npos ||
                   ContainsExactOrTaggedMaterial(name, "my_city_0facadetexture_58");
        }

        bool IsGenericGroundMaterial(const std::string &surfaceName)
        {
            std::string name = ToLower(surfaceName);
            static const char *const groundWords[] = {
                "street", "road", "asphalt", "pavement", "sidewalk", "concrete", "parking",
                "ground", "grass", "yard", "lawn", "terrain", "land", "soil", "plaza", "curb"
            };
            static const char *const blockedWords[] = {
                "roof", "facade", "wall", "window", "tree", "bark", "building"
            };

            return ContainsAny(name, groundWords, sizeof(groundWords) / sizeof(groundWords[0])) &&
                   !ContainsAny(name, blockedWords, sizeof(blockedWords) / sizeof(blockedWords[0]));
        }

        bool ProjectPointToTriangleXZ(const glm::vec3 &point, const WorldTriangle &tri, float &outY)
        {
            float det = (tri.b.z - tri.c.z) * (tri.a.x - tri.c.x) +
                        (tri.c.x - tri.b.x) * (tri.a.z - tri.c.z);
            if (std::abs(det) < 1e-5f)
            {
                return false;
            }

            float l1 = ((tri.b.z - tri.c.z) * (point.x - tri.c.x) +
                        (tri.c.x - tri.b.x) * (point.z - tri.c.z)) /
                       det;
            float l2 = ((tri.c.z - tri.a.z) * (point.x - tri.c.x) +
                        (tri.a.x - tri.c.x) * (point.z - tri.c.z)) /
                       det;
            float l3 = 1.0f - l1 - l2;

            const float eps = -1e-3f;
            if (l1 >= eps && l2 >= eps && l3 >= eps)
            {
                outY = l1 * tri.a.y + l2 * tri.b.y + l3 * tri.c.y;
                return true;
            }

            return false;
        }

        float ClosestPointOnSegmentXZ(const glm::vec3 &point, const glm::vec3 &start, const glm::vec3 &end, glm::vec3 &outClosest)
        {
            glm::vec2 p(point.x, point.z);
            glm::vec2 a(start.x, start.z);
            glm::vec2 b(end.x, end.z);
            glm::vec2 ab = b - a;
            float lenSq = glm::dot(ab, ab);
            if (lenSq < 1e-8f)
            {
                outClosest = start;
                return glm::dot(p - a, p - a);
            }

            float t = glm::dot(p - a, ab) / lenSq;
            t = glm::clamp(t, 0.0f, 1.0f);
            outClosest = start + (end - start) * t;
            glm::vec2 closest(outClosest.x, outClosest.z);
            return glm::dot(p - closest, p - closest);
        }

        float ClosestPointOnTriangleXZ(const glm::vec3 &point, const WorldTriangle &tri, glm::vec3 &outClosest)
        {
            glm::vec3 ab, bc, ca;
            float abDist = ClosestPointOnSegmentXZ(point, tri.a, tri.b, ab);
            float bcDist = ClosestPointOnSegmentXZ(point, tri.b, tri.c, bc);
            float caDist = ClosestPointOnSegmentXZ(point, tri.c, tri.a, ca);

            if (abDist <= bcDist && abDist <= caDist)
            {
                outClosest = ab;
                return abDist;
            }
            if (bcDist <= caDist)
            {
                outClosest = bc;
                return bcDist;
            }

            outClosest = ca;
            return caDist;
        }

        bool PointInsideTriangle2D(const glm::vec3 &point, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            glm::vec3 v1 = b - a;
            glm::vec3 v2 = c - a;
            glm::vec3 pv = point - a;

            float d00 = glm::dot(v1, v1);
            float d01 = glm::dot(v1, v2);
            float d11 = glm::dot(v2, v2);
            float d20 = glm::dot(pv, v1);
            float d21 = glm::dot(pv, v2);
            float denom = d00 * d11 - d01 * d01;

            if (std::abs(denom) < 1e-8f)
            {
                return false;
            }

            float v = (d11 * d20 - d01 * d21) / denom;
            float w = (d00 * d21 - d01 * d20) / denom;
            float u = 1.0f - v - w;

            const float eps = 1e-4f;
            return u >= -eps && v >= -eps && w >= -eps;
        }

        float SquaredDistanceToSegment(const glm::vec3 &point, const glm::vec3 &start, const glm::vec3 &end)
        {
            glm::vec3 ab = end - start;
            float lenSq = glm::dot(ab, ab);
            if (lenSq < 1e-8f)
            {
                return glm::dot(point - start, point - start);
            }

            float t = glm::dot(point - start, ab) / lenSq;
            t = glm::clamp(t, 0.0f, 1.0f);
            glm::vec3 closest = start + ab * t;
            return glm::dot(point - closest, point - closest);
        }

        bool SphereIntersectsTriangle(const glm::vec3 &center, float radius, const WorldTriangle &tri)
        {
            if (center.x < tri.minBounds.x - radius || center.x > tri.maxBounds.x + radius ||
                center.z < tri.minBounds.z - radius || center.z > tri.maxBounds.z + radius ||
                center.y < tri.minBounds.y - radius || center.y > tri.maxBounds.y + radius)
            {
                return false;
            }

            float planeDistance = glm::dot(center - tri.a, tri.normal);
            glm::vec3 projection = center - planeDistance * tri.normal;

            if (PointInsideTriangle2D(projection, tri.a, tri.b, tri.c) && std::abs(planeDistance) <= radius)
            {
                return true;
            }

            float edgeDistanceSq = std::min(
                std::min(SquaredDistanceToSegment(center, tri.a, tri.b), SquaredDistanceToSegment(center, tri.b, tri.c)),
                SquaredDistanceToSegment(center, tri.c, tri.a));

            return edgeDistanceSq <= radius * radius;
        }

        bool IsRoadTriangle(const glm::vec3 &normal, const glm::vec3 &minBounds, const glm::vec3 &maxBounds, bool trustedGroundSurface)
        {
            float height = maxBounds.y - minBounds.y;
            float widthX = maxBounds.x - minBounds.x;
            float widthZ = maxBounds.z - minBounds.z;
            float horizontalSpan = std::max(widthX, widthZ);

            bool upward = normal.y >= 0.58f;
            bool wideEnough = horizontalSpan >= 0.25f;
            bool hasAreaInXZ = widthX > 0.01f && widthZ > 0.01f && (widthX * widthZ) >= 0.01f;
            bool heightOk = trustedGroundSurface || height <= 4.0f;

            return upward && wideEnough && hasAreaInXZ && heightOk;
        }

        float TriangleArea(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            return 0.5f * glm::length(glm::cross(b - a, c - a));
        }

        bool IsTinyObstacle(const WorldTriangle &tri)
        {
            glm::vec3 size = tri.maxBounds - tri.minBounds;
            float height = size.y;
            float area = TriangleArea(tri.a, tri.b, tri.c);

            bool extremelyTiny = area < 0.001f;
            bool degenerate = height < 0.001f && area < 0.0001f;
            bool flatSkin = std::abs(tri.normal.y) > 0.55f;
            bool lowRoadLip = height < 0.45f && area < 12.0f;

            return extremelyTiny || degenerate || flatSkin || lowRoadLip || tri.nonBlockingSurface;
        }
    }

    int CityPhysics::GetCellCol(float x) const
    {
        if (mGridCols <= 0) return 0;
        int col = static_cast<int>((x - mWorldMinBounds.x) / mGridCellSize);
        return std::max(0, std::min(mGridCols - 1, col));
    }

    int CityPhysics::GetCellRow(float z) const
    {
        if (mGridRows <= 0) return 0;
        int row = static_cast<int>((z - mWorldMinBounds.z) / mGridCellSize);
        return std::max(0, std::min(mGridRows - 1, row));
    }

    void CityPhysics::Initialize(const Model &model, const glm::mat4 &cityMatrix)
    {
        mRoadTriangles.clear();
        mObstacleTriangles.clear();
        mGrid.clear();
        mWorldMinBounds = glm::vec3(FLT_MAX);
        mWorldMaxBounds = glm::vec3(-FLT_MAX);
        mGridCellSize = 15.0f;
        mGridCols = 0;
        mGridRows = 0;

        const auto &meshes = model.GetMeshes();
        const auto &matrices = model.GetMatricesMeshes();
        const auto &materials = model.GetMeshMaterialNames();
        const auto &collisionNames = model.GetMeshCollisionNames();

        bool hasMaterialGroundHints = false;
        for (size_t i = 0; i < meshes.size(); ++i)
        {
            std::string surfaceName = (i < collisionNames.size()) ? collisionNames[i] :
                                      ((i < materials.size()) ? materials[i] : std::string());
            if (IsKnownCityGroundMaterial(surfaceName) || IsGenericGroundMaterial(surfaceName))
            {
                hasMaterialGroundHints = true;
                break;
            }
        }

        for (size_t i = 0; i < meshes.size(); ++i)
        {
            const auto &vertices = meshes[i].vertices;
            const auto &indices = meshes[i].indices;
            if (vertices.empty() || indices.size() < 3)
            {
                continue;
            }

            glm::mat4 meshWorldMatrix = cityMatrix * matrices[i];
            std::string surfaceName = (i < collisionNames.size()) ? collisionNames[i] :
                                      ((i < materials.size()) ? materials[i] : std::string());
            bool materialAllowsGround = hasMaterialGroundHints
                                            ? (IsKnownCityGroundMaterial(surfaceName) || IsGenericGroundMaterial(surfaceName))
                                            : true;
            bool nonBlockingSurface = hasMaterialGroundHints && IsKnownCityNonBlockingGroundDetail(surfaceName);

            for (size_t t = 0; t < indices.size(); t += 3)
            {
                glm::vec3 a = glm::vec3(meshWorldMatrix * glm::vec4(vertices[indices[t]].position, 1.0f));
                glm::vec3 b = glm::vec3(meshWorldMatrix * glm::vec4(vertices[indices[t + 1]].position, 1.0f));
                glm::vec3 c = glm::vec3(meshWorldMatrix * glm::vec4(vertices[indices[t + 2]].position, 1.0f));

                mWorldMinBounds = glm::min(mWorldMinBounds, glm::min(a, glm::min(b, c)));
                mWorldMaxBounds = glm::max(mWorldMaxBounds, glm::max(a, glm::max(b, c)));

                glm::vec3 normal = ComputeTriangleNormal(a, b, c);
                glm::vec3 minBounds = ComponentMin(a, b, c);
                glm::vec3 maxBounds = ComponentMax(a, b, c);

                WorldTriangle tri;
                tri.a = a;
                tri.b = b;
                tri.c = c;
                tri.normal = normal;
                tri.minBounds = minBounds;
                tri.maxBounds = maxBounds;
                tri.isRoad = materialAllowsGround && IsRoadTriangle(normal, minBounds, maxBounds, materialAllowsGround);
                tri.nonBlockingSurface = nonBlockingSurface;
                tri.isWater = IsWaterMaterial(surfaceName);

                if (tri.isWater)
                {
                    mWaterTriangles.push_back(tri);
                }

                if (tri.isRoad)
                {
                    mRoadTriangles.push_back(tri);
                }
                else
                {
                    mObstacleTriangles.push_back(tri);
                }
            }
        }

        if (mWorldMinBounds.x > mWorldMaxBounds.x)
        {
            mWorldMinBounds = glm::vec3(0.0f);
            mWorldMaxBounds = glm::vec3(0.0f);
        }

        // Build spatial partition grid
        float sizeX = mWorldMaxBounds.x - mWorldMinBounds.x;
        float sizeZ = mWorldMaxBounds.z - mWorldMinBounds.z;
        mGridCols = static_cast<int>(std::ceil(sizeX / mGridCellSize));
        mGridRows = static_cast<int>(std::ceil(sizeZ / mGridCellSize));
        if (mGridCols <= 0) mGridCols = 1;
        if (mGridRows <= 0) mGridRows = 1;

        mGrid.resize(mGridCols * mGridRows);

        for (size_t i = 0; i < mRoadTriangles.size(); ++i)
        {
            const auto &tri = mRoadTriangles[i];
            int minCol = GetCellCol(tri.minBounds.x);
            int maxCol = GetCellCol(tri.maxBounds.x);
            int minRow = GetCellRow(tri.minBounds.z);
            int maxRow = GetCellRow(tri.maxBounds.z);
            for (int r = minRow; r <= maxRow; ++r)
            {
                for (int c = minCol; c <= maxCol; ++c)
                {
                    mGrid[r * mGridCols + c].roadTriangleIndices.push_back(i);
                }
            }
        }

        for (size_t i = 0; i < mObstacleTriangles.size(); ++i)
        {
            const auto &tri = mObstacleTriangles[i];
            int minCol = GetCellCol(tri.minBounds.x);
            int maxCol = GetCellCol(tri.maxBounds.x);
            int minRow = GetCellRow(tri.minBounds.z);
            int maxRow = GetCellRow(tri.maxBounds.z);
            for (int r = minRow; r <= maxRow; ++r)
            {
                for (int c = minCol; c <= maxCol; ++c)
                {
                    mGrid[r * mGridCols + c].obstacleTriangleIndices.push_back(i);
                }
            }
        }
    }

    bool CityPhysics::IsInWater(const glm::vec3& position) const
    {
        for (const auto& tri : mWaterTriangles)
        {
            if (position.x >= tri.minBounds.x - 1.0f && position.x <= tri.maxBounds.x + 1.0f && position.z >= tri.minBounds.z -1.0f && position.z <= tri.maxBounds.z + 1.0f)
            {
                // Is in water material
                return true;
            }
        }

        // If is down, is in water
        if (position.y < 0.0f)
        {
            return true;
        }

        return false;
    }

    glm::vec3 TriangleCenter(const WorldTriangle &tri)
    {
        return (tri.a + tri.b + tri.c) / 3.0f;
    }

    glm::vec3 CityPhysics::GetBestRoadSpawn(const glm::vec3 &preferred, float maxDistance) const
    {
        float bestDistanceSq = maxDistance * maxDistance;
        glm::vec3 bestPoint = preferred;
        for (const auto &tri : mRoadTriangles)
        {
            glm::vec3 center = TriangleCenter(tri);
            float distSq = glm::dot(center - preferred, center - preferred);
            if (distSq < bestDistanceSq)
            {
                bestDistanceSq = distSq;
                bestPoint = center;
            }
        }
        return bestPoint;
    }

    bool CityPhysics::GetGroundSample(const glm::vec3 &worldPos, float currentY, GroundSample &outSample, float snapDownMax, float snapUpMax) const
    {
        float bestY = currentY;
        float bestScore = -1e9f;
        glm::vec3 bestNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        bool found = false;

        // Clamp down/up search distances. For initial spawn or large drops, snapDownMax can be raised.
        if (snapDownMax < 0.0f)
            snapDownMax = 8.0f;
        if (snapUpMax < 0.0f)
            snapUpMax = 0.12f;

        const float edgeSnapMargin = 0.55f;

        int minCol = GetCellCol(worldPos.x - edgeSnapMargin);
        int maxCol = GetCellCol(worldPos.x + edgeSnapMargin);
        int minRow = GetCellRow(worldPos.z - edgeSnapMargin);
        int maxRow = GetCellRow(worldPos.z + edgeSnapMargin);

        for (int r = minRow; r <= maxRow; ++r)
        {
            for (int c = minCol; c <= maxCol; ++c)
            {
                const auto &cell = mGrid[r * mGridCols + c];
                for (size_t triIdx : cell.roadTriangleIndices)
                {
                    const WorldTriangle &tri = mRoadTriangles[triIdx];

                    if (worldPos.x < tri.minBounds.x - edgeSnapMargin || worldPos.x > tri.maxBounds.x + edgeSnapMargin ||
                        worldPos.z < tri.minBounds.z - edgeSnapMargin || worldPos.z > tri.maxBounds.z + edgeSnapMargin)
                        continue;

                    float y = 0.0f;
                    float horizontalDistance = 0.0f;
                    bool projected = ProjectPointToTriangleXZ(worldPos, tri, y);
                    if (!projected)
                    {
                        glm::vec3 closest;
                        float distSq = ClosestPointOnTriangleXZ(worldPos, tri, closest);
                        horizontalDistance = std::sqrt(distSq);
                        if (horizontalDistance > edgeSnapMargin)
                        {
                            continue;
                        }
                        y = closest.y;
                    }

                    if (y < currentY - snapDownMax || y > currentY + snapUpMax)
                        continue;

                    float verticalDistance = std::abs(y - currentY);
                    float score = (projected ? 1000.0f : 500.0f) - verticalDistance * 4.0f - horizontalDistance * 25.0f + y * 0.01f;
                    if (!found || score > bestScore)
                    {
                        bestScore = score;
                        bestY = y;
                        bestNormal = tri.normal;
                        found = true;
                    }
                }
            }
        }

        outSample.height = found ? bestY : currentY;
        outSample.normal = found ? glm::normalize(bestNormal) : glm::vec3(0.0f, 1.0f, 0.0f);
        outSample.found = found;
        return found;
    }

    float CityPhysics::GetHeightAt(const Model &model, const glm::mat4 &cityMatrix, float x, float z, float currentY, bool *outFound, float snapDownMax, float snapUpMax) const
    {
        GroundSample sample;
        bool found = GetGroundSample(glm::vec3(x, currentY, z), currentY, sample, snapDownMax, snapUpMax);
        if (outFound)
        {
            *outFound = found;
        }
        return found ? sample.height : currentY;
    }

    bool CityPhysics::CheckCollision(const glm::vec3 &pos, float radius) const
    {
        // Search radius: position + collision radius (expanded for safety)
        float searchRadius = radius * 2.5f;

        int minCol = GetCellCol(pos.x - searchRadius);
        int maxCol = GetCellCol(pos.x + searchRadius);
        int minRow = GetCellRow(pos.z - searchRadius);
        int maxRow = GetCellRow(pos.z + searchRadius);

        for (int r = minRow; r <= maxRow; ++r)
        {
            for (int c = minCol; c <= maxCol; ++c)
            {
                const auto &cell = mGrid[r * mGridCols + c];
                for (size_t triIdx : cell.obstacleTriangleIndices)
                {
                    const WorldTriangle &tri = mObstacleTriangles[triIdx];

                    // Skip only degenerate/floating artifacts (conservative)
                    if (IsTinyObstacle(tri))
                    {
                        continue;
                    }

                    // Quick AABB check: is the triangle close enough?
                    if (pos.x < tri.minBounds.x - searchRadius || pos.x > tri.maxBounds.x + searchRadius ||
                        pos.z < tri.minBounds.z - searchRadius || pos.z > tri.maxBounds.z + searchRadius ||
                        pos.y < tri.minBounds.y - searchRadius || pos.y > tri.maxBounds.y + searchRadius)
                    {
                        continue;
                    }

                    // Full sphere-triangle collision check
                    if (SphereIntersectsTriangle(pos, radius, tri))
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }
}
