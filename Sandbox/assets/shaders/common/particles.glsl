#ifndef COMMON_PARTICLES_GLSL
#define COMMON_PARTICLES_GLSL

#extension GL_GOOGLE_include_directive : require

#include "buffers.glsl"

// ParticleSpawnShape in MComponent.h
#define SPAWN_POINT         0
#define SPAWN_BOX           1
#define SPAWN_SPHERE        2
#define SPAWN_HEMISPHERE    3
#define SPAWN_CONE          4
#define SPAWN_CIRCLE        5

// ParticleRenderMode in MComponent.h
#define RENDER_BILLBOARD    0   // camera facing quad
#define RENDER_AXIS_Y       1   // faces the camera but stays upright
#define RENDER_VELOCITY     2   // stretched along the velocity
#define RENDER_HORIZONTAL   3   // flat on the ground plane
#define RENDER_ORIENTED     4   // fixed to the emitter axes, spun by the particle rotation
#define RENDER_MESH         5

#define STAGE_EMIT      0
#define STAGE_SIMULATE  1
#define STAGE_SORT_PAD  2   // fills the unused tail of the draw list so it sorts to the end
#define STAGE_SORT      3   // one compare and swap step of the bitonic sort

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

// mirrored by EmitterGPU in ParticleRendererVulkan.h, angles are already in radians
struct Emitter {
    mat4 transform;
    vec4 colorStart;
    vec4 colorEnd;
    vec3 shapeExtents;      float shapeRadius;
    vec3 direction;         float spreadAngle;
    vec3 force;             float gravityScale;
    float shapeConeAngle;
    float speedMin;
    float speedMax;
    float lifetimeMin;
    float lifetimeMax;
    float drag;
    float turbulenceStrength;
    float turbulenceFrequency;
    float vortexStrength;
    float attractStrength;
    float sizeMin;
    float sizeMax;
    float sizeEndScale;
    float rotationMin;
    float rotationMax;
    float angularVelocityMin;
    float angularVelocityMax;
    float fadeIn;
    float fadeOut;
    float stretchFactor;
    float flipbookFPS;
    float meshShading;
    int spawnShape;
    int velocityFromShape;
    int localSpace;
    int renderMode;
    int textureID;
    int flipbookRows;
    int flipbookCols;
    int emitFromShell;
    uint64_t meshVerticesRef;
    uint64_t meshIndicesRef;
};

// mirrored by ParticleGPU in ParticleContainer.h
struct Particle {
    vec3 position;  float age;
    vec3 velocity;  float lifetime;
    vec3 axis;      float rotation;
    float angularVelocity;
    float size;
    uint seed;
    uint alive;
};

struct Container {
    uint64_t particlesRef;
    uint64_t aliveListRef;
    uint64_t deadListRef;
    uint64_t countersRef;
};

// mirrored by ParticleDrawEntry in ParticleContainer.h
struct DrawEntry {
    float sortKey;      // ascending order draws back to front
    uint particleID;
};

// VkDrawIndirectCommand
struct DrawCommand {
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

layout(buffer_reference, scalar) buffer EmittersBuffer { Emitter emitters[]; };
layout(buffer_reference, scalar) buffer ContainersBuffer { Container containers[]; };
layout(buffer_reference, scalar) buffer ParticlesBuffer { Particle p[]; };
layout(buffer_reference, scalar) buffer IndexList { uint i[]; };
layout(buffer_reference, scalar) buffer DrawList { DrawEntry e[]; };
layout(buffer_reference, scalar) buffer Counters { int deadCount; };
layout(buffer_reference, scalar) buffer DrawCommands { DrawCommand cmds[]; };

layout(push_constant) uniform ParticlePushConstant {
    EmittersBuffer emittersRef;
    ContainersBuffer containersRef;
    DrawCommands drawCommandsRef;
    uint containerIdx;
    uint particleCount;     // spawn count while emitting, capacity while simulating, list size while sorting
    float deltaTime;
    float time;
    uint frameSeed;
    uint stage;
    uint sortBlock;         // bitonic sequence size k
    uint sortStride;        // compare distance j
    uint burstPad0;
    uint burstPad1;
    vec4 burstPosition;     // w > 0 while emitting a positioned burst, world space
    vec4 burstDirection;    // zero keeps the emitter direction
} pc;

uint pcgHash(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand01(inout uint seed) {
    seed = pcgHash(seed);
    return float(seed) / 4294967295.0;
}

vec3 rotateAxisAngle(vec3 v, vec3 axis, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.0 - c);
}

#endif
