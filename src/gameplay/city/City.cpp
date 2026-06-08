#include "City.h"
 
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <vector>

namespace
{
    glm::vec3 ExpandVertexXZ(const glm::vec3& vertex, const glm::vec3& center, float margin)
    {
        glm::vec2 dir(vertex.x - center.x, vertex.z - center.z);
        float len = glm::length(dir);
        if (len > 1e-4f)
        {
            dir /= len;
            return glm::vec3(vertex.x + dir.x * margin, vertex.y, vertex.z + dir.y * margin);
        }
        return vertex;
    }

    void AddVertex(std::vector<Vertex>& vertices, const glm::vec3& position, const glm::vec3& normal, const glm::vec3& color)
    {
        Vertex vertex;
        vertex.position = position;
        vertex.normal = normal;
        vertex.color = color;
        vertex.texUV = glm::vec2(0.0f);
        vertices.push_back(vertex);
    }

    void AddTriangle(std::vector<Vertex>& vertices, std::vector<GLuint>& indices,
                     const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                     const glm::vec3& color)
    {
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        GLuint start = static_cast<GLuint>(vertices.size());
        AddVertex(vertices, a, normal, color);
        AddVertex(vertices, b, normal, color);
        AddVertex(vertices, c, normal, color);
        indices.push_back(start);
        indices.push_back(start + 1);
        indices.push_back(start + 2);
    }

    void AddQuad(std::vector<Vertex>& vertices, std::vector<GLuint>& indices,
                 const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                 const glm::vec3& color)
    {
        AddTriangle(vertices, indices, a, b, c, color);
        AddTriangle(vertices, indices, c, d, a, color);
    }
}
 
City::City(const std::string& modelPath, float scale, float yOffset, float xOffset, float zOffset, bool autoAlign)
    : mModel(modelPath.c_str())
    , mScale(scale)
    , mYOffset(yOffset)
    , mXOffset(xOffset)
    , mZOffset(zOffset)
{
    if (autoAlign)
    {
        // Calculate minY directly from untransformed model bounds
        float minY = 1e9f;
        const auto& meshes = mModel.GetMeshes();
        const auto& matrices = mModel.GetMatricesMeshes();
        for (size_t i = 0; i < meshes.size(); ++i)
        {
            glm::mat4 meshMatrix = matrices[i];
            for (const auto& vertex : meshes[i].vertices)
            {
                glm::vec3 worldPos = glm::vec3(meshMatrix * glm::vec4(vertex.position, 1.0f));
                if (worldPos.y < minY)
                {
                    minY = worldPos.y;
                }
            }
        }
        if (minY == 1e9f) minY = 0.0f;
        mYOffset = -minY * mScale + 0.01f;
    }

    mPhysics.Initialize(mModel, GetMatrix());
    BuildVisualGapFillMesh();
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
    if (mVisualGapFillMesh)
    {
        shader.Activate();
        glUniform1i(glGetUniformLocation(shader.ID, "uUseAlpha"), 0);
        glUniform1i(glGetUniformLocation(shader.ID, "uIsEmissive"), 0);
        mVisualGapFillMesh->Draw(shader, camera, glm::mat4(1.0f));
    }
}

void City::BuildVisualGapFillMesh()
{
    const std::vector<game::WorldTriangle>& roadTriangles = mPhysics.GetRoadTriangles();
    if (roadTriangles.empty())
    {
        mVisualGapFillMesh.reset();
        return;
    }

    const float gapCoverMargin = 0.65f;
    const float topDrop = 0.035f;
    const float backingDrop = 0.16f;
    const glm::vec3 backingColor(0.62f, 0.62f, 0.58f);
    const glm::vec3 skirtColor(0.52f, 0.52f, 0.49f);

    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    vertices.reserve(roadTriangles.size() * 21);
    indices.reserve(roadTriangles.size() * 21);

    for (const game::WorldTriangle& tri : roadTriangles)
    {
        glm::vec3 center = (tri.a + tri.b + tri.c) / 3.0f;

        glm::vec3 aTop = tri.a - glm::vec3(0.0f, topDrop, 0.0f);
        glm::vec3 bTop = tri.b - glm::vec3(0.0f, topDrop, 0.0f);
        glm::vec3 cTop = tri.c - glm::vec3(0.0f, topDrop, 0.0f);

        glm::vec3 aBack = ExpandVertexXZ(tri.a, center, gapCoverMargin) - glm::vec3(0.0f, backingDrop, 0.0f);
        glm::vec3 bBack = ExpandVertexXZ(tri.b, center, gapCoverMargin) - glm::vec3(0.0f, backingDrop, 0.0f);
        glm::vec3 cBack = ExpandVertexXZ(tri.c, center, gapCoverMargin) - glm::vec3(0.0f, backingDrop, 0.0f);

        AddTriangle(vertices, indices, aBack, bBack, cBack, backingColor);
        AddQuad(vertices, indices, aTop, bTop, bBack, aBack, skirtColor);
        AddQuad(vertices, indices, bTop, cTop, cBack, bBack, skirtColor);
        AddQuad(vertices, indices, cTop, aTop, aBack, cBack, skirtColor);
    }

    static const unsigned char whitePixel[] = {255, 255, 255, 255};
    static const unsigned char blackPixel[] = {0, 0, 0, 255};
    std::vector<Texture> textures;
    textures.emplace_back(whitePixel, 1, 1, GL_RGBA, "diffuse", 0);
    textures.emplace_back(blackPixel, 1, 1, GL_RGBA, "specular", 1);
    mVisualGapFillMesh.reset(new Mesh(vertices, indices, textures));
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
 
