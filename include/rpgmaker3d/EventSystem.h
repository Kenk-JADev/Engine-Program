#pragma once
// RPG Maker 3D - Event System
// Map Events, Common Events, Interpreter
//
// Der Interpreter und die Befehls-Codes folgen dem RPG Maker XP Aufbau
// (vgl. XP_Scripts/Interpreter 1-7.rb), angepasst auf die 3D-Engine:
//  - vollstaendiger XP-Befehlssatz (Codes 101..355)
//  - strukturelle Codes (401..413, 601..603, 655)
//  - Engine-Erweiterungen fuer 3D (Codes 181..183, 500..507)
//
// Kodierung der Befehlsparameter ist in docs/EVENTS-XP.md und im
// Qt-Editor (QtEventCommandCatalog) dokumentiert - beide Seiten nutzen
// dieselbe Kodierung, daher ist der Editor immer synchron zur Engine.

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include "Types.h"

namespace rpg {

enum class EventTrigger {
    None = 0,
    ActionButton = 1,
    PlayerTouch = 2,
    EventTouch = 3,
    Autorun = 4,
    Parallel = 5
};

// XP Move-Typen (Autonome Bewegung)
enum class EventMoveType {
    Fixed = 0,    // Fest
    Random = 1,   // Zufaellig
    Approach = 2, // Annaehern (auf Spieler zu)
    Custom = 3    // Benutzerdefiniert (Move Route)
};

// Richtungen wie XP: 2=unten, 4=links, 6=rechts, 8=oben
enum EventDirection {
    DIR_DOWN = 2,
    DIR_LEFT = 4,
    DIR_RIGHT = 6,
    DIR_UP = 8
};

enum class EventCommandCode {
    None = 0,

    // ============ Seite 1: Nachricht / Ablauf / Spielstatus ============
    ShowText = 101,
    ShowChoices = 102,
    InputNumber = 103,
    ChangeTextOptions = 104,
    ButtonInputProcessing = 105,
    Wait = 106,
    Comment = 108,
    ConditionalBranch = 111,
    Loop = 112,
    BreakLoop = 113,
    ExitEventProcessing = 115,
    EraseEvent = 116,
    CallCommonEvent = 117,
    Label = 118,
    JumpToLabel = 119,
    ControlSwitches = 121,
    ControlVariables = 122,
    ControlSelfSwitch = 123,
    ControlTimer = 124,
    ChangeGold = 125,
    ChangeItems = 126,
    ChangeWeapons = 127,
    ChangeArmor = 128,
    ChangePartyMember = 129,
    ChangeWindowskin = 131,
    ChangeBattleBGM = 132,
    ChangeBattleEndME = 133,
    ChangeSaveAccess = 134,
    ChangeMenuAccess = 135,
    ChangeEncounter = 136,

    // ============ Seite 2: Bewegung / Bildschirm / Bilder / Audio ============
    TransferPlayer = 201,
    SetEventLocation = 202,
    ScrollMap = 203,
    ChangeMapSettings = 204,
    ChangeFogColorTone = 205,
    ChangeFogOpacity = 206,
    ShowAnimation = 207,
    ChangeTransparentFlag = 208,
    SetMoveRoute = 209,
    WaitForMoveCompletion = 210,
    PrepareTransition = 221,
    ExecuteTransition = 222,
    ChangeScreenColorTone = 223,
    ScreenFlash = 224,
    ScreenShake = 225,
    ShowPicture = 231,
    MovePicture = 232,
    RotatePicture = 233,
    ChangePictureColorTone = 234,
    ErasePicture = 235,
    SetWeatherEffects = 236,
    PlayBGM = 241,
    FadeOutBGM = 242,
    PlayBGS = 245,
    FadeOutBGS = 246,
    MemorizeBGM = 247,
    RestoreBGM = 248,
    PlayME = 249,
    PlaySE = 250,
    StopSE = 251,

