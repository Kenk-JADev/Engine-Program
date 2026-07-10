#include "rpgmaker3d/UI.h"
#include <imgui.h>

namespace rpg {

// --- MessageWindow ---
void MessageWindow::Show(const std::string& text) {
    mText = text;
    mDisplayed.clear();
    mCharIndex = 0;
    mTimer = 0.0f;
    mVisible = true;
    mWaitingForInput = false;
    mChoices.clear();
}
void MessageWindow::ShowWithChoices(const std::string& text, const std::vector<ChoiceOption>& choices) {
    Show(text);
    mChoices = choices;
    mSelectedChoice = 0;
}
void MessageWindow::Update(float dt) {
    if (!mVisible) return;
    if (mCharIndex < mText.size()) {
        mTimer += dt;
        while (mTimer >= mCharDelay && mCharIndex < mText.size()) {
            mDisplayed += mText[mCharIndex++];
            mTimer -= mCharDelay;
        }
    } else {
        if (!mChoices.empty()) mWaitingForInput = true;
    }
}
void MessageWindow::Draw() {
    if (!mVisible) return;
    ImGui::SetNextWindowPos(ImVec2(100, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("Message", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::TextWrapped("%s", mDisplayed.c_str());
    if (mCharIndex >= mText.size()) {
        if (mChoices.empty()) {
            if (ImGui::Button("OK")) {
                mVisible = false;
            }
        } else {
            ImGui::Separator();
            for (size_t i=0;i<mChoices.size();++i) {
                if (ImGui::Selectable(mChoices[i].text.c_str(), (int)i==mSelectedChoice)) {
                    mSelectedChoice = (int)i;
                    if (onChoice) onChoice(mSelectedChoice);
                    mVisible = false;
                }
            }
        }
    } else {
        if (ImGui::Button("Skip")) {
            mDisplayed = mText;
            mCharIndex = mText.size();
        }
    }
    ImGui::End();
}

// --- TitleScreen ---
void TitleScreen::Show() { mVisible = true; }
void TitleScreen::Update(float dt) { (void)dt; }
void TitleScreen::Draw() {
    if (!mVisible) return;
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Title", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x*0.5f - 100, ImGui::GetWindowSize().y*0.3f));
    ImGui::Text("RPG Maker 3D");
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x*0.5f - 50, ImGui::GetWindowSize().y*0.5f));
    if (ImGui::Button("New Game", ImVec2(100,30))) {
        if (onNewGame) onNewGame();
        mVisible=false;
    }
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x*0.5f - 50, ImGui::GetWindowSize().y*0.5f + 40));
    if (ImGui::Button("Continue", ImVec2(100,30))) {
        if (onContinue) onContinue();
    }
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x*0.5f - 50, ImGui::GetWindowSize().y*0.5f + 80));
    if (ImGui::Button("Exit", ImVec2(100,30))) {
        if (onExit) onExit();
    }
    ImGui::End();
}

// --- PauseMenu ---
void PauseMenu::Show() { mVisible = true; mSelected=0; }
void PauseMenu::Draw() {
    if (!mVisible) return;
    ImGui::Begin("Pause");
    const char* options[] = {"Resume", "Save", "Exit to Title"};
    for (int i=0;i<3;++i) {
        bool sel = (mSelected==i);
        if (ImGui::Selectable(options[i], sel)) {
            mSelected=i;
            if (i==0 && onResume) { onResume(); mVisible=false; }
            if (i==1 && onSave) onSave();
            if (i==2 && onExitToTitle) { onExitToTitle(); mVisible=false; }
        }
    }
    ImGui::End();
}

// --- GameUI ---
GameUI& GameUI::Get() {
    static GameUI instance;
    return instance;
}
void GameUI::Update(float dt) {
    mMessage.Update(dt);
    mTitle.Update(dt);
}
void GameUI::Draw() {
    if (mTitle.IsVisible()) mTitle.Draw();
    else if (mPause.IsVisible()) mPause.Draw();
    else if (mMessage.IsVisible()) mMessage.Draw();
}
void GameUI::ShowMessage(const std::string& text) {
    mMessage.Show(text);
}
void GameUI::ShowChoices(const std::string& text, const std::vector<std::string>& options, std::function<void(int)> callback) {
    std::vector<ChoiceOption> choices;
    for (size_t i=0;i<options.size();++i) choices.push_back({options[i], (int)i});
    mMessage.onChoice = callback;
    mMessage.ShowWithChoices(text, choices);
}

} // namespace rpg
