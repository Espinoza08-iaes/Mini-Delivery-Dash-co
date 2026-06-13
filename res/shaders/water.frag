#version 330 core

out vec4 FragColor;

in vec3 Position;
in vec3 Normal;
in vec2 texCoord;

uniform sampler2D diffuse0; // Sunflowers sky texture
uniform vec3 camPos;
uniform float uTime;
uniform vec3 uSkyTint;
uniform vec3 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;

uniform vec3 lightPos;
uniform vec4 lightColor;

void main()
{
    vec3 I = normalize(Position - camPos);
    vec3 viewDir = -I;
    
    // Dynamic rippling normals (combining multiple high-frequency wave octaves for detailed ripples)
    vec3 normal = normalize(Normal);
    
    // Wave octave 1 (Large swell)
    float w1x = sin(Position.x * 0.05 + uTime * 1.2) * 0.04f;
    float w1z = cos(Position.z * 0.05 + uTime * 1.0) * 0.04f;
    
    // Wave octave 2 (Medium choppy waves)
    float w2x = cos(Position.z * 0.15 - uTime * 2.0) * 0.035f;
    float w2z = sin(Position.x * 0.15 + uTime * 1.8) * 0.035f;
    
    // Wave octave 3 (Fine wind ripples)
    float w3x = sin((Position.x + Position.z) * 0.45f + uTime * 3.5) * 0.015f;
    float w3z = cos((Position.x - Position.z) * 0.45f - uTime * 3.0) * 0.015f;
    
    normal.x += w1x + w2x + w3x;
    normal.z += w1z + w2z + w3z;
    normal = normalize(normal);
    
    // Calculate reflection vector
    vec3 R = reflect(I, normal);
    
    // Sample skybox reflection (equirectangular mapping)
    const vec2 invAtan = vec2(0.1591549f, 0.3183098f);
    vec2 uv = vec2(atan(R.z, R.x), asin(R.y));
    uv *= invAtan;
    uv += 0.5;
    
    vec3 reflection = texture(diffuse0, vec2(uv.x, 1.0 - uv.y)).rgb * uSkyTint;
    
    // Fresnel term (reflectivity increases at shallow angles)
    float fresnel = dot(normal, viewDir);
    fresnel = clamp(1.0 - fresnel, 0.0, 1.0);
    fresnel = pow(fresnel, 3.5);
    
    // specular reflections from sun/moon
    vec3 lightDir = normalize(lightPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float specAmount = pow(max(dot(normal, halfwayDir), 0.0), 128.0);
    vec3 specular = specAmount * vec3(1.5) * lightColor.rgb;

    // Subsurface scattering approximation (translucency of wave crests facing the light)
    float scatter = max(dot(lightDir, I), 0.0) * pow(1.0 - dot(normal, viewDir), 2.0);
    vec3 scatterColor = vec3(0.0, 0.45, 0.35) * scatter * lightColor.rgb * 0.4;
    
    // Base ocean color matching time of day tint
    vec3 oceanBase = vec3(0.01f, 0.06f, 0.12f) * uSkyTint;
    vec3 waterColor = mix(oceanBase, reflection, fresnel * 0.90 + 0.10);
    waterColor += specular + scatterColor;
    
    // Distance fog
    float dist = length(camPos - Position);
    float fogFactor = clamp((uFogEnd - dist) / (uFogEnd - uFogStart), 0.0, 1.0);
    vec3 finalColor = mix(uFogColor, waterColor, fogFactor);
    
    FragColor = vec4(finalColor, 1.0f);
}
