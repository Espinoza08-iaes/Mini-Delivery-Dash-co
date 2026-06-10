#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTex;

out vec3 Position;
out vec3 Normal;
out vec2 texCoord;

uniform mat4 camMatrix;
uniform mat4 model;

void main()
{
    Position = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    texCoord = aTex;
    gl_Position = camMatrix * model * vec4(aPos, 1.0);
}
