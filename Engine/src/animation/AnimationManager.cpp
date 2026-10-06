#include "AnimationManager.h"
#include "window/AppWindow.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/BufferManager.h"
#include "core/resources/managers/SkeletonManager.h"

AnimationManager::AnimationManager()
    : Manager("AnimationManager")
{
    
}

AnimationManager::~AnimationManager()
{

}

bool AnimationManager::init(WindowConfig config)
{
    Service::init(config);
    
    m_boneMatricesBuffers.push_back(0);
    m_boneMatricesData.push_back({glm::mat4(1.0)});


    m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    assert(m_bufferManager && "failed to retrieve buffermanager");

    size_t bufferSize = sizeof(glm::mat4) * m_boneMatricesBuffers.size();
	deviceAdressBufferID = m_bufferManager->createBufferDeviceAddress(bufferSize);

    return true;
}

bool AnimationManager::onClose()
{
    return true;
}

void AnimationManager::destroy(uint32_t id)
{

}

std::vector<uint32_t> AnimationManager::listIDs() const
{
    m_logger->error("listIDs is unimplmented for AnimationManager");
    
    return std::vector<uint32_t>();
}

void AnimationManager::onUpdate()
{
    m_spriteAnimator.onUpdate(AppWindow::getDeltaTime());
    m_skinAnimator.onUpdate(AppWindow::getDeltaTime());

    int i = 0;
    for(auto& boneMatrices : m_boneMatricesData) {
        size_t bufferSize = boneMatrices.size() * sizeof(glm::mat4);
        if(bufferSize == 0){
            m_logger->warn("boneMatrices id: {} is empty", i);
        } else {
            uint32_t bufferID = m_boneMatricesBuffers[i];
            if(bufferID != 0) {
                m_bufferManager->updateBufferDeviceAddress(bufferID, m_boneMatricesData.data(), bufferSize);
            }
        }
        i++;
    }
}

uint32_t AnimationManager::getAnimationBufferAddressID()
{
    return deviceAdressBufferID;
}

uint32_t AnimationManager::addBoneMatrices(size_t boneCount)
{
    std::vector<glm::mat4> boneMatrices;
    boneMatrices.resize(boneCount, glm::mat4(1.0f));

    size_t bufferSize = boneCount * sizeof(glm::mat4);
    uint32_t bufferID = m_bufferManager->createBufferDeviceAddress(bufferSize);
    m_bufferManager->updateBufferDeviceAddress(bufferID, boneMatrices.data(), bufferSize);
    
    m_boneMatricesBuffers.push_back(bufferID);
    m_boneMatricesData.push_back(std::move(boneMatrices));

    std::vector<uint64_t> virtualMemoryAddresses;
    for(const auto& id : m_boneMatricesBuffers) {
        if(id == 0) {   // skip the first index
            continue;
        }
        virtualMemoryAddresses.push_back(m_bufferManager->getVirtualMemoryAddress(id));
    }
    
    //TODO: requires explicit sync when add and remove buffer
    size_t containerBufferSize = sizeof(uint64_t) * virtualMemoryAddresses.size();
    m_bufferManager->updateBufferDeviceAddress(deviceAdressBufferID, virtualMemoryAddresses.data(), containerBufferSize);

    return _assignID();
}

const std::vector<glm::mat4> &AnimationManager::getFinalBoneMatrices(uint32_t id) const
{
    if(id <= 0 || id >= m_boneMatricesBuffers.size()) {
        m_logger->warn("bone matrices with id: {} does not exist", id);
        return m_boneMatricesData[0];
    }

    return m_boneMatricesData[id];
}
