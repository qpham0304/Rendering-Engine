

class SceneManager;

class SkinAnimator {
public:
    SkinAnimator();
	~SkinAnimator();

    void play(float dt);
    void stop();
    void isPlaying();

private:
    SceneManager& sceneManager;

};