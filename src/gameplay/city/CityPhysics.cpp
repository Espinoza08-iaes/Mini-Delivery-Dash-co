#include "CityPhysics.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cctype>
#include <string>

namespace game
{
    // ============================================================================
    // Aninimous namespace
    // ============================================================================
    namespace
    {
        // ============================================================================
        // Constants (tunable parameters)
        // ============================================================================
        
        // Geometry thresholds
        constexpr float EPSILON = 1e-5f;           // General floating point tolerance
        constexpr float NORMAL_EPSILON = 1e-8f;    // Tolerance for zero-length normals
        
        // Road detection thresholds
        constexpr float MIN_ROAD_NORMAL_Y = 0.58f;      // Minimum Y component for road normals (cos(54°))
        constexpr float MIN_ROAD_HORIZONTAL_SPAN = 0.25f; // Minimum width/length of a road triangle
        constexpr float MIN_ROAD_AREA_XY = 0.01f;        // Minimum X*Z area for road
        constexpr float MAX_ROAD_HEIGHT = 4.0f;          // Maximum height difference for road surfaces
        
        // Obstacle filtering
        constexpr float TINY_AREA_THRESHOLD = 0.001f;    // Triangles smaller than this are ignored
        constexpr float DEGENERATE_HEIGHT = 0.001f;      // Height below which a triangle is degenerate
        constexpr float DEGENERATE_AREA = 0.0001f;       // Area below which a triangle is degenerate
        constexpr float FLAT_SKIN_NORMAL_Y = 0.55f;      // Triangles flatter than this are considered "flat skin"
        constexpr float LOW_ROAD_LIP_HEIGHT = 0.45f;     // Height threshold for curb-like obstacles
        constexpr float LOW_ROAD_LIP_AREA = 12.0f;       // Area threshold for curb-like obstacles
        
        // Ground sampling
        constexpr float DEFAULT_SNAP_DOWN_MAX = 8.0f;    // Default downward search distance (meters)
        constexpr float DEFAULT_SNAP_UP_MAX = 0.5f;     // Default upward search distance (NOTE: may be too small)
        constexpr float EDGE_SNAP_MARGIN = 0.55f;        // Margin around triangle edges for ground detection
        
        // Scoring weights for ground sampling
        constexpr float PROJECTED_SCORE_BONUS = 1000.0f; // Bonus for exact projection
        constexpr float CLOSEST_SCORE_BONUS = 500.0f;    // Bonus for closest point on edge
        constexpr float VERTICAL_DISTANCE_WEIGHT = 4.0f;  // Weight for vertical distance penalty
        constexpr float HORIZONTAL_DISTANCE_WEIGHT = 25.0f; // Weight for horizontal distance penalty
        constexpr float HEIGHT_SCORE_FACTOR = 0.01f;     // Small bonus for higher Y
        
        // Spatial grid configuration
        constexpr float GRID_CELL_SIZE = 15.0f;          // Size of each spatial partition cell (meters)
        
        // Collision detection
        constexpr float COLLISION_SEARCH_RADIUS_FACTOR = 2.5f; // Multiplier for collision search radius
        // ============================================================================
        // Basic Math
        // ============================================================================

        // Calculates the unit normal vector of a triangle defined by three vertices.
        glm::vec3 ComputeTriangleNormal(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            // Calculate the cross product of two edges to get the surface normal
            glm::vec3 n = glm::cross(b - a, c - a);
            float length = glm::length(n);

            if (length < 1e-8f) // Check for degenerate triangles (vertices are collinear or overlapping)
            {
                return glm::vec3(0.0f, 1.0f, 0.0f); // Return a fallback unit vector
            }

            return n / length; // Normalize the vector to ensure it has a length of 1.0
        }

        // Obtain minimum coordinates
        glm::vec3 ComponentMin(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            return glm::vec3(
                std::min(a.x, std::min(b.x, c.x)),
                std::min(a.y, std::min(b.y, c.y)),
                std::min(a.z, std::min(b.z, c.z))); // Returns a vector containing the minimum coordinates among three vectors.
        }

        // Obtain maximum coordinates
        glm::vec3 ComponentMax(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            return glm::vec3(
                std::max(a.x, std::max(b.x, c.x)),
                std::max(a.y, std::max(b.y, c.y)),
                std::max(a.z, std::max(b.z, c.z))); // Returns a vector containing the maximum coordinates among three vectors.
        }

