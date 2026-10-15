#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 6) in ivec4 inBoneIDs;
layout(location = 7) in vec4 inBoneWeights;

layout(push_constant) uniform MeshData {
    mat4 model;
    mat4 lightMVP;
    BoneMatrixArrayTable boneTableRef;
    uint boneMatricesIdx;       // 0: not skinned
} push;

void main() {
    mat4 skinMatrix;
    if(push.boneMatricesIdx == 0) {
        skinMatrix = mat4(1.0);
    } else {
        BoneModelBuffer modelBones = push.boneTableRef.ptrs[push.boneMatricesIdx];
        skinMatrix  = (inBoneIDs.x >= 0 ? modelBones.boneMatrices[inBoneIDs.x] * inBoneWeights.x : mat4(0.0))
                    + (inBoneIDs.y >= 0 ? modelBones.boneMatrices[inBoneIDs.y] * inBoneWeights.y : mat4(0.0))
                    + (inBoneIDs.z >= 0 ? modelBones.boneMatrices[inBoneIDs.z] * inBoneWeights.z : mat4(0.0))
                    + (inBoneIDs.w >= 0 ? modelBones.boneMatrices[inBoneIDs.w] * inBoneWeights.w : mat4(0.0));
    }

    gl_Position = push.lightMVP * push.model * skinMatrix * vec4(inPosition, 1.0);
}
