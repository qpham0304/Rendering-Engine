#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;
// layout(location = 6) in vec2 inBoneIDs;
// layout(location = 7) in vec2 inBoneWeights;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBitangent;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
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


// uniform mat4 finalBonesMatrices[MAX_BONES];
// layout(set = 0, binding = 1) uniform finalBonesMatrices[MAX_BONES];

void main() {
    mat4 modelMatrix = ssbo.models[gl_InstanceIndex];
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));

    outNormal = normalize(normalMatrix * inNormal);
    outTangent   = normalize(normalMatrix * inTangent.xyz);
    outBitangent = normalize(normalMatrix * inBitangent.xyz);

    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragWorldPos = worldPos.xyz;
}