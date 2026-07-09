#include "rpgmaker3d/AudioPreview.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Logger.h"
#include <imgui.h>
#include <filesystem>

namespace rpg {

AudioPreview::AudioPreview(AudioManager& audio) : mAudio(audio) {
}

void AudioPreview::DrawUI() {
    ImGui::Begin("Audio Preview");

    ImGui::SliderFloat("Volume", &mVolume, 0.0f, 1.0f);
    ImGui::Checkbox("Loop", &mLoop);

    ImGui::Separator();
    ImGui::Text("Audio Files");

    try {
        if (std::filesystem::exists("./assets/audio")) {
            for (const auto& entry : std::filesystem::directory_iterator("./assets/audio")) {
                std::string name = entry.path().filename().string();
                bool selected = (mSelectedAudio == name);
                if (ImGui::Selectable(name.c_str(), selected)) {
                    mSelectedAudio = entry.path().string();
                }
            }
        } else {
            ImGui::Text("No assets/audio folder found.");
        }
    } catch (...) {
        ImGui::Text("Could not read audio folder.");
    }

    ImGui::Separator();

    if (!mSelectedAudio.empty()) {
        ImGui::Text("Selected: %s", mSelectedAudio.c_str());

        if (ImGui::Button("Play Sound")) {
            mAudio.SetMasterVolume(mVolume);
            mAudio.LoadSound("preview", mSelectedAudio);
            mAudio.PlaySound("preview", mLoop);
            RPG_LOG_INFO("Playing sound: " + mSelectedAudio);
        }
        ImGui::SameLine();
        if (ImGui::Button("Play Music")) {
            mAudio.SetMasterVolume(mVolume);
            mAudio.PlayMusic(mSelectedAudio, mLoop);
            RPG_LOG_INFO("Playing music: " + mSelectedAudio);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            mAudio.StopMusic();
            mAudio.StopSound("preview");
        }
    }

    ImGui::End();
}

} // namespace rpg
