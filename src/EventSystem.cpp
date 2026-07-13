#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/AudioManager.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace rpg {

const EventPage* MapEvent::GetCurrentPage() const {
    if (pages.empty()) return nullptr;
    if (currentPage < 0 || currentPage >= (int)pages.size()) return nullptr;
    return &pages[currentPage];
}

EventInterpreter::EventInterpreter() = default;

void EventInterpreter::Setup(const std::vector<EventCommand>& list, int eventId) {
    mList = list;
    mIndex = 0;
    mEventId = eventId;
    mRunning = !list.empty();
    mWaitTime = 0.0f;
    mPausedForMessage = false;
    mBranchDepth = 0;
    mBranchResult = true;
}

void EventInterpreter::Clear() {
    mList.clear();
    mIndex = 0;
    mRunning = false;
    mWaitTime = 0.0f;
    mPausedForMessage = false;
}

void EventInterpreter::Update(float dt) {
    if (!mRunning) return;
    if (mPausedForMessage) return;

    if (mWaitTime > 0.0f) {
        mWaitTime -= dt;
        if (mWaitTime > 0) return;
        mWaitTime = 0.0f;
    }

    int executed = 0;
    while (mRunning && mIndex < mList.size() && executed < 12) {
        if (mPausedForMessage) return;
        const auto& cmd = mList[mIndex];
        bool cont = ExecuteCommand(cmd);
        mIndex++;
        executed++;
        if (!cont) return; // wait / pause
        if (mIndex >= mList.size()) mRunning = false;
    }
}

bool EventInterpreter::ExecuteCommand(const EventCommand& cmd) {
    switch (cmd.code) {
        case EventCommandCode::ShowText: {
            if (onShowText) onShowText(cmd.text.empty() ? "..." : cmd.text);
            mPausedForMessage = true;
            return false;
        }
        case EventCommandCode::ShowChoices: {
            if (onShowChoices) onShowChoices(cmd.text, cmd.param1);
            mPausedForMessage = true;
            return false;
        }
        case EventCommandCode::Wait: {
            float secs = cmd.param1 > 0 ? (cmd.param1 / 60.0f) : 0.5f;
            if (!cmd.text.empty()) {
                try { secs = std::stof(cmd.text); } catch (...) {}
            }
            SetWait(secs);
            return false;
        }
        case EventCommandCode::PlayBGM: {
            if (onPlayBGM) onPlayBGM(cmd.text.empty() ? "bgm" : cmd.text, true);
            return true;
        }
        case EventCommandCode::PlaySE: {
            if (onPlaySE) onPlaySE(cmd.text.empty() ? "se" : cmd.text);
            return true;
        }
        case EventCommandCode::Script: {
            if (onScript) onScript(cmd.text);
            return true;
        }
        case EventCommandCode::ChangeGold: {
            if (onChangeGold) onChangeGold(cmd.param1);
            return true;
        }
        case EventCommandCode::ChangeSwitch: {
            if (onChangeSwitch) onChangeSwitch(cmd.param1, cmd.param2 != 0);
            return true;
        }
        case EventCommandCode::ChangeVariable: {
            if (onChangeVariable) onChangeVariable(cmd.param1, cmd.param2);
            return true;
        }
        case EventCommandCode::TransferPlayer: {
            if (onTransferPlayer)
                onTransferPlayer(cmd.param1, cmd.param2, cmd.param3,
                    cmd.parameters.empty() ? 0 : 0);
            // also support text "map,x,y,z"
            if (!cmd.text.empty() && onTransferPlayer) {
                std::stringstream ss(cmd.text);
                int mapId = 1, x = 0, y = 0, z = 0;
                char sep;
                if (ss >> mapId >> sep >> x >> sep >> y) {
                    if (!(ss >> sep >> z)) z = 0;
                    onTransferPlayer(x, y, z, mapId);
                }
            } else if (onTransferPlayer) {
                onTransferPlayer(cmd.param1, cmd.param2, cmd.param3, 0);
            }
            return true;
        }
        case EventCommandCode::Comment:
            return true;
        case EventCommandCode::ConditionalBranch: {
            mBranchDepth++;
            mBranchResult = true;
            return true;
        }
        case EventCommandCode::EndBranch: {
            if (mBranchDepth > 0) mBranchDepth--;
            return true;
        }
        default:
            return true;
    }
}

// --- EventSystem ---
EventSystem& EventSystem::Get() {
    static EventSystem instance;
    return instance;
}

void EventSystem::Clear() {
    mEvents.clear();
    mCommonEvents.clear();
    mInterpreters.clear();
    mCallbacksBound = false;
}

void EventSystem::AddEvent(const MapEvent& ev) {
    for (auto& e : mEvents) if (e.id == ev.id) { e = ev; return; }
    mEvents.push_back(ev);
}

