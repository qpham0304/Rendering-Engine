#include "AnimationManager.h"
#include "window/AppWindow.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/BufferManager.h"
#include "core/resources/managers/SkeletonManager.h"

AnimationManager::AnimationManager()
    : Manager("AnimationManager")
{
    setFootprint("AnimationManager", sizeof(AnimationManager), MemorySystem::Animation);
    
}

AnimationManager::~AnimationManager()
{

}

bool AnimationManager::init(WindowConfig config)
{
    Service::init(config);

    m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    assert(m_bufferManager && "failed to retrieve buffermanager");

    m_skinAnimator.init(this);
    m_spriteAnimator.init(this);
    
    //populate the first array slot for an empty array of one basic bone for debugging
    m_boneMatricesData.push_back({ glm::mat4(1.0f) });

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

void AnimationManager::onUpdate(const FrameTime& frameTime)
{
    m_spriteAnimator.onUpdate(frameTime.gameDeltaTime);
    m_skinAnimator.onUpdate(frameTime.gameDeltaTime);
}

// only called once this frame's fence has signaled, so nothing on the gpu still reads its buffers
void AnimationManager::uploadBoneMatrices(uint32_t frameIndex)
{
    if(frameIndex >= m_frameBones.size()) {
        m_frameBones.resize(frameIndex + 1);
    }
    FrameBones& frame = m_frameBones[frameIndex];

    // bone sets added since this frame last ran get their buffers here, the address table grows with them
    if(frame.bufferIDs.size() < m_boneMatricesData.size()) {
        for(size_t i = frame.bufferIDs.size(); i < m_boneMatricesData.size(); i++) {
            uint32_t bufferID = m_bufferManager->createBufferDeviceAddress(m_boneMatricesData[i].size() * sizeof(glm::mat4));
            frame.bufferIDs.push_back(bufferID);
            frame.addresses.push_back(m_bufferManager->getVirtualMemoryAddress(bufferID));
        }

        if(frame.tableID != 0) {
            m_bufferManager->destroy(frame.tableID);
        }
        size_t tableSize = sizeof(uint64_t) * frame.addresses.size();
        frame.tableID = m_bufferManager->createBufferDeviceAddress(tableSize);
        m_bufferManager->updateBufferDeviceAddress(frame.tableID, frame.addresses.data(), tableSize);
    }

    for(size_t i = 0; i < m_boneMatricesData.size(); i++) {
        m_bufferManager->updateBufferDeviceAddress(frame.bufferIDs[i], m_boneMatricesData[i].data(), m_boneMatricesData[i].size() * sizeof(glm::mat4));
    }
}

uint32_t AnimationManager::getAnimationBufferAddressID(uint32_t frameIndex) const
{
    return frameIndex < m_frameBones.size() ? m_frameBones[frameIndex].tableID : 0;
}

uint32_t AnimationManager::addBoneMatrices(size_t boneCount)
{
    m_boneMatricesData.push_back(std::vector<glm::mat4>(boneCount, glm::mat4(1.0f)));
    return m_boneMatricesData.size() - 1;   // use array incremental size as access index but not thread friendly
}

// return modifiable bone matrices per unique entity
std::vector<glm::mat4> &AnimationManager::getFinalBoneMatrices(uint32_t id)
{
    if(id <= 0 || id >= m_boneMatricesData.size()) {
        m_logger->warn("bone matrices with id: {} does not exist", id);
        return m_boneMatricesData[0];
    }

    return m_boneMatricesData[id];
}

bool AnimationManager::_loadAnimation(std::string_view path)
{
    return false;
}

bool AnimationManager::addAnimations(std::string_view path, AnimationData animations)
{
    std::string key(path);
    if(m_animations.contains(key)) {
        m_logger->warn("failed ot load animation: animation with the same path {} exists", key);
        return false;
    }

    m_animations.emplace(std::move(key), std::move(animations));
    return true;
}

std::shared_ptr<Animation> AnimationManager::getAnimation(std::string_view path, std::string_view name) const
{
    auto it = m_animations.find(path.data());
    if(it == m_animations.end()) {
        return nullptr;
    }

    auto& animationData = it->second;
    auto dataIt = animationData.find(name.data());
    if(dataIt == animationData.end()) {
        return nullptr;
    }

    return dataIt->second;
}

const AnimationData* AnimationManager::getAnimationData(std::string_view path)
{
    auto it = m_animations.find(path.data());
    if(it == m_animations.end()) {
        return nullptr;
    }

    return &it->second;
}

uint32_t AnimationManager::addAnimator(uint32_t skeletonID)
{
    auto animator = std::make_shared<Animator>();
    animator->skeletonID = skeletonID;
    m_animators.try_emplace(m_ids, animator);

    return _assignID();
}

std::shared_ptr<Animator> AnimationManager::getAnimator(uint32_t animatorID)
{
    auto it = m_animators.find(animatorID);
    if(it == m_animators.end()) {
        return nullptr;
    }

    return it->second;
}
