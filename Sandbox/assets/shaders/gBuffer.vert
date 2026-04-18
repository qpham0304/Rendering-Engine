#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;

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

layout(set = 0, binding = 2, std430) readonly buffer PreviousModels {
    mat4 models[];
} prevSsbo;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out mat3 outTBN;       // mat3 takes locations 3, 4, 5
layout(location = 6) out vec4 outCurNDC;
layout(location = 7) out vec4 outPrevNDC;

void main() {
    mat4 modelMatrix = ssbo.models[gl_InstanceIndex];
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 B = normalize(normalMatrix * inBiTangent);
    vec3 N = normalize(normalMatrix * inNormal);
    outTBN = mat3(T, B, N);
    outNormal = N;
    outTexCoord = inTexCoord;

    gl_Position = ubo.proj * ubo.view * worldPos;

    mat4 prevModelMatrix = prevSsbo.models[gl_InstanceIndex]; 
    vec4 prevWorldPos = prevModelMatrix * vec4(inPosition, 1.0);
    outCurNDC = gl_Position;
    outPrevNDC = ubo.prevViewProj * prevWorldPos;
}