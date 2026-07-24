#pragma once
// RPG Maker 3D - UI System (Message Window, Menu etc.)
// Extended with ScreenText for custom HUD / floating texts

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include "Types.h"

namespace rpg {

enum class BattleActionType; // BattleSystem.h (Fwd-Dekl, UI.h bleibt leicht)
class Texture; // Fwd-Dekl fuer Face-Cache (UI.cpp inkludiert Texture.h)

// Ware im Laden (Event-Befehl 302, ShowShopGoods):
// text-Kodierung "1,2,w3,a1" - Zahl = Item, w<ID> = Waffe, a<ID> = Ruestung
struct ShopGood {
    enum class Kind { Item = 0, Weapon = 1, Armor = 2 };
    Kind kind = Kind::Item;
    int id = 0;
};

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
    // PAKET 9: XP-Farbton (battlerHue), 0..360 Grad HSL-Drehung beim Laden;
    // 0 = Original. Gecacht wird pro (Pfad, Farbton) — siehe LoadPictureTexture.
    int hue = 0;

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

    // PAKET 9: Treffer-Flash (kurzer Farb-Blitz, XP-Battler-Treffer) und
    // Ziel-Blinken (periodisches Flackern, solange im Zielmenue markiert)
    float flashTimer = 0.0f;                    // >0 = Flash aktiv (laeuft ab)
    float flashDuration = 0.25f;
    Color flashColor{1.0f, 1.0f, 1.0f, 1.0f};   // alpha = Staerke
    bool blinking = false;
    float blinkTime = 0.0f;                     // Uhr fuer die Blinkphase
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
// PAKET 10: Anzeige im GameUI-ImGui-Overlay (MenuWindow::Draw, gerufen aus
// GameUI::Draw) — kein RmlUi mehr. Die Tastatursteuerung laeuft weiterhin in
// GameUI::UpdateModalInput. Basis fuer: Spielmenue (Esc), Gegenstandsliste,
// Speicherbildschirm (4 Slots), Shop (Kaufen/Verkaufen) und Alle
// Battle-Untermenues.
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
    void Draw(); // ImGui-Overlay (ohne ImGui: No-Op, Logik laeuft weiter)

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
    // Untermenue: Ziel fuer benutzbaren Gegenstand (Heil-Items, XP);
    // deadOnly=true listet gefallene Mitglieder (Wiederbelebung, PAKET 22)
    void OpenItemTargetMenu(int itemId, bool deadOnly = false);
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
    // ShowShopGoods verkauft Items, Waffen UND Ruestungen (Kauf wie Verkauf).
    // ShowShop(vector<int>) bleibt als Kompatibilitaets-Variante (nur Items).
    void ShowShop(const std::vector<int>& itemIds, std::function<void()> onClosed = nullptr);
    void ShowShopGoods(const std::vector<ShopGood>& goods, std::function<void()> onClosed = nullptr);
    bool IsShopActive() const { return mShopActive; }

    // === XP-Kampfmenue (Aktionswahl statt Zifferntasten) ===
    // Wird von der Engine geoeffnet, sobald BattleSystem::NeedsInput() true
    // ist. Angriff/Fertigkeit/Gegenstand/Verteidigen/Flucht mit Ziel- und
    // Listen-Untermenues (Esc = zurueck zur vorherigen Auswahl).
    void OpenBattleCommands();
    /// true, solange eine Kampf-Auswahl (Befehle/Skill/Item/Ziel) offen ist
    /// (alles laeuft ueber Menu(); der Getter existiert der Lesbarkeit wegen)
    bool IsBattleMenuOpen() const;

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

    // PAKET 37: Anzeigeflaeche (px) — Engine setzt sie pro Render-Frame
    // aus mWindow, damit RUI-Fenster unabhaengig von ImGui bemessen sind.
    void SetDisplaySize(float w, float h) { mDisplayW = w; mDisplayH = h; }
    float DisplayWidth() const { return mDisplayW; }
    float DisplayHeight() const { return mDisplayH; }
    /// PAKET 10: HUD-Sichtbarkeit (F9; Startwert aus CustomConfig::nativeHud)
    void ToggleHud() { mHudVisible = !mHudVisible; }
    void SetHudVisible(bool v) { mHudVisible = v; }
    bool IsHudVisible() const { return mHudVisible; }

