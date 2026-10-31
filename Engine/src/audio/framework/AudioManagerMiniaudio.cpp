#include "AudioManagerMiniaudio.h"
#include "core/events/EventManager.h"
#include "core/features/EngineStates.h"
#include "core/features/camera.h"
#include "core/scene/SceneManager.h"
#include <miniaudio.h>

AudioManagerMiniaudio::AudioManagerMiniaudio(std::string serviceName)
    : AudioManager(serviceName)
{
    setFootprint("AudioManagerMiniaudio", sizeof(AudioManagerMiniaudio), MemorySystem::Audio);

}

AudioManagerMiniaudio::~AudioManagerMiniaudio()
{

}

bool AudioManagerMiniaudio::init(WindowConfig config)
{
    Service::init(config);

    // the device starts with the game, so nothing plays while the editor is stopped
    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.listenerCount = 1;
    engineConfig.noAutoStart = MA_TRUE;

    m_engine = std::make_unique<ma_engine>();
    ma_result result = ma_engine_init(&engineConfig, m_engine.get());
    if(result != MA_SUCCESS) {
        m_logger->error("failed to init audio engine: {}", ma_result_description(result));
        m_engine.reset();
        return false;
    }
    m_initialized = true;

    m_eventSubscriptions.push_back(EventManager::getInstance().subscribe(EventType::PlayAudio, [this](Event& event) {
        PlayAudioEvent& audioEvent = static_cast<PlayAudioEvent&>(event);
        PlayParams params;
        params.volume = audioEvent.volume;
        params.pitch = audioEvent.pitch;
        params.spatial = audioEvent.spatial;
        params.position = audioEvent.position;
        play(audioEvent.path, params);
    }));

    return true;
}

bool AudioManagerMiniaudio::onClose()
{
    m_eventSubscriptions.clear();
    if(!m_initialized) {
        return true;
    }

    stopAll();
    ma_engine_uninit(m_engine.get());
    m_engine.reset();
    m_initialized = false;
    m_lastState = EngineState::State::Stopped;
    return true;
}

void AudioManagerMiniaudio::destroy(uint32_t id)
{
    stop(id);
}

std::vector<uint32_t> AudioManagerMiniaudio::listIDs() const
{
    std::vector<uint32_t> ids;
    ids.reserve(m_sounds.size());
    for(const auto& [id, sound] : m_sounds) {
        ids.push_back(id);
    }
    return ids;
}

void AudioManagerMiniaudio::onUpdate(const FrameTime& frameTime)
{
    if(!m_initialized) {
        return;
    }

    _syncWithGameState();
    if(m_followCamera) {
        _followCamera();
    }

    std::vector<uint32_t> finished;
    for(const auto& [id, sound] : m_sounds) {
        if(ma_sound_at_end(sound.get())) {
            finished.push_back(id);
        }
    }

    // a fade out stops the sound once it reaches silence
    for(uint32_t id : m_fadingOut) {
        ma_sound* sound = _getSound(id);
        if(sound && !ma_sound_is_playing(sound)) {
            finished.push_back(id);
        }
    }

    for(uint32_t id : finished) {
        _release(id);
    }
}

