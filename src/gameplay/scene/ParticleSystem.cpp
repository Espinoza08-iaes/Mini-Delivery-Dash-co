#include "ParticleSystem.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

ParticleSystem::ParticleSystem(int maxParticles)
    : mMaxParticles(maxParticles)
{
    mParticles.reserve(mMaxParticles);
    mVertices.reserve(mMaxParticles * 4);
    mIndices.reserve(mMaxParticles * 6);
    
    SetupBuffers();
}

ParticleSystem::~ParticleSystem()
{
    glDeleteVertexArrays(1, &mVAO);
    glDeleteBuffers(1, &mVBO);
    glDeleteBuffers(1, &mEBO);
}

void ParticleSystem::SetupBuffers()
{
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);

    // Dynamic vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, mMaxParticles * 4 * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);

    // Pre-populate index buffer
    std::vector<GLuint> indices;
    indices.reserve(mMaxParticles * 6);
    for (int i = 0; i < mMaxParticles; ++i)
    {
        indices.push_back(i * 4 + 0);
        indices.push_back(i * 4 + 1);
        indices.push_back(i * 4 + 2);
        indices.push_back(i * 4 + 2);
        indices.push_back(i * 4 + 3);
        indices.push_back(i * 4 + 0);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // Attributes: Position (vec3), TexUV (vec2), Color (vec4)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, texUV));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, color));

    glBindVertexArray(0);
}

void ParticleSystem::AddParticle(const Particle& p)
{
    if (mParticles.size() < (size_t)mMaxParticles)
    {
        mParticles.push_back(p);
    }
}

