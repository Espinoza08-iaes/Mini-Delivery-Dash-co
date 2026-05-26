#version 330 core

#define NUMBER_OF_POINT_LIGHTS 4

struct Material
{
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct DirLight
{
    vec3 direction;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight
{
    vec3 position;
    
    float constant;
    float linear;
    float quadratic;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight
{
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    
    float constant;
    float linear;
    float quadratic;
    
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 color;

uniform vec3 viewPos;
uniform DirLight dirLight;
uniform PointLight pointLights[NUMBER_OF_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform Material material;
uniform int renderMode; // Variable enviada desde C++

// Function prototypes
vec3 CalcDirLight( DirLight light, vec3 normal, vec3 viewDir );
vec3 CalcPointLight( PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir );
vec3 CalcSpotLight( SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir );

void main( )
{
    // Properties
    vec3 norm = normalize( Normal );
    vec3 viewDir = normalize( viewPos - FragPos );
    
    vec3 result = vec3(0.0);

    // 1 - Iluminacion Basica (Solo luz de un punto sin importar atenuacion, o direccion global simple)
    if (renderMode == 1) {
        // En Iluminacion basica solemos tener color objeto, ambiental y difuso de una point light (light 0)
        vec3 lightColor = vec3(1.0);
        vec3 objectColor = vec3(1.0, 0.5, 0.31);
        vec3 lightDir = normalize(pointLights[0].position - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 ambient = 0.1 * lightColor;
        vec3 diffuse = diff * lightColor;
        vec3 specular = vec3(0.0); // No especular inicialmente, o lo obviamos
        float spec = pow( max( dot( viewDir, reflect( -lightDir, norm ) ), 0.0 ), 32.0 );
        specular = 0.5 * spec * lightColor;
        result = (ambient + diffuse + specular) * objectColor;
    }
    // 2 - Materiales
    else if (renderMode == 2) {
        // Ignoramos texturas y usamos material puro predefinido
        vec3 lightDir = normalize(pointLights[0].position - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 ambient = pointLights[0].ambient * vec3(1.0, 0.5, 0.31);
        vec3 diffuse = pointLights[0].diffuse * diff * vec3(1.0, 0.5, 0.31);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specular = pointLights[0].specular * spec * vec3(0.5, 0.5, 0.5);
        result = ambient + diffuse + specular;
    }
    // 3 - Mapas de iluminacion
    else if (renderMode == 3) {
        result = CalcPointLight(pointLights[0], norm, FragPos, viewDir);
    }
    // 4 - Luz direccional
    else if (renderMode == 4) {
        result = CalcDirLight(dirLight, norm, viewDir);
    }
    // 5 - Iluminacion de punto (Multiple)
    else if (renderMode == 5) {
        for ( int i = 0; i < NUMBER_OF_POINT_LIGHTS; i++ )
        {
            result += CalcPointLight( pointLights[i], norm, FragPos, viewDir );
        }
    }
    // 6 - Combinaciones completas
    else if (renderMode == 6) {
        // Directional lighting
        result = CalcDirLight( dirLight, norm, viewDir );
        
        // Point lights
        for ( int i = 0; i < NUMBER_OF_POINT_LIGHTS; i++ )
        {
            result += CalcPointLight( pointLights[i], norm, FragPos, viewDir );
        }
        
        // Spot light
        result += CalcSpotLight( spotLight, norm, FragPos, viewDir );
    }
    
    color = vec4( result, 1.0 );
}

// Calculates the color when using a directional light.
vec3 CalcDirLight( DirLight light, vec3 normal, vec3 viewDir )
{
    vec3 lightDir = normalize( -light.direction );
    
    // Diffuse shading
    float diff = max( dot( normal, lightDir ), 0.0 );
    
    // Specular shading
    vec3 reflectDir = reflect( -lightDir, normal );
    float spec = pow( max( dot( viewDir, reflectDir ), 0.0 ), material.shininess );
    
    // Combine results
    vec3 ambient = light.ambient * vec3( texture( material.diffuse, TexCoords ) );
    vec3 diffuse = light.diffuse * diff * vec3( texture( material.diffuse, TexCoords ) );
    vec3 specular = light.specular * spec * vec3( texture( material.specular, TexCoords ) );
    
    return ( ambient + diffuse + specular );
}

// Calculates the color when using a point light.
vec3 CalcPointLight( PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir )
{
    vec3 lightDir = normalize( light.position - fragPos );
    
    // Diffuse shading
    float diff = max( dot( normal, lightDir ), 0.0 );
    
    // Specular shading
    vec3 reflectDir = reflect( -lightDir, normal );
    float spec = pow( max( dot( viewDir, reflectDir ), 0.0 ), material.shininess );
    
    // Attenuation
    float distance = length( light.position - fragPos );
    float attenuation = 1.0f / ( light.constant + light.linear * distance + light.quadratic * ( distance * distance ) );
    
    // Combine results
    vec3 ambient = light.ambient * vec3( texture( material.diffuse, TexCoords ) );
    vec3 diffuse = light.diffuse * diff * vec3( texture( material.diffuse, TexCoords ) );
    vec3 specular = light.specular * spec * vec3( texture( material.specular, TexCoords ) );
    
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    
    return ( ambient + diffuse + specular );
}

// Calculates the color when using a spot light.
vec3 CalcSpotLight( SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir )
{
    vec3 lightDir = normalize( light.position - fragPos );
    
    // Diffuse shading
    float diff = max( dot( normal, lightDir ), 0.0 );
    
    // Specular shading
    vec3 reflectDir = reflect( -lightDir, normal );
    float spec = pow( max( dot( viewDir, reflectDir ), 0.0 ), material.shininess );
    
    // Attenuation
    float distance = length( light.position - fragPos );
    float attenuation = 1.0f / ( light.constant + light.linear * distance + light.quadratic * ( distance * distance ) );
    
    // Spotlight intensity
    float theta = dot( lightDir, normalize( -light.direction ) );
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp( ( theta - light.outerCutOff ) / epsilon, 0.0, 1.0 );
    
    // Combine results
    vec3 ambient = light.ambient * vec3( texture( material.diffuse, TexCoords ) );
    vec3 diffuse = light.diffuse * diff * vec3( texture( material.diffuse, TexCoords ) );
    vec3 specular = light.specular * spec * vec3( texture( material.specular, TexCoords ) );
    
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    
    return ( ambient + diffuse + specular );
}
