#include "ParticleManager.h"
#include "core/components/MComponent.h"
#include "core/features/Random.h"
#include "core/resources/managers/BufferManager.h"
#include "core/features/ServiceLocator.h"

ParticleManager::ParticleManager()
    : Manager("ParticleManager")
{
    
}

ParticleManager::~ParticleManager()
{

}

bool ParticleManager::init(WindowConfig config)
{
    Service::init(config);
    
    BufferManager* bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");

    return true;
}

bool ParticleManager::onClose()
{
    return true;
}

void ParticleManager::destroy(uint32_t id)
{
    if(id >= m_containers.size() || id < 0) {
        m_logger->error("particle container id: {} does not exist", id);
    } else {    //TODO: not thread safe, 2 request to delete the same id will causes issues
        m_toBeDestroyed.emplace_back(id);
    }
}

std::vector<uint32_t> ParticleManager::listIDs() const
{
    m_logger->error("listIDs is unimplmented for ParticleManager");

    return std::vector<uint32_t>();
}

void ParticleManager::onUpdate()
{
    if(!m_toBeDestroyed.empty()) {
        for(auto& containerID : m_toBeDestroyed) {
            m_containers.erase(m_containers.begin() + containerID);
            m_containerData.erase(m_containerData.begin() + containerID);
        }
    }
}

const ParticleManager::ContainerData& ParticleManager::getContainerData(uint32_t id) const
{
    if(id >= m_containers.size() || id < 0) {
        m_logger->error("particle container id: {} does not exist", id);
    }
    return m_containerData[id];
}

const std::vector<ParticleManager::ContainerData>& ParticleManager::getAllContainerData() const
{
    return m_containerData;
}

ParticleContainer ParticleManager::getContainer(uint32_t id)
{
    if(id >= m_containers.size() || id < 0) {
        m_logger->error("particle container id: {} does not exist", id);
    }
    return m_containers[id];
}

uint32_t ParticleManager::createContainer(uint32_t size, glm::vec3 minSpacing, glm::vec3 maxSpacing)
{
    //NOTE: m_ids starts at 1 which matches container's index 1 with dummy container at 0
    m_containers.push_back(ParticleContainer(size));
    ParticleContainer& container = m_containers[m_ids];
    size_t lifetimeSize = container.m_lifetime.size() * sizeof(double);
    size_t positionsSize = container.m_positions.size() * sizeof(glm::vec3);
    size_t scalesSize = container.m_scales.size() * sizeof(glm::vec3);
    size_t velocitiesSize = container.m_velocities.size() * sizeof(glm::vec3);
    size_t colorsSize = container.m_colors.size() * sizeof(glm::vec4);

    BufferManager* bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    
    m_containerData.push_back(ContainerData());
    ContainerData& data = m_containerData[m_ids];
    data.lifetimeBufferID = bufferManager->createBufferDeviceAddress(lifetimeSize);
    data.positionsBufferID = bufferManager->createBufferDeviceAddress(positionsSize);
    data.scalesBufferID = bufferManager->createBufferDeviceAddress(scalesSize);
    data.velocitiesBufferID = bufferManager->createBufferDeviceAddress(velocitiesSize);
    data.colorsBufferID = bufferManager->createBufferDeviceAddress(colorsSize);


    for(int i = 0; i < container.m_positions.size(); i++) {
        float x = Random::GenFloat(minSpacing.x, maxSpacing.x);
        float y = Random::GenFloat(minSpacing.y, maxSpacing.y);
        float z = Random::GenFloat(minSpacing.z, maxSpacing.z);
        container.m_positions[i] = glm::vec3(x, y ,z);
        
        float r = Random::GenFloat(0.0f, 1.0f);
        float g = Random::GenFloat(0.0f, 1.0f);
        float b = Random::GenFloat(0.0f, 1.0f);
        float a = Random::GenFloat(0.25f, 1.0f);
        container.m_colors[i] = glm::vec4(r, g, b, a);
    }
    
    bufferManager->updateBufferDeviceAddress(data.lifetimeBufferID, container.m_lifetime.data(), lifetimeSize);
    bufferManager->updateBufferDeviceAddress(data.positionsBufferID, container.m_positions.data(), positionsSize);
    bufferManager->updateBufferDeviceAddress(data.scalesBufferID, container.m_scales.data(), scalesSize);
    bufferManager->updateBufferDeviceAddress(data.velocitiesBufferID, container.m_velocities.data(), velocitiesSize);
    bufferManager->updateBufferDeviceAddress(data.colorsBufferID, container.m_colors.data(), colorsSize);

    //TODO: ideally after uploaded to gpu cpu side containers can be clearned up but keep them for now
    container.m_lifetime.clear();
    container.m_positions.clear();
    container.m_scales.clear();
    container.m_velocities.clear();
    container.m_colors.clear();

    return _assignID();
}