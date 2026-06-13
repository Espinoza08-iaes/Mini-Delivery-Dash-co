#version 330 core

out vec4 FragColor;

in vec3 crntPos;
in vec3 Normal;
in vec3 color;
in vec2 texCoord;

uniform sampler2D diffuse0;
uniform vec3 uZoneColor;
uniform float uTime;

void main()
{
    // Distance from center (assuming zone is at origin in local space)
    float dist = length(crntPos.xz);
    
    // Pulsing wave effect
    float waveSpeed = 2.0;
    float waveFrequency = 3.0;
    float wave = sin(dist * waveFrequency - uTime * waveSpeed);
    
    // Make wave a thin ring that expands
    float waveRing = smoothstep(0.8, 1.0, wave) * smoothstep(1.0, 0.8, wave);
    
    // Base color with pulsing alpha
    vec3 baseColor = uZoneColor;
    float alpha = 0.6 + 0.4 * waveRing;
    
    // Add emissive glow
    vec3 emissive = baseColor * 0.5 * (1.0 + 0.5 * waveRing);
    
    // Final color
    vec3 finalColor = baseColor + emissive;
    
    FragColor = vec4(finalColor, alpha);
}