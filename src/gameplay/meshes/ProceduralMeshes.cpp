#include "ProceduralMeshes.h"

#include <vector>
#include <cmath>
    
// ---------------------------------------------------------------------------
// Ground / marker meshes
// ---------------------------------------------------------------------------
Mesh CreateOceanMesh()
{
    const int   GRID     = 80;
    const float halfSize = 1200.0f;
    const float Y        = -0.5f;
    const float step     = (halfSize * 2.0f) / GRID;

    std::vector<Vertex>  vertices;
    std::vector<GLuint>  indices;
    vertices.reserve((GRID + 1) * (GRID + 1));
    indices.reserve(GRID * GRID * 6);

    for (int z = 0; z <= GRID; ++z)
    {
        for (int x = 0; x <= GRID; ++x)
        {
            Vertex v;
            v.position = glm::vec3(-halfSize + x * step, Y, -halfSize + z * step);
            v.normal   = glm::vec3(0.0f, 1.0f, 0.0f);
            v.color    = glm::vec3(1.0f, 1.0f, 1.0f);
            v.texUV    = glm::vec2((float)x / GRID, (float)z / GRID);
            vertices.push_back(v);
        }
    }

    for (int z = 0; z < GRID; ++z)
    {
        for (int x = 0; x < GRID; ++x)
        {
            GLuint tl = z * (GRID + 1) + x;
            GLuint tr = tl + 1;
            GLuint bl = tl + (GRID + 1);
            GLuint br = bl + 1;
            indices.push_back(tl); indices.push_back(bl); indices.push_back(tr);
            indices.push_back(tr); indices.push_back(bl); indices.push_back(br);
        }
    }

    std::vector<Texture> textures;
    textures.emplace_back("res/textures/sunflowers_puresky_4k.hdr", "diffuse", 0);
    return Mesh(vertices, indices, textures);
}

Mesh CreateGroundMesh()
{
    const float halfSize = 500.0f;
    const float uvScale = 100.0f;
    std::vector<Vertex> vertices =
        {
            {{-halfSize, 0.0f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{halfSize, 0.0f, -halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {uvScale, 0.0f}},
            {{halfSize, 0.0f, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {uvScale, uvScale}},
            {{-halfSize, 0.0f, halfSize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, uvScale}}};
    std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};

    const int texWidth = 64, texHeight = 64;
    std::vector<unsigned char> checkerData(texWidth * texHeight * 4);
    for (int y = 0; y < texHeight; ++y)
    {
        for (int x = 0; x < texWidth; ++x)
        {
            int idx = (y * texWidth + x) * 4;
            bool isDark = ((x / 8) + (y / 8)) % 2 == 0;
            checkerData[idx + 0] = isDark ? 34 : 46;
            checkerData[idx + 1] = isDark ? 68 : 92;
            checkerData[idx + 2] = isDark ? 42 : 56;
            checkerData[idx + 3] = 255;
        }
    }

    static const unsigned char blackPixel[] = {0, 0, 0, 255};
    std::vector<Texture> textures;
    textures.emplace_back(checkerData.data(), texWidth, texHeight, GL_RGBA, "diffuse", 0);
    textures.emplace_back(blackPixel, 1, 1, GL_RGBA, "specular", 1);
    return Mesh(vertices, indices, textures);
}

Mesh CreateOriginMarker()
{
    const float halfSize = 4.0f;
    std::vector<Vertex> vertices =
        {
            {{-halfSize, 0.005f, -halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {0.0f, 0.0f}},
            {{halfSize, 0.005f, -halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {1.0f, 0.0f}},
            {{halfSize, 0.005f, halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {1.0f, 1.0f}},
            {{-halfSize, 0.005f, halfSize}, {0.0f, 1.0f, 0.0f}, {0.95f, 0.25f, 0.25f}, {0.0f, 1.0f}}};
    std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};

    static const unsigned char whitePixel[] = {255, 255, 255, 255};
    static const unsigned char blackPixel[] = {0, 0, 0, 255};
    std::vector<Texture> textures;
    textures.emplace_back(whitePixel, 1, 1, GL_RGBA, "diffuse", 0);
    textures.emplace_back(blackPixel, 1, 1, GL_RGBA, "specular", 1);
    return Mesh(vertices, indices, textures);
}

Mesh CreateSkySphereMesh(const char* texturePath, int sectors, int stacks, float radius)
{
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    const float PI = 3.14159265f;

    for (int i = 0; i <= stacks; ++i)
    {
        float stackAngle = PI / 2.0f - i * PI / stacks; // from pi/2 to -pi/2
        float xy = radius * std::cos(stackAngle);
        float y = radius * std::sin(stackAngle);

        for (int j = 0; j <= sectors; ++j)
        {
            float sectorAngle = j * 2 * PI / sectors; // from 0 to 2pi

            float x = xy * std::cos(sectorAngle);
            float z = xy * std::sin(sectorAngle);

            Vertex v;
            v.position = glm::vec3(x, y, z);
            v.normal = glm::normalize(glm::vec3(-x, -y, -z)); // point inwards
            v.color = glm::vec3(1.0f);
            v.texUV = glm::vec2((float)j / sectors, (float)i / stacks);
            vertices.push_back(v);
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != (stacks - 1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    std::vector<Texture> textures;
    textures.emplace_back(texturePath, "diffuse", 0);
    return Mesh(vertices, indices, textures);
}