    // ============ Seite 3: Kampf / Akteure / System ============
    BattleProcessing = 301,
    ShopProcessing = 302,
    NameInputProcessing = 303,
    ChangeHP = 311,
    ChangeSP = 312,
    ChangeState = 313,
    RecoverAll = 314,
    ChangeEXP = 315,
    ChangeLevel = 316,
    ChangeParameters = 317,
    ChangeSkills = 318,
    ChangeEquipment = 319,
    ChangeActorName = 320,
    ChangeActorClass = 321,
    ChangeActorGraphic = 322,
    ChangeEnemyHP = 331,
    ChangeEnemySP = 332,
    ChangeEnemyState = 333,
    EnemyRecoverAll = 334,
    EnemyAppearance = 335,
    EnemyTransform = 336,
    ShowBattleAnimation = 337,
    DealDamage = 338,
    ForceAction = 339,
    AbortBattle = 340,
    OpenMenuScreen = 351,
    OpenSaveScreen = 352,
    GameOver = 353,
    ReturnToTitle = 354,
    Script = 355,

    // ============ Strukturelle Codes (kein eigener Button) ============
    TextLine = 401,        // Folgezeile von Show Text
    WhenChoice = 402,      // "Wenn [x]" in Show Choices
    WhenCancel = 403,      // "Wenn Abbruch" in Show Choices
    ChoicesEnd = 404,      // Ende des Auswahl-Blocks
    CommentLine = 408,     // Folgezeile von Comment
    Else = 411,            // "Sonst"-Zweig
    BranchEnd = 412,       // Ende der Verzweigung
    RepeatAbove = 413,     // Ende einer Loop ("Wiederhole von oben")
    IfWin = 601,           // Kampf gewonnen
    IfEscape = 602,        // Kampf geflohen
    IfLose = 603,          // Kampf verloren
    ScriptLine = 655,      // Folgezeile von Script

    // ============ Engine-Erweiterungen (3D) ============
    // 181..183: frueher belegten diese IDs (104..106) die ScreenText-Befehle.
    // Beim Laden alter Event-Dateien (formatVersion < 2) werden die alten
    // Codes automatisch auf die neuen gemappt (siehe EventSystem.cpp).
    ShowScreenText = 181,   // HUD-Text (normalisierte Position)
    ShowWorldText = 182,    // schwebender Text an Weltposition
    ClearScreenTexts = 183, // alle HUD-Texte loeschen

    SpawnEntity = 500,
    MoveEntity = 501,
    RotateEntity = 502,
    PlayAnimation = 503,
    ShowFloatingDamage = 504,
    PlayParticle = 505,
    SetTimeOfDay = 506,
    SetWeather = 507,

    // ---- Aliase (Abwaertskompatibilitaet im C++-Code) ----
    ChangeSwitch = ControlSwitches,
    ChangeVariable = ControlVariables,
    ChangeSelfSwitch = ControlSelfSwitch,
    ChangeActorHP = ChangeHP,
    ChangeActorMP = ChangeSP,
    ChangeActorState = ChangeState,
    ChangeExp = ChangeEXP,
    EndBranch = BranchEnd
};

struct EventCommand {
    EventCommandCode code = EventCommandCode::None;
    int indent = 0;
    int param1 = 0;
    int param2 = 0;
    int param3 = 0;
    std::string text;
    std::vector<std::string> parameters;
};

struct EventPage {
    int id = 0;
    EventTrigger trigger = EventTrigger::ActionButton;

    // Optionen (XP)
    bool walkAnime = true;        // Bewegungsanimation
    bool stepAnime = false;       // Stopp-Animation
    bool directionFix = false;    // Richtung fixieren
    bool through = false;         // Durchgehbar
    bool alwaysOnTop = false;     // Immer im Vordergrund

    // Autonome Bewegung (XP)
    int moveType = 0;             // EventMoveType
    int moveSpeed = 3;            // 1..6 (1=langsamstes, 6=schnellstes)
    int moveFrequency = 3;        // 1..6
    // Benutzerdefinierte Route (nur moveType == Custom)
    std::string customRoute;      // kompakter Routen-Text (U D L R F T A X W20 ...)
    bool routeRepeat = true;
    bool routeSkippable = true;

    struct Condition {
        bool switch1Valid = false;
        int switch1Id = 1;
        bool switch2Valid = false;
        int switch2Id = 1;
        bool variableValid = false;
        int variableId = 1;
        int variableValue = 0;
        bool selfSwitchValid = false;
        char selfSwitchCh = 'A';
        bool itemValid = false;
        int itemId = 1;
        bool actorValid = false;
        int actorId = 1;
    } condition;

