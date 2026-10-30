#include "AudioToLuaBindings.h"
#include "audio/AudioManager.h"

void AudioToLuaBindings::exportAudio(sol::state& luaState, AudioManager* audioManager)
{
    sol::table audio = luaState.create_named_table("audio");

    audio["play"] = [audioManager](const std::string& path, sol::optional<float> volume, sol::optional<bool> loop) {
        AudioManager::PlayParams params;
        params.volume = volume.value_or(1.0f);
        params.loop = loop.value_or(false);
        return audioManager->play(path, params);
    };

    audio["play3D"] = [audioManager](const std::string& path, const glm::vec3& position, sol::optional<float> volume, sol::optional<bool> loop) {
        AudioManager::PlayParams params;
        params.spatial = true;
        params.position = position;
        params.volume = volume.value_or(1.0f);
        params.loop = loop.value_or(false);
        return audioManager->play(path, params);
    };

    audio["stop"] = [audioManager](uint32_t id) { audioManager->stop(id); };
    audio["stopAll"] = [audioManager]() { audioManager->stopAll(); };
    audio["isPlaying"] = [audioManager](uint32_t id) { return audioManager->isPlaying(id); };
    audio["setPosition"] = [audioManager](uint32_t id, const glm::vec3& position) { audioManager->setPosition(id, position); };
    audio["setVolume"] = [audioManager](uint32_t id, float volume) { audioManager->setVolume(id, volume); };
    audio["setPitch"] = [audioManager](uint32_t id, float pitch) { audioManager->setPitch(id, pitch); };

    audio["playMusic"] = [audioManager](const std::string& path, sol::optional<float> volume, sol::optional<float> fadeSeconds) {
        audioManager->playMusic(path, volume.value_or(1.0f), fadeSeconds.value_or(0.0f));
    };
    audio["stopMusic"] = [audioManager](sol::optional<float> fadeSeconds) { audioManager->stopMusic(fadeSeconds.value_or(0.0f)); };
    audio["setMusicVolume"] = [audioManager](float volume) { audioManager->setMusicVolume(volume); };

    audio["setMasterVolume"] = [audioManager](float volume) { audioManager->setMasterVolume(volume); };
    audio["getMasterVolume"] = [audioManager]() { return audioManager->getMasterVolume(); };
}
