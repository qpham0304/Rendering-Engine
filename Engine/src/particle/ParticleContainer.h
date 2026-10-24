#pragma once

#include <vector>
#include <cstdint>
#include "core/features/MathIncludes.h"
#include "core/features/Object.h"

class ParticleManager;

// TODO: one particle on the gpu, mirrored by Particle in shaders/common/particles.glsl, make a share struct for this
struct ParticleGPU {
    glm::vec3 position { 0.0f };
    float age { 0.0f };
    glm::vec3 velocity { 0.0f };
    float lifetime { 0.0f };
    glm::vec3 axis { 0.0f, 1.0f, 0.0f };    // rotation axis for oriented quads and meshes
    float rotation { 0.0f };
    float angularVelocity { 0.0f };
    float size { 0.0f };
    uint32_t seed { 0 };
    uint32_t alive { 0 };
};
static_assert(sizeof(ParticleGPU) == 64, "ParticleGPU must match Particle in particles.glsl");

// one drawn instance, mirrored by DrawEntry in particles.glsl, the list is sorted back to front for alpha blending
struct ParticleDrawEntry {
    float sortKey { 0.0f };
    uint32_t particleID { 0 };
};

// the bitonic sort needs a power of two list
inline uint32_t particleSortSize(uint32_t capacity) {
    uint32_t size = 1;
    while(size < capacity) {
        size <<= 1;
    }
    return size;
}

// dead list stack top, popped by the emit pass and pushed by the simulate pass
struct ParticleCounters {
    int32_t deadCount { 0 };
    uint32_t pad[3] {};
};

class ParticleContainer : public Object
{
public:
    ParticleContainer() { setFootprint("ParticleContainer", sizeof(ParticleContainer), MemorySystem::Particle); };
	ParticleContainer(uint32_t size);

    uint32_t m_size { 0 };  // capacity, particles live entirely on the gpu

private:
    friend class ParticleManager;
};