    std::string graphicName;
    int graphicIndex = 0;
    Vec3 direction = Vec3(0, 0, -1);
    int direction2D = DIR_DOWN;   // XP-Richtung (2/4/6/8) - Laufzeit/Bedingungen
    std::vector<EventCommand> list;
};

// Move Route (NPC-Bewegung wie RPG Maker)
enum class MoveRouteCode {
    End = 0,
    MoveDown = 1,
    MoveLeft = 2,
    MoveRight = 3,
    MoveUp = 4,
    MoveForward = 12,
    Random = 10,
    Wait = 15,
    TurnDown = 16,
    TurnLeft = 17,
    TurnRight = 18,
    TurnUp = 19,
    TowardPlayer = 29,
    AwayFromPlayer = 30
};

struct MoveRouteStep {
    MoveRouteCode code = MoveRouteCode::End;
    int param = 0; // wait frames etc.
};

struct MoveRoute {
    std::vector<MoveRouteStep> list;
    bool repeat = true;
    bool skippable = true;
    int stepIndex = 0;
    float waitTimer = 0.0f;
};

struct MapEvent {
    int id = 0;
    std::string name = "EV001";
    int x = 0, y = 0, z = 0;
    Vec3 worldPos{0, 0, 0};
    std::vector<EventPage> pages;
    int currentPage = 0;
    bool enabled = true;
    bool erased = false;          // Laufzeit: "Event loeschen" (bis Map-Reload)
    bool routeForcing = false;    // Laufzeit: erzwungene Route laeuft
    int direction = DIR_DOWN;     // Laufzeit: Blickrichtung (2/4/6/8)
    float moveTimer = 0.0f;       // Laufzeit: autonome Bewegung Takt

    // Runtime move route (aus Seite oder SetMoveRoute-Befehl)
    MoveRoute moveRoute;
    bool hasMoveRoute = false;

    bool IsValid() const { return !pages.empty(); }
    const EventPage* GetCurrentPage() const;
    EventPage* GetCurrentPage();
};

struct CommonEvent {
    int id = 0;
    std::string name = "Common";
    EventTrigger trigger = EventTrigger::None;
    int switchId = 1;
    std::vector<EventCommand> list;
};

// Zustand fuer Bildschirm-Effekte (Screen Flash / Shake / Color Tone)
struct ScreenEffects {
    float flashTimer = 0.0f;
    float flashDuration = 0.0f;
    Color flashColor{1, 1, 1, 0};
    float shakeTimer = 0.0f;
    float shakeDuration = 0.0f;
    int shakePower = 5;
    int shakeSpeed = 10;
    Color toneTarget{0, 0, 0, 0}; // -255..255 pro Kanal (a = Grauanteil)
    Color toneCurrent{0, 0, 0, 0};
    float toneDuration = 0.0f;
    float toneElapsed = 0.0f;
    void Update(float dt);
};
ScreenEffects& GetScreenEffects();

class EventInterpreter {
public:
    EventInterpreter();
    ~EventInterpreter();

    void Setup(const std::vector<EventCommand>& list, int eventId = 0, int mapId = 0);
    void Clear();
    bool IsRunning() const { return !mList.empty(); }
    int GetEventId() const { return mEventId; }
    void Update(float dt);
    void SetWait(float seconds) { mWaitTime = seconds; }
    void Resume() { mMessageWaiting = false; }
    bool IsWaitingForMessage() const { return mMessageWaiting; }
    bool IsWaitingForChoice() const { return mChoiceWaiting; }
    bool IsWaitingForNumberInput() const { return mNumberWaiting; }
    void SetChoiceResult(int index);   // 0..3 = Wahl, 4 = Abbruch-Zweig
    void SetNumberResult(int value);
    /// XP: Events (nicht-parallel) sperren die Spielerbewegung waehrend sie laufen
    void SetBlocking(bool b) { mBlocking = b; }
    bool IsBlocking() const { return mBlocking; }

