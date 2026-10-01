#include "ParticleContainer.h"

ParticleContainer::ParticleContainer(uint32_t size) : m_size(size)
{
    _allocate(size);
}

ParticleContainer::ParticleContainer(uint32_t size, uint32_t rows, uint32_t cols)
    : m_size(size), m_numRows(rows), m_numCols(cols)
{
    _allocate(size);
}

void ParticleContainer::_allocate(uint32_t size)
{
    m_lifetime.resize(size, 1.0);
    m_positions.resize(size, glm::vec3(1.0));
    m_scales.resize(size, glm::vec3(0.5));
    m_velocities.resize(size, glm::vec3(2.5));
    m_colors.resize(size, glm::vec4(1.0, 0.5, 1.0, 1.0));
}