uint32_t AudioManagerMiniaudio::play(const std::string& path, const PlayParams& params)
{
    if(!m_initialized) {
        return 0;
    }

    // decoded once and shared by every sound of the same file
    uint32_t id = _load(path, MA_SOUND_FLAG_DECODE);
    if(id == 0) {
        return 0;
    }

    ma_sound* sound = _getSound(id);
    ma_sound_set_volume(sound, params.volume);
    ma_sound_set_pitch(sound, params.pitch);
    ma_sound_set_looping(sound, params.loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_spatialization_enabled(sound, params.spatial ? MA_TRUE : MA_FALSE);
    if(params.spatial) {
        ma_sound_set_position(sound, params.position.x, params.position.y, params.position.z);
        ma_sound_set_min_distance(sound, params.minDistance);
        ma_sound_set_max_distance(sound, params.maxDistance);
    }
    ma_sound_start(sound);
    return id;
}

void AudioManagerMiniaudio::stop(uint32_t id)
{
    if(id == m_musicID) {
        m_musicID = 0;
    }
    _release(id);
}

void AudioManagerMiniaudio::stopAll()
{
    for(auto& [id, sound] : m_sounds) {
        ma_sound_uninit(sound.get());
    }
    m_sounds.clear();
    m_fadingOut.clear();
    m_musicID = 0;
}

bool AudioManagerMiniaudio::isPlaying(uint32_t id) const
{
    ma_sound* sound = _getSound(id);
    return sound && ma_sound_is_playing(sound);
}

void AudioManagerMiniaudio::setPosition(uint32_t id, const glm::vec3& position)
{
    if(ma_sound* sound = _getSound(id)) {
        ma_sound_set_position(sound, position.x, position.y, position.z);
    }
}

void AudioManagerMiniaudio::setVolume(uint32_t id, float volume)
{
    if(ma_sound* sound = _getSound(id)) {
        ma_sound_set_volume(sound, volume);
    }
}

void AudioManagerMiniaudio::setPitch(uint32_t id, float pitch)
{
    if(ma_sound* sound = _getSound(id)) {
        ma_sound_set_pitch(sound, pitch);
    }
}

void AudioManagerMiniaudio::playMusic(const std::string& path, float volume, float fadeSeconds)
{
    if(!m_initialized) {
        return;
    }

    stopMusic(fadeSeconds);

    // streamed from disk and loaded off the main thread, a track is too big to decode up front
    uint32_t id = _load(path, MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_NO_SPATIALIZATION);
    if(id == 0) {
        return;
    }

    ma_sound* sound = _getSound(id);
    ma_sound_set_volume(sound, volume);
    ma_sound_set_looping(sound, MA_TRUE);
    if(fadeSeconds > 0.0f) {
        ma_sound_set_fade_in_milliseconds(sound, 0.0f, 1.0f, static_cast<ma_uint64>(fadeSeconds * 1000.0f));
    }
    ma_sound_start(sound);
    m_musicID = id;
}

void AudioManagerMiniaudio::stopMusic(float fadeSeconds)
{
    if(m_musicID == 0) {
        return;
    }

    if(fadeSeconds > 0.0f) {
        ma_sound_stop_with_fade_in_milliseconds(_getSound(m_musicID), static_cast<ma_uint64>(fadeSeconds * 1000.0f));
        m_fadingOut.push_back(m_musicID);
    }
    else {
        _release(m_musicID);
    }
    m_musicID = 0;
}

void AudioManagerMiniaudio::setMusicVolume(float volume)
{
    setVolume(m_musicID, volume);
}

void AudioManagerMiniaudio::setMasterVolume(float volume)
{
    if(m_initialized) {
        ma_engine_set_volume(m_engine.get(), volume);
    }
}

float AudioManagerMiniaudio::getMasterVolume() const
{
    return m_initialized ? ma_engine_get_volume(m_engine.get()) : 0.0f;
}

void AudioManagerMiniaudio::setListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up)
{
    if(!m_initialized) {
        return;
    }

    m_followCamera = false;
    ma_engine_listener_set_position(m_engine.get(), 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(m_engine.get(), 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(m_engine.get(), 0, up.x, up.y, up.z);
}

uint32_t AudioManagerMiniaudio::_load(const std::string& path, uint32_t flags)
{
    auto sound = std::make_unique<ma_sound>();
    ma_result result = ma_sound_init_from_file(m_engine.get(), path.c_str(), flags, nullptr, nullptr, sound.get());
    if(result != MA_SUCCESS) {
        m_logger->error("failed to load sound {}: {}", path, ma_result_description(result));
        return 0;
    }

    uint32_t id = _assignID();
    m_sounds.emplace(id, std::move(sound));
    return id;
}

ma_sound* AudioManagerMiniaudio::_getSound(uint32_t id) const
{
    auto it = m_sounds.find(id);
    return it != m_sounds.end() ? it->second.get() : nullptr;
}

void AudioManagerMiniaudio::_release(uint32_t id)
{
    auto it = m_sounds.find(id);
    if(it == m_sounds.end()) {
        return;
    }

    ma_sound_uninit(it->second.get());
    m_sounds.erase(it);
    std::erase(m_fadingOut, id);
}

void AudioManagerMiniaudio::_followCamera()
{
    Camera* camera = SceneManager::cameraController;
    if(!camera) {
        return;
    }

    glm::vec3 position = camera->getPosition();
    glm::vec3 forward = camera->getOrientation();
    ma_engine_listener_set_position(m_engine.get(), 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(m_engine.get(), 0, forward.x, forward.y, forward.z);
}

void AudioManagerMiniaudio::_syncWithGameState()
{
    EngineState::State state = EngineState::getState();
    if(state == m_lastState) {
        return;
    }

    if(state == EngineState::State::Playing) {
        ma_engine_start(m_engine.get());
    }
    else {
        ma_engine_stop(m_engine.get());
    }

    // a pause keeps every sound where it was, a stop reloads the scene and its scripts start their sounds again
    if(state == EngineState::State::Stopped) {
        stopAll();
    }
    m_lastState = state;
}
