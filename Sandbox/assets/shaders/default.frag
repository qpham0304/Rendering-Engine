#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragLightPos;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D albedoMaps;
layout(set = 1, binding = 1) uniform sampler2D normalMaps;
layout(set = 1, binding = 2) uniform sampler2D metalnessMaps;
layout(set = 1, binding = 3) uniform sampler2D roughnessMaps;
layout(set = 1, binding = 4) uniform sampler2D aoMaps;
layout(set = 1, binding = 5) uniform sampler2D emissiveMaps;

layout(push_constant) uniform PushConstantData {
    vec3 color;
    vec3 range;
    bool flag;
    float data;
} pushConstantData;

struct Light {
    vec4 color;
    int modelIndex;
    float intensity;
};

layout(set = 0, binding = 2, std430) readonly buffer LightSSBO {
    Light lights[];
} lightData;


void main() {
    // vec4 L = vec4(0.0, 0.0, 1.0, 1.0);
    
    // if (pushConstantData.flag) {
    //     outColor = texture(albedoMaps, fragTexCoord);
    // } else {
    //     outColor = texture(normalMaps, fragTexCoord);
    // }
    // outColor = vec4(pow(outColor.xyz, vec3(1.0f/2.2f)), 1.0f);
    
    vec4 albedo = texture(albedoMaps, fragTexCoord);
    vec3 totalLighting = vec3(0.0);

    for(int i = 0; i < 2; i++) {
        vec3 color = lightData.lights[i].color.rgb;
        float intensity = lightData.lights[i].intensity;

        int mIdx = lightData.lights[i].modelIndex;
        vec3 worldPos = fragLightPos.xyz; 

        float d = distance(worldPos, fragWorldPos);
        totalLighting += color * (intensity / (d * d + 1.0));
    }
    
    
    if (pushConstantData.flag) {
        outColor = vec4(albedo.rgb * totalLighting, albedo.a);
    } else {
        outColor = texture(normalMaps, fragTexCoord);
    }
    
    outColor = vec4(pow(outColor.xyz, vec3(1.0f/2.2f)), 1.0f);
}