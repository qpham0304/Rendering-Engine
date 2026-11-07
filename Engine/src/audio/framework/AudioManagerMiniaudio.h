#pragma once

#include "audio/AudioManager.h"
#include "core/features/EngineStates.h"
#include <unordered_map>

typedef struct ma_engine ma_engine;
typedef struct ma_sound ma_sound;

class AudioManagerMiniaudio : public AudioManager
{
public:
	AudioManagerMiniaudio(std::string serviceName = "AudioManager");
	~AudioManagerMiniaudio();

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
	virtual void onUpdate(const FrameTime& frameTime) override;

	virtual uint32_t play(const std::string& path, const PlayParams& params = {}) override;
	virtual void stop(uint32_t id) override;
	virtual void stopAll() override;
	virtual bool isPlaying(uint32_t id) const override;
	virtual void setPosition(uint32_t id, const glm::vec3& position) override;
	virtual void setVolume(uint32_t id, float volume) override;
	virtual void setPitch(uint32_t id, float pitch) override;

	virtual void playMusic(const std::string& path, float volume = 1.0f, float fadeSeconds = 0.0f) override;
	virtual void stopMusic(float fadeSeconds = 0.0f) override;
	virtual void setMusicVolume(float volume) override;
	virtual float getMusicVolume() const override;

	virtual void setMasterVolume(float volume) override;
	virtual float getMasterVolume() const override;

	virtual void setListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) override;

private:
	uint32_t _load(const std::string& path, uint32_t flags);
	ma_sound* _getSound(uint32_t id) const;
	void _release(uint32_t id);
	void _followCamera();
	void _syncWithGameState();

	std::unique_ptr<ma_engine> m_engine;
	std::unordered_map<uint32_t, std::unique_ptr<ma_sound>> m_sounds;
	std::vector<uint32_t> m_fadingOut;		// stopped with a fade, released once silent
	uint32_t m_musicID { 0 };
	float m_musicVolume { 1.0f };
	bool m_initialized { false };
	EngineState::State m_lastState { EngineState::State::Stopped };
};
