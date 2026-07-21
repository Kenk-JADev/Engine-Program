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
    void Show(const std::string& text, const std::string& speaker, int position /*0=bot 1=mid 2=top*/, const std::string& face = "");
    void ShowWithChoices(const std::string& text, const std::vector<ChoiceOption>& choices);
    void Hide() { mVisible = false; mText.clear(); mChoices.clear(); mSpeakerName.clear(); mFaceName.clear(); }
    bool IsVisible() const { return mVisible; }
    bool IsBusy() const { return mVisible; }
    /// Skip typewriter or close when complete (keyboard)
    void AdvanceInput();

    // Fuer RmlUi-Spiegelung (Ruby UI.show_message -> GameUI -> RmlUi)
    const std::string& GetFullText() const { return mText; }
    const std::string& GetDisplayedText() const { return mDisplayed; }
    bool HasChoices() const { return !mChoices.empty(); }
    int GetChoiceCount() const { return (int)mChoices.size(); }
    int GetSelectedChoice() const { return mSelectedChoice; }
    void SetSelectedChoice(int idx) {
        if (idx >= 0 && idx < (int)mChoices.size()) mSelectedChoice = idx;
    }
    bool IsTextComplete() const { return mCharIndex >= mText.size(); }
    const std::vector<ChoiceOption>& GetChoices() const { return mChoices; }
    /// Auswahl bestaetigen (oder via overrideIdx abbrechen mit -1)
    void ConfirmChoice(int overrideIdx = -2);
    const std::string& GetSpeakerName() const { return mSpeakerName; }
    const std::string& GetFaceName() const { return mFaceName; }
    int GetPosition() const { return mPosition; } // 0 bottom 1 mid 2 top

    void Update(float dt);
    void Draw(); // ImGui rendering (optional, wenn RPGMAKER3D_ENABLE_IMGUI)

    std::function<void(int)> onChoice; // choice index (-1 = Abbruch)

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
    std::string mSpeakerName;
    std::string mFaceName;
    int mPosition = 0; // 0 bottom, 1 mid, 2 top
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
    // XP: Esc oeffnet das Spiel-MENUE (keine improvisierte Pause-Liste mehr).
    // Show/Hide/IsVisible delegieren an GameUI::Menu() - die Callbacks
    // (onResume/onSave/onExitToTitle) verdrahtet die Engine wie bisher.
    void Show();
    void Hide();
    bool IsVisible() const;
    void Draw();

    std::function<void()> onResume;
    std::function<void()> onSave;
    std::function<void()> onExitToTitle; // „Zum Titelbildschirm" (Editor: Playtest-Stop)
    std::function<void()> onQuitGame;    // „Spiel verlassen" (Player: RequestQuit)
};

// ---------------------------------------------------------------------------
// Generisches Listen-Menue im XP-Stil (Titel + nummerierte Eintraege + Cursor)
// Anzeige erfolgt ueber RmlUi (#menu_box); die Tastatursteuerung laeuft in
// GameUI::UpdateModalInput. Basis fuer: Spielmenue (Esc), Gegenstandsliste,
// Speicherbildschirm (4 Slots) und Shop (Kaufen/Verkaufen).
// ---------------------------------------------------------------------------
class MenuWindow {
public:
    struct Entry {
        std::string text;   // komplette Zeile (z. B. "Trank ......... 50 G")
        bool enabled = true; // ausgegraute Eintraege ueberspringt der Cursor
    };

    void Show(const std::string& title, const std::vector<Entry>& items,
              std::function<void(int)> onPick, bool cancelable = true);
    void Hide();
    bool IsVisible() const { return mVisible; }

    void MoveCursor(int dir); // +/-1, ueberspringt deaktivierte Eintraege
    void Confirm();           // ruft onPick(cursor)
    void Cancel();            // onCancel() oder Hide()

    const std::string& GetTitle() const { return mTitle; }
    const std::vector<Entry>& GetItems() const { return mItems; }
    int GetCursor() const { return mCursor; }
    bool IsCancelable() const { return mCancelable; }

    std::function<void(int)> onPick;
    std::function<void()> onCancel;

private:
    std::string mTitle;
    std::vector<Entry> mItems;
    int mCursor = 0;
    bool mVisible = false;
    bool mCancelable = true;
};

class GameUI {
public:
    static GameUI& Get();

    MessageWindow& Message() { return mMessage; }
    TitleScreen& Title() { return mTitle; }
    PauseMenu& Pause() { return mPause; }
    MenuWindow& Menu() { return mMenu; }

    // === XP-Spielmenue (Esc) ===
    // Gegenstaende / Speichern / Spiel beenden / Zurueck - aufgebaut aus den
    // in Pause() verdrahteten Callbacks. Respektiert Game::System().HasMenuAccess().
    void OpenGameMenu();
    // Untermenue: Inventarliste (Items mit Anzahl; Enter: benutzen/info)
    void OpenItemsMenu();
    // Untermenue: Ziel fuer benutzbaren Gegenstand (Heil-Items, XP)
    void OpenItemTargetMenu(int itemId);
    // Untermenue: Fertigkeiten eines Mitglieds (Heil-Skills benutzbar, XP)
    void OpenSkillsMenu();
    // Untermenue: Ausruestung eines Mitglieds (Waffe/Schild/Helm/Koerper/Accessoire)
    void OpenEquipMenu();
    /// Loest einen Bilddateinamen gegen Projekt-/Engine-Ordner auf
    /// (nutzt den von der Engine injizierten Resolver; leer = nicht gefunden)
    static std::string ResolvePicturePath(const std::string& filename);
    // Untermenue: Gruppenmitglieder-Status (Level/HP/MP/EXP)
    void OpenStatusMenu();

