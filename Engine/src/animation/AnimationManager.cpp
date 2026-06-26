#include "AnimationManager.h"
#include "window/AppWindow.h"

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
    
    return true;
}

bool AnimationManager::onClose()
{
    return false;
}

void AnimationManager::destroy(uint32_t id)
{

}

std::vector<uint32_t> AnimationManager::listIDs() const
{
    return std::vector<uint32_t>();
}

void AnimationManager::onUpdate()
{
    m_spriteAnimator.onUpdate(AppWindow::getDeltaTime());
    // m_skinAnimator.onUpdate(AppWindow::getDeltaTime());
}
