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

vec4 pointLight()
{
    // used in two variables so I calculate it here to not have to do it twice
    vec3 lightVec = lightPos - crntPos;

    // intensity of light with respect to distance
    float dist = length(lightVec);
    float a = 3.0;
    float b = 0.7;
    float inten = 1.0f / (a * dist * dist + b * dist + 1.0f);

    // ambient lighting
    float ambient = 0.20f;

    // diffuse lighting
    vec3 normal = normalize(Normal);
    vec3 lightDirection = normalize(lightVec);
    float diffuse = max(dot(normal, lightDirection), 0.0f);

    // specular lighting
    float specularLight = 0.50f;
    vec3 viewDirection = normalize(camPos - crntPos);
    vec3 reflectionDirection = reflect(-lightDirection, normal);
    float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 16);
    float specular = specAmount * specularLight;

    vec4 texColor = texture(diffuse0, texCoord) * vec4(color, 1.0f);
    float spec = texture(specular0, texCoord).r * specular * inten;
    float outAlpha = uUseAlpha ? texColor.a : 1.0f;
    return vec4((texColor.rgb * (diffuse * inten + ambient) + vec3(spec)) * lightColor.rgb, outAlpha);
}

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

    vec4 texColor = texture(diffuse0, texCoord) * vec4(color, 1.0f);
    float spec = texture(specular0, texCoord).r * specular;
    float outAlpha = uUseAlpha ? texColor.a : 1.0f;
    return vec4((texColor.rgb * (diffuse + ambient) + vec3(spec)) * lightColor.rgb, outAlpha);
}

vec4 spotLight()
{
    // controls how big the area that is lit up is
    float outerCone = 0.90f;
    float innerCone = 0.95f;

    // ambient lighting
    float ambient = 0.20f;

    // diffuse lighting
    vec3 normal = normalize(Normal);
    vec3 lightDirection = normalize(lightPos - crntPos);
    float diffuse = max(dot(normal, lightDirection), 0.0f);

    // specular lighting
    float specularLight = 0.50f;
    vec3 viewDirection = normalize(camPos - crntPos);
    vec3 reflectionDirection = reflect(-lightDirection, normal);
    float specAmount = pow(max(dot(viewDirection, reflectionDirection), 0.0f), 16);
    float specular = specAmount * specularLight;

    // calculates the intensity of the crntPos based on its angle to the center of the light cone
    float angle = dot(vec3(0.0f, -1.0f, 0.0f), -lightDirection);
    float clampAngle = clamp((angle - outerCone) / (innerCone - outerCone), 0.0f, 1.0f);

    vec4 texColor = texture(diffuse0, texCoord) * vec4(color, 1.0f);
    float spec = texture(specular0, texCoord).r * specular * clampAngle;
    float outAlpha = uUseAlpha ? texColor.a : 1.0f;
    return vec4((texColor.rgb * (diffuse * clampAngle + ambient) + vec3(spec)) * lightColor.rgb, outAlpha);
}

void main()
{
    // outputs final color
    FragColor = direcLight();
}
