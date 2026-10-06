#include "SkinAnimator.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/resources/managers/BufferManager.h"
#include "core/resources/managers/SkeletonManager.h"


SkinAnimator::SkinAnimator()
    : sceneManager(SceneManager::getInstance())
{
}

SkinAnimator::~SkinAnimator()
{

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

}
