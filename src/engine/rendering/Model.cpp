#include "Model.h"
#include "../graphics/Frustum.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <map>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{
void SplitWheelMesh(const Mesh& originalMesh, const std::string& materialName, 
                    std::vector<Mesh>& outMeshes, 
                    std::vector<std::string>& outMaterials, 
                    std::vector<glm::vec3>& outCenters, 
                    std::vector<bool>& outAlpha)
{
    std::vector<Vertex> sectorVertices[4];
    std::vector<GLuint> sectorIndices[4];
    
    std::vector<int> oldToNewIndex[4];
    for (int s = 0; s < 4; ++s)
    {
        oldToNewIndex[s].assign(originalMesh.vertices.size(), -1);
    }
    
    for (size_t i = 0; i < originalMesh.indices.size(); i += 3)
    {
        GLuint idx0 = originalMesh.indices[i];
        GLuint idx1 = originalMesh.indices[i+1];
        GLuint idx2 = originalMesh.indices[i+2];
        
        const Vertex& v0 = originalMesh.vertices[idx0];
        const Vertex& v1 = originalMesh.vertices[idx1];
        const Vertex& v2 = originalMesh.vertices[idx2];
        
        float triX = (v0.position.x + v1.position.x + v2.position.x) / 3.0f;
        float triZ = (v0.position.z + v1.position.z + v2.position.z) / 3.0f;
        
        int sector = 0;
        if (triX >= 0.0f)
        {
            sector = (triZ >= 0.0f) ? 0 : 2; // FL (0) or RL (2)
        }
        else
        {
            sector = (triZ >= 0.0f) ? 1 : 3; // FR (1) or RR (3)
        }
        
        auto addVertex = [&](int s, GLuint oldIdx) -> GLuint {
            if (oldToNewIndex[s][oldIdx] != -1)
                return oldToNewIndex[s][oldIdx];
            
            GLuint newIdx = sectorVertices[s].size();
            sectorVertices[s].push_back(originalMesh.vertices[oldIdx]);
            oldToNewIndex[s][oldIdx] = newIdx;
            return newIdx;
        };
        
        GLuint newIdx0 = addVertex(sector, idx0);
        GLuint newIdx1 = addVertex(sector, idx1);
        GLuint newIdx2 = addVertex(sector, idx2);
        
        sectorIndices[sector].push_back(newIdx0);
        sectorIndices[sector].push_back(newIdx1);
        sectorIndices[sector].push_back(newIdx2);
    }
    
    std::string prefix[4] = { "FL_", "FR_", "RL_", "RR_" };
    for (int s = 0; s < 4; ++s)
    {
        if (sectorVertices[s].empty())
            continue;
            
        glm::vec3 center(0.0f);
        for (const auto& v : sectorVertices[s])
        {
            center += v.position;
        }
        center /= static_cast<float>(sectorVertices[s].size());
        
        std::vector<Texture> texturesCopy = originalMesh.textures;
        outMeshes.push_back(Mesh(sectorVertices[s], sectorIndices[s], texturesCopy));
        outMaterials.push_back(prefix[s] + materialName);
        outCenters.push_back(center);
        outAlpha.push_back(false);
    }
}

std::string get_file_contents(const char* filename)
{
    std::ifstream in(filename, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error(std::string("Failed to open file: ") + filename);
    }

    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
}

std::string getDirectory(const std::string& filePath)
{
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash == std::string::npos)
    {
        return "";
    }
    return filePath.substr(0, lastSlash + 1);
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool hasExtension(const std::string& filePath, const char* extension)
{
    std::string lowerPath = toLower(filePath);
    std::string lowerExt = toLower(std::string(extension));
    if (lowerPath.length() < lowerExt.length())
    {
        return false;
    }
    return lowerPath.compare(lowerPath.length() - lowerExt.length(), lowerExt.length(), lowerExt) == 0;
}

struct ObjIndex
{
    int v = -1;
    int vt = -1;
    int vn = -1;
};

int resolveIndex(int idx, size_t size)
{
    if (idx > 0)
    {
        return idx - 1;
    }
    if (idx < 0)
    {
        return static_cast<int>(size) + idx;
    }
    return -1;
}

ObjIndex parseObjIndex(const std::string& token)
{
    ObjIndex index;
    size_t firstSlash = token.find('/');
    if (firstSlash == std::string::npos)
    {
        index.v = std::stoi(token);
        return index;
    }

    std::string vStr = token.substr(0, firstSlash);
    if (!vStr.empty())
    {
        index.v = std::stoi(vStr);
    }

    size_t secondSlash = token.find('/', firstSlash + 1);
    if (secondSlash == std::string::npos)
    {
        std::string vtStr = token.substr(firstSlash + 1);
        if (!vtStr.empty())
        {
            index.vt = std::stoi(vtStr);
        }
        return index;
    }

    std::string vtStr = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
    if (!vtStr.empty())
    {
        index.vt = std::stoi(vtStr);
    }

    std::string vnStr = token.substr(secondSlash + 1);
    if (!vnStr.empty())
    {
        index.vn = std::stoi(vnStr);
    }

    return index;
}

glm::vec3 getMaterialColor(const std::string& matName)
{
    if (matName == "McLaren_F1_1993_By_Alex_Ka_") return glm::vec3(0.92f, 0.92f, 0.92f); // Gloss Metallic Silver/White
    if (matName == "McLAREN_RED_LINE") return glm::vec3(0.9f, 0.05f, 0.05f); // Red Accent
    if (matName == "F1_red_F" || matName == "F1_red_R") return glm::vec3(0.9f, 0.05f, 0.05f); // Red
    if (matName == "brakelight" || matName == "rear_lamp") return glm::vec3(0.9f, 0.05f, 0.05f); // Red
    if (matName == "exhaust_gold") return glm::vec3(0.9f, 0.72f, 0.15f); // Gold
    if (matName == "bronze") return glm::vec3(0.7f, 0.52f, 0.2f); // Bronze
    if (matName == "chrome" || matName == "chrome2" || matName == "rim" || matName == "rimbolt") return glm::vec3(0.95f, 0.95f, 0.95f); // Chrome/Silver
    if (matName == "headlight_1" || matName == "headlight_2") return glm::vec3(0.9f, 0.9f, 0.9f); // Silver
    if (matName == "front_turn_signal" || matName == "side_turn_signal" || matName == "rear_turn_signal") return glm::vec3(1.0f, 0.55f, 0.0f); // Amber
    if (matName == "exhaust" || matName == "exhaust_metal_circles") return glm::vec3(0.6f, 0.6f, 0.6f); // Brushed Steel
    if (matName == "floor") return glm::vec3(1.0f, 1.0f, 1.0f); // White floor
    
    // Default to dark/black if plastic or matte
    if (matName.find("black") != std::string::npos || matName == "pl" || matName == "_" || matName.find("grill") != std::string::npos || matName == "F1_front_plastic")
    {
        return glm::vec3(0.12f, 0.12f, 0.12f);
    }
    
    // Default to white for anything else
    return glm::vec3(1.0f, 1.0f, 1.0f);
}

