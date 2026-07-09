#pragma once

#include <string>

namespace rpg {

class AudioManager;

class AudioPreview {
public:
    AudioPreview(AudioManager& audio);

    void DrawUI();

private:
    void DrawFileBrowser(const std::string& path);

    AudioManager& mAudio;
    std::string mSelectedAudio;
    float mVolume = 1.0f;
    bool mLoop = false;
};

} // namespace rpg
