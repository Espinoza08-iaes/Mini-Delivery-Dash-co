#version 330 core
out vec4 FragColor;

in vec2 texCoord;
in vec4 color;

uniform int uType; // 0 = Smoke, 1 = Rain, 2 = Leaf

void main()
{
    if (uType == 0) // Smoke
    {
        // Radial soft gradient
        float dist = length(texCoord - vec2(0.5));
        float alpha = smoothstep(0.5, 0.0, dist);
        if (alpha <= 0.0) discard;
        FragColor = vec4(color.rgb, color.a * alpha);
    }
    else if (uType == 1) // Rain
    {
        // Elongated thin capsule
        float dx = texCoord.x - 0.5;
        float dy = texCoord.y - 0.5;
        // make it thin: x width is small
        if (abs(dx) > 0.08) discard;
        float alpha = smoothstep(0.5, 0.3, abs(dy)) * smoothstep(0.08, 0.0, abs(dx));
        if (alpha <= 0.0) discard;
        FragColor = vec4(color.rgb, color.a * alpha * 0.7);
    }
    else if (uType == 2) // Leaf
    {
        // Leaf shape using trigonometric curves
        vec2 p = texCoord - vec2(0.5);
        float r = length(p);
        if (r > 0.5) discard;
        
        float angle = atan(p.y, p.x);
        // Leaf boundary equation: slightly elongated along main axis and pinched at ends
        float leafShape = 0.45 * (1.0 + 0.35 * cos(angle * 2.0));
        
        if (r > leafShape) discard;
        
        // Simple leaf shading: center vein
        float vein = smoothstep(0.02, 0.0, abs(p.y - 0.1 * p.x));
        vec3 finalColor = mix(color.rgb, color.rgb * 0.6, vein);
        
        FragColor = vec4(finalColor, color.a);
    }
    else
    {
        FragColor = color;
    }
}
