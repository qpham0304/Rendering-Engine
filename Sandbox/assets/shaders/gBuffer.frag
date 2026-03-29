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

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;
layout(set = 1, binding = 4) uniform sampler2D aoMap;
layout(set = 1, binding = 5) uniform sampler2D emissiveMaps;

// vec3 getNormalFromMap() {
//     vec3 tangentNormal = texture(normalMap, inTexCoord).rgb * 2.0 - 1.0;

//     vec3 N = normalize(inNormal); // Geometric Normal
//     vec3 T = normalize(inTBN[0] - dot(inTBN[0], N) * N);
//     vec3 B = cross(N, T);
//     mat3 TBN = mat3(T, B, N);
    
//     vec3 worldNormal = normalize(TBN * tangentNormal);

//     vec3 geomNormal = normalize(inNormal);
//     if (dot(worldNormal, geomNormal) < 0.0) {
//         worldNormal = normalize(worldNormal - geomNormal * dot(worldNormal, geomNormal));
//     }

//     return worldNormal;
// }

vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(normalMap, inTexCoord).xyz * 2.0 - 1.0;

    vec3 N = normalize(inTBN[2]);
    vec3 T = normalize(inTBN[0]);
    // Re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    // Reconstruct B
    vec3 B = cross(N, T);

    mat3 tbn = mat3(T, B, N);
    return normalize(tbn * tangentNormal);
}

void main() {
    outPos = vec4(inWorldPos, 1.0);
    
    vec3 N = getNormalFromMap();
    outNorm = vec4(N, 1.0);
    // outNorm = vec4(inNormal, 1.0);
    
    outAlbedo = texture(albedoMap, inTexCoord);
    
    // Sampling PBR maps
    float ao        = texture(aoMap, inTexCoord).r;
    float roughness = texture(roughnessMap, inTexCoord).g;
    float metallic  = texture(metallicMap, inTexCoord).b;
    outPBR = vec4(ao, roughness, metallic, 1.0);
}