    // ---- Engine-Callbacks (von EventSystem::WireInterpreter gesetzt) ----
    std::function<void(const std::string&)> onShowText;
    std::function<void(const std::string&, int)> onShowChoices; // (formatierter Text, cancel)
    std::function<void(int x, int y, int z, int mapId)> onTransferPlayer;
    std::function<void(const std::string& path, bool loop)> onPlayBGM;
    std::function<void(const std::string& path)> onPlaySE;
    std::function<void(const std::string&)> onScript;
    std::function<void(int gold)> onChangeGold;
    std::function<void(int id, bool value)> onChangeSwitch;
    std::function<void(int id, int value)> onChangeVariable;
    std::function<void(const std::string&, float, float, float, float, float, float)> onShowScreenText;
    std::function<void(const std::string&, float, float, float, float, float, float, float)> onShowWorldText;
    std::function<void()> onClearScreenTexts;
    std::function<void(int itemId, int amount)> onChangeItems;
    std::function<void(int actorId, int hp)> onChangeActorHP;
    std::function<void(int troopId, bool canEscape, bool canLose)> onBattleProcessing;
    std::function<void(const std::vector<int>& itemIds)> onShopProcessing;
    std::function<void(int slot)> onOpenSave;
    std::function<void(int slot)> onOpenLoad;
    std::function<void()> onGameOver;
    std::function<void()> onReturnToTitle;
    std::function<void(int commonEventId)> onCallCommonEvent;
    std::function<void(int actorId)> onRecoverAll;
    std::function<void(int actorId, int exp)> onChangeExp;
    std::function<void(int actorId, int level)> onChangeLevel;
    std::function<void(int eventId, char ch, bool value)> onChangeSelfSwitch;
    std::function<void(int eventId, const MoveRoute& route)> onSetMoveRoute;

    // Zusaetzliche Abfragen (Engine-kontext), optional:
    std::function<int()> pollButtonCode;              // XP-Tastencode oder 0
    std::function<bool()> isAnyRouteForcing;          // Wait-For-Move
    std::function<void(int eventId)> eraseEvent;      // Event loeschen (116)
    std::function<void(int eventId, int x, int z)> setEventLocation; // (202)
    std::function<int(int digits, int initial, std::function<void(int)>)> showNumberInput;   // (103)
    std::function<void(int actorId, int maxChars, std::function<void(const std::string&)>)> showNameInput; // (303)
    std::function<int(int eventId)> getEventDirection;
    std::function<bool(int index)> isEnemyAppeared;

private:
    bool ExecuteCommand();        // XP execute_command
    bool CommandSkip();           // XP command_skip
    int  CurrentIndent() const;
    bool EvalCondition(const EventCommand& cmd);
    int  ResolveOperand(const EventCommand& cmd, int index0) const; // Variablen-Operand
    void ApplyToActors(int actorIdOrAll, const std::function<void(int)>& fn);
    int  FindLabel(const std::string& name) const;

    std::vector<EventCommand> mList;
    size_t mIndex = 0;
    int mEventId = 0;
    int mMapId = 0;
    float mWaitTime = 0.0f;
    bool mMessageWaiting = false;
    bool mChoiceWaiting = false;
    bool mNumberWaiting = false;
    bool mNameWaiting = false;
    bool mShopWaiting = false;   // Shop-Bildschirm (302) offen
    bool mSaveWaiting = false;   // Speicherbildschirm (352) offen
    bool mMoveRouteWaiting = false;
    int mButtonInputVariableId = 0;
    int mChoiceIndent = 0;
    bool mBlocking = true;        // Action/Autorun = true, Parallel = false
    std::map<int, int> mBranch;   // indent -> Zustand (0/1 = Bedingung, 0..3 Wahl, 4 Abbruch)
    std::unique_ptr<EventInterpreter> mChild; // Call Common Event
    int mLoopSafety = 0;

