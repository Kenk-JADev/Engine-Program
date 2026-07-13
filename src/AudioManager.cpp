#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Logger.h"
#include <miniaudio/miniaudio.h>
#include <algorithm>
#include <iostream>

namespace rpg {

// ==================== Deleters ====================
void SoundDeleter::operator()(ma_sound* s) const {
    if (s) {
        ma_sound_uninit(s);
        delete s;
    }
}

void EngineDeleter::operator()(ma_engine* e) const {
    if (e) {
        ma_engine_uninit(e);
        delete e;
    }
}

// ==================== AudioManager ====================
AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
    Shutdown();
}

bool AudioManager::Initialize() {
    auto engine = std::make_unique<ma_engine>();
    ma_engine_config config = ma_engine_config_init();
    config.noAutoStart = MA_FALSE;
    
    if (ma_engine_init(&config, engine.get()) != MA_SUCCESS) {
        RPG_LOG_ERROR("Failed to initialize miniaudio engine");
        return false;
    }
    mEngine.reset(engine.release());
    
    // Set default listener
    ma_engine_listener_set_position(mEngine.get(), 0, mListenerPosition.x, mListenerPosition.y, mListenerPosition.z);
    ma_engine_listener_set_direction(mEngine.get(), 0, mListenerForward.x, mListenerForward.y, mListenerForward.z);
    
    RPG_LOG_INFO("AudioManager initialized");
    return true;
}

void AudioManager::Shutdown() {
    // Stop all sounds with fade
    FadeOutAll(0.5f);
    
    // Wait a bit for fades to complete (in real engine, you'd wait properly)
    // For now just stop immediately
    for (auto& [handle, instance] : mSounds) {
        ma_sound_stop(instance.sound.get());
    }
    
    mSounds.clear();
    mPreloadedSounds.clear();
    mEngine.reset();
    mNextHandleId = 1;
    RPG_LOG_INFO("AudioManager shutdown");
}

// ==================== Core Playback ====================

SoundHandle AudioManager::PlayBGM(const std::string& path, bool loop, float volume, float pitch, float fadeInTime) {
    // Stop current BGM with crossfade
    if (mCurrentBGM.valid()) {
        Stop(mCurrentBGM, std::max(fadeInTime, 0.5f));
    }
    
    SoundHandle handle = Play(path, AudioType::BGM, loop, volume * mBGMVolume, pitch, fadeInTime);
    if (handle.valid()) {
        mCurrentBGM = handle;
    }
    return handle;
}

SoundHandle AudioManager::PlayBGS(const std::string& path, bool loop, float volume, float pitch, float fadeInTime) {
    if (mCurrentBGS.valid()) {
        Stop(mCurrentBGS, std::max(fadeInTime, 0.5f));
    }
    
    SoundHandle handle = Play(path, AudioType::BGS, loop, volume * mBGSVolume, pitch, fadeInTime);
    if (handle.valid()) {
        mCurrentBGS = handle;
    }
    return handle;
}

SoundHandle AudioManager::PlaySE(const std::string& path, bool loop, float volume, float pitch) {
    return Play(path, AudioType::SE, loop, volume * mSEVolume, pitch, 0.0f);
}

SoundHandle AudioManager::PlayME(const std::string& path, bool loop, float volume, float pitch) {
    return Play(path, AudioType::ME, loop, volume * mMEVolume, pitch, 0.0f);
}

