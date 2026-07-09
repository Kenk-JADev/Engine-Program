#include "rpgmaker3d/AudioManager.h"
#include <miniaudio/miniaudio.h>
#include <iostream>

namespace rpg {

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

AudioManager::AudioManager() = default;

AudioManager::~AudioManager() {
    Shutdown();
}

bool AudioManager::Initialize() {
    auto engine = std::make_unique<ma_engine>();
    if (ma_engine_init(nullptr, engine.get()) != MA_SUCCESS) {
        std::cerr << "Failed to initialize miniaudio engine" << std::endl;
        return false;
    }
    mEngine.reset(engine.release());
    return true;
}

void AudioManager::Shutdown() {
    StopMusic();
    mSounds.clear();
    mEngine.reset();
}

bool AudioManager::LoadSound(const std::string& name, const std::string& path) {
    if (!mEngine) return false;

    auto sound = std::make_unique<ma_sound>();
    if (ma_sound_init_from_file(mEngine.get(), path.c_str(), 0, nullptr, nullptr, sound.get()) != MA_SUCCESS) {
        std::cerr << "Failed to load sound: " << path << std::endl;
        return false;
    }

    mSounds[name].reset(sound.release());
    return true;
}

void AudioManager::PlaySound(const std::string& name, bool loop) {
    auto it = mSounds.find(name);
    if (it == mSounds.end()) return;
    ma_sound_set_looping(it->second.get(), loop ? MA_TRUE : MA_FALSE);
    ma_sound_start(it->second.get());
}

void AudioManager::StopSound(const std::string& name) {
    auto it = mSounds.find(name);
    if (it == mSounds.end()) return;
    ma_sound_stop(it->second.get());
}

bool AudioManager::PlayMusic(const std::string& path, bool loop) {
    if (!mEngine) return false;
    StopMusic();

    auto sound = std::make_unique<ma_sound>();
    if (ma_sound_init_from_file(mEngine.get(), path.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, sound.get()) != MA_SUCCESS) {
        std::cerr << "Failed to load music: " << path << std::endl;
        return false;
    }

    mMusic.reset(sound.release());
    ma_sound_set_looping(mMusic.get(), loop ? MA_TRUE : MA_FALSE);
    ma_sound_start(mMusic.get());
    return true;
}

void AudioManager::StopMusic() {
    if (mMusic) {
        ma_sound_stop(mMusic.get());
        mMusic.reset();
    }
}

void AudioManager::PauseMusic() {
    if (mMusic) ma_sound_stop(mMusic.get());
}

void AudioManager::ResumeMusic() {
    if (mMusic) ma_sound_start(mMusic.get());
}

void AudioManager::SetMasterVolume(float volume) {
    if (mEngine) ma_engine_set_volume(mEngine.get(), volume);
}

} // namespace rpg
