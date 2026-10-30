#pragma once

#include "core/resources/managers/Manager.h"
#include "core/features/MathIncludes.h"

class MYGRAPHICSENGINE_EXPORT AudioManager : public Manager
{
public:
	struct PlayParams {
		float volume { 1.0f };
		float pitch { 1.0f };
		bool loop { false };
		bool spatial { false };
		glm::vec3 position { 0.0f };
		float minDistance { 1.0f };		// full volume inside, falls off past it
		float maxDistance { 100.0f };	// no more falloff past it
	};

	AudioManager(std::string serviceName = "AudioManager") : Manager(serviceName) { setFootprint("AudioManager", sizeof(AudioManager), MemorySystem::Audio); };
	virtual ~AudioManager() = default;

	virtual bool init(WindowConfig config) = 0;
	virtual bool onClose() = 0;
	virtual void destroy(uint32_t id) = 0;
	virtual std::vector<uint32_t> listIDs() const = 0;
	virtual void onUpdate(const FrameTime& frameTime) override = 0;

	// returns 0 when the file fails to load, non looping sounds release themselves once they end
	virtual uint32_t play(const std::string& path, const PlayParams& params = {}) = 0;
	virtual void stop(uint32_t id) = 0;
	virtual void stopAll() = 0;
	virtual bool isPlaying(uint32_t id) const = 0;
	virtual void setPosition(uint32_t id, const glm::vec3& position) = 0;
	virtual void setVolume(uint32_t id, float volume) = 0;
	virtual void setPitch(uint32_t id, float pitch) = 0;

	// one streamed looping background track, a new one replaces the old
	virtual void playMusic(const std::string& path, float volume = 1.0f, float fadeSeconds = 0.0f) = 0;
	virtual void stopMusic(float fadeSeconds = 0.0f) = 0;
	virtual void setMusicVolume(float volume) = 0;

	virtual void setMasterVolume(float volume) = 0;
	virtual float getMasterVolume() const = 0;

	// the listener follows the active camera until it is set by hand
	virtual void setListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f)) = 0;
	void setListenerFollowCamera(bool follow) { m_followCamera = follow; }

protected:
	bool m_followCamera { true };
};
