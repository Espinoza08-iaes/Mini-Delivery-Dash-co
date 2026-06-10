#version 330 core
out vec4 FragColor;

in vec3 crntPos;
in vec3 Normal;
in vec3 color;
in vec2 texCoord;
in vec4 vLightSpacePos;

uniform sampler2D diffuse0;
uniform sampler2D specular0;
uniform vec4 lightColor;
uniform vec3 lightPos;
uniform vec3 camPos;
uniform bool uUseAlpha;
uniform float uAmbientStrength = 0.20;
uniform int  uStreetLightsOn;
uniform vec3 uFogColor = vec3(0.07, 0.13, 0.17);
uniform float uFogStart = 40.0;
uniform float uFogEnd   = 250.0;

void main()
{
    vec4 texColor = texture(diffuse0, texCoord) * vec4(color, 1.0);

    // Detectar bombilla por su color amarillo cálido
    bool isBulb = (color.r > 0.8 && color.g > 0.8 && (color.r - color.b) > 0.15);

    vec3 result;
    if (isBulb && uStreetLightsOn == 1)
    {
        // Bombilla encendida de noche: emissive
        result = texColor.rgb * 3.0;
    }
    else if (isBulb)
    {
        // Bombilla apagada de día: color normal suave
        result = texColor.rgb * 0.6;
    }
    else
    {
        // Poste metálico: iluminación simple
        vec3 normal   = normalize(Normal);
        vec3 lightDir = normalize(lightPos);
        float diff    = max(dot(normal, lightDir), 0.0);
        result = texColor.rgb * (uAmbientStrength + diff * 0.5) * lightColor.rgb;
    }

    // Niebla
    float dist      = length(camPos - crntPos);
    float fogFactor = clamp((uFogEnd - dist) / (uFogEnd - uFogStart), 0.0, 1.0);
    result = mix(uFogColor, result, fogFactor);

    FragColor = vec4(result, 1.0);
}