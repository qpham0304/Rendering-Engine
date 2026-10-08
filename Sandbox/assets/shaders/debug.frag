#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 0) out vec4 outColor;

#include "common/buffers.glsl"

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

layout(set = 1, binding = 0) uniform sampler2D globalTextures[];

layout (push_constant) uniform PushConstant {
    int index;
} pc;

void main() {
    outColor = vec4(1.0, 1.0, 0.0, 1.0);
}