#include "Model.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{
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
} // namespace

Model::Model(const char* file)
{
    std::ofstream log("debug_log.txt", std::ios::app);
    log << "Model constructor called with file: '" << file << "'" << std::endl;
    std::string fileStr = std::string(file);
    log << "  hasExtension(.obj): " << (hasExtension(fileStr, ".obj") ? "YES" : "NO") << std::endl;
    log.close();

    Model::file = file;
    if (hasExtension(fileStr, ".obj"))
    {
        loadObj(fileStr);
        return;
    }

    // Make a JSON object
    std::string text = get_file_contents(file);
    JSON = json::parse(text);

    // Get the binary data
    data = getData();

    // Traverse all nodes
    traverseNode(0);
}

void Model::Draw(Shader& shader, Camera& camera)
{
    // Go over all meshes and draw each one
    for (unsigned int i = 0; i < meshes.size(); i++)
    {
        glUniform1i(glGetUniformLocation(shader.ID, "uUseAlpha"), meshesUseAlpha[i] ? 1 : 0);
        meshes[i].Mesh::Draw(shader, camera, matricesMeshes[i]);
    }
}

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

    // Combine the vertices, indices, and textures into a mesh
    meshes.push_back(Mesh(vertices, indices, textures));
    meshesUseAlpha.push_back(false);
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

        if (matName == "McLaren_F1_1993_By_Alex_Ka_") diffuseTex = "res/Modelos3d/textures/McLAREN_F1.png";
        else if (matName == "tire") diffuseTex = "res/Modelos3d/textures/tire.jpeg";
        else if (matName == "tire_side") diffuseTex = "res/Modelos3d/textures/tire side.jpeg";
        else if (matName == "interior") diffuseTex = "res/Modelos3d/textures/interior.jpeg";
        else if (matName == "plate_F") diffuseTex = "res/Modelos3d/textures/plate F.jpeg";
        else if (matName == "plate_R") diffuseTex = "res/Modelos3d/textures/plate R.jpeg";
        else if (matName == "engine") diffuseTex = "res/Modelos3d/textures/engine.jpeg";
        else if (matName == "bottom") diffuseTex = "res/Modelos3d/textures/bottom.jpeg";
        else if (matName == "F1_side" || matName == "McLAREN_sidelogo" || matName == "McLAREN_sidelogo_FBUMPER") diffuseTex = "res/Modelos3d/textures/McLaren F1 side.png";
        else if (matName == "suport" || matName == "McLaren_supportlogo") diffuseTex = "res/Modelos3d/textures/McLaren support logo.png";
        else if (matName == "door_stitch") diffuseTex = "res/Modelos3d/textures/stitch.png";
        else if (matName == "headlightglass") diffuseTex = "res/Modelos3d/textures/glass.png";
        else if (matName == "windo" || matName == "windo_F" || matName == "windo_R" || matName == "windo_S") diffuseTex = "res/Modelos3d/textures/windo.png";
        else if (matName == "floor") diffuseTex = "res/Modelos3d/textures/Floor Circle.png";

        if (matName == "McLaren_F1_1993_By_Alex_Ka_") specularTex = "res/Modelos3d/textures/carshadow.png";

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

        std::string diffuseTex = "";
        if (matName == "McLaren_F1_1993_By_Alex_Ka_") diffuseTex = "res/Modelos3d/textures/McLAREN_F1.png";
        else if (matName == "tire") diffuseTex = "res/Modelos3d/textures/tire.jpeg";
        else if (matName == "tire_side") diffuseTex = "res/Modelos3d/textures/tire side.jpeg";
        else if (matName == "interior") diffuseTex = "res/Modelos3d/textures/interior.jpeg";
        else if (matName == "plate_F") diffuseTex = "res/Modelos3d/textures/plate F.jpeg";
        else if (matName == "plate_R") diffuseTex = "res/Modelos3d/textures/plate R.jpeg";
        else if (matName == "engine") diffuseTex = "res/Modelos3d/textures/engine.jpeg";
        else if (matName == "bottom") diffuseTex = "res/Modelos3d/textures/bottom.jpeg";
        else if (matName == "F1_side" || matName == "McLAREN_sidelogo" || matName == "McLAREN_sidelogo_FBUMPER") diffuseTex = "res/Modelos3d/textures/McLaren F1 side.png";
        else if (matName == "suport" || matName == "McLaren_supportlogo") diffuseTex = "res/Modelos3d/textures/McLaren support logo.png";
        else if (matName == "door_stitch") diffuseTex = "res/Modelos3d/textures/stitch.png";
        else if (matName == "headlightglass") diffuseTex = "res/Modelos3d/textures/glass.png";
        else if (matName == "windo" || matName == "windo_F" || matName == "windo_R" || matName == "windo_S") diffuseTex = "res/Modelos3d/textures/windo.png";
        else if (matName == "floor") diffuseTex = "res/Modelos3d/textures/Floor Circle.png";

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
    for (unsigned int i = beginningOfData; i < beginningOfData + lengthOfData; i)
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
        for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 4; i)
        {
            unsigned char bytes[] = { data[i++], data[i++], data[i++], data[i++] };
            unsigned int value;
            std::memcpy(&value, bytes, sizeof(unsigned int));
            indices.push_back((GLuint)value);
        }
    }
    else if (componentType == 5123)
    {
        for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
        {
            unsigned char bytes[] = { data[i++], data[i++] };
            unsigned short value;
            std::memcpy(&value, bytes, sizeof(unsigned short));
            indices.push_back((GLuint)value);
        }
    }
    else if (componentType == 5122)
    {
        for (unsigned int i = beginningOfData; i < byteOffset + accByteOffset + count * 2; i)
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