    friend class EventSystem;
};

// Optional: Ruby-Script-Runner fuer Event-Befehl "Script" (Engine setzt das)
void EventSystem_SetScriptRunner(std::function<void(const std::string&)> fn);
// Optional: Tasten-Abfrage fuer "Button Input Processing" / Bedingung "Taste"
void EventSystem_SetButtonProvider(std::function<int()> fn);
/// Audio-Bruecke fuer Event-Befehle + Karten-Autoplay:
/// Die Engine injiziert hier ihre Wiedergabe (die Projekt-Pfadaufloesung
/// nach Audio/BGM|BGS|ME|SE passiert engine-seitig).
/// kind: 0=BGM, 1=BGS, 2=ME, 3=SE. Leerer name stoppt die Art (Fadeout).
void EventSystem_SetAudioPlayer(
    std::function<void(const std::string& name, int kind, bool loop)> fn);
/// Bequemer Aufruf ueber die Bruecke (no-op, wenn nichts injiziert wurde).
void EventSystem_PlayAudio(const std::string& name, int kind, bool loop);

/// Map-Wechsel-Bruecke: Die Engine injiziert hier das Nachladen von
/// Karten-Visualisierung + Events (Transfer-Befehl 201, Savegame laden).
/// Vorher wechselte nur die GameMap-ID - die alte Karte blieb sichtbar.
void EventSystem_SetMapChangeHandler(std::function<void(int mapId)> fn);
/// Loest den injizierten Handler aus (no-op, wenn nichts injiziert wurde).
void EventSystem_NotifyMapChanged(int mapId);

class EventSystem {
public:
    static EventSystem& Get();

    void Clear();
    void LoadMapEvents(int mapId, const std::string& projectPath);
    void SaveMapEvents(int mapId, const std::string& projectPath) const;

    void AddEvent(const MapEvent& ev);
    void RemoveEvent(int id);

    MapEvent* GetEvent(int id);
    const std::vector<MapEvent>& GetEvents() const { return mEvents; }
    std::vector<MapEvent>& GetEvents() { return mEvents; }

    void AddCommonEvent(const CommonEvent& ev);
    CommonEvent* GetCommonEvent(int id);
    std::vector<CommonEvent>& GetCommonEvents() { return mCommonEvents; }
    const std::vector<CommonEvent>& GetCommonEvents() const { return mCommonEvents; }

    /// Wire engine callbacks (message UI, audio, transfer, etc.)
    void BindRuntimeCallbacks();

    void Update(float dt, const Vec3& playerPos);
    /// Player pressed interact (E / Enter) near an event
    void TryInteract(const Vec3& playerPos, float radius = 1.35f);

    void StartEvent(int eventId);
    bool IsEventRunning(int eventId) const;
    bool IsAnyEventRunning() const;
    bool IsWaitingForMessage() const;
    /// Laeuft ein blockierendes Event (Action/Autorun/CE-Autorun)? -> Spieler sperren
    bool IsBlockingEventRunning() const;

    /// "Event loeschen" (116): Event bis Map-Reload verbergen
    void EraseEvent(int eventId);
    /// Position eines Events setzen (202)
    void SetEventLocation(int eventId, int x, int z);
    /// Laeuft noch eine erzwungene Move Route? (210 / 209 wait)
    bool IsAnyRouteForcing() const;

    /// Create a demo NPC event near origin for playtesting
    void EnsureDemoEvent();

    int GetCurrentMapId() const { return mCurrentMapId; }
    void SetCurrentMapId(int id) { mCurrentMapId = id; }

    /// Choice result (0..3) oder Abbruch (4)
    void SetChoiceResult(int index);
    int ConsumeChoiceResult(); // -1 if none

    /// Refresh aller Event-Seiten (z.B. nach Switch-Aenderung)
    void RefreshAllPages();

private:
    EventSystem() = default;
    void WireInterpreter(EventInterpreter& interp);
    bool ConditionsMet(const EventPage::Condition& c, int eventId) const;
    void RefreshEventPage(MapEvent& ev);
    void UpdateMoveRoutes(float dt, const Vec3& playerPos);
    void StartCustomRoute(MapEvent& ev, const std::string& routeText, bool repeat, bool skippable);

    std::vector<MapEvent> mEvents;
    std::vector<CommonEvent> mCommonEvents;
    std::vector<std::unique_ptr<EventInterpreter>> mInterpreters;
    int mCurrentMapId = 1;
    bool mCallbacksBound = false;
    int mLastChoice = -1;

    friend class EventInterpreter; // fuer Child-Interpreter-Verdrahtung (117)
};

} // namespace rpg