        // Calculates the surface area of a triangle defined by three vertices using the cross product.
        float TriangleArea(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            return 0.5f * glm::length(glm::cross(b - a, c - a));
        }

        // ============================================================================
        // Functions based on the geometry
        // ============================================================================

        // Projects a point onto a triangle in the XZ plane and interpolates the Y height using barycentric coordinates.
        bool ProjectPointToTriangleXZ(const glm::vec3 &point, const WorldTriangle &tri, float &outY)
        { 
            // Calculate the determinant for barycentric coordinate transformation.
            float det = (tri.b.z - tri.c.z) * (tri.a.x - tri.c.x) + (tri.c.x - tri.b.x) * (tri.a.z - tri.c.z);

            if (std::abs(det) < 1e-5f) // Avoid division by zero for degenerate (vertical) triangles.
            {
                return false;
            }

            // Compute barycentric weights (l1, l2, l3) for the XZ projection.
            float l1 = ((tri.b.z - tri.c.z) * (point.x - tri.c.x) + (tri.c.x - tri.b.x) * (point.z - tri.c.z)) / det;
            float l2 = ((tri.c.z - tri.a.z) * (point.x - tri.c.x) + (tri.a.x - tri.c.x) * (point.z - tri.c.z)) / det;

            float l3 = 1.0f - l1 - l2;

            const float eps = -1e-3f;
            if (l1 >= eps && l2 >= eps && l3 >= eps)
            {
                // Interpolate Y value based on the computed weights.
                outY = l1 * tri.a.y + l2 * tri.b.y + l3 * tri.c.y;
                return true;
            }

            return false;
        }

        // Finds the closest point on a 2D line segment in the XZ plane and returns the squared distance.
        float ClosestPointOnSegmentXZ(const glm::vec3 &point, const glm::vec3 &start, const glm::vec3 &end, glm::vec3 &outClosest)
        {
            glm::vec2 p(point.x, point.z);
            glm::vec2 a(start.x, start.z);
            glm::vec2 b(end.x, end.z);
            glm::vec2 ab = b - a;
            float lenSq = glm::dot(ab, ab);

            // Handle degenerate segment (start and end points are the same).
            if (lenSq < 1e-8f)
            {
                outClosest = start;
                return glm::dot(p - a, p - a);
            }

            // Project point onto the line and clamp
            float t = glm::dot(p - a, ab) / lenSq;
            t = glm::clamp(t, 0.0f, 1.0f);
            // Calculate the 3D position of the closest point.
            outClosest = start + (end - start) * t;
            // Return the squared distance between the input point and the closest point in XZ.
            glm::vec2 closest(outClosest.x, outClosest.z);
            return glm::dot(p - closest, p - closest);
        }

