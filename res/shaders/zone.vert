#version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
layout(location=3) in vec2 aTexCoord;

out vec3 crntPos;
out vec3 Normal;
out vec3 color;
out vec2 texCoord;

uniform mat4 model;
uniform mat4 cameraMatrix;
uniform float uTime;

void main()
{
    // Apply rotation around Y axis based on time for spinning effect
    float rotationSpeed = 1.5; // Rotation speed
    float angle = uTime * rotationSpeed;
    
    mat4 rotationMatrix = mat4(
        cos(angle), 0.0, sin(angle), 0.0,
        0.0, 1.0, 0.0, 0.0,
        -sin(angle), 0.0, cos(angle), 0.0,
        0.0, 0.0, 0.0, 1.0
    );
    
    vec4 rotatedPos = rotationMatrix * vec4(aPos, 1.0);
    vec4 worldPos = model * rotatedPos;
    
    crntPos = vec3(worldPos);
    Normal = mat3(transpose(inverse(model * rotationMatrix))) * aNormal;
    color = aColor;
    texCoord = aTexCoord;
    
    gl_Position = cameraMatrix * worldPos;
}