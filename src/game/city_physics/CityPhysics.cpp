#include "CityPhysics.h"

#include <algorithm>
#include <cmath>

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

        bool IsRoadTriangle(const glm::vec3 &normal, const glm::vec3 &minBounds, const glm::vec3 &maxBounds)
        {
            float height = maxBounds.y - minBounds.y;
            float widthX = maxBounds.x - minBounds.x;
            float widthZ = maxBounds.z - minBounds.z;
            float horizontalSpan = std::max(widthX, widthZ);

            // DESPUÉS: más estricto — solo superficies casi horizontales son carretera
            bool mostlyHorizontal = std::abs(normal.y) >= 0.65f;
            bool lowProfile = height <= 6.0f;
            bool wideEnough = horizontalSpan >= 0.35f;

            return mostlyHorizontal && lowProfile && wideEnough;
        }

        bool IsRoadPoint(const glm::vec3 &point, const WorldTriangle &tri)
        {
            return point.x >= tri.minBounds.x && point.x <= tri.maxBounds.x &&
                   point.z >= tri.minBounds.z && point.z <= tri.maxBounds.z;
        }

        bool ProjectToTriangle(const glm::vec3 &point, const WorldTriangle &tri, float &outY)
        {
            float det = (tri.b.z - tri.c.z) * (tri.a.x - tri.c.x) + (tri.c.x - tri.b.x) * (tri.a.z - tri.c.z);
            if (std::abs(det) < 1e-5f)
            {
                return false;
            }

            float l1 = ((tri.b.z - tri.c.z) * (point.x - tri.c.x) + (tri.c.x - tri.b.x) * (point.z - tri.c.z)) / det;
            float l2 = ((tri.c.z - tri.a.z) * (point.x - tri.c.x) + (tri.a.x - tri.c.x) * (point.z - tri.c.z)) / det;
            float l3 = 1.0f - l1 - l2;

            const float eps = -1e-3f;
            if (l1 >= eps && l2 >= eps && l3 >= eps)
            {
                outY = l1 * tri.a.y + l2 * tri.b.y + l3 * tri.c.y;
                return true;
            }

            return false;
        }

        float TriangleArea(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
        {
            return 0.5f * glm::length(glm::cross(b - a, c - a));
        }

        bool IsTinyObstacle(const WorldTriangle &tri)
        {
            glm::vec3 size = tri.maxBounds - tri.minBounds;
            float minSpan = std::min(size.x, size.z);
            float height = size.y;
            float area = TriangleArea(tri.a, tri.b, tri.c);

            // Ignorar detalles muy planos, delgados, de área pequeña o muy pegados al suelo
            bool superFlat = height < 0.055f && area < 0.18f && std::abs(tri.normal.y) > 0.82f;
            bool tinyFlat = minSpan < 0.11f && height < 0.16f;
            bool tinyArea = area < 0.006f && height < 0.38f;
            bool flatDetail = height < 0.16f && std::abs(tri.normal.y) > 0.91f;
            bool nearGround = tri.minBounds.y < 0.12f && height < 0.18f && std::abs(tri.normal.y) > 0.8f;

            return superFlat || tinyFlat || tinyArea || flatDetail || nearGround;
        }
    }

    void CityPhysics::Initialize(const Model &model, const glm::mat4 &cityMatrix)
    {
        mRoadTriangles.clear();
        mObstacleTriangles.clear();

        const auto &meshes = model.GetMeshes();
        const auto &matrices = model.GetMatricesMeshes();

        for (size_t i = 0; i < meshes.size(); ++i)
        {
            const auto &vertices = meshes[i].vertices;
            const auto &indices = meshes[i].indices;
            if (vertices.empty() || indices.size() < 3)
            {
                continue;
            }

            glm::mat4 meshWorldMatrix = cityMatrix * matrices[i];

            for (size_t t = 0; t < indices.size(); t += 3)
            {
                glm::vec3 a = glm::vec3(meshWorldMatrix * glm::vec4(vertices[indices[t]].position, 1.0f));
                glm::vec3 b = glm::vec3(meshWorldMatrix * glm::vec4(vertices[indices[t + 1]].position, 1.0f));
                glm::vec3 c = glm::vec3(meshWorldMatrix * glm::vec4(vertices[indices[t + 2]].position, 1.0f));

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
                tri.isRoad = IsRoadTriangle(normal, minBounds, maxBounds);

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
    }

    bool CityPhysics::GetGroundSample(const glm::vec3 &worldPos, float currentY, GroundSample &outSample) const
    {
        float bestY = currentY;
        float bestScore = -1e9f;
        glm::vec3 bestNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        bool found = false;

        // Limitar subida máxima a 0.12 unidades para evitar que el coche "salte" a techos/puentes elevados
        const float snapDownMax = 8.0f;
        const float snapUpMax = 0.12f; // Solo permite subir a superficies casi a la misma altura

        for (size_t i = 0; i < mRoadTriangles.size(); ++i)
        {
            const WorldTriangle &tri = mRoadTriangles[i];
            if (!IsRoadPoint(worldPos, tri))
                continue;

            float y = 0.0f;
            if (!ProjectToTriangle(worldPos, tri, y))
                continue;

            if (y < currentY - snapDownMax || y > currentY + snapUpMax)
                continue;

            // Entre los válidos, preferir el más alto (el suelo más cercano por abajo)
            float score = y;
            if (!found || score > bestScore)
            {
                bestScore = score;
                bestY = y;
                bestNormal = tri.normal;
                found = true;
            }
        }

        outSample.height = found ? bestY : currentY;
        outSample.normal = found ? glm::normalize(bestNormal) : glm::vec3(0.0f, 1.0f, 0.0f);
        outSample.found = found;
        return found;
    }

    float CityPhysics::GetHeightAt(const Model &model, const glm::mat4 &cityMatrix, float x, float z, float currentY, bool *outFound) const
    {
        GroundSample sample;
        bool found = GetGroundSample(glm::vec3(x, currentY, z), currentY, sample);
        if (outFound)
        {
            *outFound = found;
        }
        return found ? sample.height : currentY;
    }

    bool CityPhysics::CheckCollision(const glm::vec3 &pos, float radius) const
    {
        for (size_t i = 0; i < mObstacleTriangles.size(); ++i)
        {
            if (IsTinyObstacle(mObstacleTriangles[i]))
            {
                continue;
            }

            if (SphereIntersectsTriangle(pos, radius, mObstacleTriangles[i]))
            {
                return true;
            }
        }
        return false;
    }
}