void ParticleSystem::Update(float dt, const glm::vec3& playerPos, const glm::vec3& playerVelocity, 
                            bool isBraking, bool isDrifting, const glm::vec3& exhaustL, const glm::vec3& exhaustR,
                            const std::vector<glm::vec3>& tirePositions, float groundHeight)
{
    // 1. Update existing particles
    for (size_t i = 0; i < mParticles.size(); )
    {
        Particle& p = mParticles[i];
        p.life -= dt;

        if (p.life <= 0.0f)
        {
            // Swap with back and pop
            p = mParticles.back();
            mParticles.pop_back();
            continue;
        }

        // Apply leaf drift
        if (p.type == 2)
        {
            // Sine wave horizontal motion for leaves
            float t = p.maxLife - p.life;
            p.velocity.x += std::sin(t * 4.0f) * 0.05f;
            p.velocity.z += std::cos(t * 3.0f) * 0.05f;
        }

        p.position += p.velocity * dt;
        p.rotation += p.angularVelocity * dt;

        // Size & Alpha changes over time
        if (p.type == 0) // Smoke
        {
            p.size += 0.8f * dt; // Smoke grows
            p.color.a = (p.life / p.maxLife) * 0.35f; // Fade out
        }
        else if (p.type == 2) // Leaf
        {
            // Slight fade at the very end of life
            if (p.life < 1.0f)
            {
                p.color.a = p.life * 0.85f;
            }
        }

        // Rain ground collision -> spawn splash
        if (p.type == 1 && p.position.y <= groundHeight)
        {
            p.life = 0.0f; // Kill rain particle
            
            // Spawn splash particles (1-2)
            int splashCount = 1 + rand() % 2;
            for (int s = 0; s < splashCount; ++s)
            {
                Particle splash;
                splash.type = 0; // Smoke type used as splash
                splash.position = glm::vec3(p.position.x, groundHeight + 0.02f, p.position.z);
                splash.velocity = glm::vec3(
                    ((rand() % 100) - 50) * 0.04f,
                    ((rand() % 100) * 0.03f) + 0.5f,
                    ((rand() % 100) - 50) * 0.04f
                );
                splash.color = glm::vec4(0.8f, 0.88f, 0.95f, 0.30f);
                splash.size = 0.04f + ((rand() % 100) * 0.0006f);
                splash.life = 0.1f + ((rand() % 100) * 0.001f);
                splash.maxLife = splash.life;
                AddParticle(splash);
            }
            
            // Swapping logic duplicate check since we killed it
            p = mParticles.back();
            mParticles.pop_back();
            continue;
        }

        ++i;
    }

    // 2. Spawn environmental particles around player
    // Rain (density: ~12 per update frame)
    int rainToSpawn = 10;
    for (int r = 0; r < rainToSpawn; ++r)
    {
        Particle rain;
        rain.type = 1;
        float rx = playerPos.x + ((rand() % 1600) - 800) * 0.05f; // -40 to +40
        float rz = playerPos.z + ((rand() % 1600) - 800) * 0.05f;
        float ry = playerPos.y + 20.0f + ((rand() % 500) * 0.03f); // 20 to 35 high
        
        rain.position = glm::vec3(rx, ry, rz);
        // Fall down fast + slight wind
        rain.velocity = glm::vec3(-1.5f, -24.0f, -0.5f);
        rain.color = glm::vec4(0.78f, 0.86f, 0.96f, 0.28f);
        rain.size = 0.35f + ((rand() % 100) * 0.002f); // Rain length
        rain.life = 1.8f;
        rain.maxLife = rain.life;
        
        AddParticle(rain);
    }

    // Leaves (density: ~1 per frame)
    if ((rand() % 100) < 65)
    {
        Particle leaf;
        leaf.type = 2;
        float lx = playerPos.x + ((rand() % 1800) - 900) * 0.05f; // -45 to +45
        float lz = playerPos.z + ((rand() % 1800) - 900) * 0.05f;
        float ly = playerPos.y + 10.0f + ((rand() % 300) * 0.05f); // 10 to 25 high
        
        leaf.position = glm::vec3(lx, ly, lz);
        leaf.velocity = glm::vec3(
            -0.8f + ((rand() % 100) * 0.004f),
            -1.2f - ((rand() % 100) * 0.006f),
            -0.4f + ((rand() % 100) * 0.004f)
        );
        
        // Random leaf colors (autumnal tones)
        int colorType = rand() % 4;
        if (colorType == 0)      leaf.color = glm::vec4(0.35f, 0.65f, 0.22f, 0.85f); // Green
        else if (colorType == 1) leaf.color = glm::vec4(0.82f, 0.45f, 0.12f, 0.85f); // Orange
        else if (colorType == 2) leaf.color = glm::vec4(0.76f, 0.18f, 0.12f, 0.85f); // Red
        else                     leaf.color = glm::vec4(0.85f, 0.72f, 0.15f, 0.85f); // Yellow
        
        leaf.size = 0.12f + ((rand() % 100) * 0.001f);
        leaf.life = 5.0f;
        leaf.maxLife = leaf.life;
        leaf.rotation = ((rand() % 360) * 3.14159f / 180.0f);
        leaf.angularVelocity = -2.0f + ((rand() % 100) * 0.04f);
        
        AddParticle(leaf);
    }

    // 3. Spawn vehicle exhaust smoke (continuous but reacts to speed)
    static float exhaustAccumulator = 0.0f;
    exhaustAccumulator += dt;
    float smokeInterval = (std::abs(playerVelocity.y) > 5.0f) ? 0.02f : 0.05f; // spawn faster under acceleration
    if (exhaustAccumulator >= smokeInterval)
    {
        exhaustAccumulator = 0.0f;
        for (int i = 0; i < 2; ++i)
        {
            glm::vec3 pos = (i == 0) ? exhaustL : exhaustR;
            
            Particle smoke;
            smoke.type = 0;
            smoke.position = pos;
            
            // Blow slightly backwards relative to car orientation
            glm::vec3 backDir = glm::normalize(playerVelocity + glm::vec3(0.0001f));
            if (glm::length(playerVelocity) < 0.1f)
            {
                backDir = glm::vec3(0.0f, 0.0f, -1.0f); // Default back direction
            }
            else
            {
                backDir = -backDir;
            }
            
            smoke.velocity = backDir * 1.5f + glm::vec3(
                ((rand() % 100) - 50) * 0.008f,
                ((rand() % 100) * 0.006f) + 0.1f,
                ((rand() % 100) - 50) * 0.008f
            );
            
            smoke.color = glm::vec4(0.65f, 0.65f, 0.65f, 0.30f);
            smoke.size = 0.06f;
            smoke.life = 0.4f + ((rand() % 100) * 0.003f);
            smoke.maxLife = smoke.life;
            smoke.rotation = ((rand() % 360) * 3.14159f / 180.0f);
            smoke.angularVelocity = -1.0f + ((rand() % 100) * 0.02f);
            
            AddParticle(smoke);
        }
    }

    // 4. Spawn tire smoke on drift or hard braking
    if (isDrifting || isBraking)
    {
        for (const auto& tirePos : tirePositions)
        {
            // Spawn multiple tire smoke puffs per frame
            int smokeCount = isDrifting ? 3 : 1;
            for (int s = 0; s < smokeCount; ++s)
            {
                Particle tireSmoke;
                tireSmoke.type = 0;
                // Offset slightly from the wheel bottom
                tireSmoke.position = tirePos - glm::vec3(0.0f, 0.06f, 0.0f);
                
                // Expand outward from tire
                tireSmoke.velocity = glm::vec3(
                    ((rand() % 100) - 50) * 0.018f,
                    ((rand() % 100) * 0.015f) + 0.4f,
                    ((rand() % 100) - 50) * 0.018f
                );
                
                // Drift smoke is whiter
                tireSmoke.color = glm::vec4(0.85f, 0.85f, 0.85f, 0.38f);
                tireSmoke.size = 0.12f;
                tireSmoke.life = 0.5f + ((rand() % 100) * 0.003f);
                tireSmoke.maxLife = tireSmoke.life;
                tireSmoke.rotation = ((rand() % 360) * 3.14159f / 180.0f);
                tireSmoke.angularVelocity = -1.5f + ((rand() % 100) * 0.03f);
                
                AddParticle(tireSmoke);
            }
        }
    }
}