glm::vec3 calculateCenter(const std::vector<Vertex>& vertices)
{
    if (vertices.empty())
    {
        return glm::vec3(0.0f);
    }

    glm::vec3 sum(0.0f);
    for (const Vertex& vertex : vertices)
    {
        sum += vertex.position;
    }

    return sum / static_cast<float>(vertices.size());
}

void calculateBounds(const std::vector<Vertex>& vertices, glm::vec3& outMin, glm::vec3& outMax)
{
    if (vertices.empty())
    {
        outMin = glm::vec3(0.0f);
        outMax = glm::vec3(0.0f);
        return;
    }
    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(-std::numeric_limits<float>::max());
    for (const Vertex& v : vertices)
    {
        outMin = glm::min(outMin, v.position);
        outMax = glm::max(outMax, v.position);
    }
}

bool materialUsesAlpha(const std::string& matName)
{
    return matName == "windo" || matName == "windo_F" || matName == "windo_R" || matName == "windo_S" ||
           matName == "headlightglass" ||
           matName == "F1_side" || matName == "McLAREN_sidelogo" || matName == "McLAREN_sidelogo_FBUMPER" ||
           matName == "suport" || matName == "McLaren_supportlogo" ||
           matName == "door_stitch";
}

std::string getKnownDiffuseTexture(const std::string& matName)
{
    if (matName == "McLaren_F1_1993_By_Alex_Ka_") return "res/models/mclaren/textures/McLAREN_F1.png";
    if (matName == "tire") return "res/models/mclaren/textures/tire.jpeg";
    if (matName == "tire_side") return "res/models/mclaren/textures/tire side.jpeg";
    if (matName == "interior") return "res/models/mclaren/textures/interior.jpeg";
    if (matName == "plate_F") return "res/models/mclaren/textures/plate F.jpeg";
    if (matName == "plate_R") return "res/models/mclaren/textures/plate R.jpeg";
    if (matName == "engine") return "res/models/mclaren/textures/engine.jpeg";
    if (matName == "bottom") return "res/models/mclaren/textures/bottom.jpeg";
    if (matName == "F1_side" || matName == "McLAREN_sidelogo" || matName == "McLAREN_sidelogo_FBUMPER") return "res/models/mclaren/textures/McLaren F1 side.png";
    if (matName == "suport" || matName == "McLaren_supportlogo") return "res/models/mclaren/textures/McLaren support logo.png";
    if (matName == "door_stitch") return "res/models/mclaren/textures/stitch.png";
    if (matName == "headlightglass") return "res/models/mclaren/textures/glass.png";
    if (matName == "windo" || matName == "windo_F" || matName == "windo_R" || matName == "windo_S") return "res/models/mclaren/textures/windo.png";
    if (matName == "floor") return "res/models/mclaren/textures/Floor Circle.png";
    return "";
}

std::string getKnownSpecularTexture(const std::string& matName)
{
    if (matName == "McLaren_F1_1993_By_Alex_Ka_") return "res/models/mclaren/textures/carshadow.png";
    return "";
}

bool fileExists(const std::string& path)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    return f.good();
}

bool isAbsolutePath(const std::string& path)
{
    return (path.length() > 2 && path[1] == ':') || (!path.empty() && (path[0] == '/' || path[0] == '\\'));
}

std::string joinAssetPath(const std::string& directory, const std::string& assetPath)
{
    if (assetPath.empty() || assetPath[0] == '*')
    {
        return "";
    }

    if (isAbsolutePath(assetPath))
    {
        return assetPath;
    }

    return directory + assetPath;
}

bool getMaterialTexturePath(aiMaterial* material, aiTextureType textureType, const std::string& fileDirectory, std::string& outPath)
{
    if (material == nullptr || material->GetTextureCount(textureType) == 0)
    {
        return false;
    }

    aiString texturePath;
    if (material->GetTexture(textureType, 0, &texturePath) != AI_SUCCESS)
    {
        return false;
    }

    outPath = joinAssetPath(fileDirectory, texturePath.C_Str());
    return !outPath.empty() && fileExists(outPath);
}

glm::mat4 toGlmMatrix(const aiMatrix4x4& matrix)
{
    return glm::mat4(
        matrix.a1, matrix.b1, matrix.c1, matrix.d1,
        matrix.a2, matrix.b2, matrix.c2, matrix.d2,
        matrix.a3, matrix.b3, matrix.c3, matrix.d3,
        matrix.a4, matrix.b4, matrix.c4, matrix.d4
    );
}

glm::vec3 getAssimpMaterialColor(aiMaterial* material, const std::string& matName)
{
    glm::vec3 color = getMaterialColor(matName);
    aiColor4D assimpColor;
    if (material != nullptr && aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &assimpColor) == AI_SUCCESS)
    {
        color = glm::vec3(assimpColor.r, assimpColor.g, assimpColor.b);
    }
    else if (material != nullptr && aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &assimpColor) == AI_SUCCESS)
    {
        color = glm::vec3(assimpColor.r, assimpColor.g, assimpColor.b);
    }
    return color;
}

} // namespace

Model::Model(const char* file)
{
    std::ofstream log("debug_log.txt", std::ios::app);
    log << "Model constructor called with file: '" << file << "'" << std::endl;
    std::string fileStr = std::string(file);
    log << "  hasExtension(.obj): " << (hasExtension(fileStr, ".obj") ? "YES" : "NO") << std::endl;
    log.close();

    Model::file = file;
    if (hasExtension(fileStr, ".obj") || hasExtension(fileStr, ".gltf") || hasExtension(fileStr, ".glb"))
    {
        loadAssimp(fileStr);
        preclassifyMeshes();
        return;
    }

    // Make a JSON object
    std::string text = get_file_contents(file);
    JSON = json::parse(text);

    // Get the binary data
    data = getData();

    // Traverse all nodes
    traverseNode(0);
    preclassifyMeshes();
}

