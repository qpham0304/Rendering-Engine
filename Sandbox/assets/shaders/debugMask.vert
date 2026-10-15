#version 460

// highlight mask, geometry is cached in body space so only the transform changes per frame
layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragWorldPos;

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

layout (push_constant) uniform PushConstant {
    mat4 model;
    float kind;
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    fragWorldPos = worldPos.xyz;
}