void ParticleSystem::Draw(Camera& camera, Shader& particleShader)
{
    if (mParticles.empty()) return;

    mVertices.clear();

    // Get camera right and up vectors for CPU billboarding
    glm::vec3 camRight = glm::normalize(glm::cross(camera.Orientation, camera.Up));
    glm::vec3 camUp = camera.Up;

    for (const auto& p : mParticles)
    {
        if (p.type == 1) // Rain is aligned along its velocity, not face-billboarded
        {
            glm::vec3 dir = glm::normalize(p.velocity);
            glm::vec3 right = glm::normalize(glm::cross(dir, camera.Orientation));
            
            glm::vec3 wOffset = right * (0.016f); // thin width
            glm::vec3 hOffset = dir * (p.size * 0.5f); // length
            
            ParticleVertex v0{ p.position - wOffset - hOffset, glm::vec2(0.0f, 0.0f), p.color };
            ParticleVertex v1{ p.position + wOffset - hOffset, glm::vec2(1.0f, 0.0f), p.color };
            ParticleVertex v2{ p.position + wOffset + hOffset, glm::vec2(1.0f, 1.0f), p.color };
            ParticleVertex v3{ p.position - wOffset + hOffset, glm::vec2(0.0f, 1.0f), p.color };
            
            mVertices.push_back(v0);
            mVertices.push_back(v1);
            mVertices.push_back(v2);
            mVertices.push_back(v3);
        }
        else // Smoke (0) and Leaf (2) are standard camera-facing billboards with rotation
        {
            float cosR = std::cos(p.rotation);
            float sinR = std::sin(p.rotation);
            
            glm::vec3 localRight = camRight * cosR - camUp * sinR;
            glm::vec3 localUp = camRight * sinR + camUp * cosR;
            
            float halfSize = p.size * 0.5f;
            glm::vec3 rOffset = localRight * halfSize;
            glm::vec3 uOffset = localUp * halfSize;
            
            ParticleVertex v0{ p.position - rOffset - uOffset, glm::vec2(0.0f, 0.0f), p.color };
            ParticleVertex v1{ p.position + rOffset - uOffset, glm::vec2(1.0f, 0.0f), p.color };
            ParticleVertex v2{ p.position + rOffset + uOffset, glm::vec2(1.0f, 1.0f), p.color };
            ParticleVertex v3{ p.position - rOffset + uOffset, glm::vec2(0.0f, 1.0f), p.color };
            
            mVertices.push_back(v0);
            mVertices.push_back(v1);
            mVertices.push_back(v2);
            mVertices.push_back(v3);
        }
    }

    // Bind and upload dynamic data
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, mVertices.size() * sizeof(ParticleVertex), mVertices.data());

    // Disable face culling or depth writing if transparent blending
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE); // Particles don't write to depth buffer (prevents ugly outlines)

    particleShader.Activate();
    camera.Matrix(particleShader, "camMatrix");

    // Group and draw particles by type to minimize uniform changes
    // (There are only 3 types, so we can group them in 3 passes or just draw them one-by-one by type)
    int currentType = -1;
    for (size_t i = 0; i < mParticles.size(); ++i)
    {
        int pType = mParticles[i].type;
        if (pType != currentType)
        {
            currentType = pType;
            glUniform1i(glGetUniformLocation(particleShader.ID, "uType"), currentType);
        }
        
        // Draw 6 indices (2 triangles) per particle
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)(i * 6 * sizeof(GLuint)));
    }

    glDepthMask(GL_TRUE); // Re-enable depth writing
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}
