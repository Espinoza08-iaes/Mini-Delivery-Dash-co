#include "PropsManager.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

PropsManager::PropsManager() {}

PropsManager::~PropsManager() {}

void PropsManager::Initialize(const std::vector<glm::vec3>& lampPositions, const City& city)
{
    mProps.clear();

    // 1. Create procedural meshes
    CreateConeMesh();
    CreateBoxMesh();

    // 2. Distribute props near street lamps
    std::srand(12345); // deterministic seed for uniform spawning
    for (size_t i = 0; i < lampPositions.size(); i += 10) // Spawn at every 10th lamp post
    {
        glm::vec3 lampPos = lampPositions[i];
        
        // Try to spawn two props near each selected lamp post
        for (int pIdx = 0; pIdx < 2; ++pIdx)
        {
            // Random offset around lamp post
            float offsetX = ((rand() % 100) - 50) * 0.06f; // -3.0m to +3.0m
            float offsetZ = ((rand() % 100) - 50) * 0.06f;
            
            glm::vec3 testPos = lampPos + glm::vec3(offsetX, 1.5f, offsetZ);
            
            game::GroundSample sample;
            if (city.GetGroundSample(testPos, testPos.y, sample, 6.0f, 1.5f) && sample.found)
            {
                DestructibleProp prop;
                prop.position = glm::vec3(testPos.x, sample.height + 0.01f, testPos.z);
                prop.basePosition = prop.position;
                
                // Random Y rotation
                float angle = (rand() % 360) * 3.141592f / 180.0f;
                prop.rotation = glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));
                prop.baseRotation = prop.rotation;
                
                prop.type = (rand() % 2 == 0) ? 0 : 1; // 50% Cone, 50% Box
                prop.scale = 1.0f + ((rand() % 100) - 50) * 0.002f; // Slight scale variation
                prop.hit = false;
                
                mProps.push_back(prop);
            }
        }
    }
}

void PropsManager::Update(float dt, const glm::vec3& carPos, const glm::vec3& carVelocity, float carRadius, const City& city)
{
    float carSpeed = glm::length(carVelocity);
    
    for (auto& prop : mProps)
    {
        if (!prop.hit)
        {
            // 1. Check collision with car (2D distance check on XZ plane is enough and very fast)
            glm::vec3 diff = prop.position - carPos;
            float distXZ = std::sqrt(diff.x * diff.x + diff.z * diff.z);
            float propRadius = 0.35f * prop.scale;
            
            // Height overlap check
            float heightOverlap = std::abs(prop.position.y - carPos.y);
            
            if (distXZ < (carRadius + propRadius) && heightOverlap < 1.6f)
            {
                prop.hit = true;
                prop.hitTime = 0.0f;
                
                // Calculate impact direction and strength
                glm::vec3 hitDir = glm::normalize(diff);
                if (carSpeed < 0.2f)
                {
                    // If car is barely moving, push it away gently
                    prop.velocity = hitDir * 1.5f + glm::vec3(0.0f, 1.0f, 0.0f);
                }
                else
                {
                    // Fly away proportional to car velocity + upward launch
                    prop.velocity = carVelocity * 1.1f + hitDir * (carSpeed * 0.3f) + glm::vec3(0.0f, 4.0f + carSpeed * 0.2f, 0.0f);
                }
                
                // Give it some wild spin
                prop.angularVelocity = glm::vec3(
                    ((rand() % 100) - 50) * 0.2f * carSpeed,
                    ((rand() % 100) - 50) * 0.1f * carSpeed,
                    ((rand() % 100) - 50) * 0.2f * carSpeed
                );
            }
        }
        else
        {
            // 2. Prop is flying! Apply simple physics
            prop.hitTime += dt;
            
            // Gravity
            prop.velocity.y -= 9.81f * dt;
            
            // Drag/friction in air
            prop.velocity.x *= (1.0f - 0.2f * dt);
            prop.velocity.z *= (1.0f - 0.2f * dt);
            
            prop.position += prop.velocity * dt;
            
            // Update rotation
            glm::quat spin = glm::quat(0.0f, prop.angularVelocity.x * dt, prop.angularVelocity.y * dt, prop.angularVelocity.z * dt);
            prop.rotation += spin * prop.rotation * 0.5f;
            prop.rotation = glm::normalize(prop.rotation);
            
            // Ground check
            game::GroundSample sample;
            if (city.GetGroundSample(prop.position, prop.position.y + 0.8f, sample, 2.5f, 0.5f) && sample.found)
            {
                float groundY = sample.height;
                if (prop.position.y <= groundY)
                {
                    prop.position.y = groundY;
                    
                    // Bounce
                    if (prop.velocity.y < -1.0f)
                    {
                        prop.velocity.y = -prop.velocity.y * 0.35f; // Restitution
                        
                        // Ground friction on bounce
                        prop.velocity.x *= 0.6f;
                        prop.velocity.z *= 0.6f;
                        prop.angularVelocity *= 0.5f;
                    }
                    else
                    {
                        prop.velocity = glm::vec3(0.0f);
                        prop.angularVelocity = glm::vec3(0.0f);
                    }
                }
            }
            else if (prop.position.y < -15.0f) // Fell out of world boundaries (water level)
            {
                // Reset immediately
                prop.position = prop.basePosition;
                prop.rotation = prop.baseRotation;
                prop.velocity = glm::vec3(0.0f);
                prop.angularVelocity = glm::vec3(0.0f);
                prop.hit = false;
            }
            
            // 3. Reset after being hit for 12 seconds if player is far away
            if (prop.hitTime > 12.0f)
            {
                float playerDist = glm::distance(prop.position, carPos);
                if (playerDist > 35.0f)
                {
                    prop.position = prop.basePosition;
                    prop.rotation = prop.baseRotation;
                    prop.velocity = glm::vec3(0.0f);
                    prop.angularVelocity = glm::vec3(0.0f);
                    prop.hit = false;
                }
            }
        }
    }
}

