#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/EventSystem.h"
#include <imgui.h>
#include <algorithm>

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
void MessageWindow::AdvanceInput() {
    if (!mVisible) return;
    if (mCharIndex < mText.size()) {
        mDisplayed = mText;
        mCharIndex = mText.size();
        return;
    }
    if (mChoices.empty()) {
        mVisible = false;
    }
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
    ImGuiIO& io = ImGui::GetIO();
    float w = io.DisplaySize.x * 0.72f;
    float h = 160.0f;
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - w) * 0.5f, io.DisplaySize.y - h - 28.0f));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.92f));
    ImGui::Begin("##MessageBox", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
    ImGui::TextWrapped("%s", mDisplayed.c_str());
    ImGui::Dummy(ImVec2(0, 8));
    if (mCharIndex >= mText.size()) {
        if (mChoices.empty()) {
            ImGui::TextDisabled("E / Enter / Space  -  weiter");
            if (ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::Button("OK", ImVec2(100, 0))) {
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
        ImGui::TextDisabled("...");
        if (ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Space, false) || ImGui::Button("Skip")) {
            mDisplayed = mText;
            mCharIndex = mText.size();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
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


void GameUI::DrawPlayHud(bool playtest) {
    if (mTitle.IsVisible() || mPause.IsVisible()) return;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(12, 12));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##PlayHUD", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
    if (playtest) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "PLAYTEST");
        ImGui::SameLine();
        ImGui::TextDisabled("F5 Stop");
    }
    auto& party = Game::Get().Party();
    int hp = 0, maxhp = 0;
    if (!party.Members().empty()) {
        hp = party.Members()[0].hp;
        // rough max from current if no max stored
        maxhp = std::max(hp, 100);
    }
    ImGui::Text("HP %d  |  Gold %d", hp, party.GetGold());
    Vec3 p = Game::Get().Player().GetPosition();
    ImGui::Text("Pos %.1f, %.1f", p.x, p.z);
    if (!EventSystem::Get().IsAnyEventRunning()) {
        ImGui::TextDisabled("E: Sprechen  |  WASD: Bewegen");
    } else if (EventSystem::Get().IsWaitingForMessage()) {
        ImGui::TextColored(ImVec4(1,0.9f,0.4f,1), "Dialog...");
    }
    ImGui::End();
    (void)io;
}
