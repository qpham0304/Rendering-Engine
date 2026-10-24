#include "ParticleContainer.h"

ParticleContainer::ParticleContainer(uint32_t size) : m_size(size)
{
	setFootprint("ParticleContainer", sizeof(ParticleContainer), MemorySystem::Particle);
}
