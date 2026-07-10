#pragma once
// RPG Maker 3D - Event System
// 3D-Version von RPG Maker Events: Map Events, Common Events, Interpreter

#include <string>
#include <vector>
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

enum class EventCommandCode {
    None = 0,
    ShowText = 101,
    ShowChoices = 102,
    InputNumber = 103,
    ChangeGold = 125,
    ChangeItems = 126,
    ChangeSwitch = 121,
    ChangeVariable = 122,
    ChangeSelfSwitch = 123,
    TransferPlayer = 201,
    SetMoveRoute = 205,
    Wait = 230,
    PlayBGM = 241,
    PlaySE = 250,
    Script = 355,
    Comment = 108,
    ConditionalBranch = 111,
    EndBranch = 412,
    Label = 118,
    JumpToLabel = 119,
    ControlSelfSwitch = 123,
    ChangeActorHP = 311,
    BattleProcessing = 301,
    ShopProcessing = 302,
    // 3D extensions
    SpawnEntity = 500,
    MoveEntity = 501,
    RotateEntity = 502,
    PlayAnimation = 503
};

struct EventCommand {
    EventCommandCode code = EventCommandCode::None;
    int indent = 0;
    std::vector<std::string> parameters; // string params (json style)
    // Für einfache Zahlen
    int param1 = 0;
    int param2 = 0;
    int param3 = 0;
    std::string text; // für ShowText etc.
};

struct EventPage {
    int id = 0;
    EventTrigger trigger = EventTrigger::ActionButton;
    bool walkAnime = true;
    bool stepAnime = false;
    bool directionFix = false;
    bool through = false;
    int moveType = 0; // 0 fixed, 1 random, 2 approach, 3 custom
    int moveSpeed = 3;
    int moveFrequency = 3;

    // Conditions
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

    std::string graphicName; // model/sprite name
    int graphicIndex = 0;
    Vec3 direction = Vec3(0, 0, -1);

    std::vector<EventCommand> list; // command list
};

struct MapEvent {
    int id = 0;
    std::string name = "EV001";
    int x = 0, y = 0, z = 0; // map pos
    Vec3 worldPos{0, 0, 0};
    std::vector<EventPage> pages;
    int currentPage = 0;

    bool IsValid() const { return !pages.empty(); }
    const EventPage* GetCurrentPage() const;
};

struct CommonEvent {
    int id = 0;
    std::string name = "Common";
    EventTrigger trigger = EventTrigger::None;
    int switchId = 1;
    std::vector<EventCommand> list;
};

// Laufzeit-Interpreter für Events
class EventInterpreter {
public:
    EventInterpreter();

    void Setup(const std::vector<EventCommand>& list, int eventId = 0);
    void Clear();
    bool IsRunning() const { return mRunning; }
    int GetEventId() const { return mEventId; }

    // Update pro Frame, gibt true zurück wenn ein Wait aktiv ist
    void Update(float dt);

    void SetWait(float seconds) { mWaitTime = seconds; }

    // Callbacks die die Engine setzen kann
    std::function<void(const std::string&)> onShowText;
    std::function<void(const std::string&, int)> onShowChoices; // text, choicesId
    std::function<void(int x, int y, int z, int mapId)> onTransferPlayer;
    std::function<void(const std::string& path, bool loop)> onPlayBGM;
    std::function<void(const std::string& path)> onPlaySE;
    std::function<void(const std::string&)> onScript;
    std::function<void(int gold)> onChangeGold;

private:
    bool ExecuteCommand(const EventCommand& cmd);

    std::vector<EventCommand> mList;
    size_t mIndex = 0;
    int mEventId = 0;
    bool mRunning = false;
    float mWaitTime = 0.0f;
    int mBranchDepth = 0;
    bool mBranchResult = true;
};

// Event Manager hält alle Map Events
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

    void AddCommonEvent(const CommonEvent& ev);
    CommonEvent* GetCommonEvent(int id);

    // Runtime
    void Update(float dt, const Vec3& playerPos);

    void StartEvent(int eventId);
    bool IsEventRunning(int eventId) const;

private:
    EventSystem() = default;
    std::vector<MapEvent> mEvents;
    std::vector<CommonEvent> mCommonEvents;
    std::vector<std::unique_ptr<EventInterpreter>> mInterpreters;
};

} // namespace rpg
