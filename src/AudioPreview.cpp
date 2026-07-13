#include "rpgmaker3d/AudioPreview.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Logger.h"
#include <imgui.h>
#include <filesystem>

namespace rpg {

AudioPreview::AudioPreview(AudioManager& audio) : mAudio(audio) {
}

void AudioPreview::LoadAndPlay(const std::string& path, bool loop) {
    mSelectedAudio = path;
    mAudio.SetMasterVolume(mVolume);
    mAudio.PlaySE(path, loop, mVolume);
    RPG_LOG_INFO("Playing sound: " + path);
}

void AudioPreview::DrawUI() {
    ImGui::Begin("Audio Vorschau");

    ImGui::SliderFloat("Lautstärke", &mVolume, 0.0f, 1.0f);
    ImGui::Checkbox("Loop", &mLoop);
    ImGui::SliderFloat("Pitch", &mPitch, 0.5f, 2.0f);
    ImGui::SliderFloat("Pan", &mPan, -1.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("Audio-Dateien");

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
            ImGui::Text("Kein assets/audio Ordner gefunden.");
        }
    } catch (...) {
        ImGui::Text("Audio-Ordner konnte nicht gelesen werden.");
    }

    ImGui::Separator();

    if (!mSelectedAudio.empty()) {
        ImGui::Text("Ausgewählt: %s", mSelectedAudio.c_str());

        if (ImGui::Button("Als SE abspielen")) {
            mAudio.SetMasterVolume(mVolume);
            mAudio.PlaySE(mSelectedAudio, mLoop, mVolume, mPitch);
            RPG_LOG_INFO("Spiele SE: " + mSelectedAudio);
        }
        ImGui::SameLine();
        if (ImGui::Button("Als BGM abspielen")) {
            mAudio.SetMasterVolume(mVolume);
            mAudio.PlayBGM(mSelectedAudio, mLoop, mVolume, mPitch);
            RPG_LOG_INFO("Spiele BGM: " + mSelectedAudio);
        }
        ImGui::SameLine();
        if (ImGui::Button("Als BGS abspielen")) {
            mAudio.SetMasterVolume(mVolume);
            mAudio.PlayBGS(mSelectedAudio, mLoop, mVolume, mPitch);
            RPG_LOG_INFO("Spiele BGS: " + mSelectedAudio);
        }
        ImGui::SameLine();
        if (ImGui::Button("Als ME abspielen")) {
            mAudio.SetMasterVolume(mVolume);
            mAudio.PlayME(mSelectedAudio, mLoop, mVolume, mPitch);
            RPG_LOG_INFO("Spiele ME: " + mSelectedAudio);
        }
        ImGui::SameLine();
        if (ImGui::Button("Alles stoppen")) {
            mAudio.FadeOutAll(0.5f);
        }
    }

    ImGui::End();
}

} // namespace rpg
