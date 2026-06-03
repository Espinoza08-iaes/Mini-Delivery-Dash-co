#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 3) in vec2 aTex;

out vec3 localPos;
out vec2 texCoord;

uniform mat4 camMatrix;
uniform mat4 model;
uniform mat4 translation;
uniform mat4 rotation;
uniform mat4 scale;

void main()
{
    localPos = aPos;
    texCoord = aTex;
    gl_Position = camMatrix * model * translation * rotation * scale * vec4(aPos, 1.0);
}
