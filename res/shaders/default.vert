#version 330 core

// Positions/Coordinates
layout (location = 0) in vec3 aPos;
// Normals (not necessarily normalized)
layout (location = 1) in vec3 aNormal;
// Colors
layout (location = 2) in vec3 aColor;
// Texture Coordinates
layout (location = 3) in vec2 aTex;

// Outputs the current position for the Fragment Shader
out vec3 crntPos;
// Outputs the normal for the Fragment Shader
out vec3 Normal;
// Outputs the color for the Fragment Shader
out vec3 color;
// Outputs the texture coordinates to the Fragment Shader
out vec2 texCoord;

// Imports the camera matrix
uniform mat4 camMatrix;
// Imports the transformation matrices
uniform mat4 model;
uniform mat4 translation;
uniform mat4 rotation;
uniform mat4 scale;

uniform mat4 uLightSpaceMatrix;

out vec4 vLightSpacePos;

void main()
{
    // Calculates current position
    crntPos = vec3(model * translation * rotation * scale * vec4(aPos, 1.0f));
    // Transform normal to world space and normalize
    Normal = normalize(mat3(model * translation * rotation * scale) * aNormal);
    // Assigns the colors from the Vertex Data to "color"
    color = aColor;
    // Assigns the texture coordinates from the Vertex Data to "texCoord"
    texCoord = aTex;

    vLightSpacePos = uLightSpaceMatrix * vec4(crntPos, 1.0f);

    // Outputs the positions/coordinates of all vertices
    gl_Position = camMatrix * vec4(crntPos, 1.0);
}
