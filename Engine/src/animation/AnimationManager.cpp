#include "AnimationManager.h"
#include "window/AppWindow.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/BufferManager.h"
#include "core/resources/managers/SkeletonManager.h"
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp_glm_helpers.h>

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

    m_bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    assert(m_bufferManager && "failed to retrieve buffermanager");

    m_skinAnimator.init(this);
    m_spriteAnimator.init(this);
    
    // m_boneMatricesBuffers.push_back(0);
    // m_boneMatricesData.push_back({glm::mat4(1.0)});
    
    //populate the first array slot for an empty array of one basic bone for debugging
    const int boneCount = 1;
    std::vector<glm::mat4> boneMatrices{{glm::mat4(1.0)}};
    boneMatrices.resize(boneCount, glm::mat4(1.0f));

    size_t bufferSize = boneCount * sizeof(glm::mat4);
    uint32_t bufferID = m_bufferManager->createBufferDeviceAddress(bufferSize);
    m_bufferManager->updateBufferDeviceAddress(bufferID, boneMatrices.data(), bufferSize);
    m_boneMatricesData.push_back(std::move(boneMatrices));

    m_boneMatricesBuffers.emplace_back(m_bufferManager->getVirtualMemoryAddress(bufferID));
    size_t containerBufferSize = sizeof(uint64_t) * m_boneMatricesBuffers.size();
    deviceAdressBufferID = m_bufferManager->createBufferDeviceAddress(containerBufferSize);
    m_bufferManager->updateBufferDeviceAddress(deviceAdressBufferID, m_boneMatricesBuffers.data(), containerBufferSize);


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
            //TODO: brute force search all buffers to find a match because no way to store
            // m_boneMatricesBuffers as bufferid instead of direct memory address without breaking
            // review this when have time
            uint32_t bufferID = m_bufferManager->findBufferID(m_boneMatricesBuffers[i]);
            if(bufferID != 0) {
                m_bufferManager->updateBufferDeviceAddress(bufferID, m_boneMatricesData[i].data(), bufferSize);
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
    m_boneMatricesData.push_back(std::move(boneMatrices));

    
    m_boneMatricesBuffers.emplace_back(m_bufferManager->getVirtualMemoryAddress(bufferID));
    size_t containerBufferSize = sizeof(uint64_t) * m_boneMatricesBuffers.size();
    if(deviceAdressBufferID != 0) {  // resizeable bda so this need to be recreated every new added bone
        m_bufferManager->destroy(deviceAdressBufferID);
    }
    deviceAdressBufferID = m_bufferManager->createBufferDeviceAddress(containerBufferSize);
    m_bufferManager->updateBufferDeviceAddress(deviceAdressBufferID, m_boneMatricesBuffers.data(), containerBufferSize);

    return m_boneMatricesData.size() - 1;   // use array incremental size as access index but not thread friendly
}

// return modifiable bone matrices per unique entity
std::vector<glm::mat4> &AnimationManager::getFinalBoneMatrices(uint32_t id)
{
    if(id <= 0 || id >= m_boneMatricesBuffers.size()) {
        m_logger->warn("bone matrices with id: {} does not exist", id);
        return m_boneMatricesData[0];
    }

    return m_boneMatricesData[id];
}

bool AnimationManager::_loadAnimation(std::string_view path)
{
    return false;
}

bool AnimationManager::loadAnimation(std::string_view path, const aiScene* scene)
{
    auto it = m_animations.find(path.data());
    if(it != m_animations.end()) {
        m_logger->warn("failed ot load animation: animation with the same path {} exists", path.data());
        return false;
    }
    
    m_animations[path.data()] = {};
    // m_animations.try_emplace(path.data(), {});
    auto& animationData = m_animations.at(path.data());

    for(int i = 0; i < scene->mNumAnimations; i++) {
        aiAnimation* aiAnimation = scene->mAnimations[i];
        
        auto animation = std::make_shared<Animation>();
        animation->ticksPerSecond = aiAnimation->mTicksPerSecond;
        animation->duration = aiAnimation->mDuration;

        //TODO: support mesh morph channel
        for(int j = 0; j < aiAnimation->mNumMorphMeshChannels; j++) {
            aiMeshMorphAnim* aiMeshMorphChannel = aiAnimation->mMorphMeshChannels[j];

            for(int k = 0; k < aiMeshMorphChannel->mNumKeys; k++) {
                aiMeshMorphKey morpKey = aiMeshMorphChannel->mKeys[k];

            }
        }
        
        for(int j = 0; j < aiAnimation->mNumChannels; j++) {
            aiNodeAnim* aiChannel = aiAnimation->mChannels[j];

            Track track{};
            track.boneName = aiChannel->mNodeName.C_Str();

            for(int k = 0; k < aiChannel->mNumPositionKeys; k++) {
                aiVectorKey aiPositionKey = aiChannel->mPositionKeys[k];
                KeyPosition keyPosition{};
                keyPosition.timeStamp = aiPositionKey.mTime;
                keyPosition.position = AssimpGLMHelpers::toGlmVec3(aiPositionKey.mValue);
                track.keyPositions.push_back(keyPosition);
            }
     
            for(int k = 0; k < aiChannel->mNumRotationKeys; k++) {
                aiQuatKey aiRotationKey = aiChannel->mRotationKeys[k];
                aiQuaternion aiQuat = aiRotationKey.mValue;
                KeyRotation keyRotation{};
                keyRotation.timeStamp = aiRotationKey.mTime;
                keyRotation.orientation.fromGlmQuat(glm::quat(aiQuat.w, aiQuat.x, aiQuat.y, aiQuat.z));
                track.keyRotations.push_back(keyRotation);
            }
     
            for(int k = 0; k < aiChannel->mNumScalingKeys; k++) {
                aiVectorKey aiScalingKey = aiChannel->mScalingKeys[k];
                KeyScale keyScale{};
                keyScale.timeStamp = aiScalingKey.mTime;
                keyScale.scale = AssimpGLMHelpers::toGlmVec3(aiScalingKey.mValue);
                track.keyScales.push_back(keyScale);
            }
            
            animation->tracks.push_back(track);
        }
        animationData.try_emplace(aiAnimation->mName.C_Str(), animation);
    }
    
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