    // === Screen Text System (NEW) ===
    // Add a screen-space text (0..1 normalized). Returns id. Duration 0 = infinite, else seconds.
    int AddScreenText(const std::string& text, Vec2 screenPos = Vec2(0.5f, 0.1f), Color color = Color(1,1,1,1), float duration = 3.0f, bool centered = true, float scale = 1.0f);
    // Add a world-space floating text that follows a 3D position
    int AddWorldText(const std::string& text, Vec3 worldPos, Color color = Color(1,1,0,1), float duration = 2.5f, float scale = 1.0f);
    void RemoveScreenText(int id);
    void ClearScreenTexts();
    /// Text eines bestehenden Screen-Texts aktualisieren (z. B. Kampfstatus)
    void SetScreenText(int id, const std::string& text);
    const std::vector<ScreenText>& GetScreenTexts() const { return mScreenTexts; }
    // Tween for ScreenText
    void MoveScreenText(int id, Vec2 targetPos, float duration = 0.8f, int easing = 2);
    void TweenScreenText(int id, Vec2 targetPos, float targetScale, float duration, int easing = 2);

    // === Screen Picture / Sprite System (NEW) - for custom systems like original RPG Maker ===
    // hue: optionaler XP-Farbton 0..360 (Battler-Grafiken, PAKET 9); 0 = Original.
    int ShowPicture(const std::string& filename, Vec2 screenPos = Vec2(0.5f, 0.5f), float scale = 1.0f, float opacity = 1.0f, float duration = 0.0f, const std::string& name = "", int hue = 0);
    int ShowPicture(const std::string& filename, const std::string& name, Vec2 screenPos, float scale = 1.0f, float opacity = 1.0f, float duration = 0.0f, int hue = 0);
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
    // PAKET 9: XP-Battler-Feedback — kurzer Farb-Blitz (Standard: weiss)
    // und periodisches Ziel-Blinken (Zielmenue-Markierung)
    void FlashPicture(int id, const Color& color = Color(1.0f, 1.0f, 1.0f, 1.0f), float duration = 0.25f);
    void FlashPicture(const std::string& name, const Color& color = Color(1.0f, 1.0f, 1.0f, 1.0f), float duration = 0.25f);
    void SetPictureBlinking(const std::string& name, bool on);
    /// Pfad-Aufloeser fuer Bilddateien (Engine injiziert Projekt-Suche
    /// nach Graphics/Pictures|Titles etc.; XP-Ordnerstruktur).
    static void SetPicturePathResolver(
        std::function<std::string(const std::string&)> fn);

