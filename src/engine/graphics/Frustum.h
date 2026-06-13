#ifndef FRUSTUM_CLASS_H
#define FRUSTUM_CLASS_H

#include <glm/glm.hpp>

// ============================================================================
// Frustum Culling
// ----------------------------------------------------------------------------
// Extracts the 6 planes of the view frustum from a VP matrix and tests
// axis-aligned bounding boxes (AABBs) against them.
// Algorithm: Gribb/Hartmann plane extraction method.
// ============================================================================
class Frustum
{
public:
    // Extracts 6 frustum planes from a combined View-Projection matrix.
    // Call this once per frame after updating the camera.
    void Update(const glm::mat4& vpMatrix)
    {
        // Left   plane: row3 + row0
        planes[0] = glm::vec4(
            vpMatrix[0][3] + vpMatrix[0][0],
            vpMatrix[1][3] + vpMatrix[1][0],
            vpMatrix[2][3] + vpMatrix[2][0],
            vpMatrix[3][3] + vpMatrix[3][0]
        );
        // Right  plane: row3 - row0
        planes[1] = glm::vec4(
            vpMatrix[0][3] - vpMatrix[0][0],
            vpMatrix[1][3] - vpMatrix[1][0],
            vpMatrix[2][3] - vpMatrix[2][0],
            vpMatrix[3][3] - vpMatrix[3][0]
        );
        // Bottom plane: row3 + row1
        planes[2] = glm::vec4(
            vpMatrix[0][3] + vpMatrix[0][1],
            vpMatrix[1][3] + vpMatrix[1][1],
            vpMatrix[2][3] + vpMatrix[2][1],
            vpMatrix[3][3] + vpMatrix[3][1]
        );
        // Top    plane: row3 - row1
        planes[3] = glm::vec4(
            vpMatrix[0][3] - vpMatrix[0][1],
            vpMatrix[1][3] - vpMatrix[1][1],
            vpMatrix[2][3] - vpMatrix[2][1],
            vpMatrix[3][3] - vpMatrix[3][1]
        );
        // Near   plane: row3 + row2
        planes[4] = glm::vec4(
            vpMatrix[0][3] + vpMatrix[0][2],
            vpMatrix[1][3] + vpMatrix[1][2],
            vpMatrix[2][3] + vpMatrix[2][2],
            vpMatrix[3][3] + vpMatrix[3][2]
        );
        // Far    plane: row3 - row2
        planes[5] = glm::vec4(
            vpMatrix[0][3] - vpMatrix[0][2],
            vpMatrix[1][3] - vpMatrix[1][2],
            vpMatrix[2][3] - vpMatrix[2][2],
            vpMatrix[3][3] - vpMatrix[3][2]
        );

        // Normalize all planes
        for (int i = 0; i < 6; ++i)
        {
            float len = glm::length(glm::vec3(planes[i]));
            if (len > 0.0f)
            {
                planes[i] /= len;
            }
        }
    }

    // Tests whether an AABB (defined by min/max corners) intersects the frustum.
    // Returns true if the box is at least partially inside (should be drawn).
    bool IsBoxVisible(const glm::vec3& boxMin, const glm::vec3& boxMax) const
    {
        for (int i = 0; i < 6; ++i)
        {
            // Find the "positive vertex" — the corner most aligned with the plane normal
            glm::vec3 pVertex;
            pVertex.x = (planes[i].x >= 0.0f) ? boxMax.x : boxMin.x;
            pVertex.y = (planes[i].y >= 0.0f) ? boxMax.y : boxMin.y;
            pVertex.z = (planes[i].z >= 0.0f) ? boxMax.z : boxMin.z;

            // If the most-positive vertex is behind this plane, the AABB is fully outside
            float dist = planes[i].x * pVertex.x + planes[i].y * pVertex.y + planes[i].z * pVertex.z + planes[i].w;
            if (dist < 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    // Tests whether a sphere is visible within the frustum.
    bool IsSphereVisible(const glm::vec3& center, float radius) const
    {
        for (int i = 0; i < 6; ++i)
        {
            float dist = planes[i].x * center.x + planes[i].y * center.y + planes[i].z * center.z + planes[i].w;
            if (dist < -radius)
            {
                return false;
            }
        }
        return true;
    }

private:
    glm::vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far
};

#endif