void Model::Draw(Shader& shader, Camera& camera, glm::mat4 worldMatrix, float wheelSpin, float steeringAngle, bool headlightsOn, bool braking, const Frustum* frustum)
{
    for (unsigned int i = 0; i < meshes.size(); i++)
    {
        // Skip hidden meshes
        if (meshesHidden[i])
        {
            continue;
        }

        glm::mat4 meshMatrix = matricesMeshes[i];

        if (frustum)
        {
            glm::mat4 M = worldMatrix * meshMatrix;
            glm::vec3 localMin = meshBoundsMin[i];
            glm::vec3 localMax = meshBoundsMax[i];
            glm::vec3 worldMin(M[3]);
            glm::vec3 worldMax(M[3]);
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    float a = M[c][r] * localMin[c];
                    float b = M[c][r] * localMax[c];
                    if (a < b) {
                        worldMin[r] += a;
                        worldMax[r] += b;
                    } else {
                        worldMin[r] += b;
                        worldMax[r] += a;
                    }
                }
            }
            if (!frustum->IsBoxVisible(worldMin, worldMax))
            {
                continue;
            }
        }

        glUniform1i(glGetUniformLocation(shader.ID, "uUseAlpha"), meshesUseAlpha[i] ? 1 : 0);

        // Emissive lights (headlights / brakelights)
        bool isEmissive = false;
        glm::vec3 emissiveColor(0.0f);
        int emissiveType = meshEmissiveType[i];

        if (emissiveType == 1 && headlightsOn)
        {
            isEmissive = true;
            emissiveColor = glm::vec3(1.5f, 1.5f, 1.2f); // Bright yellow-white headlights
        }
        else if (emissiveType == 2 && headlightsOn)
        {
            isEmissive = true;
            emissiveColor = glm::vec3(1.5f, 0.7f, 0.0f); // Bright amber signals
        }
        else if (emissiveType == 3 && braking)
        {
            isEmissive = true;
            emissiveColor = glm::vec3(2.0f, 0.0f, 0.0f); // Intense red braking light
        }
        else if (emissiveType == 4 && headlightsOn)
        {
            isEmissive = true;
            emissiveColor = glm::vec3(0.9f, 0.1f, 0.1f); // Standard red tail lamp
        }

        glUniform1i(glGetUniformLocation(shader.ID, "uIsEmissive"), isEmissive ? 1 : 0);
        if (isEmissive)
        {
            glUniform3f(glGetUniformLocation(shader.ID, "uEmissiveColor"), emissiveColor.x, emissiveColor.y, emissiveColor.z);
        }

        // Animate wheels if this is a split wheel mesh
        int wheelType = meshWheelType[i];
        if (wheelType != 0)
        {
            glm::vec3 C = meshCenters[i];
            glm::mat4 wheelTransform = glm::mat4(1.0f);
            
            // Translate to wheel local center
            wheelTransform = glm::translate(wheelTransform, C);
            
            // Apply steering rotation for front wheels (FL = 1 and FR = 2)
            if (wheelType == 1 || wheelType == 2)
            {
                wheelTransform = glm::rotate(wheelTransform, steeringAngle, glm::vec3(0.0f, 1.0f, 0.0f));
            }
            
            // Apply wheel spin rotation (all wheels around their local axles)
            wheelTransform = glm::rotate(wheelTransform, wheelSpin, glm::vec3(1.0f, 0.0f, 0.0f));
            
            // Translate back
            wheelTransform = glm::translate(wheelTransform, -C);
            
            meshMatrix = wheelTransform * meshMatrix;
        }

        glUniform1f(glGetUniformLocation(shader.ID, "uReflectivity"), meshReflectivity[i]);

        meshes[i].Mesh::Draw(shader, camera, worldMatrix * meshMatrix);
    }
}


//Funcion tipo caja de colisiones, se le pasa la matriz del mundo para transformar las posiciones de los vertices a coordenadas globales, y se le pasan los tamaños minimos para filtrar las cajas de colision que sean demasiado pequeñas para ser relevantes (como las de los arboles o farolas)

/*
std::vector<CollisionBox> Model::BuildCollisionBoxes(
    const glm::mat4& worldMatrix,
    float minimumHeight,
    float minimumHorizontalSize
) const
{
    std::vector<CollisionBox> boxes;

    for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
    {
        const Mesh& mesh = meshes[meshIndex];
        if (mesh.vertices.empty())
        {
            continue;
        }

        glm::mat4 meshMatrix = worldMatrix * matricesMeshes[meshIndex];
        glm::vec3 minBounds(std::numeric_limits<float>::max());
        glm::vec3 maxBounds(-std::numeric_limits<float>::max());

        for (const Vertex& vertex : mesh.vertices)
        {
            glm::vec3 worldPos = glm::vec3(meshMatrix * glm::vec4(vertex.position, 1.0f));
            minBounds = glm::min(minBounds, worldPos);
            maxBounds = glm::max(maxBounds, worldPos);
        }

        glm::vec3 size = maxBounds - minBounds;
        if (size.y < minimumHeight)
        {
            continue;
        }
        if (size.x < minimumHorizontalSize && size.z < minimumHorizontalSize)
        {
            continue;
        }

        boxes.push_back({ minBounds, maxBounds });
    }

    return boxes;
}
 */


void Model::loadMesh(unsigned int indMesh)
{
    // Get all accessor indices
    unsigned int posAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["POSITION"];
    unsigned int normalAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["NORMAL"];
    unsigned int texAccInd = JSON["meshes"][indMesh]["primitives"][0]["attributes"]["TEXCOORD_0"];
    unsigned int indAccInd = JSON["meshes"][indMesh]["primitives"][0]["indices"];

    // Use accessor indices to get all vertices components
    std::vector<float> posVec = getFloats(JSON["accessors"][posAccInd]);
    std::vector<glm::vec3> positions = groupFloatsVec3(posVec);
    std::vector<float> normalVec = getFloats(JSON["accessors"][normalAccInd]);
    std::vector<glm::vec3> normals = groupFloatsVec3(normalVec);
    std::vector<float> texVec = getFloats(JSON["accessors"][texAccInd]);
    std::vector<glm::vec2> texUVs = groupFloatsVec2(texVec);

    // Combine all the vertex components and also get the indices and textures
    std::vector<Vertex> vertices = assembleVertices(positions, normals, texUVs);
    std::vector<GLuint> indices = getIndices(JSON["accessors"][indAccInd]);
    std::vector<Texture> textures = getTextures();
    std::string meshName = JSON["meshes"][indMesh].value("name", std::string("gltf"));
    meshMaterialNames.push_back(meshName);
    meshCollisionNames.push_back(meshName);
    meshCenters.push_back(calculateCenter(vertices));

    // Compute AABB for frustum culling
    glm::vec3 bMin, bMax;
    calculateBounds(vertices, bMin, bMax);
    meshBoundsMin.push_back(bMin);
    meshBoundsMax.push_back(bMax);

    // Combine the vertices, indices, and textures into a mesh
    meshes.push_back(Mesh(vertices, indices, textures));
    meshesUseAlpha.push_back(false);
}

