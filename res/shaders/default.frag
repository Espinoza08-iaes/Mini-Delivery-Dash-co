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
// Imports the position in light space
in vec4 vLightSpacePos;

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
uniform int  uStreetLightsOn;
uniform vec3 uStreetLightPos[8];

// Emissive lighting for glowing parts
uniform bool uIsEmissive;
uniform vec3 uEmissiveColor;
uniform int uIsFacade;

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

uniform sampler2D uSkyReflectionMap;
uniform float uReflectivity = 0.0;
uniform float uSkyRotationAngle = 0.0;
uniform vec3 uSkyTint = vec3(1.0);

uniform sampler2DShadow uShadowMap;

// Poisson Disk offsets for soft shadow sampling
const vec2 poissonDisk[8] = vec2[](
    vec2(-0.94201624,  0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581, -0.30217051),
    vec2( 0.70228851,  0.56332015),
    vec2(-0.57008775,  0.75952130),
    vec2( 0.21341862, -0.40724183)
);

float ShadowCalculation(vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0)
        return 0.0;
        
    float bias = max(0.0015 * (1.0 - dot(normal, lightDir)), 0.0002);
    
    // Generate pseudo-random angle based on world coordinates to prevent shimmering/noise moving with screen coordinates
    float angle = fract(sin(dot(crntPos, vec3(12.9898, 78.233, 45.164))) * 43758.5453) * 6.28318530718;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rotationMatrix = mat2(c, -s, s, c);
    
    float litAmount = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    
    // Penumbra search radius
    float penumbraRadius = 2.2;
    
    for(int i = 0; i < 8; ++i)
    {
        vec2 offset = rotationMatrix * poissonDisk[i] * penumbraRadius * texelSize;
        litAmount += texture(uShadowMap, vec3(projCoords.xy + offset, projCoords.z - bias));
    }
    litAmount /= 8.0;
    
    // Soften shadow max opacity (0.88 instead of 1.0) so it's not pitch black
    return (1.0 - litAmount) * 0.88;
}

uniform sampler2D uCameraDepthMap;
uniform bool uUseSSAO = true;

float LinearizeDepth(float depth)
{
    float near = 0.1;
    float far = 20000.0;
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

float CalculateSSAO()
{
    vec2 texSize = textureSize(uCameraDepthMap, 0);
    vec2 uv = gl_FragCoord.xy / texSize;
    
    float centerDepth = texture(uCameraDepthMap, uv).r;
    float centerLinear = LinearizeDepth(centerDepth);
    
    if (centerDepth > 0.999)
        return 1.0;
        
    float occlusion = 0.0;
    
    vec2 dirs[4] = vec2[](
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.707, 0.707),
        vec2(-0.707, 0.707)
    );
    
    float baseRadius = 8.0; 
    float radius = clamp(baseRadius / (centerLinear * 0.02), 1.0, 24.0);
    vec2 texel = radius / texSize;
    
    for (int i = 0; i < 4; ++i)
    {
        vec2 d = dirs[i] * texel;
        float depth1 = texture(uCameraDepthMap, uv + d).r;
        float depth2 = texture(uCameraDepthMap, uv - d).r;
        
        float linear1 = LinearizeDepth(depth1);
        float linear2 = LinearizeDepth(depth2);
        
        float diff = (linear1 + linear2) - 2.0 * centerLinear;
        
        if (diff > 0.01)
        {
            float weight = smoothstep(5.0, 0.01, diff);
            occlusion += diff * weight * 0.35;
        }
    }
    
    occlusion = clamp(occlusion / 4.0, 0.0, 0.75);
    return 1.0 - occlusion;
}

vec3 RotateY(vec3 v, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return vec3(
        v.x * c - v.z * s,
        v.y,
        v.x * s + v.z * c
    );
}

vec2 SampleEquirectangular(vec3 r)
{
    float phi = atan(r.z, r.x);
    float theta = asin(r.y);
    const float PI = 3.14159265359;
    float u = 1.0 - (phi / (2.0 * PI) + 0.5);
    float v = theta / PI + 0.5;
    return vec2(u, v);
}

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
    float outerCone = 0.82f;
    float innerCone = 0.92f;
    float intensity = clamp((angle - outerCone) / (innerCone - outerCone), 0.0, 1.0);

    return (baseColor * diffuse + vec3(specular)) * uHeadlightColor * inten * intensity;
}

