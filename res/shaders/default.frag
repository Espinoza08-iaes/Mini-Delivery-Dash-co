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

// Headlights spotlights uniforms
uniform bool uHeadlightsLightOn;
uniform vec3 uHeadlightLeftPos;
uniform vec3 uHeadlightRightPos;
uniform vec3 uHeadlightDir;
uniform vec3 uHeadlightColor;

uniform vec3 uFogColor = vec3(0.07f, 0.13f, 0.17f);
uniform float uFogStart = 40.0f;
uniform float uFogEnd = 250.0f;
uniform float uAmbientStrength = 0.20f;

vec3 calculateSpotLight(vec3 spotLightPos, vec3 spotLightDir, vec3 normal, vec3 viewDir, vec3 baseColor, float specTex)
{
    vec3 lightVec = spotLightPos - crntPos;
    float dist = length(lightVec);
    
    // Attenuation of the light over distance (significantly reduced for longer throw headlights)
    float a = 0.0018f;
    float b = 0.015f;
    float inten = 1.0f / (a * dist * dist + b * dist + 1.0f);

    // Diffuse lighting
    vec3 lightDirection = normalize(lightVec);
    float diffuse = max(dot(normal, lightDirection), 0.0f);

    // Specular lighting (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDirection + viewDir);
    float specAmount = pow(max(dot(normal, halfwayDir), 0.0f), 32);
    float specular = specAmount * 0.50f * specTex;

    // Angle between the light ray to fragment and the headlight center direction
    float angle = dot(-lightDirection, spotLightDir);
    
    // Spotlight cone angles (outerCone ~30 deg, innerCone ~20 deg spread)
    float outerCone = 0.88f;
    float innerCone = 0.94f;
    float intensity = clamp((angle - outerCone) / (innerCone - outerCone), 0.0, 1.0);

    return (baseColor * diffuse + vec3(specular)) * uHeadlightColor * inten * intensity;
}

vec4 direcLight()
{
    // ambient lighting
    float ambient = uAmbientStrength;
    float diffuseFactor = 1.0f;

    if (uHeadlightsLightOn)
    {
        ambient = min(ambient, 0.12f); // Brighter cozy night ambient so the city is still visible
        diffuseFactor = 0.18f;          // Brighter moon lighting contribution
    }

    // diffuse lighting
    vec3 normal = normalize(Normal);
    vec3 lightDirection = normalize(vec3(1.0f, 1.0f, 0.0f));
    float diffuse = max(dot(normal, lightDirection), 0.0f) * diffuseFactor;

    // specular lighting
    float specularLight = 0.50f;
    vec3 viewDirection = normalize(camPos - crntPos);
    
    // Blinn-Phong specular
    vec3 halfwayDir = normalize(lightDirection + viewDirection);
    float specAmount = pow(max(dot(normal, halfwayDir), 0.0f), 32);
    float specular = specAmount * specularLight;

    vec4 texColor = texture(diffuse0, texCoord) * vec4(color, 1.0f);
    float spec = texture(specular0, texCoord).r * specular;
    float outAlpha = uUseAlpha ? texColor.a : 1.0f;
    return vec4((texColor.rgb * (diffuse + ambient) + vec3(spec)) * lightColor.rgb, outAlpha);
}

void main()
{
    vec4 finalColor;
    if (uIsEmissive)
    {
        vec4 texColor = texture(diffuse0, texCoord) * vec4(color, 1.0f);
        vec3 finalEmissive = texColor.rgb * 1.5 + uEmissiveColor;
        float outAlpha = uUseAlpha ? texColor.a : 1.0f;
        finalColor = vec4(finalEmissive, outAlpha);
    }
    else
    {
        vec4 baseLight = direcLight();
        
        if (uHeadlightsLightOn)
        {
            vec3 normal = normalize(Normal);
            vec3 viewDir = normalize(camPos - crntPos);
            vec4 texColor = texture(diffuse0, texCoord) * vec4(color, 1.0f);
            float specTex = texture(specular0, texCoord).r;
            
            vec3 leftSpot = calculateSpotLight(uHeadlightLeftPos, uHeadlightDir, normal, viewDir, texColor.rgb, specTex);
            vec3 rightSpot = calculateSpotLight(uHeadlightRightPos, uHeadlightDir, normal, viewDir, texColor.rgb, specTex);
            
            baseLight.rgb += leftSpot + rightSpot;
        }
        
        finalColor = baseLight;
    }

    // Apply distance fog
    float dist = length(camPos - crntPos);
    float fogFactor = clamp((uFogEnd - dist) / (uFogEnd - uFogStart), 0.0, 1.0);
    finalColor.rgb = mix(uFogColor, finalColor.rgb, fogFactor);

    FragColor = finalColor;
}