void EventSystem::RemoveEvent(int id) {
    mEvents.erase(std::remove_if(mEvents.begin(), mEvents.end(),
        [&](const MapEvent& e) { return e.id == id; }), mEvents.end());
}

MapEvent* EventSystem::GetEvent(int id) {
    for (auto& e : mEvents) if (e.id == id) return &e;
    return nullptr;
}

void EventSystem::AddCommonEvent(const CommonEvent& ev) {
    for (auto& e : mCommonEvents) if (e.id == ev.id) { e = ev; return; }
    mCommonEvents.push_back(ev);
}

CommonEvent* EventSystem::GetCommonEvent(int id) {
    for (auto& e : mCommonEvents) if (e.id == id) return &e;
    return nullptr;
}

void EventSystem::WireInterpreter(EventInterpreter& interp) {
    interp.onShowText = [](const std::string& txt) {
        GameUI::Get().ShowMessage(txt);
        RPG_LOG_INFO(std::string("[Event] ") + txt);
    };
    interp.onShowChoices = [](const std::string& txt, int) {
        GameUI::Get().ShowMessage(txt);
    };
    interp.onPlayBGM = [](const std::string& p, bool loop) {
        // Path may be relative; AudioManager handles missing files gracefully
        // We need an engine pointer - use a static weak path via Game only logs if no audio
        RPG_LOG_INFO("[Event] BGM: " + p + (loop ? " (loop)" : ""));
    };
    interp.onPlaySE = [](const std::string& p) {
        RPG_LOG_INFO("[Event] SE: " + p);
    };
    interp.onChangeGold = [](int gold) {
        Game::Get().Party().GainGold(gold);
        RPG_LOG_INFO("[Event] Gold += " + std::to_string(gold) +
                     " (now " + std::to_string(Game::Get().Party().GetGold()) + ")");
    };
    interp.onChangeSwitch = [](int id, bool val) {
        Game::Get().Switches().Set(id, val);
    };
    interp.onChangeVariable = [](int id, int val) {
        Game::Get().Variables().Set(id, val);
    };
    interp.onTransferPlayer = [](int x, int y, int z, int mapId) {
        Game::Get().Player().SetPosition(Vec3((float)x, (float)y, (float)z));
        if (mapId > 0) Game::Get().Map().Setup(mapId);
        RPG_LOG_INFO("[Event] Transfer player to map " + std::to_string(mapId) +
                     " (" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + ")");
    };
    interp.onScript = [](const std::string& code) {
        RPG_LOG_INFO("[Event] Script: " + code);
    };
}

void EventSystem::BindRuntimeCallbacks() {
    mCallbacksBound = true;
}

bool EventSystem::ConditionsMet(const EventPage::Condition& c) const {
    if (c.switch1Valid && !Game::Get().Switches().Get(c.switch1Id)) return false;
    if (c.switch2Valid && !Game::Get().Switches().Get(c.switch2Id)) return false;
    if (c.variableValid && Game::Get().Variables().Get(c.variableId) < c.variableValue) return false;
    return true;
}

void EventSystem::RefreshEventPage(MapEvent& ev) {
    // Pick highest page whose conditions are met (RPG Maker style: last matching)
    int best = 0;
    for (int i = 0; i < (int)ev.pages.size(); ++i) {
        if (ConditionsMet(ev.pages[i].condition)) best = i;
    }
    ev.currentPage = best;
}

void EventSystem::Update(float dt, const Vec3& playerPos) {
    // Resume interpreters waiting on message when message closed
    bool msgBusy = GameUI::Get().Message().IsBusy();
    for (auto& interp : mInterpreters) {
        if (interp->IsWaitingForMessage() && !msgBusy) {
            interp->Resume();
        }
        if (interp->IsRunning()) interp->Update(dt);
    }
    mInterpreters.erase(std::remove_if(mInterpreters.begin(), mInterpreters.end(),
        [](const std::unique_ptr<EventInterpreter>& i) { return !i->IsRunning(); }),
        mInterpreters.end());

    // Autorun / Parallel / Touch triggers
    for (auto& ev : mEvents) {
        if (!ev.enabled || !ev.IsValid()) continue;
        RefreshEventPage(ev);
        const EventPage* page = ev.GetCurrentPage();
        if (!page) continue;
        if (IsEventRunning(ev.id)) continue;

        if (page->trigger == EventTrigger::Autorun || page->trigger == EventTrigger::Parallel) {
            StartEvent(ev.id);
            continue;
        }

        if (page->trigger == EventTrigger::PlayerTouch || page->trigger == EventTrigger::EventTouch) {
            Vec3 ep = ev.worldPos;
            if (glm::length(ep) < 0.001f) {
                ep = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
            }
            float dist = glm::length(Vec3(playerPos.x - ep.x, 0.0f, playerPos.z - ep.z));
            if (dist < 1.1f) {
                StartEvent(ev.id);
            }
        }
    }
}

