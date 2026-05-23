#version 330 core

// Outputs colors in RGBA
out vec4 FragColor;

// Imports the current position from the Vertex Shader
in vec3 crntPos;
// Imports the normal from the Vertex Shader
in vec3 Normal;
// Imports the color from the Vertex Shader
in vec3 color;
// Imports the texture coordinates from the Vertex Shader
in vec2 texCoord;

// Gets the Texture Units from the main function
uniform sampler2D diffuse0;
uniform sampler2D specular0;
// Gets the color of the light from the main function
uniform vec4 lightColor;
// Gets the position of the light from the main function
uniform vec3 lightPos;
// Gets the position of the camera from the main function
uniform vec3 camPos;
// Controls whether to use texture alpha channel for transparency blending
uniform bool uUseAlpha;

// Emissive lighting for glowing parts
uniform bool uIsEmissive;
uniform vec3 uEmissiveColor;

// Color override for custom body paint
uniform bool uUseColorOverride;
uniform vec3 uColorOverride;

vec4 direcLight()
{
    // ambient lighting
    float ambient = 0.20f;

    // diffuse lighting
    vec3 normal = normalize(Normal);
    vec3 lightDirection = normalize(vec3(1.0f, 1.0f, 0.0f));
    float diffuse = max(dot(normal, lightDirection), 0.0f);

    // specular lighting
    float specularLight = 0.50f;
    vec3 viewDirection = normalize(camPos - crntPos);
    vec3 reflectionDirection = reflect(-lightDirection, normal);
    float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 16);
    float specular = specAmount * specularLight;

    vec3 finalColor = color;
    if (uUseColorOverride)
    {
        finalColor = uColorOverride;
    }

    vec4 texColor = texture(diffuse0, texCoord) * vec4(finalColor, 1.0f);
    float spec = texture(specular0, texCoord).r * specular;
    float outAlpha = uUseAlpha ? texColor.a : 1.0f;
    return vec4((texColor.rgb * (diffuse + ambient) + vec3(spec)) * lightColor.rgb, outAlpha);
}

void main()
{
    if (uIsEmissive)
    {
        vec3 baseColor = color;
        if (uUseColorOverride)
        {
            baseColor = uColorOverride;
        }
        vec4 texColor = texture(diffuse0, texCoord) * vec4(baseColor, 1.0f);
        // Make emissive surfaces glow with uEmissiveColor
        vec3 finalEmissive = texColor.rgb * 1.5 + uEmissiveColor;
        float outAlpha = uUseAlpha ? texColor.a : 1.0f;
        FragColor = vec4(finalEmissive, outAlpha);
    }
    else
    {
        FragColor = direcLight();
    }
}
