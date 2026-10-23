#include "ParticleManager.h"
#include "core/components/MComponent.h"
#include "core/resources/managers/BufferManager.h"
#include "core/features/ServiceLocator.h"
#include <algorithm>

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

    m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");

    return true;
}

bool ParticleManager::onClose()
{
    return true;
}

void ParticleManager::destroy(uint32_t id)
{
    if(id == 0 || id >= m_containers.size()) {
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

void ParticleManager::onUpdate(const FrameTime& frameTime)
{
    // slots are emptied instead of erased so the ids held by emitters stay valid
    for(auto& containerID : m_toBeDestroyed) {
        _releaseBuffers(m_containerData[containerID]);
        m_containers[containerID] = {};
        m_version++;
    }
    m_toBeDestroyed.clear();
}

const ParticleManager::ContainerData& ParticleManager::getContainerData(uint32_t id) const
{
    if(id >= m_containerData.size()) {
        m_logger->error("particle container id: {} does not exist", id);
        return m_containerData[0];
    }
    return m_containerData[id];
}

const std::vector<ParticleManager::ContainerData>& ParticleManager::getAllContainerData() const
{
    return m_containerData;
}

ParticleContainer ParticleManager::getContainer(uint32_t id)
{
    if(id >= m_containers.size()) {
        m_logger->error("particle container id: {} does not exist", id);
        return m_containers[0];
    }
    return m_containers[id];
}

uint32_t ParticleManager::createContainer(uint32_t capacity)
{
    //NOTE: m_ids starts at 1 which matches container's index 1 with dummy container at 0
    capacity = std::max(capacity, 1u);
    m_containers.push_back(ParticleContainer(capacity));
    m_containerData.push_back(ContainerData());

    ContainerData& data = m_containerData[m_ids];
    _allocateBuffers(data, capacity);
    _uploadInitialState(data, capacity);
    m_version++;

    return _assignID();
}

void ParticleManager::resizeContainer(uint32_t id, uint32_t capacity)
{
    if(id == 0 || id >= m_containers.size()) {
        m_logger->error("particle container id: {} does not exist", id);
        return;
    }

    capacity = std::max(capacity, 1u);
    ContainerData& data = m_containerData[id];
    _releaseBuffers(data);
    _allocateBuffers(data, capacity);
    _uploadInitialState(data, capacity);
    m_containers[id].m_size = capacity;
    m_version++;
}

void ParticleManager::resetContainer(uint32_t id)
{
    if(id == 0 || id >= m_containers.size()) {
        m_logger->error("particle container id: {} does not exist", id);
        return;
    }
    _uploadInitialState(m_containerData[id], m_containers[id].m_size);
}

void ParticleManager::_allocateBuffers(ContainerData& data, uint32_t capacity)
{
    data.particlesBufferID = m_bufferManager->createBufferDeviceAddress(capacity * sizeof(ParticleGPU));
    data.aliveListBufferID = m_bufferManager->createBufferDeviceAddress(particleSortSize(capacity) * sizeof(ParticleDrawEntry));
    data.deadListBufferID = m_bufferManager->createBufferDeviceAddress(capacity * sizeof(uint32_t));
    data.countersBufferID = m_bufferManager->createBufferDeviceAddress(sizeof(ParticleCounters));
}

void ParticleManager::_releaseBuffers(ContainerData& data)
{
    for(uint32_t* bufferID : { &data.particlesBufferID, &data.aliveListBufferID, &data.deadListBufferID, &data.countersBufferID }) {
        if(*bufferID != 0) {
            m_bufferManager->destroy(*bufferID);
            *bufferID = 0;
        }
    }
}

void ParticleManager::_uploadInitialState(const ContainerData& data, uint32_t capacity)
{
    // every slot starts dead and on the free stack
    std::vector<ParticleGPU> particles(capacity);
    std::vector<uint32_t> deadList(capacity);
    for(uint32_t i = 0; i < capacity; i++) {
        deadList[i] = capacity - 1 - i;     // low slots get popped first
    }
    ParticleCounters counters{};
    counters.deadCount = static_cast<int32_t>(capacity);

    m_bufferManager->updateBufferDeviceAddress(data.particlesBufferID, particles.data(), particles.size() * sizeof(ParticleGPU));
    m_bufferManager->updateBufferDeviceAddress(data.deadListBufferID, deadList.data(), deadList.size() * sizeof(uint32_t));
    m_bufferManager->updateBufferDeviceAddress(data.countersBufferID, &counters, sizeof(ParticleCounters));
}