void Model::loadAssimp(const std::string& filePath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality
    );

    if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr)
    {
        throw std::runtime_error(std::string("Assimp failed to load model: ") + importer.GetErrorString());
    }

    std::string fileDirectory = getDirectory(filePath);

    size_t meshCountBefore = meshes.size();
    processAssimpNode(scene->mRootNode, scene, glm::mat4(1.0f), fileDirectory);

    std::ofstream logFile("debug_log.txt", std::ios::app);
    logFile << "Assimp loaded scene meshes: " << scene->mNumMeshes
            << ", drawable meshes: " << (meshes.size() - meshCountBefore)
            << " from file: " << filePath << std::endl;
    logFile.close();
}

void Model::processAssimpNode(aiNode* node, const aiScene* scene, const glm::mat4& parentMatrix, const std::string& fileDirectory, const std::string& nodePath)
{
    if (node == nullptr)
    {
        return;
    }

    glm::mat4 nodeMatrix = parentMatrix * toGlmMatrix(node->mTransformation);
    std::string nodeName = node->mName.C_Str();
    std::string currentNodePath = nodePath;
    if (!nodeName.empty())
    {
        currentNodePath = currentNodePath.empty() ? nodeName : currentNodePath + " " + nodeName;
    }

    for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex)
    {
        unsigned int sceneMeshIndex = node->mMeshes[meshIndex];
        if (sceneMeshIndex < scene->mNumMeshes)
        {
            processAssimpMesh(scene->mMeshes[sceneMeshIndex], scene, nodeMatrix, fileDirectory, currentNodePath);
        }
    }

    for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
    {
        processAssimpNode(node->mChildren[childIndex], scene, nodeMatrix, fileDirectory, currentNodePath);
    }
}

