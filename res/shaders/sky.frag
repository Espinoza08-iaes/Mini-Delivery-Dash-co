#version 330 core

out vec4 FragColor;

in vec2 texCoord;

uniform sampler2D diffuse0;
uniform vec3 uSkyTint;

void main()
{
    vec3 color = texture(diffuse0, vec2(texCoord.x, 1.0 - texCoord.y)).rgb;
    color *= uSkyTint;
    FragColor = vec4(color, 1.0);
}
