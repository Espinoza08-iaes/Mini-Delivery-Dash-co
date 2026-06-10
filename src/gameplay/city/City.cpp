#include "City.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <algorithm>
#include <fstream>

#include <cmath>
#include <vector>

namespace
{
    glm::vec3 ExpandVertexXZ(const glm::vec3 &vertex, const glm::vec3 &center, float margin)
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

    void AddVertex(std::vector<Vertex> &vertices, const glm::vec3 &position, const glm::vec3 &normal, const glm::vec3 &color)
    {
        Vertex vertex;
        vertex.position = position;
        vertex.normal = normal;
        vertex.color = color;
        vertex.texUV = glm::vec2(0.0f);
        vertices.push_back(vertex);
    }

    void AddTriangle(std::vector<Vertex> &vertices, std::vector<GLuint> &indices,
                     const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c,
                     const glm::vec3 &color)
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

    void AddQuad(std::vector<Vertex> &vertices, std::vector<GLuint> &indices,
                 const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &d,
                 const glm::vec3 &color)
    {
        AddTriangle(vertices, indices, a, b, c, color);
        AddTriangle(vertices, indices, c, d, a, color);
    }
}

City::City(const std::string &modelPath, float scale, float yOffset, float xOffset, float zOffset, bool autoAlign)
    : mModel(modelPath.c_str()), mScale(scale), mYOffset(yOffset), mXOffset(xOffset), mZOffset(zOffset)
{
    if (autoAlign)
    {
        // Calculate minY directly from untransformed model bounds
        float minY = 1e9f;
        const auto &meshes = mModel.GetMeshes();
        const auto &matrices = mModel.GetMatricesMeshes();
        for (size_t i = 0; i < meshes.size(); ++i)
        {
            glm::mat4 meshMatrix = matrices[i];
            for (const auto &vertex : meshes[i].vertices)
            {
                glm::vec3 worldPos = glm::vec3(meshMatrix * glm::vec4(vertex.position, 1.0f));
                if (worldPos.y < minY)
                {
                    minY = worldPos.y;
                }
            }
        }
        if (minY == 1e9f)
            minY = 0.0f;
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

void City::Draw(Shader &shader, Camera &camera)
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
    const std::vector<game::WorldTriangle> &roadTriangles = mPhysics.GetRoadTriangles();
    if (roadTriangles.empty())
    {
        mVisualGapFillMesh.reset();
        return;
    }

    const float gapCoverMargin = 4.5f;
    const float topDrop = 0.02f;
    const float backingDrop = 2.0f;
    const glm::vec3 backingColor(0.46f, 0.46f, 0.46f);
    const glm::vec3 skirtColor(0.44f, 0.44f, 0.44f);

    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    vertices.reserve(roadTriangles.size() * 21);
    indices.reserve(roadTriangles.size() * 21);

    for (const game::WorldTriangle &tri : roadTriangles)
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

float City::GetHeightAt(float x, float z, float currentY, bool *outFound, float snapDownMax, float snapUpMax) const
{
    return mPhysics.GetHeightAt(mModel, GetMatrix(), x, z, currentY, outFound, snapDownMax, snapUpMax);
}

bool City::GetGroundSample(const glm::vec3 &worldPos, float currentY, game::GroundSample &outSample, float snapDownMax, float snapUpMax) const
{
    return mPhysics.GetGroundSample(worldPos, currentY, outSample, snapDownMax, snapUpMax);
}

bool City::CheckCollision(const glm::vec3 &pos, float radius) const
{
    return mPhysics.CheckCollision(pos, radius);
}

glm::vec3 City::GetBestRoadSpawn(const glm::vec3 &preferred, float maxDistance) const
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

std::vector<glm::vec3> City::GetStreetLampPositions(float spacing) const
{
    const auto &tris = mPhysics.GetRoadTriangles();
    if (tris.empty())
        return {};

    // Calcular centroide global de la ciudad para saber hacia donde es "afuera"
    glm::vec3 centroid(0.0f);
    for (const auto &tri : tris)
        centroid += (tri.a + tri.b + tri.c) / 3.0f;
    centroid /= static_cast<float>(tris.size());

    // Contar cuantas veces aparece cada arista
    auto quantize = [](float v)
    { return static_cast<int>(std::round(v * 10.0f)); };
    auto makeKey = [&](glm::vec3 p, glm::vec3 q)
    {
        auto pk = std::make_pair(quantize(p.x), quantize(p.z));
        auto qk = std::make_pair(quantize(q.x), quantize(q.z));
        if (pk > qk)
            std::swap(pk, qk);
        return std::make_pair(pk, qk);
    };

    using EdgeKey = std::pair<std::pair<int, int>, std::pair<int, int>>;
    std::map<EdgeKey, int> edgeCount;
    struct Edge
    {
        glm::vec3 a, b;
    };
    std::vector<Edge> edges;
    edges.reserve(tris.size() * 3);

    for (const auto &tri : tris)
    {
        edges.push_back({tri.a, tri.b});
        edges.push_back({tri.b, tri.c});
        edges.push_back({tri.c, tri.a});
        edgeCount[makeKey(tri.a, tri.b)]++;
        edgeCount[makeKey(tri.b, tri.c)]++;
        edgeCount[makeKey(tri.c, tri.a)]++;
    }

    std::vector<glm::vec3> positions;
    const float minDistSq = (spacing * 0.85f) * (spacing * 0.85f);

    for (const auto &e : edges)
    {
        if (edgeCount[makeKey(e.a, e.b)] != 1)
            continue; // solo bordes exteriores

        float len = glm::length(e.b - e.a);
        if (len < spacing * 0.4f)
            continue;

        int count = std::max(1, static_cast<int>(len / spacing));
        for (int k = 0; k <= count; ++k)
        {
            float t = (float)k / count;
            glm::vec3 pos = glm::mix(e.a, e.b, t);

            // Empujar hacia afuera del centroide
            // ---- NUEVO: detectar cuál lado tiene la carretera y empujar al opuesto ----
            glm::vec3 edgeDir = glm::normalize(e.b - e.a);
            glm::vec3 perpRight = glm::vec3(-edgeDir.z, 0.0f, edgeDir.x);
            glm::vec3 perpLeft = glm::vec3(edgeDir.z, 0.0f, -edgeDir.x);

            glm::vec3 testR = pos + perpRight * 1.5f;
            glm::vec3 testL = pos + perpLeft * 1.5f;

            game::GroundSample sr, sl;
            bool roadRight = mPhysics.GetGroundSample(testR, pos.y + 1.0f, sr, 3.0f, 1.5f) && sr.found;
            bool roadLeft = mPhysics.GetGroundSample(testL, pos.y + 1.0f, sl, 3.0f, 1.5f) && sl.found;

            glm::vec3 outDir;
            if (roadRight && !roadLeft)
                outDir = perpLeft; // carretera a la derecha → empujar a la izquierda
            else if (roadLeft && !roadRight)
                outDir = perpRight; // carretera a la izquierda → empujar a la derecha
            else
            {
                // fallback al centroide si ambos lados tienen carretera o ninguno
                outDir = pos - centroid;
                outDir.y = 0.0f;
            }

            float outLen = glm::length(outDir);
            if (outLen > 1e-4f)
                outDir /= outLen;
            else
                outDir = glm::vec3(1.0f, 0.0f, 0.0f);

            pos += outDir * 2.8f;

            // Bajar el poste al suelo real usando GetGroundSample
            game::GroundSample sample;
            if (mPhysics.GetGroundSample(pos, pos.y + 2.0f, sample, 8.0f, 2.0f) && sample.found)
                pos.y = sample.height;
            else
                pos.y = e.a.y; // fallback a la altura del borde

            // Evitar que queden muy juntos
            bool tooClose = false;
            for (const auto &existing : positions)
            {
                glm::vec3 diff = existing - pos;
                diff.y = 0.0f;
                if (glm::dot(diff, diff) < minDistSq)
                {
                    tooClose = true;
                    break;
                }
            }
            if (!tooClose)
                positions.push_back(pos);
        }
    }

    return positions;
}

std::vector<glm::vec3> City::GetStreetLampPositionsFromFile(const char* filePath) const
{
    std::vector<glm::vec3> positions;
    std::ifstream f(filePath);
    if (!f.good()) return positions;
    float x, y, z;
    while (f >> x >> y >> z)
        positions.push_back(glm::vec3(x, y, z));
    std::cout << "[LAMPS] Loaded " << positions.size() << " positions from " << filePath << std::endl;
    return positions;
}
