#pragma once

#include <string>
#include <memory>
#include <unordered_map>

struct ma_engine;
struct ma_sound;

namespace rpg {

struct SoundDeleter {
    void operator()(ma_sound* s) const;
};

struct EngineDeleter {
    void operator()(ma_engine* e) const;
};

using SoundPtr = std::unique_ptr<ma_sound, SoundDeleter>;
using EnginePtr = std::unique_ptr<ma_engine, EngineDeleter>;

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool Initialize();
    void Shutdown();

    bool LoadSound(const std::string& name, const std::string& path);
    void PlaySound(const std::string& name, bool loop = false);
    void StopSound(const std::string& name);

    bool PlayMusic(const std::string& path, bool loop = true);
    void StopMusic();
    void PauseMusic();
    void ResumeMusic();
    void SetMasterVolume(float volume);

private:
    EnginePtr mEngine;
    SoundPtr mMusic;
    std::unordered_map<std::string, SoundPtr> mSounds;
};

} // namespace rpg
