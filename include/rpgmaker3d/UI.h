#pragma once
// RPG Maker 3D - UI System (Message Window, Menu etc.)

#include <string>
#include <vector>
#include <functional>
#include "Types.h"

namespace rpg {

struct ChoiceOption {
    std::string text;
    int value = 0;
};

class MessageWindow {
public:
    void Show(const std::string& text);
    void ShowWithChoices(const std::string& text, const std::vector<ChoiceOption>& choices);
    void Hide() { mVisible = false; mText.clear(); }
    bool IsVisible() const { return mVisible; }
    bool IsBusy() const { return mVisible; }

    void Update(float dt);
    void Draw(); // ImGui rendering

    std::function<void(int)> onChoice; // choice index

private:
    std::string mText;
    std::string mDisplayed;
    bool mVisible = false;
    float mTimer = 0.0f;
    float mCharDelay = 0.03f;
    size_t mCharIndex = 0;
    bool mWaitingForInput = false;

    std::vector<ChoiceOption> mChoices;
    int mSelectedChoice = 0;
};

class TitleScreen {
public:
    void Show();
    void Hide() { mVisible = false; }
    bool IsVisible() const { return mVisible; }
    void Update(float dt);
    void Draw();

    std::function<void()> onNewGame;
    std::function<void()> onContinue;
    std::function<void()> onExit;

private:
    bool mVisible = false;
};

class PauseMenu {
public:
    void Show();
    void Hide() { mVisible = false; }
    bool IsVisible() const { return mVisible; }
    void Draw();

    std::function<void()> onResume;
    std::function<void()> onSave;
    std::function<void()> onExitToTitle;

private:
    bool mVisible = false;
    int mSelected = 0;
};

class GameUI {
public:
    static GameUI& Get();

    MessageWindow& Message() { return mMessage; }
    TitleScreen& Title() { return mTitle; }
    PauseMenu& Pause() { return mPause; }

    void Update(float dt);
    void Draw();

    void ShowMessage(const std::string& text);
    void ShowChoices(const std::string& text, const std::vector<std::string>& options, std::function<void(int)> callback);

private:
    GameUI() = default;
    MessageWindow mMessage;
    TitleScreen mTitle;
    PauseMenu mPause;
};

} // namespace rpg
