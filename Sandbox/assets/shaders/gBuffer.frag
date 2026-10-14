#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

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


layout(buffer_reference, scalar) buffer BoneModelBuffer {
    mat4 boneMatrices[];
};

layout(buffer_reference, scalar) buffer BoneMatrixArrayTable {
    BoneModelBuffer ptrs[];
};

layout(set = 1, binding = 0) uniform sampler2D samplerImages[];


layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
    uint objIdx;
    uint boneMatricesIdx;
} pc;


vec3 getNormalFromMap(vec4 normalMap, Material mat) {
    vec3 texNormal = normalMap.xyz * 2.0 - 1.0;

    float ripple = sin(inWorldPos.x * 5.0 + inWorldPos.z * 5.0 + mat.emissiveFactor * 10.0); 
    
    vec3 animatedNormal = vec3(
        texNormal.xy + (mat.normalFactor.xy * ripple), 
        texNormal.z
    );

    return normalize(inTBN * normalize(animatedNormal));
}

void main() {
    Object obj = pc.objRef.objects[pc.objIdx];

    MaterialsBuffer materials = MaterialsBuffer(obj.materialsRef);
    MatIndicesBuffer matIndices = MatIndicesBuffer(obj.materialIndicesRef);
    uint matID = matIndices.i[gl_PrimitiveID];
    Material mat = materials.m[matID];

    outPos = vec4(inWorldPos, 1.0);
    
    vec4 normalMap = texture(samplerImages[mat.normalIdx], inTexCoord + mat.uvOffset);
    vec3 N = getNormalFromMap(normalMap, mat);
    outNorm = vec4(N, 1.0);
    
    outAlbedo = texture(samplerImages[mat.albedoIdx], inTexCoord + mat.uvOffset) * mat.albedoFactor;
    
    float ao        = texture(samplerImages[mat.aoIdx], inTexCoord + mat.uvOffset).r        * mat.aoFactor;
    float roughness = texture(samplerImages[mat.roughnessIdx], inTexCoord + mat.uvOffset).g * mat.roughnessFactor;
    float metallic  = texture(samplerImages[mat.metalnessIdx], inTexCoord + mat.uvOffset).b * mat.metallicFactor;
    outPBR = vec4(ao, roughness, metallic, 1.0);

    outEmissive = texture(samplerImages[mat.emissiveIdx], inTexCoord + mat.uvOffset) * mat.emissiveFactor;

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