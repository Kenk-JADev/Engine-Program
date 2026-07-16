#pragma once
// RPG Maker 3D - UI System (Message Window, Menu etc.)
// Extended with ScreenText for custom HUD / floating texts

#include <string>
#include <vector>
#include <functional>
#include "Types.h"

namespace rpg {

struct ChoiceOption {
    std::string text;
    int value = 0;
};

// Screen text - on-screen HUD text or world-space floating text
struct ScreenText {
    int id = 0;
    std::string text;
    Vec2 screenPos{0.5f, 0.1f}; // normalized 0..1 (0 top-left) or pixel if usePixel true
    Vec2 pixelOffset{0.0f, 0.0f};
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    float duration = 3.0f; // 0 = infinite
    float elapsed = 0.0f;
    bool fading = true;
    bool worldSpace = false;
    Vec3 worldPos{0.0f};
    float fontScale = 1.0f;
    bool centered = true;
    bool withBackground = false;
    Color bgColor{0.0f, 0.0f, 0.0f, 0.6f};

    // Tween animation for ScreenText (NEW)
    Vec2 tweenStartPos{0.5f, 0.1f};
    Vec2 tweenTargetPos{0.5f, 0.1f};
    float tweenDuration = 0.0f;
    float tweenElapsed = 0.0f;
    bool isTweening = false;
    int easingType = 2; // 0=linear, 2=easeOutQuad default
    float tweenStartScale = 1.0f;
    float tweenTargetScale = 1.0f;
    bool tweenScale = false;
};

// Screen picture / sprite - image on screen (HUD, Picture, etc.)
struct ScreenPicture {
    int id = 0;
    std::string filename; // path to image
    Vec2 screenPos{0.5f, 0.5f}; // normalized 0..1
    Vec2 size{0.2f, 0.2f}; // normalized size or pixel
    float scale = 1.0f;
    float rotation = 0.0f; // degrees
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    float opacity = 1.0f;
    float duration = 0.0f; // 0 = infinite
    float elapsed = 0.0f;
    bool fading = false;
    bool centered = true;
    bool withBackground = false;
    unsigned int textureId = 0; // loaded texture id
    bool loaded = false;
    std::string name; // picture name for reference

    // Tween animation
    Vec2 tweenStartPos{0.5f, 0.5f};
    Vec2 tweenTargetPos{0.5f, 0.5f};
    float tweenDuration = 0.0f;
    float tweenElapsed = 0.0f;
    bool isTweening = false;
    int easingType = 2; // 0=linear, 1=easeInQuad, 2=easeOutQuad, 3=easeInOutQuad, 4=easeOutBounce, 5=easeInOutSine

    // Scale/Opacity/Rotation tween
    float tweenStartScale = 1.0f;
    float tweenTargetScale = 1.0f;
    float tweenStartOpacity = 1.0f;
    float tweenTargetOpacity = 1.0f;
    float tweenStartRotation = 0.0f;
    float tweenTargetRotation = 0.0f;
    bool tweenScale = false;
    bool tweenOpacity = false;
    bool tweenRotation = false;
};

class MessageWindow {
public:
    void Show(const std::string& text);
    void ShowWithChoices(const std::string& text, const std::vector<ChoiceOption>& choices);
    void Hide() { mVisible = false; mText.clear(); mChoices.clear(); }
    bool IsVisible() const { return mVisible; }
    bool IsBusy() const { return mVisible; }
    /// Skip typewriter or close when complete (keyboard)
    void AdvanceInput();

    // Fuer RmlUi-Spiegelung (Ruby UI.show_message -> GameUI -> RmlUi)
    const std::string& GetFullText() const { return mText; }
    const std::string& GetDisplayedText() const { return mDisplayed; }
    bool HasChoices() const { return !mChoices.empty(); }

    void Update(float dt);
    void Draw(); // ImGui rendering (optional, wenn RPGMAKER3D_ENABLE_IMGUI)

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

    /// Compact playtest/game HUD (HP/Gold/hints)
    void DrawPlayHud(bool playtest);

    // === Screen Text System (NEW) ===
    // Add a screen-space text (0..1 normalized). Returns id. Duration 0 = infinite, else seconds.
    int AddScreenText(const std::string& text, Vec2 screenPos = Vec2(0.5f, 0.1f), Color color = Color(1,1,1,1), float duration = 3.0f, bool centered = true, float scale = 1.0f);
    // Add a world-space floating text that follows a 3D position
    int AddWorldText(const std::string& text, Vec3 worldPos, Color color = Color(1,1,0,1), float duration = 2.5f, float scale = 1.0f);
    void RemoveScreenText(int id);
    void ClearScreenTexts();
    const std::vector<ScreenText>& GetScreenTexts() const { return mScreenTexts; }
    // Tween for ScreenText
    void MoveScreenText(int id, Vec2 targetPos, float duration = 0.8f, int easing = 2);
    void TweenScreenText(int id, Vec2 targetPos, float targetScale, float duration, int easing = 2);

    // === Screen Picture / Sprite System (NEW) - for custom systems like original RPG Maker ===
    int ShowPicture(const std::string& filename, Vec2 screenPos = Vec2(0.5f, 0.5f), float scale = 1.0f, float opacity = 1.0f, float duration = 0.0f, const std::string& name = "");
    int ShowPicture(const std::string& filename, const std::string& name, Vec2 screenPos, float scale = 1.0f, float opacity = 1.0f, float duration = 0.0f);
    void MovePicture(int id, Vec2 targetPos, float duration = 1.0f, int easing = 2);
    void MovePicture(const std::string& name, Vec2 targetPos, float duration = 1.0f, int easing = 2);
    // Tween animations
    void TweenPicture(int id, Vec2 targetPos, float targetScale, float targetOpacity, float targetRotation, float duration, int easing = 2);
    void TweenPictureScale(int id, float targetScale, float duration, int easing = 2);
    void TweenPictureOpacity(int id, float targetOpacity, float duration, int easing = 2);
    void TweenPictureRotation(int id, float targetRotation, float duration, int easing = 2);
    void RemovePicture(int id);
    void RemovePicture(const std::string& name);
    void ClearPictures();
    const std::vector<ScreenPicture>& GetPictures() const { return mPictures; }
    // For custom code: set picture properties (instant)
    void SetPictureOpacity(int id, float opacity);
    void SetPictureScale(int id, float scale);
    void SetPictureRotation(int id, float degrees);
    // Easing helpers
    static float ApplyEasing(float t, int easingType);

private:
    void UpdateScreenTexts(float dt);
    void DrawScreenTexts();
    void UpdatePictures(float dt);
    void DrawPictures();
    bool LoadPictureTexture(ScreenPicture& pic);

    GameUI() = default;
    MessageWindow mMessage;
    TitleScreen mTitle;
    PauseMenu mPause;

    std::vector<ScreenText> mScreenTexts;
    int mNextScreenTextId = 1;

    std::vector<ScreenPicture> mPictures;
    int mNextPictureId = 1;
};

} // namespace rpg