void Model::processAssimpMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& meshMatrix, const std::string& fileDirectory, const std::string& nodePath)
{
    if (mesh == nullptr)
    {
        return;
    }

    std::string materialName = "default";
    aiMaterial* material = nullptr;
    if (scene->mMaterials != nullptr && mesh->mMaterialIndex < scene->mNumMaterials)
    {
        material = scene->mMaterials[mesh->mMaterialIndex];
        aiString matName;
        if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
        {
            materialName = matName.C_Str();
        }
    }

    std::string meshName = mesh->mName.C_Str();
    std::string collisionName = materialName;
    if (!meshName.empty())
    {
        collisionName += " " + meshName;
    }
    if (!nodePath.empty())
    {
        collisionName += " " + nodePath;
    }

    glm::vec3 materialColor = getAssimpMaterialColor(material, materialName);

    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    vertices.reserve(mesh->mNumVertices);
    for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
    {
        Vertex vertex{};
        vertex.position = glm::vec3(
            mesh->mVertices[vertexIndex].x,
            mesh->mVertices[vertexIndex].y,
            mesh->mVertices[vertexIndex].z
        );

        if (mesh->HasNormals())
        {
            vertex.normal = glm::vec3(
                mesh->mNormals[vertexIndex].x,
                mesh->mNormals[vertexIndex].y,
                mesh->mNormals[vertexIndex].z
            );
        }
        else
        {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (mesh->HasTextureCoords(0))
        {
            vertex.texUV = glm::vec2(
                mesh->mTextureCoords[0][vertexIndex].x,
                mesh->mTextureCoords[0][vertexIndex].y
            );
        }
        else
        {
            vertex.texUV = glm::vec2(0.0f, 0.0f);
        }

        vertex.color = materialColor;
        vertices.push_back(vertex);
    }

    for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
    {
        const aiFace& face = mesh->mFaces[faceIndex];
        for (unsigned int i = 0; i < face.mNumIndices; ++i)
        {
            indices.push_back(face.mIndices[i]);
        }
    }

    if (vertices.empty() || indices.empty())
    {
        return;
    }

    std::vector<Texture> textures = buildAssimpTextures(material, materialName, fileDirectory);

    // Check if this is a wheel mesh and needs to be split
    bool isWheel = (materialName == "tire" || materialName == "tire_side" ||
                    materialName == "rim" || materialName == "rimbolt" ||
                    materialName == "rimlogo" || materialName == "brakedisk" ||
                    materialName == "F1_nip_logo");

    if (isWheel)
    {
        Mesh tempMesh(vertices, indices, textures);
        std::vector<Mesh> splitM;
        std::vector<std::string> splitMat;
        std::vector<glm::vec3> splitCent;
        std::vector<bool> splitAlpha;

        SplitWheelMesh(tempMesh, materialName, splitM, splitMat, splitCent, splitAlpha);

        for (size_t k = 0; k < splitM.size(); ++k)
        {
            meshMaterialNames.push_back(splitMat[k]);
            meshCollisionNames.push_back(splitMat[k] + " " + collisionName);
            meshCenters.push_back(splitCent[k]);
            glm::vec3 bMin, bMax;
            calculateBounds(splitM[k].vertices, bMin, bMax);
            meshBoundsMin.push_back(bMin);
            meshBoundsMax.push_back(bMax);
            meshes.push_back(splitM[k]);
            translationsMeshes.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
            rotationsMeshes.push_back(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            scalesMeshes.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
            matricesMeshes.push_back(meshMatrix);
            meshesUseAlpha.push_back(splitAlpha[k]);
        }
    }
    else
    {
        meshMaterialNames.push_back(materialName);
        meshCollisionNames.push_back(collisionName);
        meshCenters.push_back(calculateCenter(vertices));
        glm::vec3 bMin, bMax;
        calculateBounds(vertices, bMin, bMax);
        meshBoundsMin.push_back(bMin);
        meshBoundsMax.push_back(bMax);
        meshes.push_back(Mesh(vertices, indices, textures));
        translationsMeshes.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
        rotationsMeshes.push_back(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        scalesMeshes.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
        matricesMeshes.push_back(meshMatrix);
        meshesUseAlpha.push_back(materialUsesAlpha(materialName));
    }
}

std::vector<Texture> Model::buildAssimpTextures(aiMaterial* material, const std::string& materialName, const std::string& fileDirectory)
{
    std::vector<Texture> textures;
    static const unsigned char whitePixel[] = { 255, 255, 255, 255 };
    static const unsigned char blackPixel[] = { 0, 0, 0, 255 };

    // Helper lambda to find texture with fallback paths
    auto findTexture = [&](const std::vector<aiTextureType>& types, const std::string& knownPath) -> std::string {
        // First try known texture path
        if (!knownPath.empty() && fileExists(knownPath))
        {
            return knownPath;
        }

        // Try to get from assimp material
        std::string assimpPath;
        for (aiTextureType type : types)
        {
            if (getMaterialTexturePath(material, type, fileDirectory, assimpPath))
            {
                return assimpPath;
            }
        }

        // Fallback: Try finding in Scene_City.fbm/ folder
        // Extract material name and look for matching texture
        std::string fbmPath = fileDirectory + "Scene_City.fbm/" + materialName + ".png";
        if (fileExists(fbmPath))
        {
            return fbmPath;
        }

        // Try with spaces replaced by underscores
        std::string materialNameModified = materialName;
        size_t pos = 0;
        while ((pos = materialNameModified.find(' ', pos)) != std::string::npos)
        {
            materialNameModified.replace(pos, 1, "_");
            pos += 1;
        }
        fbmPath = fileDirectory + "Scene_City.fbm/" + materialNameModified + ".png";
        if (fileExists(fbmPath))
        {
            return fbmPath;
        }

        return "";
    };

    // Load diffuse texture
    bool hasDiffuse = false;
    std::string diffuseTex = getKnownDiffuseTexture(materialName);
    diffuseTex = findTexture({aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE}, diffuseTex);
    
    if (!diffuseTex.empty())
    {
        textures.push_back(loadTextureCached(diffuseTex, "diffuse", 0));
        hasDiffuse = true;
    }

    if (!hasDiffuse)
    {
        textures.push_back(loadSolidTextureCached("white-diffuse", whitePixel, "diffuse", 0));
    }

    // Load specular texture
    bool hasSpecular = false;
    std::string specularTex = getKnownSpecularTexture(materialName);
    specularTex = findTexture({aiTextureType_SPECULAR, aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS}, specularTex);
    
    if (!specularTex.empty())
    {
        textures.push_back(loadTextureCached(specularTex, "specular", 1));
        hasSpecular = true;
    }

    if (!hasSpecular)
    {
        textures.push_back(loadSolidTextureCached("black-specular", blackPixel, "specular", 1));
    }

    return textures;
}

Texture Model::loadTextureCached(const std::string& texturePath, const char* type, GLuint slot)
{
    std::string cacheKey = std::string("file:") + type + ":" + texturePath;
    for (size_t i = 0; i < loadedTexName.size(); ++i)
    {
        if (loadedTexName[i] == cacheKey)
        {
            return loadedTex[i];
        }
    }

    Texture texture(texturePath.c_str(), type, slot);
    loadedTex.push_back(texture);
    loadedTexName.push_back(cacheKey);
    return texture;
}

Texture Model::loadSolidTextureCached(const std::string& cacheKey, const unsigned char* pixel, const char* type, GLuint slot)
{
    std::string fullKey = std::string("solid:") + cacheKey;
    for (size_t i = 0; i < loadedTexName.size(); ++i)
    {
        if (loadedTexName[i] == fullKey)
        {
            return loadedTex[i];
        }
    }

    Texture texture(pixel, 1, 1, GL_RGBA, type, slot);
    loadedTex.push_back(texture);
    loadedTexName.push_back(fullKey);
    return texture;
}

void Model::loadObj(const std::string& filePath)
{
    std::ifstream in(filePath);
    if (!in)
    {
        throw std::runtime_error(std::string("Failed to open OBJ file: ") + filePath);
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;

    struct MeshData
    {
        std::vector<Vertex> vertices;
        std::vector<GLuint> indices;
    };

    std::map<std::string, MeshData> materialMeshes;
    std::string currentMaterial = "default";
    MeshData* currentMesh = &materialMeshes[currentMaterial];

    std::string line;
    while (std::getline(in, line))
    {
        if (line.size() < 2)
        {
            continue;
        }

        // Parse material change
        if (line.rfind("usemtl ", 0) == 0)
        {
            currentMaterial = line.substr(7);
            // Trim whitespace/newlines
            currentMaterial.erase(currentMaterial.find_last_not_of(" \n\r\t") + 1);
            currentMesh = &materialMeshes[currentMaterial];
            continue;
        }

        if (line.rfind("v ", 0) == 0)
        {
            std::istringstream ss(line.substr(2));
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (line.rfind("vt ", 0) == 0)
        {
            std::istringstream ss(line.substr(3));
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            texCoords.push_back(uv);
        }
        else if (line.rfind("vn ", 0) == 0)
        {
            std::istringstream ss(line.substr(3));
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (line.rfind("f ", 0) == 0)
        {
            std::istringstream ss(line.substr(2));
            std::vector<ObjIndex> face;
            std::string token;
            while (ss >> token)
            {
                face.push_back(parseObjIndex(token));
            }

            if (face.size() < 3)
            {
                continue;
            }

            for (size_t i = 1; i + 1 < face.size(); i++)
            {
                ObjIndex idx0 = face[0];
                ObjIndex idx1 = face[i];
                ObjIndex idx2 = face[i + 1];

                int p0 = resolveIndex(idx0.v, positions.size());
                int p1 = resolveIndex(idx1.v, positions.size());
                int p2 = resolveIndex(idx2.v, positions.size());
                if (p0 < 0 || p1 < 0 || p2 < 0)
                {
                    continue;
                }

                glm::vec3 pos0 = positions[p0];
                glm::vec3 pos1 = positions[p1];
                glm::vec3 pos2 = positions[p2];
                glm::vec3 faceNormal = glm::normalize(glm::cross(pos1 - pos0, pos2 - pos0));

                auto addVertex = [&](const ObjIndex& idx) -> GLuint
                {
                    Vertex vertex{};
                    int posIndex = resolveIndex(idx.v, positions.size());
                    vertex.position = positions[posIndex];

                    int normIndex = resolveIndex(idx.vn, normals.size());
                    if (normIndex >= 0 && normIndex < static_cast<int>(normals.size()))
                    {
                        vertex.normal = normals[normIndex];
                    }
                    else
                    {
                        vertex.normal = faceNormal;
                    }

                    int texIndex = resolveIndex(idx.vt, texCoords.size());
                    if (texIndex >= 0 && texIndex < static_cast<int>(texCoords.size()))
                    {
                        vertex.texUV = texCoords[texIndex];
                    }
                    else
                    {
                        vertex.texUV = glm::vec2(0.0f, 0.0f);
                    }

                    vertex.color = getMaterialColor(currentMaterial);
                    currentMesh->vertices.push_back(vertex);
                    return static_cast<GLuint>(currentMesh->vertices.size() - 1);
                };

                currentMesh->indices.push_back(addVertex(idx0));
                currentMesh->indices.push_back(addVertex(idx1));
                currentMesh->indices.push_back(addVertex(idx2));
            }
        }
    }

    // Now instantiate a Mesh for each material grouping that contains geometry
    for (auto& pair : materialMeshes)
    {
        const std::string& matName = pair.first;
        MeshData& meshData = pair.second;

        if (meshData.indices.empty() || meshData.vertices.empty())
        {
            continue;
        }

        std::string diffuseTex = "";
        std::string specularTex = "";

        if (matName == "McLaren_F1_1993_By_Alex_Ka_") diffuseTex = "res/models/mclaren/textures/McLAREN_F1.png";
        else if (matName == "tire") diffuseTex = "res/models/mclaren/textures/tire.jpeg";
        else if (matName == "tire_side") diffuseTex = "res/models/mclaren/textures/tire side.jpeg";
        else if (matName == "interior") diffuseTex = "res/models/mclaren/textures/interior.jpeg";
        else if (matName == "plate_F") diffuseTex = "res/models/mclaren/textures/plate F.jpeg";
        else if (matName == "plate_R") diffuseTex = "res/models/mclaren/textures/plate R.jpeg";
        else if (matName == "engine") diffuseTex = "res/models/mclaren/textures/engine.jpeg";
        else if (matName == "bottom") diffuseTex = "res/models/mclaren/textures/bottom.jpeg";
        else if (matName == "F1_side" || matName == "McLAREN_sidelogo" || matName == "McLAREN_sidelogo_FBUMPER") diffuseTex = "res/models/mclaren/textures/McLaren F1 side.png";
        else if (matName == "suport" || matName == "McLaren_supportlogo") diffuseTex = "res/models/mclaren/textures/McLaren support logo.png";
        else if (matName == "door_stitch") diffuseTex = "res/models/mclaren/textures/stitch.png";
        else if (matName == "headlightglass") diffuseTex = "res/models/mclaren/textures/glass.png";
        else if (matName == "windo" || matName == "windo_F" || matName == "windo_R" || matName == "windo_S") diffuseTex = "res/models/mclaren/textures/windo.png";
        else if (matName == "floor") diffuseTex = "res/models/mclaren/textures/Floor Circle.png";

        if (matName == "McLaren_F1_1993_By_Alex_Ka_") specularTex = "res/models/mclaren/textures/carshadow.png";

        std::vector<Texture> textures;
        bool hasDiffuse = false;
        if (!diffuseTex.empty())
        {
            std::ifstream f(diffuseTex);
            if (f.good())
            {
                textures.emplace_back(diffuseTex.c_str(), "diffuse", 0);
                hasDiffuse = true;
            }
        }

        if (!hasDiffuse)
        {
            unsigned char white[] = { 255, 255, 255, 255 };
            textures.emplace_back(white, 1, 1, GL_RGBA, "diffuse", 0);
        }

        bool hasSpecular = false;
        if (!specularTex.empty())
        {
            std::ifstream f(specularTex);
            if (f.good())
            {
                textures.emplace_back(specularTex.c_str(), "specular", 1);
                hasSpecular = true;
            }
        }

        if (!hasSpecular)
        {
            unsigned char black[] = { 0, 0, 0, 255 };
            textures.emplace_back(black, 1, 1, GL_RGBA, "specular", 1);
        }

        glm::mat4 initMatrix = glm::mat4(1.0f);

        meshes.push_back(Mesh(meshData.vertices, meshData.indices, textures));
        translationsMeshes.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
        rotationsMeshes.push_back(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        scalesMeshes.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
        matricesMeshes.push_back(initMatrix);

        bool useAlpha = false;
        if (matName == "windo" || matName == "windo_F" || matName == "windo_R" || matName == "windo_S" ||
            matName == "headlightglass" ||
            matName == "F1_side" || matName == "McLAREN_sidelogo" || matName == "McLAREN_sidelogo_FBUMPER" ||
            matName == "suport" || matName == "McLaren_supportlogo" ||
            matName == "door_stitch")
        {
            useAlpha = true;
        }
        meshesUseAlpha.push_back(useAlpha);
    }

    std::ofstream logFile("debug_log.txt", std::ios::app);
    logFile << "Starting OBJ parse..." << std::endl;
    logFile << "Total material groups: " << materialMeshes.size() << std::endl;
    for (auto& pair : materialMeshes)
    {
        const std::string& matName = pair.first;
        MeshData& meshData = pair.second;

        if (meshData.indices.empty() || meshData.vertices.empty())
        {
            continue;
        }

        meshMaterialNames.push_back(matName);
        meshCollisionNames.push_back(matName);
        meshCenters.push_back(calculateCenter(meshData.vertices));

        std::string diffuseTex = "";
        if (matName == "McLaren_F1_1993_By_Alex_Ka_") diffuseTex = "res/models/mclaren/textures/McLAREN_F1.png";
        else if (matName == "tire") diffuseTex = "res/models/mclaren/textures/tire.jpeg";
        else if (matName == "tire_side") diffuseTex = "res/models/mclaren/textures/tire side.jpeg";
        else if (matName == "interior") diffuseTex = "res/models/mclaren/textures/interior.jpeg";
        else if (matName == "plate_F") diffuseTex = "res/models/mclaren/textures/plate F.jpeg";
        else if (matName == "plate_R") diffuseTex = "res/models/mclaren/textures/plate R.jpeg";
        else if (matName == "engine") diffuseTex = "res/models/mclaren/textures/engine.jpeg";
        else if (matName == "bottom") diffuseTex = "res/models/mclaren/textures/bottom.jpeg";
        else if (matName == "F1_side" || matName == "McLAREN_sidelogo" || matName == "McLAREN_sidelogo_FBUMPER") diffuseTex = "res/models/mclaren/textures/McLaren F1 side.png";
        else if (matName == "suport" || matName == "McLaren_supportlogo") diffuseTex = "res/models/mclaren/textures/McLaren support logo.png";
        else if (matName == "door_stitch") diffuseTex = "res/models/mclaren/textures/stitch.png";
        else if (matName == "headlightglass") diffuseTex = "res/models/mclaren/textures/glass.png";
        else if (matName == "windo" || matName == "windo_F" || matName == "windo_R" || matName == "windo_S") diffuseTex = "res/models/mclaren/textures/windo.png";
        else if (matName == "floor") diffuseTex = "res/models/mclaren/textures/Floor Circle.png";

        bool hasDiffuse = false;
        if (!diffuseTex.empty())
        {
            std::ifstream f(diffuseTex);
            if (f.good())
            {
                hasDiffuse = true;
            }
        }

        logFile << "Material: '" << matName << "', Vertices: " << meshData.vertices.size() 
                << ", TexturePath: '" << diffuseTex << "', Exists: " << (hasDiffuse ? "YES" : "NO") << std::endl;
    }
    logFile.close();
}

void Model::traverseNode(unsigned int nextNode, glm::mat4 matrix)
{
    // Current node
    json node = JSON["nodes"][nextNode];

    // Get translation if it exists
    glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
    if (node.find("translation") != node.end())
    {
        float transValues[3];
        for (unsigned int i = 0; i < node["translation"].size(); i++)
            transValues[i] = (node["translation"][i]);
        translation = glm::make_vec3(transValues);
    }
    // Get quaternion if it exists
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.find("rotation") != node.end())
    {
        float rotValues[4] =
        {
            node["rotation"][3],
            node["rotation"][0],
            node["rotation"][1],
            node["rotation"][2]
        };
        rotation = glm::make_quat(rotValues);
    }
    // Get scale if it exists
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
    if (node.find("scale") != node.end())
    {
        float scaleValues[3];
        for (unsigned int i = 0; i < node["scale"].size(); i++)
            scaleValues[i] = (node["scale"][i]);
        scale = glm::make_vec3(scaleValues);
    }
    // Get matrix if it exists
    glm::mat4 matNode = glm::mat4(1.0f);
    if (node.find("matrix") != node.end())
    {
        float matValues[16];
        for (unsigned int i = 0; i < node["matrix"].size(); i++)
            matValues[i] = (node["matrix"][i]);
        matNode = glm::make_mat4(matValues);
    }

    // Initialize matrices
    glm::mat4 trans = glm::mat4(1.0f);
    glm::mat4 rot = glm::mat4(1.0f);
    glm::mat4 sca = glm::mat4(1.0f);

    // Use translation, rotation, and scale to change the initialized matrices
    trans = glm::translate(trans, translation);
    rot = glm::mat4_cast(rotation);
    sca = glm::scale(sca, scale);

    // Multiply all matrices together
    glm::mat4 matNextNode = matrix * matNode * trans * rot * sca;

    // Check if the node contains a mesh and if it does load it
    if (node.find("mesh") != node.end())
    {
        translationsMeshes.push_back(translation);
        rotationsMeshes.push_back(rotation);
        scalesMeshes.push_back(scale);
        matricesMeshes.push_back(matNextNode);

        loadMesh(node["mesh"]);
    }

    // Check if the node has children, and if it does, apply this function to them with the matNextNode
    if (node.find("children") != node.end())
    {
        for (unsigned int i = 0; i < node["children"].size(); i++)
            traverseNode(node["children"][i], matNextNode);
    }
}

std::vector<unsigned char> Model::getData()
{
    // Create a place to store the raw text, and get the uri of the .bin file
    std::string bytesText;
    std::string uri = JSON["buffers"][0]["uri"];

    // Store raw text data into bytesText
    std::string fileStr = std::string(file);
    std::string fileDirectory = getDirectory(fileStr);
    bytesText = get_file_contents((fileDirectory + uri).c_str());

    // Transform the raw text data into bytes and put them in a vector
    std::vector<unsigned char> data(bytesText.begin(), bytesText.end());
    return data;
}

std::vector<float> Model::getFloats(json accessor)
{
    std::vector<float> floatVec;

    // Get properties from the accessor
    unsigned int buffViewInd = accessor.value("bufferView", 1);
    unsigned int count = accessor["count"];
    unsigned int accByteOffset = accessor.value("byteOffset", 0);
    std::string type = accessor["type"];

    // Get properties from the bufferView
    json bufferView = JSON["bufferViews"][buffViewInd];
    unsigned int byteOffset = bufferView["byteOffset"];

    // Interpret the type and store it into numPerVert
    unsigned int numPerVert;
    if (type == "SCALAR") numPerVert = 1;
    else if (type == "VEC2") numPerVert = 2;
    else if (type == "VEC3") numPerVert = 3;
    else if (type == "VEC4") numPerVert = 4;
    else throw std::invalid_argument("Type is invalid (not SCALAR, VEC2, VEC3, or VEC4)");

    // Go over all the bytes in the data at the correct place using the properties from above
    unsigned int beginningOfData = byteOffset + accByteOffset;
    unsigned int lengthOfData = count * 4 * numPerVert;
    for (unsigned int i = beginningOfData; i < beginningOfData + lengthOfData;)
    {
        unsigned char bytes[] = { data[i++], data[i++], data[i++], data[i++] };
        float value;
        std::memcpy(&value, bytes, sizeof(float));
        floatVec.push_back(value);
    }

    return floatVec;
}

std::vector<GLuint> Model::getIndices(json accessor)
{
    std::vector<GLuint> indices;

    // Get properties from the accessor
    unsigned int buffViewInd = accessor.value("bufferView", 0);
    unsigned int count = accessor["count"];
    unsigned int accByteOffset = accessor.value("byteOffset", 0);
    unsigned int componentType = accessor["componentType"];

    // Get properties from the bufferView
    json bufferView = JSON["bufferViews"][buffViewInd];
    unsigned int byteOffset = bufferView["byteOffset"];

    // Get indices with regards to their type: unsigned int, unsigned short, or short
    unsigned int beginningOfData = byteOffset + accByteOffset;
    if (componentType == 5125)
    {
        for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 4;)
        {
            unsigned char bytes[] = { data[i++], data[i++], data[i++], data[i++] };
            unsigned int value;
            std::memcpy(&value, bytes, sizeof(unsigned int));
            indices.push_back((GLuint)value);
        }
    }
    else if (componentType == 5123)
    {
        for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2;)
        {
            unsigned char bytes[] = { data[i++], data[i++] };
            unsigned short value;
            std::memcpy(&value, bytes, sizeof(unsigned short));
            indices.push_back((GLuint)value);
        }
    }
    else if (componentType == 5122)
    {
        for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2;)
        {
            unsigned char bytes[] = { data[i++], data[i++] };
            short value;
            std::memcpy(&value, bytes, sizeof(short));
            indices.push_back((GLuint)value);
        }
    }

    return indices;
}

std::vector<Texture> Model::getTextures()
{
    std::vector<Texture> textures;

    std::string fileStr = std::string(file);
    std::string fileDirectory = getDirectory(fileStr);

    // Go over all images
    for (unsigned int i = 0; i < JSON["images"].size(); i++)
    {
        // uri of current texture
        std::string texPath = JSON["images"][i]["uri"];

        // Check if the texture has already been loaded
        bool skip = false;
        for (unsigned int j = 0; j < loadedTexName.size(); j++)
        {
            if (loadedTexName[j] == texPath)
            {
                textures.push_back(loadedTex[j]);
                skip = true;
                break;
            }
        }

        // If the texture has been loaded, skip this
        if (!skip)
        {
            // Load diffuse texture
            if (texPath.find("baseColor") != std::string::npos)
            {
                Texture diffuse = Texture((fileDirectory + texPath).c_str(), "diffuse", loadedTex.size());
                textures.push_back(diffuse);
                loadedTex.push_back(diffuse);
                loadedTexName.push_back(texPath);
            }
            // Load specular texture
            else if (texPath.find("metallicRoughness") != std::string::npos)
            {
                Texture specular = Texture((fileDirectory + texPath).c_str(), "specular", loadedTex.size());
                textures.push_back(specular);
                loadedTex.push_back(specular);
                loadedTexName.push_back(texPath);
            }
        }
    }

    return textures;
}

std::vector<Vertex> Model::assembleVertices
(
    std::vector<glm::vec3> positions,
    std::vector<glm::vec3> normals,
    std::vector<glm::vec2> texUVs
)
{
    std::vector<Vertex> vertices;
    for (size_t i = 0; i < positions.size(); i++)
    {
        vertices.push_back
        (
            Vertex
            {
                positions[i],
                normals[i],
                glm::vec3(1.0f, 1.0f, 1.0f),
                texUVs[i]
            }
        );
    }
    return vertices;
}

std::vector<glm::vec2> Model::groupFloatsVec2(std::vector<float> floatVec)
{
    std::vector<glm::vec2> vectors;
    for (size_t i = 0; i + 1 < floatVec.size(); i += 2)
    {
        vectors.push_back(glm::vec2(floatVec[i], floatVec[i + 1]));
    }
    return vectors;
}

std::vector<glm::vec3> Model::groupFloatsVec3(std::vector<float> floatVec)
{
    std::vector<glm::vec3> vectors;
    for (size_t i = 0; i + 2 < floatVec.size(); i += 3)
    {
        vectors.push_back(glm::vec3(floatVec[i], floatVec[i + 1], floatVec[i + 2]));
    }
    return vectors;
}

std::vector<glm::vec4> Model::groupFloatsVec4(std::vector<float> floatVec)
{
    std::vector<glm::vec4> vectors;
    for (size_t i = 0; i + 3 < floatVec.size(); i += 4)
    {
        vectors.push_back(glm::vec4(floatVec[i], floatVec[i + 1], floatVec[i + 2], floatVec[i + 3]));
    }
    return vectors;
}

void Model::preclassifyMeshes()
{
    meshesHidden.assign(meshes.size(), false);
    meshEmissiveType.assign(meshes.size(), 0);
    meshWheelType.assign(meshes.size(), 0);
    meshReflectivity.assign(meshes.size(), 0.0f);

    for (size_t i = 0; i < meshes.size(); ++i)
    {
        std::string mat = (i < meshMaterialNames.size()) ? meshMaterialNames[i] : "";
        std::string matLower = toLower(mat);

        // Classify hidden
        if (matLower.find("floor") != std::string::npos ||
            matLower.find("bottom") != std::string::npos ||
            matLower.find("suport") != std::string::npos ||
            matLower.find("support") != std::string::npos ||
            matLower == "_")
        {
            meshesHidden[i] = true;
        }

        // Classify emissive type
        if (mat == "headlight_1" || mat == "headlight_2" || mat == "headlightglass")
        {
            meshEmissiveType[i] = 1; // headlight
        }
        else if (mat == "front_turn_signal" || mat == "side_turn_signal" || mat == "rear_turn_signal")
        {
            meshEmissiveType[i] = 2; // turn signal
        }
        else if (mat == "brakelight")
        {
            meshEmissiveType[i] = 3; // brakelight
        }
        else if (mat == "rear_lamp")
        {
            meshEmissiveType[i] = 4; // rear lamp
        }

        // Classify wheel type
        if (mat.rfind("FL_", 0) == 0)
        {
            meshWheelType[i] = 1;
        }
        else if (mat.rfind("FR_", 0) == 0)
        {
            meshWheelType[i] = 2;
        }
        else if (mat.rfind("RL_", 0) == 0)
        {
            meshWheelType[i] = 3;
        }
        else if (mat.rfind("RR_", 0) == 0)
        {
            meshWheelType[i] = 4;
        }

        // Classify reflectivity
        float reflectivity = 0.0f;
        if (mat == "McLaren_F1_1993_By_Alex_Ka_")
        {
            reflectivity = 0.35f;
        }
        else if (mat == "rim" || mat == "chrome" || mat == "chrome2")
        {
            reflectivity = 0.5f;
        }
        else if (mat == "windo" || mat == "windo_F" || mat == "windo_R" || mat == "windo_S" || mat == "headlightglass")
        {
            reflectivity = 0.6f;
        }
        else if (matLower.find("window") != std::string::npos || matLower.find("glass") != std::string::npos)
        {
            reflectivity = 0.4f;
        }
        meshReflectivity[i] = reflectivity;
    }
}

