#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common/buffers.glsl"

struct Container {
    uint64_t lifetimeRef;
    uint64_t positionsRef;
    uint64_t scalesRef;
    uint64_t velocitiesRef;
    uint64_t colorsRef;
};

layout(buffer_reference, scalar) buffer ContainerBuffers { Container containers[]; };
layout(buffer_reference, scalar) buffer Lifetime { float lifetime[]; };
layout(buffer_reference, scalar) buffer Positions { vec3 positions[]; };
layout(buffer_reference, scalar) buffer Scales { vec3 scales[]; };
layout(buffer_reference, scalar) buffer Velocities { vec3 velocities[]; };
layout(buffer_reference, scalar) buffer Colors { vec4 colors[]; };

layout(push_constant) uniform ParticleContainerRefs {
    ContainerBuffers containersRef;
    uint containerIdx; // The index for THIS draw call
    uint particleCount;
    float deltaTime;
} pc;


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

layout(set = 0, binding = 1) uniform EmitterUBO {
    int emitMax;
    int emitCount;
    bool areRecycled;
    float emitAccumulator;
    float emitRate;
    float lifetimeMin;
    float lifetimeMax;
    float speedMin;
    float speedMax;
    vec3 spawnPosition;
    vec3 force;
    bool resetPosition;
} emitter;

void main() {
    const vec2 localOffsets[6] = vec2[](
        vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
        vec2(-0.5, -0.5), vec2(0.5, 0.5),  vec2(-0.5, 0.5)
    );
    
    //gl_VertexIndex defined from vkCmdDraw
    uint particleID = gl_VertexIndex / 6;
    uint vertexInQuad = gl_VertexIndex % 6;

    Container c = pc.containersRef.containers[pc.containerIdx];
    Positions posBuffer = Positions(c.positionsRef);
    vec2 offset = localOffsets[vertexInQuad] * 0.1;
    vec3 particlePos = posBuffer.positions[particleID];

    // vec4 worldPos = vec4(particlePos + vec3(offset, 0.0), 1.0);
    // gl_Position = ubo.proj * ubo.view * worldPos;

    // camera facing billboard
    vec4 viewCenter = ubo.view * vec4(particlePos, 1.0);
    vec4 viewPos = viewCenter + vec4(offset, 0.0, 0.0);
    gl_Position = ubo.proj * viewPos;

}