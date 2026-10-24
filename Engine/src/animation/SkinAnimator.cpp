#include "SkinAnimator.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/resources/managers/BufferManager.h"
#include "core/resources/managers/SkeletonManager.h"
#include "core/resources/managers/ModelManager.h"
#include "AnimationManager.h"
#include "Animation.h"
#include <cmath>


SkinAnimator::SkinAnimator()
    : sceneManager(SceneManager::getInstance())
{
    setFootprint("SkinAnimator", sizeof(SkinAnimator), MemorySystem::Animation);
}

SkinAnimator::~SkinAnimator()
{

}

void SkinAnimator::init(AnimationManager* manager)
{
    animationManager = manager;
    
    skeletonManager = &ServiceLocator::GetService<SkeletonManager>("SkeletonManager");
    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerSPD");
}

void SkinAnimator::play(std::string_view name)
{

}

void SkinAnimator::stop()
{

}

void SkinAnimator::isPlaying()
{
    
}

void SkinAnimator::onUpdate(float dt)
{
    for(uint32_t sceneID : sceneManager.listIDs()) {
        Scene* scene = sceneManager.getScene(sceneID);

        auto func = std::function<void(Entity)>([&](Entity entity) -> void {
            auto& skinning = entity.getComponent<SkinningComponent>();
            if(skinning.boneMatricesID == 0 || skinning.animatorID == 0){
                return;
            }
            _calculateBoneTransform(skinning.boneMatricesID, skinning.animatorID, dt);
        });

        scene->forEnitiesWith<SkinningComponent>(func);
    }
}

void SkinAnimator::_calculateBoneTransform(uint32_t boneMatricesID, uint32_t animatorID, float dt)
{
    auto& boneMatrices = animationManager->getFinalBoneMatrices(boneMatricesID);
    auto animator = animationManager->getAnimator(animatorID);
    auto skeleton = skeletonManager->getSkeleton(animator->skeletonID);
    auto animation = animationManager->getAnimation(animator->animationPath, animator->animationName);
    assert(animator && "fail to retrieve animator data to calculate bone transform");
    assert(skeleton && "fail to retrieve skeleton data to calculate bone transform");
    assert(animation && "fail to retrieve animation data to calculate bone transform");

    animator->currentTime += animation->ticksPerSecond * dt;
    animator->deltaTime = dt;
    animator->currentTime = fmod(animator->currentTime, animation->duration);   //loop back

    typedef std::function<void(uint32_t, glm::mat4)> TransformData;
    TransformData calculateTransform = [&] (uint32_t nodeIndex, glm::mat4 parentTransform) {
        SkeletonManager::SkeletonNode& node = skeleton->nodes[nodeIndex];

        //TODO: not ideal, find a good way to map bone with track to prvent loop back search
        Track* selectedTrack{ nullptr };
        for(auto& track : animation->tracks) {
            if(track.boneName == node.name) {
                selectedTrack = &track;
                break;
            }
        }

        if(selectedTrack) {
            node.vqs.position = _interpolatePosition(animator->currentTime, *selectedTrack);
            node.vqs.rotation = _interpolateRotation(animator->currentTime, *selectedTrack);
            node.vqs.scale = _interpolateScale(animator->currentTime, *selectedTrack);
        }

        glm::mat4 globalTransform = parentTransform * node.vqs.toMat4();
        boneMatrices[nodeIndex] = globalTransform * node.offsetMatrix;

        for(auto& childrenNodeIndex : node.childrenNodes) {
            calculateTransform(childrenNodeIndex, globalTransform);
        }
    };

    calculateTransform(skeleton->rootIndex, glm::mat4(1.0));
}

glm::vec3 SkinAnimator::_interpolatePosition(float currentTime, const Track &track)
{
    if(track.keyPositions.empty()) {
        return glm::vec3(0.0f);
    }

    if(track.keyPositions.size() == 1) {
        return track.keyPositions[0].position;
    }

    int p0Index = 0;
    for(int i = 0; i < track.keyPositions.size() - 1; i++) {
        if(currentTime < track.keyPositions[i].timeStamp) {
            p0Index = i;
            break;
        }
    }
    size_t p1Index = p0Index + 1;
    if(p1Index >= track.keyPositions.size()) {
        return track.keyPositions.back().position;
    }

    float t0 = track.keyPositions[p0Index].timeStamp;
    float t1 = track.keyPositions[p1Index].timeStamp;
    float timeDelta = t1 - t0;

    float factor = 0.0f;
    if(timeDelta > 0.0001f) {
        factor = (currentTime - t0) / timeDelta;
        factor = glm::clamp(factor, 0.0f, 1.0f);
    }

    const glm::vec3& start = track.keyPositions[p0Index].position;
    const glm::vec3& end = track.keyPositions[p1Index].position;

    return Math::lerp(start, end, factor);
}

Math::Quat SkinAnimator::_interpolateRotation(float currentTime, const Track &track)
{
    if(track.keyRotations.empty()) {
        return Math::Quat(0.0f);
    }

    if(track.keyRotations.size() == 1) {
        return track.keyRotations[0].orientation;
    }

    int p0Index = 0;
    for(int i = 0; i < track.keyRotations.size() - 1; i++) {
        if(currentTime < track.keyRotations[i].timeStamp) {
            p0Index = i;
            break;
        }
    }
    size_t p1Index = p0Index + 1;
    if(p1Index >= track.keyRotations.size()) {
        return track.keyRotations.back().orientation;
    }

    float t0 = track.keyRotations[p0Index].timeStamp;
    float t1 = track.keyRotations[p1Index].timeStamp;
    float timeDelta = t1 - t0;

    float factor = 0.0f;
    if(timeDelta > 0.0001f) {
        factor = (currentTime - t0) / timeDelta;
        factor = glm::clamp(factor, 0.0f, 1.0f);
    }

    const Math::Quat& start = track.keyRotations[p0Index].orientation;
    const Math::Quat& end = track.keyRotations[p1Index].orientation;

    return Math::slerp(start, end, factor);
}

glm::vec3 SkinAnimator::_interpolateScale(float currentTime, const Track &track)
{
    if(track.keyScales.empty()) {
        return glm::vec3(0.0f);
    }

    if(track.keyScales.size() == 1) {
        return track.keyScales[0].scale;
    }

    int p0Index = 0;
    for(int i = 0; i < track.keyScales.size() - 1; i++) {
        if(currentTime < track.keyScales[i].timeStamp) {
            p0Index = i;
            break;
        }
    }
    size_t p1Index = p0Index + 1;
    if(p1Index >= track.keyScales.size()) {
        return track.keyScales.back().scale;
    }

    float t0 = track.keyScales[p0Index].timeStamp;
    float t1 = track.keyScales[p1Index].timeStamp;
    float timeDelta = t1 - t0;

    float factor = 0.0f;
    if(timeDelta > 0.0001f) {
        factor = (currentTime - t0) / timeDelta;
        factor = glm::clamp(factor, 0.0f, 1.0f);
    }

    const glm::vec3& start = track.keyScales[p0Index].scale;
    const glm::vec3& end = track.keyScales[p1Index].scale;

    return Math::lerp(start, end, factor);
}