void EventSystem::TryInteract(const Vec3& playerPos, float radius) {
    if (IsAnyEventRunning()) return;
    float best = radius;
    int bestId = -1;
    for (auto& ev : mEvents) {
        if (!ev.enabled || !ev.IsValid()) continue;
        RefreshEventPage(ev);
        const EventPage* page = ev.GetCurrentPage();
        if (!page || page->trigger != EventTrigger::ActionButton) continue;
        Vec3 ep = ev.worldPos;
        if (glm::length(ep) < 0.001f) ep = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
        float dist = glm::length(Vec3(playerPos.x - ep.x, 0.0f, playerPos.z - ep.z));
        if (dist < best) { best = dist; bestId = ev.id; }
    }
    if (bestId >= 0) StartEvent(bestId);
}

void EventSystem::StartEvent(int eventId) {
    auto* ev = GetEvent(eventId);
    if (!ev || !ev->IsValid()) return;
    RefreshEventPage(*ev);
    const EventPage* page = ev->GetCurrentPage();
    if (!page || page->list.empty()) return;
    if (IsEventRunning(eventId) && page->trigger != EventTrigger::Parallel) return;

    auto interpreter = std::make_unique<EventInterpreter>();
    WireInterpreter(*interpreter);
    interpreter->Setup(page->list, eventId);
    mInterpreters.push_back(std::move(interpreter));
    RPG_LOG_INFO("Started event " + std::to_string(eventId) + " (" + ev->name + ")");
}

bool EventSystem::IsEventRunning(int eventId) const {
    for (auto& it : mInterpreters)
        if (it->GetEventId() == eventId && it->IsRunning()) return true;
    return false;
}

bool EventSystem::IsAnyEventRunning() const {
    for (auto& it : mInterpreters) if (it->IsRunning()) return true;
    return false;
}

bool EventSystem::IsWaitingForMessage() const {
    for (auto& it : mInterpreters) if (it->IsWaitingForMessage()) return true;
    return false;
}

void EventSystem::EnsureDemoEvent() {
    if (!mEvents.empty()) return;

    MapEvent ev;
    ev.id = 1;
    ev.name = "Village Elder";
    ev.x = 2; ev.y = 0; ev.z = 2;
    ev.worldPos = Vec3(2.0f, 0.0f, 2.0f);

    EventPage page;
    page.id = 1;
    page.trigger = EventTrigger::ActionButton;

    EventCommand hello;
    hello.code = EventCommandCode::ShowText;
    hello.text = "Willkommen im RPG Maker 3D Playtest!\nDruecke E in der Naehe von NPCs.\nWASD bewegt den Spieler.";
    page.list.push_back(hello);

    EventCommand gold;
    gold.code = EventCommandCode::ChangeGold;
    gold.param1 = 50;
    page.list.push_back(gold);

    EventCommand goldMsg;
    goldMsg.code = EventCommandCode::ShowText;
    goldMsg.text = "Du erhaeltst 50 Gold.";
    page.list.push_back(goldMsg);

    EventCommand wait;
    wait.code = EventCommandCode::Wait;
    wait.param1 = 30; // 0.5s
    page.list.push_back(wait);

    EventCommand bye;
    bye.code = EventCommandCode::ShowText;
    bye.text = "Viel Erfolg bei deinem Abenteuer!";
    page.list.push_back(bye);

    ev.pages.push_back(page);
    mEvents.push_back(ev);

    // Touch trigger demo
    MapEvent sign;
    sign.id = 2;
    sign.name = "Sign";
    sign.worldPos = Vec3(-2.0f, 0.0f, 1.0f);
    sign.x = -2; sign.z = 1;
    EventPage p2;
    p2.trigger = EventTrigger::PlayerTouch;
    EventCommand t;
    t.code = EventCommandCode::ShowText;
    t.text = "(Schild) Norden: Dorf  |  Sueden: Wald";
    p2.list.push_back(t);
    sign.pages.push_back(p2);
    mEvents.push_back(sign);

    RPG_LOG_INFO("Demo events created (Elder + Sign)");
}

void EventSystem::LoadMapEvents(int mapId, const std::string& projectPath) {
    mCurrentMapId = mapId;
    mEvents.clear();
    mInterpreters.clear();
    // Optional file: projectPath/maps/map{id}_events.json (simple stub for now)
    (void)projectPath;
    EnsureDemoEvent();
    RPG_LOG_INFO("LoadMapEvents map " + std::to_string(mapId) +
                 " (" + std::to_string(mEvents.size()) + " events)");
}

void EventSystem::SaveMapEvents(int mapId, const std::string& projectPath) const {
    (void)mapId; (void)projectPath;
    RPG_LOG_INFO("SaveMapEvents stub for map " + std::to_string(mapId));
}

} // namespace rpg
