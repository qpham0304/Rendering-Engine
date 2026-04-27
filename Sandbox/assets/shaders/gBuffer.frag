#version 460

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in mat3 inTBN;       // mat3 takes locations 3, 4, 5
layout(location = 6) in vec4 outCurNDC ;
layout(location = 7) in vec4 outPrevNDC;

layout(location = 0) out vec4 outPos;    
layout(location = 1) out vec4 outNorm;   
layout(location = 2) out vec4 outAlbedo; 
layout(location = 3) out vec4 outPBR;
layout(location = 4) out vec4 outEmissive;
layout(location = 5) out vec4 outMotion;


layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

layout(set = 2, binding = 0)  uniform MaterialUniform {
    int materialIdx;
    vec2 uv;
    vec4 albedo;
    vec4 normal;
    float metallic;
    float roughness;
    float ao;
    float emissive;
} material;

layout(buffer_reference, std430) readonly buffer MaterialBlock { 
    uint albedoIdx;
    uint normalIdx;
    uint metalnessIdx;
    uint roughnessIdx;
    uint aoIdx;
    uint emissiveIdx;
};

layout(push_constant) uniform PushConstant {
    MaterialBlock ref;
} pc;


vec3 getNormalFromMap() {
    vec3 texNormal = texture(samplerImages[pc.ref.normalIdx], inTexCoord).xyz * 2.0 - 1.0;

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
    
    outAlbedo = texture(samplerImages[pc.ref.albedoIdx], inTexCoord + material.uv) * material.albedo;
    
    float ao        = texture(samplerImages[pc.ref.aoIdx], inTexCoord + material.uv).r        * material.ao;
    float roughness = texture(samplerImages[pc.ref.roughnessIdx], inTexCoord + material.uv).g * material.roughness;
    float metallic  = texture(samplerImages[pc.ref.metalnessIdx], inTexCoord + material.uv).b  * material.metallic;
    outPBR = vec4(ao, roughness, metallic, 1.0);

    outEmissive = texture(samplerImages[pc.ref.emissiveIdx], inTexCoord + material.uv) * material.emissive;

    vec2 curNDC = outCurNDC.xy / outCurNDC.w;
    vec2 prevNDC = outPrevNDC.xy / outPrevNDC.w;

    vec2 curUV = curNDC * 0.5 + 0.5;
    vec2 prevUV = prevNDC * 0.5 + 0.5;
    vec2 velocity = curUV - prevUV;
    
    // don't accumulate noise if there's no history available to reject invalid edges
    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        outMotion = vec4(0.0, 0.0, 0.0, 0.0); 
    } else {
        outMotion = vec4(velocity, 0.0, 1.0);
    }
}