#pragma once

#include <string>

namespace rpg {

class AudioManager;

class AudioPreview {
public:
    AudioPreview(AudioManager& audio);

    void DrawUI();
    void LoadAndPlay(const std::string& path, bool loop = false);

private:
    AudioManager& mAudio;
    std::string mSelectedAudio;
    float mVolume = 1.0f;
    float mPitch = 1.0f;
    float mPan = 0.0f;
    bool mLoop = false;
};

} // namespace rpg