vec3 calculateStreetLight(vec3 lampPos, vec3 normal, vec3 albedo)
{
    vec3 toLight = lampPos - crntPos;
    float dist = length(toLight);
    if (dist < 0.001 || dist > 40.0)
        return vec3(0.0);

    vec3 lightDir = normalize(toLight);
    float wrapDiffuse = max((dot(normal, lightDir) + 0.35) / 1.35, 0.0);

    float range = 40.0;
    float atten = pow(clamp(1.0 - dist / range, 0.0, 1.0), 1.1);
    atten /= (1.0 + 0.012 * dist + 0.003 * dist * dist);

    // Dirección desde la lámpara hacia el fragmento (cono hacia abajo)
    vec3 fromLamp = normalize(crntPos - lampPos);
    float downCone = smoothstep(0.30, 0.90, dot(fromLamp, vec3(0.0, -1.0, 0.0)));
    float facadeSpill = 1.0 - smoothstep(0.0, 0.80, abs(fromLamp.y));
    float distribution = max(downCone, facadeSpill * 0.70);

    vec3 warmColor = vec3(1.0, 0.90, 0.65);
    vec3 diffuse = albedo * warmColor * wrapDiffuse * atten * distribution * 0.50;
    vec3 glow = warmColor * wrapDiffuse * atten * distribution * 0.24;
    return diffuse + glow;
}

vec4 direcLight()
{
    // ambient lighting
    float ambient = uAmbientStrength;
    float diffuseFactor = 1.0f;

    if (uHeadlightsLightOn)
    {
        ambient = ambient; // Brighter cozy night ambient so the city is still visible
        diffuseFactor = 1.0f;          // Brighter moon lighting contribution
    }

    // diffuse lighting
    vec3 normal = normalize(Normal);
    vec3 lightDirection = normalize(lightPos);
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

    // Calculate shadow factor
    float shadow = ShadowCalculation(vLightSpacePos, normal, lightDirection);

    // Calculate ambient occlusion factor (suavizar de noche para evitar ruido en aceras)
    float ao = 1.0;
    if (uUseSSAO)
    {
        ao = CalculateSSAO();
        if (uStreetLightsOn == 1)
            ao = mix(ao, 1.0, 0.5);
    }
    float ambientWithAO = ambient * ao;

    vec3 litColor = (texColor.rgb * (diffuse * (1.0 - shadow) + ambientWithAO) + vec3(spec * (1.0 - shadow))) * lightColor.rgb;

    if (uReflectivity > 0.0)
    {
        vec3 R = reflect(-viewDirection, normal);
        vec3 rRotated = RotateY(R, uSkyRotationAngle);
        vec2 uv = SampleEquirectangular(rRotated);
        vec3 reflectionColor = texture(uSkyReflectionMap, uv).rgb * uSkyTint;
        
        float cosTheta = clamp(dot(viewDirection, normal), 0.0, 1.0);
        float F0 = uReflectivity;
        float fresnel = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
        
        litColor = mix(litColor, reflectionColor, fresnel);
    }

    return vec4(litColor, outAlpha);
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

        // --- Street light contribution (antes del fog) ---
        if (uStreetLightsOn == 1)
        {
            vec3 normal = normalize(Normal);
            vec4 texColor = texture(diffuse0, texCoord) * vec4(color, 1.0f);
            vec3 streetLight = vec3(0.0);

            for (int i = 0; i < 8; ++i)
                streetLight += calculateStreetLight(uStreetLightPos[i], normal, texColor.rgb);

            streetLight = min(streetLight, vec3(0.85));
            baseLight.rgb += streetLight;
        }

        // --- Facade window glow (night only) ---
        if (uIsFacade == 1 && uStreetLightsOn == 1)
        {
            vec2 gridSize = vec2(32.0, 16.0);
            vec2 uvGrid = texCoord * gridSize;
            vec2 windowID = floor(uvGrid);
            vec2 windowCell = fract(uvGrid);

            if (windowCell.x > 0.28 && windowCell.x < 0.72 && windowCell.y > 0.25 && windowCell.y < 0.75)
            {
                float randVal = fract(sin(dot(windowID, vec2(12.9898, 78.233))) * 43758.5453);
                if (randVal > 0.75) // 25% of windows lit
                {
                    vec3 glowColor = (fract(randVal * 7.0) > 0.4) ? vec3(1.0, 0.82, 0.45) : vec3(0.65, 0.8, 1.0);
                    baseLight.rgb = mix(baseLight.rgb, glowColor * 1.5, 0.8);
                }
            }
        }
        
        FragColor = baseLight;
    }
}
