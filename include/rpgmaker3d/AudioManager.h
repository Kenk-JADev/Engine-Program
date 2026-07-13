#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "Types.h"

// Forward declarations for miniaudio types (global namespace)
struct ma_engine;
struct ma_sound;

namespace rpg {

enum class AudioType {
    BGM,    // Background Music (streaming, looping)
    BGS,    // Background Sound (ambience, looping)
    SE,     // Sound Effects (short, usually not looping)
    ME      // Music Effects (fanfares, usually not looping)
};

struct SoundHandle {
    uint32_t id = 0;
    bool valid() const { return id != 0; }
    bool operator==(const SoundHandle& other) const noexcept { return id == other.id; }
};

// Custom hasher for SoundHandle to avoid std::hash specialization issues
struct SoundHandleHash {
    size_t operator()(const SoundHandle& h) const noexcept {
        return std::hash<uint32_t>{}(h.id);
    }
};

// Forward declare deleters using global miniaudio types
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
    void Update(float dt);

    // Core playback
    SoundHandle PlayBGM(const std::string& path, bool loop = true, float volume = 1.0f, float pitch = 1.0f, float fadeInTime = 0.0f);
    SoundHandle PlayBGS(const std::string& path, bool loop = true, float volume = 1.0f, float pitch = 1.0f, float fadeInTime = 0.0f);
    SoundHandle PlaySE(const std::string& path, bool loop = false, float volume = 1.0f, float pitch = 1.0f);
    SoundHandle PlayME(const std::string& path, bool loop = false, float volume = 1.0f, float pitch = 1.0f);

    // Generic play with full control
    SoundHandle Play(const std::string& path, AudioType type, bool loop, float volume, float pitch, float fadeInTime = 0.0f);

    // Control by handle
    void Stop(SoundHandle handle, float fadeOutTime = 0.0f);
    void Pause(SoundHandle handle);
    void Resume(SoundHandle handle);
    void SetVolume(SoundHandle handle, float volume);
    void SetPitch(SoundHandle handle, float pitch);
    void SetLooping(SoundHandle handle, bool loop);
    void SetLoopPoints(SoundHandle handle, float loopStartSec, float loopEndSec);
    void SetPan(SoundHandle handle, float pan); // -1.0 left to 1.0 right

    // Queries
    bool IsPlaying(SoundHandle handle) const;
    bool IsPaused(SoundHandle handle) const;
    float GetPosition(SoundHandle handle) const; // in seconds
    float GetDuration(SoundHandle handle) const; // in seconds

    // Master controls
    void SetMasterVolume(float volume);
    float GetMasterVolume() const { return mMasterVolume; }
    void SetBGMVolume(float volume);
    void SetBGSVolume(float volume);
    void SetSEVolume(float volume);
    void SetMEVolume(float volume);

    // Fade controls
    void FadeOutBGM(float durationSec);
    void FadeOutBGS(float durationSec);
    void FadeOutAll(float durationSec);

    // 3D Audio (for spatial sounds)
    void SetListenerPosition(const Vec3& position);
    void SetListenerOrientation(const Vec3& forward, const Vec3& up);
    void Set3DPosition(SoundHandle handle, const Vec3& position);
    void Set3DVelocity(SoundHandle handle, const Vec3& velocity);
    void Set3DCone(SoundHandle handle, float innerAngle, float outerAngle, float outerVolume);
    void Set3DAttenuation(SoundHandle handle, float minDistance, float maxDistance, float rolloffFactor);

    // Preloading
    bool Preload(const std::string& path, AudioType type);
    void Unload(const std::string& path);

    // Event callbacks
    std::function<void(SoundHandle)> onSoundEnd; // called when sound finishes (non-looping)

private:
    struct SoundInstance {
        SoundPtr sound;
        AudioType type;
        SoundHandle handle;
        bool isStreaming = false;
        float fadeInTime = 0.0f;
        float fadeOutTime = 0.0f;
        float fadeTimer = 0.0f;
        bool fadingIn = false;
        bool fadingOut = false;
        float targetVolume = 1.0f;

        SoundInstance() = default;
        SoundInstance(SoundInstance&& other) noexcept = default;
        SoundInstance& operator=(SoundInstance&& other) noexcept = default;
    };

    EnginePtr mEngine;
    // Use custom hasher to avoid std::hash specialization issues
    std::unordered_map<SoundHandle, SoundInstance, SoundHandleHash> mSounds;
    std::unordered_map<std::string, SoundPtr> mPreloadedSounds;
    uint32_t mNextHandleId = 1;
    
    float mMasterVolume = 1.0f;
    float mBGMVolume = 1.0f;
    float mBGSVolume = 1.0f;
    float mSEVolume = 1.0f;
    float mMEVolume = 1.0f;

    SoundHandle mCurrentBGM;
    SoundHandle mCurrentBGS;

    Vec3 mListenerPosition{0, 0, 0};
    Vec3 mListenerForward{0, 0, -1};
    Vec3 mListenerUp{0, 1, 0};

    void UpdateFade(float dt);
    void Update3DAudio();
    void CleanupFinishedSounds();
    SoundHandle CreateSoundInstance(SoundPtr sound, AudioType type, bool loop, float volume, float pitch);
    float GetTypeVolume(AudioType type) const;
};

} // namespace rpg
