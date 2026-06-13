#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTex;

out vec3 Position;
out vec3 Normal;
out vec2 texCoord;

uniform mat4 camMatrix;
uniform mat4 model;
uniform float uTime;

void main()
{
    vec3 pos = aPos;

    // Desplazamiento vertical real de vértices (3 octavas de ondas)
    float w1 = sin(pos.x * 0.05  + uTime * 1.2) * cos(pos.z * 0.04  + uTime * 0.9) * 0.30;
    float w2 = sin(pos.z * 0.08  - uTime * 1.5) * cos(pos.x * 0.07  + uTime * 1.1) * 0.18;
    float w3 = sin((pos.x + pos.z) * 0.13 + uTime * 2.2) * 0.08;
    pos.y += w1 + w2 + w3;

    // Normal analítica derivada del desplazamiento
    float dx = cos(pos.x * 0.05 + uTime * 1.2) * 0.05 * 0.30
             - sin(pos.x * 0.07 + uTime * 1.1) * 0.07 * 0.18
             + cos((pos.x + pos.z) * 0.13 + uTime * 2.2) * 0.13 * 0.08;
    float dz = -sin(pos.z * 0.04 + uTime * 0.9) * 0.04 * 0.30
             + cos(pos.z * 0.08 - uTime * 1.5) * 0.08 * 0.18
             + cos((pos.x + pos.z) * 0.13 + uTime * 2.2) * 0.13 * 0.08;

    Position = vec3(model * vec4(pos, 1.0));
    Normal   = normalize(vec3(-dx, 1.0, -dz));
    texCoord = aTex;
    gl_Position = camMatrix * model * vec4(pos, 1.0);
}