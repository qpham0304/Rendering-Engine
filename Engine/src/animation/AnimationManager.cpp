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
    
    m_boneMatricesBuffers.emplace_back(0, glm::mat4(1.0));    // empty zero location for debugging

    m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    assert(m_bufferManager && "failed to retrieve buffermanager");

    // size_t bufferSize = sizeof(glm::mat4) * m_boneMatricesBuffers.size();
	// deviceAdressBufferID = m_bufferManager->createBufferDeviceAddress(bufferSize);
    // m_bufferManager->updateBufferDeviceAddress(deviceAdressBufferID, m_boneMatricesBuffers.data(), bufferSize);

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
    for(auto& boneMatrices : m_boneMatricesBuffers) {
        size_t bufferSize = boneMatrices.finalBoneMatrices.size() * sizeof(glm::mat4);
        if(bufferSize == 0){
            m_logger->warn("boneMatrices id: {} is empty", i);
            return;
        }
        m_bufferManager->updateBufferDeviceAddress(boneMatrices.gpuBufferID, boneMatrices.finalBoneMatrices.data(), bufferSize);
        i++;
    }
}

uint32_t AnimationManager::getAnimationBufferAddressID()
{
    return deviceAdressBufferID;
}

uint32_t AnimationManager::addBoneMatrices(size_t boneCount)
{
    AnimationManager::BoneMatricesData instance;
    instance.finalBoneMatrices.resize(boneCount, glm::mat4(1.0f));

    size_t bufferSize = boneCount * sizeof(glm::mat4);
    instance.gpuBufferID = m_bufferManager->createBufferDeviceAddress(bufferSize);

    m_boneMatricesBuffers[m_ids] = std::move(instance);


    // size_t containerSize = sizeof(glm::mat4) * m_boneMatricesBuffers.size();
    // m_bufferManager->updateBufferDeviceAddress(deviceAdressBufferID, m_boneMatricesBuffers.data(), containerSize);


    return _assignID();
}

const std::vector<glm::mat4> &AnimationManager::getFinalBoneMatrices(uint32_t id) const
{
    if(id <= 0 || id > m_boneMatricesBuffers.size()){
        return {};
    }

    return m_boneMatricesBuffers[id].finalBoneMatrices;
}
