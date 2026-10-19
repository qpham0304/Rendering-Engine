#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;
layout(location = 6) in ivec4 inBoneIDs;
layout(location = 7) in vec4 inBoneWeights;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBitangent;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
    mat4 prevViewProj;
    mat4 proj;
    vec4 cameraPos;
    mat4 invView;
    mat4 invProj;
    float width;
    float height;
} ubo;


layout(set = 0, binding = 1, std430) readonly buffer StorageBufferObject {
    mat4 models[];
} ssbo;

layout(buffer_reference, scalar) buffer Materials{  Material m[]; };

layout(push_constant) uniform PushConstant {
    Materials materialsRef;
    float bias;
    float time;
    mat4 sunlightMVP;
    vec4 direction;
    // vec4 color;
    float numLights;
    uint materialIdx;
    uint boneMatricesIdx;
    BoneMatrixArrayTable boneTableRef;
} pc;

void main() {
    BoneModelBuffer modelBones = pc.boneTableRef.ptrs[pc.boneMatricesIdx];

    mat4 skinMatrix;
    if(pc.boneMatricesIdx == 0) {
        skinMatrix = mat4(1.0);
    } else {
        skinMatrix  = (inBoneIDs.x >= 0 ? modelBones.boneMatrices[inBoneIDs.x] * inBoneWeights.x : mat4(0.0))
                    + (inBoneIDs.y >= 0 ? modelBones.boneMatrices[inBoneIDs.y] * inBoneWeights.y : mat4(0.0))
                    + (inBoneIDs.z >= 0 ? modelBones.boneMatrices[inBoneIDs.z] * inBoneWeights.z : mat4(0.0))
                    + (inBoneIDs.w >= 0 ? modelBones.boneMatrices[inBoneIDs.w] * inBoneWeights.w : mat4(0.0));
    }

    vec4 localPosition = skinMatrix * vec4(inPosition, 1.0);
    mat4 modelMatrix = ssbo.models[gl_InstanceIndex];
    vec4 worldPos = modelMatrix * localPosition;
    gl_Position = ubo.proj * ubo.view * worldPos;

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix * skinMatrix)));

    outNormal = normalize(normalMatrix * inNormal);
    outTangent   = normalize(normalMatrix * inTangent.xyz);
    outBitangent = normalize(normalMatrix * inBitangent.xyz);

    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
}