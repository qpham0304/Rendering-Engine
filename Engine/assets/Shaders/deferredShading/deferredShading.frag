#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssaoTex;

struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
    float Radius;
};
const int NR_LIGHTS = 200;
uniform Light lights[NR_LIGHTS];
uniform vec3 viewPos;
uniform float intensity;
uniform mat4 view;

void main()
{             
    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;
    float SSAO = texture(ssaoTex, TexCoords).r;

    vec4 worldSpaceNormal = texture(gNormal, TexCoords);
    vec3 viewSpaceNormal = normalize(mat3(view) * worldSpaceNormal.xyz);
    float Metallic = normalize(worldSpaceNormal.a); //reflection mask in alpha channel
    vec4 worldSpacePosition = texture(gPosition, TexCoords);
    vec3 viewSpacePosition = vec3(view * vec4(texture(gPosition, TexCoords).xyz, 1.0));
    FragPos = viewSpacePosition;
    Normal = viewSpaceNormal;

    // then calculate lighting as usual
    vec3 ambient = vec3(0.3f * Diffuse * SSAO);    
    vec3 lighting = ambient;
    // vec3 lighting = Diffuse * 0.1; // TODO: overwrite for SSR demo for now, remove when done
    vec3 viewDir  = normalize(FragPos);
    for(int i = 0; i < NR_LIGHTS; ++i)
    {
        // calculate distance between light source and current fragment
        float distance = length(lights[i].Position - FragPos);
        if(distance < lights[i].Radius)
        {
            // diffuse
            vec3 lightDir = normalize(lights[i].Position - FragPos);
            vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * lights[i].Color;
            // specular
            vec3 halfwayDir = normalize(lightDir + viewDir);  
            float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
            vec3 specular = lights[i].Color * spec * Specular;
            // attenuation
            float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
            attenuation *= intensity;
            diffuse *= attenuation;
            specular *= attenuation;
            lighting += diffuse + specular;
        }
    }
    FragColor = vec4(lighting, 1.0);
}