void PropsManager::Draw(Shader& shader, Camera& camera)
{
    shader.Activate();
    glUniform1i(glGetUniformLocation(shader.ID, "uUseAlpha"), 0);
    glUniform1i(glGetUniformLocation(shader.ID, "uIsEmissive"), 0);
    glUniform1i(glGetUniformLocation(shader.ID, "uIsFacade"), 0);

    for (const auto& prop : mProps)
    {
        // Simple distance culling for props rendering (only render props within 150m of camera)
        if (glm::distance(prop.position, camera.Position) > 150.0f)
            continue;
            
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, prop.position);
        modelMatrix = modelMatrix * glm::mat4_cast(prop.rotation);
        modelMatrix = glm::scale(modelMatrix, glm::vec3(prop.scale));
        
        if (prop.type == 0 && mConeMesh)
        {
            mConeMesh->Draw(shader, camera, modelMatrix);
        }
        else if (prop.type == 1 && mBoxMesh)
        {
            mBoxMesh->Draw(shader, camera, modelMatrix);
        }
    }
}

void PropsManager::ResetAll()
{
    for (auto& prop : mProps)
    {
        prop.position = prop.basePosition;
        prop.rotation = prop.baseRotation;
        prop.velocity = glm::vec3(0.0f);
        prop.angularVelocity = glm::vec3(0.0f);
        prop.hit = false;
    }
}

