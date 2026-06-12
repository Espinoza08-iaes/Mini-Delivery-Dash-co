#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../../engine/graphics/Camera.h"
#include "../../engine/graphics/Shader.h"

struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec4 color;
    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    float size = 1.0f;
    float life = 0.0f;
    float maxLife = 1.0f;
    int type = 0; // 0 = Smoke, 1 = Rain, 2 = Leaf
};

struct ParticleVertex {
    glm::vec3 position;
    glm::vec2 texUV;
    glm::vec4 color;
};

class ParticleSystem {
public:
    ParticleSystem(int maxParticles = 2500);
    ~ParticleSystem();

    void Update(float dt, const glm::vec3& playerPos, const glm::vec3& playerVelocity, 
                bool isBraking, bool isDrifting, const glm::vec3& exhaustL, const glm::vec3& exhaustR,
                const std::vector<glm::vec3>& tirePositions, float groundHeight);
    
    void Draw(Camera& camera, Shader& particleShader);

    void AddParticle(const Particle& p);

private:
    int mMaxParticles;
    std::vector<Particle> mParticles;
    
    GLuint mVAO, mVBO, mEBO;
    std::vector<ParticleVertex> mVertices;
    std::vector<GLuint> mIndices;

    void SetupBuffers();
};

#endif
