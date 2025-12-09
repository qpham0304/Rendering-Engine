#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gSpecular;
uniform sampler2D depthMap;
uniform sampler2D ssaoTex;

uniform float intensity = 15.0f;

struct Light {
    vec3 Position;
    vec3 Color;
    
    float Linear;
    float Quadratic;
    float Radius;
};

const int NR_LIGHTS = 200;
uniform Light lights[NR_LIGHTS];
uniform mat4 invProjection;
uniform mat4 invView;

vec3 getViewSpacePosition(float z) {
    vec2 posCanonical  = TexCoords * 2 - 1.0; //position in Canonical View Volume
	vec4 posView = invProjection * vec4(posCanonical, z , 1.0);
	posView /= posView.w;
	return posView.xyz;
}

void main()
{             
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    Normal = mat3(invView) * Normal;

    vec3 Diffuse = texture(gAlbedo, TexCoords).rgb;
    float Specular = texture(gAlbedo, TexCoords).a;
    float SSAO = texture(ssaoTex, TexCoords).r;

	float depth = texture(depthMap, TexCoords).r * 2 - 1;
	vec3 viewSpacePosition = getViewSpacePosition(depth);

    float Metallic = normalize(texture(gNormal, TexCoords).a); //reflection mask in alpha channel

    vec3 ambient = vec3(0.2f * Diffuse * SSAO);    
    vec3 lighting = ambient * 1;
    // vec3 viewDir  = normalize(FragPos);
    for(int i = 0; i < NR_LIGHTS; ++i)
    {
    //     // calculate distance between light source and current fragment
        float distance = length(lights[i].Position - viewSpacePosition);
        vec3 lightDir = normalize(lights[i].Position - viewSpacePosition);
        vec3 view = normalize(-viewSpacePosition);
        vec3 halfwayDir = normalize(lightDir + view);  

        vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * lights[i].Color;
        float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
        vec3 specular = lights[i].Color * spec * Specular;
        // attenuation
        float attenuation = 1.0 / (1.0 + lights[i].Linear * distance + lights[i].Quadratic * distance * distance);
        attenuation *= intensity;
        lighting += (diffuse + specular) * attenuation;
    //     if(distance < lights[i].Radius)
    //     {

    //     }
    }
    FragColor = vec4(lighting, 1.0);
}
