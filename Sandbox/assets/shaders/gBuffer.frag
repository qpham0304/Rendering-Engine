#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in mat3 inTBN;

layout(location = 0) out vec4 outPos;    
layout(location = 1) out vec4 outNorm;   
layout(location = 2) out vec4 outAlbedo; 
layout(location = 3) out vec4 outPBR;    

layout(set = 1, binding = 0) uniform sampler2D albedoSampler;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;
layout(set = 1, binding = 4) uniform sampler2D aoMap;

void main() {
    outPos = vec4(inWorldPos, 1.0);
    
    vec3 normal = texture(normalMap, inTexCoord).rgb;
    normal = normalize(normal * 2.0 - 1.0);
    vec3 worldNormal = normalize(inTBN * normal);
    
    outNorm = vec4(worldNormal, 1.0);
    
    outAlbedo = texture(albedoSampler, inTexCoord);
    
    float ao        = texture(aoMap, inTexCoord).r;
    float roughness = texture(roughnessMap, inTexCoord).r;
    float metallic  = texture(metallicMap, inTexCoord).r;
    outPBR = vec4(ao, roughness, metallic, 1.0);
}