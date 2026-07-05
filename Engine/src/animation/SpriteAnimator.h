

class SceneManager;

class SpriteAnimator {
public:
    SpriteAnimator();
	~SpriteAnimator();

    void play(std::string_view name);
    void stop();
    void isPlaying();
    void onUpdate(float dt);

private:
    SceneManager& sceneManager;

};