SoundHandle AudioManager::Play(const std::string& path, AudioType type, bool loop, float volume, float pitch, float fadeInTime) {
    // Check preloaded
    SoundPtr sound;
    auto itPreload = mPreloadedSounds.find(path);
    if (itPreload != mPreloadedSounds.end()) {
        // Clone the preloaded sound
        sound = SoundPtr{new ma_sound()};
        ma_sound_init_from_file(mEngine.get(), path.c_str(), type == AudioType::BGM || type == AudioType::BGS ? MA_SOUND_FLAG_STREAM : 0, nullptr, nullptr, sound.get());
    } else {
        sound = SoundPtr{new ma_sound()};
        ma_uint32 flags = 0;
        if (type == AudioType::BGM || type == AudioType::BGS) {
            flags |= MA_SOUND_FLAG_STREAM;
        }
        ma_result result = ma_sound_init_from_file(mEngine.get(), path.c_str(), flags, nullptr, nullptr, sound.get());
        if (result != MA_SUCCESS) {
            RPG_LOG_ERROR("Failed to load sound: " + path + " (error: " + std::to_string(result) + ")");
            return SoundHandle{0};
        }
    }

    return CreateSoundInstance(std::move(sound), type, loop, volume, pitch);
}

SoundHandle AudioManager::CreateSoundInstance(SoundPtr sound, AudioType type, bool loop, float volume, float pitch) {
    if (!sound) return SoundHandle{0};
    
    SoundHandle handle{mNextHandleId++};
    if (mNextHandleId == 0) mNextHandleId = 1; // wrap protection
    
    // Apply initial settings
    ma_sound_set_looping(sound.get(), loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(sound.get(), std::clamp(volume, 0.0f, 1.0f));
    ma_sound_set_pitch(sound.get(), std::max(pitch, 0.01f));
    
    // Start playing
    ma_sound_start(sound.get());
    
    SoundInstance instance;
    instance.sound = std::move(sound);
    instance.type = type;
    instance.handle = handle;
    instance.fadeInTime = 0.0f; // fadeIn handled at start
    
    mSounds[handle] = std::move(instance);
    return handle;
}

// ==================== Control by Handle ====================
void AudioManager::Stop(SoundHandle handle, float fadeOutTime) {
    auto it = mSounds.find(handle);
    if (it == mSounds.end()) return;
    
    if (fadeOutTime > 0.0f) {
        it->second.fadingOut = true;
        it->second.fadeOutTime = fadeOutTime;
        it->second.fadeTimer = 0.0f;
        it->second.targetVolume = 0.0f;
    } else {
        ma_sound_stop(it->second.sound.get());
        mSounds.erase(it);
    }
    
    // Clear current if this was it
    if (mCurrentBGM == handle) mCurrentBGM = SoundHandle{0};
    if (mCurrentBGS == handle) mCurrentBGS = SoundHandle{0};
}

void AudioManager::Pause(SoundHandle handle) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        ma_sound_stop(it->second.sound.get());
    }
}

void AudioManager::Resume(SoundHandle handle) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        ma_sound_start(it->second.sound.get());
    }
}

float AudioManager::GetTypeVolume(AudioType type) const {
    switch (type) {
        case AudioType::BGM: return mBGMVolume;
        case AudioType::BGS: return mBGSVolume;
        case AudioType::SE:  return mSEVolume;
        case AudioType::ME:  return mMEVolume;
        default: return mMasterVolume;
    }
}

void AudioManager::SetVolume(SoundHandle handle, float volume) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        volume = std::clamp(volume * GetTypeVolume(it->second.type), 0.0f, 1.0f);
        ma_sound_set_volume(it->second.sound.get(), volume);
    }
}

void AudioManager::SetPitch(SoundHandle handle, float pitch) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        ma_sound_set_pitch(it->second.sound.get(), std::max(pitch, 0.01f));
    }
}

void AudioManager::SetLooping(SoundHandle handle, bool loop) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        ma_sound_set_looping(it->second.sound.get(), loop ? MA_TRUE : MA_FALSE);
    }
}

void AudioManager::SetLoopPoints(SoundHandle handle, float loopStartSec, float loopEndSec) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end() && it->second.sound) {
        // miniaudio doesn't have loop point setting via seconds, use pcm frames via data source
        ma_data_source* ds = static_cast<ma_data_source*>(it->second.sound.get());
        ma_data_source_set_loop_point_in_pcm_frames(ds, 
            (ma_uint64)(loopStartSec * ma_engine_get_sample_rate(mEngine.get())),
            (ma_uint64)(loopEndSec * ma_engine_get_sample_rate(mEngine.get())));
    }
}

