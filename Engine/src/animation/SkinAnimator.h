#pragma once

#include "Core/features/Math.h"
#include "core/features/Object.h"

class SceneManager;
class ModelManager;
class AnimationManager;
class Logger;
class SkeletonManager;
struct Track;

class SkinAnimator : public Object {
public:
    SkinAnimator();
	~SkinAnimator();

    void init(AnimationManager* manager);
    void play(std::string_view name);
    void stop();
    void isPlaying();
    void onUpdate(float dt);

private:
    Logger* m_logger;
    AnimationManager* animationManager;
    ModelManager* modelManager;
    SkeletonManager* skeletonManager;
    SceneManager& sceneManager;


    void _calculateBoneTransform(uint32_t boneMatricesID, uint32_t animatorID, float dt);
    glm::vec3 _interpolatePosition(float currentTime, const Track& track);
    Math::Quat _interpolateRotation(float currentTime, const Track& track);
    glm::vec3 _interpolateScale(float currentTime, const Track& track);
};