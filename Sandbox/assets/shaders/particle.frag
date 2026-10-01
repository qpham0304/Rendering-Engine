#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) out vec4 outColor;

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

struct Container {
    uint64_t lifetimeRef;
    uint64_t positionsRef;
    uint64_t scalesRef;
    uint64_t velocitiesRef;
    uint64_t colorsRef;
};

layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

layout(buffer_reference, scalar) buffer ContainerBuffers { Container containers[]; };
layout(buffer_reference, scalar) buffer Lifetime { float lifetime[]; };
layout(buffer_reference, scalar) buffer Positions { vec3 positions[]; };
layout(buffer_reference, scalar) buffer Scales { vec3 scales[]; };
layout(buffer_reference, scalar) buffer Velocities { vec3 velocities[]; };
layout(buffer_reference, scalar) buffer Colors { vec4 colors[]; };

layout(push_constant) uniform ParticleContainerRefs {
    ContainerBuffers containersRef;
    uint containerIdx;
    uint particleCount;
    float deltaTime;
} pc;

void main() {
    outColor = vec4(1.0, 1.0, 0.5, 1.0);
}