void PropsManager::CreateConeMesh()
{
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    
    // Colors
    glm::vec3 orangeColor(1.0f, 0.35f, 0.0f);
    glm::vec3 whiteColor(0.92f, 0.92f, 0.92f);
    
    // --- 1. Base (flat square box) ---
    // Width: 0.45, thickness: 0.04
    float w = 0.225f;
    float h = 0.02f;
    
    // 8 vertices for the base box
    glm::vec3 basePoints[8] = {
        glm::vec3(-w, 0.0f, -w), glm::vec3(w, 0.0f, -w), glm::vec3(w, 0.0f, w), glm::vec3(-w, 0.0f, w),
        glm::vec3(-w, h, -w),    glm::vec3(w, h, -w),    glm::vec3(w, h, w),    glm::vec3(-w, h, w)
    };
    
    // Add base vertices and faces
    for (int i = 0; i < 8; ++i)
    {
        Vertex v;
        v.position = basePoints[i];
        v.normal = glm::normalize(basePoints[i] - glm::vec3(0.0f, h*0.5f, 0.0f));
        v.color = orangeColor;
        v.texUV = glm::vec2(0.0f);
        vertices.push_back(v);
    }
    
    // Base Indices (6 faces)
    GLuint baseIdx[] = {
        0, 1, 2, 2, 3, 0, // Bottom
        4, 5, 6, 6, 7, 4, // Top
        0, 1, 5, 5, 4, 0, // Front
        1, 2, 6, 6, 5, 1, // Right
        2, 3, 7, 7, 6, 2, // Back
        3, 0, 4, 4, 7, 3  // Left
    };
    for (GLuint idx : baseIdx) indices.push_back(idx);

    // --- 2. Cone Body ---
    // Generate stacked rings of vertices to easily apply the striped colors
    int sectors = 12;
    int stacks = 4; // 0 = bottom, 1 = mid1, 2 = mid2, 3 = top
    float stackHeights[] = { h, 0.16f, 0.32f, 0.48f, 0.62f };
    float stackRadii[] = { 0.15f, 0.11f, 0.075f, 0.04f, 0.01f };
    
    GLuint startVert = (GLuint)vertices.size();
    
    for (int s = 0; s <= stacks; ++s)
    {
        float y = stackHeights[s];
        float r = stackRadii[s];
        
        // Striped color pattern
        glm::vec3 color = orangeColor;
        if (s == 2 || s == 3)
        {
            color = whiteColor;
        }
        
        for (int i = 0; i < sectors; ++i)
        {
            float angle = (float)i / sectors * 2.0f * 3.141592f;
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);
            
            Vertex v;
            v.position = glm::vec3(cosA * r, y, sinA * r);
            v.normal = glm::normalize(glm::vec3(cosA, 0.15f, sinA));
            v.color = color;
            v.texUV = glm::vec2((float)i / sectors, (float)s / stacks);
            vertices.push_back(v);
        }
    }
    
    // Generate indices for the cone stacks
    for (int s = 0; s < stacks; ++s)
    {
        GLuint ring0 = startVert + s * sectors;
        GLuint ring1 = startVert + (s + 1) * sectors;
        
        for (int i = 0; i < sectors; ++i)
        {
            int next = (i + 1) % sectors;
            
            indices.push_back(ring0 + i);
            indices.push_back(ring0 + next);
            indices.push_back(ring1 + next);
            
            indices.push_back(ring1 + next);
            indices.push_back(ring1 + i);
            indices.push_back(ring0 + i);
        }
    }

    // Static texture maps fallback
    std::vector<Texture> textures;
    static const unsigned char whitePixel[] = {255, 255, 255, 255};
    static Texture whiteTexture(whitePixel, 1, 1, GL_RGBA, "diffuse", 0);
    static const unsigned char blackPixel[] = {0, 0, 0, 255};
    static Texture blackTexture(blackPixel, 1, 1, GL_RGBA, "specular", 1);
    textures.push_back(whiteTexture);
    textures.push_back(blackTexture);

    mConeMesh = std::unique_ptr<Mesh>(new Mesh(vertices, indices, textures));
}

