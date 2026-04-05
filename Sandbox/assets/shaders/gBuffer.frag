#version 460
#extension GL_EXT_vulkan_glsl : enable

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in mat3 inTBN;

layout(location = 0) out vec4 outPos;    
layout(location = 1) out vec4 outNorm;   
layout(location = 2) out vec4 outAlbedo; 
layout(location = 3) out vec4 outPBR;
layout(location = 4) out vec4 outEmissive;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;
layout(set = 1, binding = 4) uniform sampler2D aoMap;
layout(set = 1, binding = 5) uniform sampler2D emissiveMap;

layout(set = 1, binding = 6)  uniform MaterialUniform {
    int materialIdx;
    vec2 uv;
    vec4 albedo;
    vec4 normal;
    float metallic;
    float roughness;
    float ao;
    float emissive;
} material;

vec3 getNormalFromMap() {
    vec3 texNormal = texture(normalMap, inTexCoord).xyz * 2.0 - 1.0;

    float ripple = sin(inWorldPos.x * 5.0 + inWorldPos.z * 5.0 + material.emissive * 10.0); 
    
    vec3 animatedNormal = vec3(
        texNormal.xy + (material.normal.xy * ripple), 
        texNormal.z
    );

    return normalize(inTBN * normalize(animatedNormal));
}

void main() {
    outPos = vec4(inWorldPos, 1.0);
    
    vec3 N = getNormalFromMap();
    outNorm = vec4(N, 1.0);
    
    outAlbedo = texture(albedoMap, inTexCoord + material.uv) * material.albedo;
    
    float ao        = texture(aoMap, inTexCoord + material.uv).r        * material.ao;
    float roughness = texture(roughnessMap, inTexCoord + material.uv).g * material.roughness;
    float metallic  = texture(metallicMap, inTexCoord + material.uv).b  * material.metallic;
    outPBR = vec4(ao, roughness, metallic, 1.0);

    outEmissive = texture(emissiveMap, inTexCoord + material.uv) * material.emissive;
}