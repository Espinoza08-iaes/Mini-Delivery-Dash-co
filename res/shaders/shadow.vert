#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 translation;
uniform mat4 rotation;
uniform mat4 scale;

uniform mat4 lightSpaceMatrix;

void main()
{
    vec4 worldPos = model * translation * rotation * scale * vec4(aPos, 1.0);
    gl_Position = lightSpaceMatrix * worldPos;
}