    // === XP-Kampf-Statusfenster (PAKET 9) ===
    // Party-Status unten im Kampf (Gesicht, Name, HP-/MP-Balken, K.O.),
    // ersetzt die einfache Textzeile: Schnappschuss pro Status-Tick setzen,
    // Clear am Kampfende; Draw rendert die Leiste (ImGui-Overlay, wie der
    // Rest der GameUI-HUDs).
    struct BattleStatusEntry {
        std::string name;
        int hp = 0, maxHp = 1;
        int mp = 0, maxMp = 1;
        bool dead = false;
        std::string faceName; // Graphics/Faces/<faceName> (leer = kein Gesicht)
        int faceIndex = 0;    // Index im 4x2-Face-Sheet (VX-Stil), 0 = erstes
        // PAKET 17: hoechstpriorisierter aktiver Zustand („" = keiner) —
        // XP zeigt den Statusnamen im Aktionsstatus-Fenster.
        std::string stateName;
    };
    void SetBattleStatusEntries(std::vector<BattleStatusEntry> entries);
    void ClearBattleStatus();
    bool IsBattleStatusActive() const { return mBattleStatusActive; }
    // Easing helpers
    static float ApplyEasing(float t, int easingType);

private:
    // Interne Untermenues (Member-Auswahl -> Listen)
    void OpenSkillListMenu(int memberIndex);
    // deadOnly=true listet gefallene Mitglieder (Wiederbelebung, PAKET 22)
    void OpenSkillTargetMenu(int memberIndex, int skillId, bool deadOnly = false);
    // PAKET 22: gemeinsame Menue-Anwendung fuer Ziel-, Gruppen- und
    // Wiederbelebungs-Pfade (XP-Scope 4/5/6/7 ohne eigenes Untermenue)
    void UseMenuItemOnMember(int itemId, int targetIndex, bool revive);
    void UseMenuItemOnGroup(int itemId, bool deadOnly);
    void UseMenuSkillOnMember(int memberIndex, int skillId, int targetIndex, bool revive);
    void UseMenuSkillOnGroup(int memberIndex, int skillId, bool deadOnly);
    void OpenEquipSlotMenu(int memberIndex, int slotKind); // -1 Uebersicht, -2 Waffe, 0..3 Ruestungstyp

    // Interne Kampf-Untermenues
    void OpenBattleSkillMenu(int actorIndex);
    void OpenBattleItemMenu(int actorIndex);
    void OpenBattleTargetMenu(int actorIndex, int mode, int id); // Gegner: mode 0=Angriff, 1=Skill, 2=Item
    // Verbuendete: mode 1=Skill, 2=Item; deadOnly=true listet nur
    // Gefallene (XP-Scope „Verbuendeter (tot)", PAKET 22)
    void OpenBattleAllyMenu(int actorIndex, int mode, int id, bool deadOnly = false);
    void ConfirmBattleAction(int actorIndex, BattleActionType type, int id, int targetIndex, bool targetIsActor);

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

    // Shop-State (ShowShop / ShowShopGoods)
    bool mShopActive = false;
    std::vector<ShopGood> mShopGoods;
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

    // XP-Kampf-Statusfenster (PAKET 9): Schnappschuss + Face-Cache
    std::vector<BattleStatusEntry> mBattleStatusEntries;
    bool mBattleStatusActive = false;
    std::unordered_map<std::string, std::shared_ptr<Texture>> mFaceCache;
    unsigned int GetFaceTexture(const std::string& faceName, int& outW, int& outH);
    void DrawBattleStatus();

    // PAKET 10: Modale Fenster im ImGui-Overlay (loest das RmlUi-#menu_box ab):
    // Menue (MenuWindow::Draw), Zahleneingabe, Namenseingabe; Choices zeichnet
    // weiterhin MessageWindow::Draw direkt im Nachrichtenfenster.
    void DrawModalWindows();
    void DrawNumberInput();
    void DrawNameInput();
    bool mHudVisible = true; // HUD-Sichtbarkeit (F9)

    // PAKET 37: Anzeigeflaechen-Groesse OHNE ImGui (Engine setzt sie pro
    // Frame aus mWindow). RUI-Fenster bemessen sich in echten Pixeln.
    float mDisplayW = 1280.0f;
    float mDisplayH = 720.0f;
    // FPS als gleitender Schnitt (DrawPlayHud; ImGui::GetIO().Framerate
    // faellt im ImGui-freien Pfad weg).
    float mFpsEma = 60.0f;
    // Lebenszyklus der RUI-HUD-Fenster: welche Fenster-IDs existieren
    // bereits (stale Fenster verschwindener Texte/Bilder entfernen).
    std::vector<int> mRuiTextIds;
    std::vector<int> mRuiPictureIds;

    // PAKET 11: Bildschirm-Effekte (Befehle 223/224) und Wetter (236) als
    // Vollbild-Overlays; Shake (225) zittert die Kamera (Engine-Seite).
    void DrawScreenEffects();
    void DrawWeather();
};

} // namespace rpg
