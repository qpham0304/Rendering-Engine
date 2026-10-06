

class SceneManager;

class SkinAnimator {
public:

    SkinAnimator();
	~SkinAnimator();

    void play(std::string_view name);
    void stop();
    void isPlaying();
    void onUpdate(float dt);

private:
    SceneManager& sceneManager;
};