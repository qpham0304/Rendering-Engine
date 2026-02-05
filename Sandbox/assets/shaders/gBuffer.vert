#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(set = 0, binding = 1, std430) readonly buffer StorageBufferObject {
    mat4 models[];
} ssbo;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;

void main() {
    mat4 modelMatrix = ssbo.models[gl_InstanceIndex];
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // Pass normal in world space (or view space, just be consistent)
    // Note: use a normal matrix if you have non-uniform scaling
    outWorldPos = worldPos.xyz;
    outNormal = mat3(modelMatrix) * inNormal;
    outTexCoord = inTexCoord;
}