void PropsManager::CreateBoxMesh()
{
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    // Cardboard brown colors
    glm::vec3 boxColor(0.55f, 0.40f, 0.26f);
    glm::vec3 flapColor(0.48f, 0.34f, 0.20f); // Darker flaps
    glm::vec3 tapeColor(0.35f, 0.25f, 0.15f); // Tape lines

    float w = 0.20f; // size = 0.40m

    // Helper to add a flat quad face with custom normals and colors
    auto addFace = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, 
                       const glm::vec3& normal, const glm::vec3& faceColor)
    {
        GLuint start = (GLuint)vertices.size();
        
        Vertex v0{p0, normal, faceColor, glm::vec2(0.0f, 0.0f)};
        Vertex v1{p1, normal, faceColor, glm::vec2(1.0f, 0.0f)};
        Vertex v2{p2, normal, faceColor, glm::vec2(1.0f, 1.0f)};
        Vertex v3{p3, normal, faceColor, glm::vec2(0.0f, 1.0f)};
        
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        vertices.push_back(v3);
        
        indices.push_back(start + 0);
        indices.push_back(start + 1);
        indices.push_back(start + 2);
        indices.push_back(start + 2);
        indices.push_back(start + 3);
        indices.push_back(start + 0);
    };

    // 6 Cube faces
    // Bottom Face (boxColor)
    addFace(glm::vec3(-w, 0.0f, -w), glm::vec3(w, 0.0f, -w), glm::vec3(w, 0.0f, w), glm::vec3(-w, 0.0f, w),
            glm::vec3(0.0f, -1.0f, 0.0f), boxColor);

    // Front Face
    addFace(glm::vec3(-w, 0.0f, w), glm::vec3(w, 0.0f, w), glm::vec3(w, 2.0f*w, w), glm::vec3(-w, 0.0f, w),
            glm::vec3(0.0f, 0.0f, 1.0f), boxColor);
            
    // Right Face
    addFace(glm::vec3(w, 0.0f, w), glm::vec3(w, 0.0f, -w), glm::vec3(w, 2.0f*w, -w), glm::vec3(w, 2.0f*w, w),
            glm::vec3(1.0f, 0.0f, 0.0f), boxColor);

    // Back Face
    addFace(glm::vec3(w, 0.0f, -w), glm::vec3(-w, 0.0f, -w), glm::vec3(-w, 2.0f*w, -w), glm::vec3(w, 2.0f*w, -w),
            glm::vec3(0.0f, 0.0f, -1.0f), boxColor);

    // Left Face
    addFace(glm::vec3(-w, 0.0f, -w), glm::vec3(-w, 0.0f, w), glm::vec3(-w, 2.0f*w, w), glm::vec3(-w, 2.0f*w, -w),
            glm::vec3(-1.0f, 0.0f, 0.0f), boxColor);

    // Top Face (drawn with a tape stripe in the middle)
    // Left flap
    addFace(glm::vec3(-w, 2.0f*w, -w), glm::vec3(-0.03f, 2.0f*w, -w), glm::vec3(-0.03f, 2.0f*w, w), glm::vec3(-w, 2.0f*w, w),
            glm::vec3(0.0f, 1.0f, 0.0f), flapColor);
    // Right flap
    addFace(glm::vec3(0.03f, 2.0f*w, -w), glm::vec3(w, 2.0f*w, -w), glm::vec3(w, 2.0f*w, w), glm::vec3(0.03f, 2.0f*w, w),
            glm::vec3(0.0f, 1.0f, 0.0f), flapColor);
    // Center Tape
    addFace(glm::vec3(-0.03f, 2.0f*w + 0.001f, -w), glm::vec3(0.03f, 2.0f*w + 0.001f, -w), glm::vec3(0.03f, 2.0f*w + 0.001f, w), glm::vec3(-0.03f, 2.0f*w + 0.001f, w),
            glm::vec3(0.0f, 1.0f, 0.0f), tapeColor);

    // Static textures
    std::vector<Texture> textures;
    static const unsigned char whitePixel[] = {255, 255, 255, 255};
    static Texture whiteTexture(whitePixel, 1, 1, GL_RGBA, "diffuse", 0);
    static const unsigned char blackPixel[] = {0, 0, 0, 255};
    static Texture blackTexture(blackPixel, 1, 1, GL_RGBA, "specular", 1);
    textures.push_back(whiteTexture);
    textures.push_back(blackTexture);

    mBoxMesh = std::unique_ptr<Mesh>(new Mesh(vertices, indices, textures));
}