void AudioManager::SetPan(SoundHandle handle, float pan) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        pan = std::clamp(pan, -1.0f, 1.0f);
        ma_sound_set_pan(it->second.sound.get(), pan);
    }
}

// ==================== Queries ====================
bool AudioManager::IsPlaying(SoundHandle handle) const {
    auto it = mSounds.find(handle);
    if (it == mSounds.end()) return false;
    return ma_sound_is_playing(it->second.sound.get());
}

bool AudioManager::IsPaused(SoundHandle handle) const {
    auto it = mSounds.find(handle);
    if (it == mSounds.end()) return false;
    return !ma_sound_is_playing(it->second.sound.get()) && !IsPlaying(handle);
}

float AudioManager::GetPosition(SoundHandle handle) const {
    auto it = mSounds.find(handle);
    if (it == mSounds.end()) return 0.0f;
    
    ma_uint64 frames = 0;
    ma_sound_get_cursor_in_pcm_frames(it->second.sound.get(), &frames);
    float sampleRate = (float)ma_engine_get_sample_rate(mEngine.get());
    return frames / sampleRate;
}

float AudioManager::GetDuration(SoundHandle handle) const {
    auto it = mSounds.find(handle);
    if (it == mSounds.end()) return 0.0f;
    
    ma_uint64 frames = 0;
    ma_sound_get_length_in_pcm_frames(it->second.sound.get(), &frames);
    float sampleRate = (float)ma_engine_get_sample_rate(mEngine.get());
    return frames / sampleRate;
}

// ==================== Master Controls ====================
void AudioManager::SetMasterVolume(float volume) {
    mMasterVolume = std::clamp(volume, 0.0f, 1.0f);
    if (mEngine) {
        ma_engine_set_volume(mEngine.get(), mMasterVolume);
    }
}

void AudioManager::SetBGMVolume(float volume) {
    mBGMVolume = std::clamp(volume, 0.0f, 1.0f);
    if (mCurrentBGM.valid()) {
        auto it = mSounds.find(mCurrentBGM);
        if (it != mSounds.end()) {
            ma_sound_set_volume(it->second.sound.get(), mBGMVolume * it->second.targetVolume);
        }
    }
}

void AudioManager::SetBGSVolume(float volume) {
    mBGSVolume = std::clamp(volume, 0.0f, 1.0f);
    if (mCurrentBGS.valid()) {
        auto it = mSounds.find(mCurrentBGS);
        if (it != mSounds.end()) {
            ma_sound_set_volume(it->second.sound.get(), mBGSVolume * it->second.targetVolume);
        }
    }
}

void AudioManager::SetSEVolume(float volume) {
    mSEVolume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioManager::SetMEVolume(float volume) {
    mMEVolume = std::clamp(volume, 0.0f, 1.0f);
}

// ==================== Fade Controls ====================
void AudioManager::FadeOutBGM(float durationSec) {
    if (mCurrentBGM.valid()) {
        Stop(mCurrentBGM, durationSec);
    }
}

void AudioManager::FadeOutBGS(float durationSec) {
    if (mCurrentBGS.valid()) {
        Stop(mCurrentBGS, durationSec);
    }
}

void AudioManager::FadeOutAll(float durationSec) {
    for (auto& [handle, instance] : mSounds) {
        Stop(handle, durationSec);
    }
}

// ==================== 3D Audio ====================
void AudioManager::SetListenerPosition(const Vec3& position) {
    mListenerPosition = position;
    if (mEngine) {
        ma_engine_listener_set_position(mEngine.get(), 0, position.x, position.y, position.z);
    }
}

void AudioManager::SetListenerOrientation(const Vec3& forward, const Vec3& up) {
    mListenerForward = forward;
    mListenerUp = up;
    if (mEngine) {
        ma_engine_listener_set_direction(mEngine.get(), 0, forward.x, forward.y, forward.z);
    }
}

void AudioManager::Set3DPosition(SoundHandle handle, const Vec3& position) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        ma_sound_set_position(it->second.sound.get(), position.x, position.y, position.z);
    }
}