        // Finds the closest point on the triangle's perimeter to a given point in the XZ plane.
        float ClosestPointOnTriangleXZ(const glm::vec3 &point, const WorldTriangle &tri, glm::vec3 &outClosest)
        {
            glm::vec3 ab, bc, ca;
            // Calculate distances to each of the triangle's edges.
            float abDist = ClosestPointOnSegmentXZ(point, tri.a, tri.b, ab);
            float bcDist = ClosestPointOnSegmentXZ(point, tri.b, tri.c, bc);
            float caDist = ClosestPointOnSegmentXZ(point, tri.c, tri.a, ca);

            // Determine which edge is nearest and store the closest point and distance.
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

        // Checks if a 3D point lies within a triangle using barycentric coordinates.
        bool PointInsideTriangle2D(const glm::vec3 &point, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            // Compute vectors from the first vertex to the others.
            glm::vec3 v1 = b - a;
            glm::vec3 v2 = c - a;
            glm::vec3 pv = point - a;
            // Compute dot products for the barycentric coordinate system.
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
            // Compute barycentric weights (v, w, u).
            float v = (d11 * d20 - d01 * d21) / denom;
            float w = (d00 * d21 - d01 * d20) / denom;
            float u = 1.0f - v - w;
            // Return true if the point is inside or on the triangle edges within a tolerance.
            const float eps = 1e-4f;
            return u >= -eps && v >= -eps && w >= -eps;
        }

        // Computes the squared distance between a 3D point and a line segment.
        float SquaredDistanceToSegment(const glm::vec3 &point, const glm::vec3 &start, const glm::vec3 &end)
        {
            glm::vec3 ab = end - start;
            float lenSq = glm::dot(ab, ab);
            // Handle degenerate segment where start and end points coincide.
            if (lenSq < 1e-8f)
            {
                return glm::dot(point - start, point - start);
            }
            // Project point onto the segment and clamp to stay within [start, end] boundaries.
            float t = glm::dot(point - start, ab) / lenSq;
            t = glm::clamp(t, 0.0f, 1.0f);
            // Calculate the actual closest point on the segment.
            glm::vec3 closest = start + ab * t;
            // Return the squared distance.
            return glm::dot(point - closest, point - closest);
        }

        // ============================================================================
        // Collision handling
        // ============================================================================

        // Performs a collision check between a sphere and a triangle.
        bool SphereIntersectsTriangle(const glm::vec3 &center, float radius, const WorldTriangle &tri)
        {
            // Early exit using AABB (Axis-Aligned Bounding Box) to quickly skip distant triangles.
            if (center.x < tri.minBounds.x - radius || center.x > tri.maxBounds.x + radius ||
                center.z < tri.minBounds.z - radius || center.z > tri.maxBounds.z + radius ||
                center.y < tri.minBounds.y - radius || center.y > tri.maxBounds.y + radius)
            {
                return false;
            }
            // Project sphere center onto the triangle's plane and check distance to plane.
            float planeDistance = glm::dot(center - tri.a, tri.normal);
            glm::vec3 projection = center - planeDistance * tri.normal;

            if (PointInsideTriangle2D(projection, tri.a, tri.b, tri.c) && std::abs(planeDistance) <= radius)
            {
                return true;
            }
            // Finally, check for collisions against the triangle's edges.
            float edgeDistanceSq = std::min(std::min(SquaredDistanceToSegment(center, tri.a, tri.b), SquaredDistanceToSegment(center, tri.b, tri.c)), SquaredDistanceToSegment(center, tri.c, tri.a));

            return edgeDistanceSq <= radius * radius;
        }

        // ============================================================================
        // String control
        // ============================================================================

        // Converts all characters in a string to lowercase.
        std::string ToLower(std::string value)
        {
            // Apply std::tolower to each character in the string.
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));});

            return value;
        }

        // Checks if a string contains any of the provided substrings (needles).
        bool ContainsAny(const std::string &value, const char *const *needles, size_t count)
        {
            // Iterate through the list of substrings.
            for (size_t i = 0; i < count; ++i)
            {
                if (value.find(needles[i]) != std::string::npos)
                {
                    return true; // Return true if the current substring is found within the value.
                }
            }
            return false; // Return false if none of the substrings match.
        }

        // Checks if a string exactly matches a material tag or contains it as a discrete word (tagged).
        bool ContainsExactOrTaggedMaterial(const std::string &value, const std::string &materialTag)
        { // Direct equality check for exact match.
            if (value == materialTag)
            {
                return true;
            }

            // Wrap both strings in spaces to ensure the tag is found as a whole word, not a substring.
            std::string paddedValue = " " + value + " ";
            std::string paddedTag = " " + materialTag + " ";
            return paddedValue.find(paddedTag) != std::string::npos;
        }

        // ============================================================================
        // Verification of specific materials
        // ============================================================================

        // Determines if a surface name corresponds to common urban ground materials.
        bool IsKnownCityGroundMaterial(const std::string &surfaceName)
        {
            // Normalize input to lowercase for case-insensitive comparison.
            std::string name = ToLower(surfaceName);
            // Define keywords related to urban ground surfaces.
            static const char *const cityGroundWords[] = {
                "yardground", "parkinglot", "pavement", "street", "concrete", "curbs"
            };
            // Define specific material asset identifiers for city textures.
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
            // Check if the surface name matches any specific city material tags.
            for (size_t i = 0; i < sizeof(cityGroundMaterials) / sizeof(cityGroundMaterials[0]); ++i)
            {
                if (ContainsExactOrTaggedMaterial(name, cityGroundMaterials[i]))
                {
                    return true;
                }
            }
            // Return false if no urban material matches were found.
            return false;
        }

        // Determines if a surface is a minor ground detail (like a curb) that should not block movement.
        bool IsKnownCityNonBlockingGroundDetail(const std::string &surfaceName)
        {
            // Normalize input to lowercase for case-insensitive matching.
            std::string name = ToLower(surfaceName);
            // Returns true if the name contains "curb" or matches the specific curb material tag.
            return name.find("curb") != std::string::npos || ContainsExactOrTaggedMaterial(name, "my_city_0facadetexture_58");
        }

        // Identifies if a surface is a walkable ground material while excluding structural or vertical elements.
        bool IsGenericGroundMaterial(const std::string &surfaceName)
        {
            // Normalize input to lowercase for case-insensitive matching.
            std::string name = ToLower(surfaceName);
            // Keywords representing valid walkable ground surfaces.
            static const char *const groundWords[] = {
                "street", "road", "asphalt", "pavement", "sidewalk", "concrete", "parking",
                "ground", "grass", "yard", "lawn", "terrain", "land", "soil", "plaza", "curb"
            };
            static const char *const blockedWords[] = {
                "roof", "facade", "wall", "window", "tree", "bark", "building"
            };
            // Return true only if it matches a ground keyword and does not contain any blocked keywords.
            return ContainsAny(name, groundWords, sizeof(groundWords) / sizeof(groundWords[0])) &&
                   !ContainsAny(name, blockedWords, sizeof(blockedWords) / sizeof(blockedWords[0]));
        }

        // ============================================================================
        // Logic applied to land
        // ============================================================================

        // Evaluates if a triangle qualifies as a road or walkable surface based on slope, dimensions, and confidence.
        bool IsRoadTriangle(const glm::vec3 &normal, const glm::vec3 &minBounds, const glm::vec3 &maxBounds, bool trustedGroundSurface)
        {
                // If material explicitly marks this as ground, trust it
            if (trustedGroundSurface) return true;
            // Calculate dimensions and the largest horizontal extent.
            float height = maxBounds.y - minBounds.y;
            float widthX = maxBounds.x - minBounds.x;
            float widthZ = maxBounds.z - minBounds.z;
            float horizontalSpan = std::max(widthX, widthZ);

            bool upward = normal.y >= MIN_ROAD_NORMAL_Y;
            bool wideEnough = horizontalSpan >= MIN_ROAD_HORIZONTAL_SPAN;
            bool hasAreaInXZ = widthX > MIN_ROAD_AREA_XY && widthZ > MIN_ROAD_AREA_XY && (widthX * widthZ) >= MIN_ROAD_AREA_XY;
            bool heightOk = trustedGroundSurface || height <= MAX_ROAD_HEIGHT;

            return upward && wideEnough && hasAreaInXZ && heightOk;
        }

        // Determines if a triangle represents a negligible obstacle that can be ignored for collision or navigation.
        bool IsTinyObstacle(const WorldTriangle &tri)
        {
            glm::vec3 size = tri.maxBounds - tri.minBounds;
            float height = size.y;
            float area = TriangleArea(tri.a, tri.b, tri.c);
            // Flag triangles with almost no surface area.
            bool extremelyTiny = area < TINY_AREA_THRESHOLD;
            bool degenerate = height < DEGENERATE_HEIGHT && area < DEGENERATE_AREA;
            bool flatSkin = std::abs(tri.normal.y) > FLAT_SKIN_NORMAL_Y;
            bool lowRoadLip = height < LOW_ROAD_LIP_HEIGHT && area < LOW_ROAD_LIP_AREA;
            // Return true if any condition is met or if the surface is explicitly marked as non-blocking.
            return extremelyTiny || degenerate || flatSkin || lowRoadLip || tri.nonBlockingSurface;
        }
    }

    // ============================================================================
    // Methods belonging to the CityPhysics class
    // ============================================================================

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
        mGridCellSize = GRID_CELL_SIZE;
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

                // Clasificación simple: carretera u obstáculo (sin agua)
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
            snapDownMax = DEFAULT_SNAP_DOWN_MAX;
        if (snapUpMax < 0.0f)
            snapUpMax = DEFAULT_SNAP_UP_MAX;

        const float edgeSnapMargin = EDGE_SNAP_MARGIN;

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
                    float score = (projected ? PROJECTED_SCORE_BONUS : CLOSEST_SCORE_BONUS) - verticalDistance * VERTICAL_DISTANCE_WEIGHT - horizontalDistance * HORIZONTAL_DISTANCE_WEIGHT + y * HEIGHT_SCORE_FACTOR;
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

    float CityPhysics::GetHeightAt(float x, float z, float currentY, bool *outFound, float snapDownMax, float snapUpMax) const
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
        float searchRadius = radius * COLLISION_SEARCH_RADIUS_FACTOR;

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