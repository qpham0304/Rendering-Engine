#version 460

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec2 inNormal;
layout(location = 4) in vec2 inTangent;
layout(location = 5) in vec2 inBitangent;
// layout(location = 6) in vec2 inBoneIDs;
// layout(location = 7) in vec2 inBoneWeights;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;


layout(set = 0, binding = 1, std430) readonly buffer StorageBufferObject {
    mat4 models[];
} ssbo;

// uniform mat4 finalBonesMatrices[MAX_BONES];
// layout(set = 0, binding = 1) uniform finalBonesMatrices[MAX_BONES];

void main() {
    gl_Position = ubo.proj * ubo.view * ssbo.models[gl_InstanceIndex] * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}