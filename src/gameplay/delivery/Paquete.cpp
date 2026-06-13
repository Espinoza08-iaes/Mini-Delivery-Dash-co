#include "Paquete.h"
#include <vector>

Paquete::Paquete(const glm::vec3& pos)
    : position(pos)
    , rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f))
    , scale(glm::vec3(1.0f))
    , mesh(CreatePackageMesh())
{
}

void Paquete::Render(Shader& shader, Camera& camera)
{
    mesh.Draw(shader, camera, glm::mat4(1.0f), position, rotation, scale);
}

void Paquete::SetPosition(const glm::vec3& pos)
{
    position = pos;
}

void Paquete::SetRotation(const glm::quat& rot)
{
    rotation = rot;
}

void Paquete::SetScale(const glm::vec3& s)
{
    scale = s;
}

Mesh Paquete::CreatePackageMesh()
{
    const float size = 1.0f;
    const float half = size / 2.0f;
    
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    // Cube vertices with proper UV mapping
    // Front face
    vertices.push_back({{-half, -half,  half}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ half, -half,  half}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ half,  half,  half}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-half,  half,  half}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}});
    
    // Back face
    vertices.push_back({{-half, -half, -half}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{-half,  half, -half}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{ half,  half, -half}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}});
    vertices.push_back({{ half, -half, -half}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
    
    // Left face
    vertices.push_back({{-half, -half, -half}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{-half, -half,  half}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{-half,  half,  half}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-half,  half, -half}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}});
    
    // Right face
    vertices.push_back({{ half, -half, -half}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ half,  half, -half}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{ half,  half,  half}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}});
    vertices.push_back({{ half, -half,  half}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
    
    // Top face
    vertices.push_back({{-half,  half, -half}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}});
    vertices.push_back({{-half,  half,  half}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ half,  half,  half}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ half,  half, -half}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}});
    
    // Bottom face
    vertices.push_back({{-half, -half, -half}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
    vertices.push_back({{ half, -half, -half}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}});
    vertices.push_back({{ half, -half,  half}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}});
    vertices.push_back({{-half, -half,  half}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}});
    
    // Indices for each face (2 triangles per face)
    std::vector<GLuint> faceIndices = {
        0, 1, 2, 2, 3, 0,    // Front
        4, 5, 6, 6, 7, 4,    // Back
        8, 9, 10, 10, 11, 8, // Left
        12, 13, 14, 14, 15, 12, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };
    
    indices = faceIndices;
    
    // Load texture
    std::vector<Texture> textures;
    textures.emplace_back("res/textures/paquete.jpg", "diffuse", 0);
    
    return Mesh(vertices, indices, textures);
}
