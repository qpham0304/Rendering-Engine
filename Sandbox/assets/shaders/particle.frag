#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) flat in int particleID;
layout(location = 1) in vec2 fragTexCoord;

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

layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

struct Emitter {
    int emitMax;
    int emitCount;
    int areRecycled;
    float emitAccumulator;
    float emitRate;
    float lifetimeMin;
    float lifetimeMax;
    float speedMin;
    float speedMax;
    vec3 spawnPosition;
    vec3 force;
    int resetPosition;
    vec2 uvOffset;
    vec2 uvScale;
    int textureID;
    int behaviorType;
};

struct Container {
    uint64_t lifetimeRef;
    uint64_t positionsRef;
    uint64_t scalesRef;
    uint64_t velocitiesRef;
    uint64_t colorsRef;
};

layout(buffer_reference, scalar) buffer EmittersBuffers { Emitter emitters[]; };
layout(buffer_reference, scalar) buffer ContainerBuffers { Container containers[]; };
layout(buffer_reference, scalar) buffer Lifetime { float lifetime[]; };
layout(buffer_reference, scalar) buffer Positions { vec3 positions[]; };
layout(buffer_reference, scalar) buffer Scales { vec3 scales[]; };
layout(buffer_reference, scalar) buffer Velocities { vec3 velocities[]; };
layout(buffer_reference, scalar) buffer Colors { vec4 colors[]; };

layout(push_constant) uniform ParticleContainerRefs {
    EmittersBuffers emittersRef;
    ContainerBuffers containersRef;
    uint containerIdx;
    uint particleCount;
    float deltaTime;
} pc;

void main() {
    Emitter emitter = pc.emittersRef.emitters[pc.containerIdx];
    Container c = pc.containersRef.containers[pc.containerIdx];
    Colors colorBuffer = Colors(c.colorsRef);
    vec4 color = colorBuffer.colors[pc.containerIdx];

    //TODO: kinda work but slow, better generate in the vertex shader
    if(emitter.textureID != 0) {
        vec2 frameUV = (fragTexCoord * emitter.uvScale) + emitter.uvOffset;
        if (any(lessThan(emitter.uvScale, vec2(1.0 - 1e-4)))) { //treats binlinear filtering
            vec2 texSize = vec2(textureSize(samplerImages[emitter.textureID], 0));
            vec2 halfTexel = 0.5 / texSize;

            vec2 cellMin = emitter.uvOffset + halfTexel;
            vec2 cellMax = emitter.uvOffset + emitter.uvScale - halfTexel;
            frameUV = clamp(frameUV, cellMin, cellMax);
        }

        vec4 texture = texture(samplerImages[emitter.textureID], frameUV);
        outColor = texture;
        outColor.a = color.a;
        return;
    }

    outColor = color;
}