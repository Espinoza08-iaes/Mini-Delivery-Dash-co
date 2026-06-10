#include "streetLamp.h"

#include <vector>
#include <cmath>

static void AddCylinderY(std::vector<Vertex>& vertices, std::vector<GLuint>& indices,
                          float cx, float cy, float cz,
                          float r, float h, int segs, glm::vec3 color)
{
    const float PI2 = 2.0f * 3.14159265f;
    GLuint base = static_cast<GLuint>(vertices.size());
    for (int i = 0; i <= segs; ++i)
    {
        float a  = i * PI2 / segs;
        float nx = std::cos(a), nz = std::sin(a);
        Vertex vb, vt;
        vb.position = glm::vec3(cx + r * nx, cy,     cz + r * nz);
        vb.normal   = glm::vec3(nx, 0.0f, nz);
        vb.color    = color;
        vb.texUV    = glm::vec2((float)i / segs, 0.0f);
        vt.position = glm::vec3(cx + r * nx, cy + h, cz + r * nz);
        vt.normal   = glm::vec3(nx, 0.0f, nz);
        vt.color    = color;
        vt.texUV    = glm::vec2((float)i / segs, 1.0f);
        vertices.push_back(vb);
        vertices.push_back(vt);
    }
    for (int i = 0; i < segs; ++i)
    {
        GLuint b0 = base + i * 2;
        indices.push_back(b0);     indices.push_back(b0+1); indices.push_back(b0+2);
        indices.push_back(b0+1);   indices.push_back(b0+3); indices.push_back(b0+2);
    }
}

static void AddCylinderX(std::vector<Vertex>& vertices, std::vector<GLuint>& indices,
                          float cx, float cy, float cz,
                          float r, float len, int segs, glm::vec3 color)
{
    const float PI2 = 2.0f * 3.14159265f;
    GLuint base = static_cast<GLuint>(vertices.size());
    for (int i = 0; i <= segs; ++i)
    {
        float a  = i * PI2 / segs;
        float ny = std::cos(a), nz = std::sin(a);
        Vertex vb, vt;
        vb.position = glm::vec3(cx,       cy + r * ny, cz + r * nz);
        vb.normal   = glm::vec3(0.0f, ny, nz);
        vb.color    = color;
        vb.texUV    = glm::vec2((float)i / segs, 0.0f);
        vt.position = glm::vec3(cx + len, cy + r * ny, cz + r * nz);
        vt.normal   = glm::vec3(0.0f, ny, nz);
        vt.color    = color;
        vt.texUV    = glm::vec2((float)i / segs, 1.0f);
        vertices.push_back(vb);
        vertices.push_back(vt);
    }
    for (int i = 0; i < segs; ++i)
    {
        GLuint b0 = base + i * 2;
        indices.push_back(b0);   indices.push_back(b0+1); indices.push_back(b0+2);
        indices.push_back(b0+1); indices.push_back(b0+3); indices.push_back(b0+2);
    }
}

Mesh CreateStreetLampMesh()
{
    std::vector<Vertex>  vertices;
    std::vector<GLuint>  indices;

    glm::vec3 poleColor(0.20f, 0.20f, 0.22f);
    glm::vec3 bulbColor(1.0f,  0.97f, 0.80f);

    // Poste vertical
    AddCylinderY(vertices, indices, 0.0f, 0.0f, 0.0f,  0.04f, 4.5f, 8, poleColor);
    // Brazo horizontal
    AddCylinderX(vertices, indices, 0.0f, 4.5f, 0.0f,  0.032f, 0.7f, 8, poleColor);
    // Bombilla al final del brazo
    AddCylinderY(vertices, indices, 0.7f, 4.3f, -0.13f, 0.13f, 0.22f, 8, bulbColor);

    static const unsigned char white[] = {255, 255, 255, 255};
    static const unsigned char black[] = {0,   0,   0,   255};
    std::vector<Texture> textures;
    textures.emplace_back(white, 1, 1, GL_RGBA, "diffuse",  0);
    textures.emplace_back(black, 1, 1, GL_RGBA, "specular", 1);

    return Mesh(vertices, indices, textures);
}