    // === XP-Speicherbildschirm (4 Slots mit Infozeile) ===
    // saveMode=true: Enter speichert in den Slot; false: nur belegte Slots ladbar.
    // onClosed wird (auch bei Abbruch) genau einmal aufgerufen - nutzt der
    // Event-Interpreter, um nach dem Schliessen weiterzulaufen.
    void ShowSaveScreen(bool saveMode, std::function<void()> onClosed = nullptr);

    // === XP-Laden (Event-Befehl 302) ===
    // Kaufen/Verkaufen/Abbrechen, Preise aus der Datenbank; Enter kauft 1x.
    void ShowShop(const std::vector<int>& itemIds, std::function<void()> onClosed = nullptr);
    bool IsShopActive() const { return mShopActive; }

    void Update(float dt);
    void Draw();

    void ShowMessage(const std::string& text);
    void ShowMessage(const std::string& text, const std::string& speaker, int position = 0, const std::string& face = "");
    // cancelAllowed = XP "Bei Abbruch: Abbruch nicht erlaubt" -> Escape sperren
    void ShowChoices(const std::string& text, const std::vector<std::string>& options, std::function<void(int)> callback, bool cancelAllowed = true);

    // === Zahleneingabe (Event-Befehl 103) ===
    void ShowNumberInput(const std::string& prompt, int digits, int initial, std::function<void(int)> onDone);
    bool IsNumberInputActive() const { return mNumberActive; }
    int GetNumberInputValue() const { return mNumberValue; }
    int GetNumberInputDigits() const { return mNumberDigits; }
    int GetNumberInputCursor() const { return mNumberCursor; }
    const std::string& GetNumberInputPrompt() const { return mNumberPrompt; }

    // === Namenseingabe (Event-Befehl 303) ===
    void ShowNameInput(const std::string& prompt, const std::string& initial, int maxChars, std::function<void(const std::string&)> onDone);
    bool IsNameInputActive() const { return mNameActive; }
    const std::string& GetNameInputText() const { return mNameText; }
    int GetNameInputMaxChars() const { return mNameMaxChars; }
    const std::string& GetNameInputPrompt() const { return mNamePrompt; }

    /// Tastatursteuerung fuer Choices / Zahlen- / Namenseingabe.
    /// Wird von Engine::Update im PlayMode vor dem Message-Advance aufgerufen.
    void UpdateModalInput(class Input& input);

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
    void SetPictureSize(int id, float sizeX, float sizeY); // normalisiert 0..1
    /// Pfad-Aufloeser fuer Bilddateien (Engine injiziert Projekt-Suche
    /// nach Graphics/Pictures|Titles etc.; XP-Ordnerstruktur).
    static void SetPicturePathResolver(
        std::function<std::string(const std::string&)> fn);
    // Easing helpers
    static float ApplyEasing(float t, int easingType);

private:
    // Interne Untermenues (Member-Auswahl -> Listen)
    void OpenSkillListMenu(int memberIndex);
    void OpenSkillTargetMenu(int memberIndex, int skillId);
    void OpenEquipSlotMenu(int memberIndex, int slotKind); // -1 Uebersicht, -2 Waffe, 0..3 Ruestungstyp

    void UpdateScreenTexts(float dt);
    void DrawScreenTexts();
    void UpdatePictures(float dt);
    void DrawPictures();
    bool LoadPictureTexture(ScreenPicture& pic);

    GameUI() = default;
    MessageWindow mMessage;
    TitleScreen mTitle;
    PauseMenu mPause;
    MenuWindow mMenu;

    // Shop-State (ShowShop)
    bool mShopActive = false;
    std::vector<int> mShopGoods;
    std::function<void()> mShopOnClosed;

    // Choices: Abbruch per Escape erlaubt? (XP: "Abbruch nicht erlaubt")
    bool mChoiceCancelAllowed = true;

    // Zahleneingabe (103)
    bool mNumberActive = false;
    int mNumberValue = 0;
    int mNumberDigits = 4;
    int mNumberCursor = 0;
    std::string mNumberPrompt;
    std::function<void(int)> mNumberDone;

    // Namenseingabe (303)
    bool mNameActive = false;
    std::string mNameText;
    std::string mNameInitial;
    int mNameMaxChars = 8;
    std::string mNamePrompt;
    std::function<void(const std::string&)> mNameDone;

    std::vector<ScreenText> mScreenTexts;
    int mNextScreenTextId = 1;

    std::vector<ScreenPicture> mPictures;
    int mNextPictureId = 1;
};

} // namespace rpg
