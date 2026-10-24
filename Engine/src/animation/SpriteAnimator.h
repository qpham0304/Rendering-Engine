#pragma once
#include "core/features/Object.h"

class SceneManager;
class AnimationManager;
class SpriteAnimator : public Object {
public:
    SpriteAnimator();
	~SpriteAnimator();

    void init(AnimationManager* manager);
    void play(std::string_view name);
    void stop();
    void isPlaying();
    void onUpdate(float dt);

private:
    AnimationManager* animationManager;
    SceneManager& sceneManager;

};