void AudioManager::Set3DVelocity(SoundHandle handle, const Vec3& velocity) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        // miniaudio doesn't directly support velocity, but we can update position over time
        // For now, just store it
    }
}

void AudioManager::Set3DCone(SoundHandle handle, float innerAngle, float outerAngle, float outerVolume) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        ma_sound_set_cone(it->second.sound.get(), innerAngle, outerAngle, outerVolume);
    }
}

void AudioManager::Set3DAttenuation(SoundHandle handle, float minDistance, float maxDistance, float rolloffFactor) {
    auto it = mSounds.find(handle);
    if (it != mSounds.end()) {
        ma_sound_set_attenuation_model(it->second.sound.get(), ma_attenuation_model_inverse);
        ma_sound_set_rolloff(it->second.sound.get(), rolloffFactor);
    }
}

// ==================== Preloading ====================
bool AudioManager::Preload(const std::string& path, AudioType type) {
    // Just load and keep in preloaded map
    auto sound = SoundPtr{new ma_sound()};
    ma_uint32 flags = (type == AudioType::BGM || type == AudioType::BGS) ? MA_SOUND_FLAG_STREAM : 0;
    ma_result result = ma_sound_init_from_file(mEngine.get(), path.c_str(), flags, nullptr, nullptr, sound.get());
    if (result != MA_SUCCESS) {
        RPG_LOG_ERROR("Failed to preload sound: " + path);
        return false;
    }
    mPreloadedSounds[path] = std::move(sound);
    return true;
}

void AudioManager::Unload(const std::string& path) {
    mPreloadedSounds.erase(path);
}

// ==================== Internal Updates ====================
void AudioManager::UpdateFade(float dt) {
    for (auto it = mSounds.begin(); it != mSounds.end(); ) {
        auto& instance = it->second;
        bool erase = false;
        
        if (instance.fadingIn) {
            instance.fadeTimer += dt;
            float t = std::min(instance.fadeTimer / instance.fadeInTime, 1.0f);
            float vol = instance.targetVolume * t;
            ma_sound_set_volume(instance.sound.get(), vol);
            if (t >= 1.0f) instance.fadingIn = false;
        }
        
        if (instance.fadingOut) {
            instance.fadeTimer += dt;
            float t = std::min(instance.fadeTimer / instance.fadeOutTime, 1.0f);
            float vol = instance.targetVolume * (1.0f - t);
            ma_sound_set_volume(instance.sound.get(), vol);
            if (t >= 1.0f) {
                ma_sound_stop(instance.sound.get());
                erase = true;
            }
        }
        
        if (erase) {
            it = mSounds.erase(it);
        } else {
            ++it;
        }
    }
}

void AudioManager::Update3DAudio() {
    // Update listener
    if (mEngine) {
        ma_engine_listener_set_position(mEngine.get(), 0, mListenerPosition.x, mListenerPosition.y, mListenerPosition.z);
        ma_engine_listener_set_direction(mEngine.get(), 0, mListenerForward.x, mListenerForward.y, mListenerForward.z);
    }
}

void AudioManager::CleanupFinishedSounds() {
    for (auto it = mSounds.begin(); it != mSounds.end(); ) {
        if (!ma_sound_is_playing(it->second.sound.get()) && !it->second.fadingOut && !it->second.fadingIn) {
            // Check if it should loop
            if (!it->second.sound) {
                it = mSounds.erase(it);
                continue;
            }
            // Non-looping sounds that finished
            if (onSoundEnd) {
                onSoundEnd(it->first);
            }
            it = mSounds.erase(it);
        } else {
            ++it;
        }
    }
}

// Public update function (call from Engine::Update)
void AudioManager::Update(float dt) {
    UpdateFade(dt);
    Update3DAudio();
    CleanupFinishedSounds();
}

} // namespace rpg