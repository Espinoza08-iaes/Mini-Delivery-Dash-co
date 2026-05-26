#ifndef MODEL_CLASS_H
#define MODEL_CLASS_H

#include <json/json.h>
#include <string>
#include <vector>
#include <assimp/material.h>
#include <assimp/scene.h>

#include "Mesh.h"

using json = nlohmann::json;

struct CollisionBox
{
    glm::vec3 min;
    glm::vec3 max;
};


class Model
{
public:
    Model(const char* file);

    const std::vector<Mesh>& GetMeshes() const { return meshes; }
    const std::vector<glm::mat4>& GetMatricesMeshes() const { return matricesMeshes; }

    void Draw(
        Shader& shader,
        Camera& camera,
        glm::mat4 worldMatrix = glm::mat4(1.0f),
        float wheelSpin = 0.0f,
        float steeringAngle = 0.0f,
        bool headlightsOn = false,
        bool braking = false
    );

    //Declara la función para construir las cajas de colisión a partir de las mallas del modelo
 /*
 std::vector<CollisionBox> BuildCollisionBoxes(
        const glm::mat4& worldMatrix,
        float minimumHeight,
        float minimumHorizontalSize
    ) const;
 */
    
private:
    const char* file;
    std::vector<unsigned char> data;
    json JSON;

    std::vector<Mesh> meshes;
    std::vector<glm::vec3> translationsMeshes;
    std::vector<glm::quat> rotationsMeshes;
    std::vector<glm::vec3> scalesMeshes;
    std::vector<glm::mat4> matricesMeshes;
    std::vector<bool> meshesUseAlpha;
    std::vector<std::string> meshMaterialNames;
    std::vector<glm::vec3> meshCenters;

    std::vector<std::string> loadedTexName;
    std::vector<Texture> loadedTex;

    void loadMesh(unsigned int indMesh);
    void loadAssimp(const std::string& filePath);
    void loadObj(const std::string& filePath);
    void processAssimpNode(aiNode* node, const aiScene* scene, const glm::mat4& parentMatrix, const std::string& fileDirectory);
    void processAssimpMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& meshMatrix, const std::string& fileDirectory);

    void traverseNode(unsigned int nextNode, glm::mat4 matrix = glm::mat4(1.0f));

    std::vector<unsigned char> getData();
    std::vector<float> getFloats(json accessor);
    std::vector<GLuint> getIndices(json accessor);
    std::vector<Texture> getTextures();
    std::vector<Texture> buildAssimpTextures(aiMaterial* material, const std::string& materialName, const std::string& fileDirectory);
    Texture loadTextureCached(const std::string& texturePath, const char* type, GLuint slot);
    Texture loadSolidTextureCached(const std::string& cacheKey, const unsigned char* pixel, const char* type, GLuint slot);

    std::vector<Vertex> assembleVertices(
        std::vector<glm::vec3> positions,
        std::vector<glm::vec3> normals,
        std::vector<glm::vec2> texUVs
    );

    std::vector<glm::vec2> groupFloatsVec2(std::vector<float> floatVec);
    std::vector<glm::vec3> groupFloatsVec3(std::vector<float> floatVec);
    std::vector<glm::vec4> groupFloatsVec4(std::vector<float> floatVec);
};

#endif