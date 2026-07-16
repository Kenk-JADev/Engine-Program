#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/JsonUtils.h"
#include "rpgmaker3d/BattleSystem.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/RubyVM.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace rpg {

// Optional: Event-Befehl "Script" -> Ruby (von Engine gesetzt)
static std::function<void(const std::string&)> s_scriptRunner;

void EventSystem_SetScriptRunner(std::function<void(const std::string&)> fn) {
    s_scriptRunner = std::move(fn);
}

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
        // Wenn Branch false: bis EndBranch ueberspringen
        if (!mBranchResult && mBranchDepth > 0 &&
            cmd.code != EventCommandCode::EndBranch &&
            cmd.code != EventCommandCode::ConditionalBranch) {
            mIndex++;
            executed++;
            continue;
        }
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
        case EventCommandCode::ShowScreenText: {
            // text = message, param1 = x*100, param2 = y*100, param3 = duration*10
            // Also support parameters array for color etc.
            if (onShowScreenText) {
                float x = cmd.param1 / 100.0f;
                float y = cmd.param2 / 100.0f;
                float dur = cmd.param3 > 0 ? cmd.param3 / 10.0f : 3.0f;
                // Default centered yellowish
                onShowScreenText(cmd.text, x, y, 1.0f, 1.0f, 0.8f, dur);
            }
            return true;
        }
        case EventCommandCode::ShowWorldText: {
            if (onShowWorldText) {
                float x = (float)cmd.param1;
                float y = (float)cmd.param2;
                float z = (float)cmd.param3;
                // If text contains comma separated coords, try parse
                if (!cmd.text.empty() && cmd.text.find(',') != std::string::npos) {
                    // text format: "Hello" but we use separate, so just use text as is
                }
                onShowWorldText(cmd.text, x, y, z, 1.0f, 1.0f, 0.2f, 2.5f);
            }
            return true;
        }
        case EventCommandCode::ClearScreenTexts: {
            if (onClearScreenTexts) onClearScreenTexts();
            return true;
        }
        case EventCommandCode::ShowFloatingDamage: {
            if (onShowWorldText) {
                onShowWorldText(cmd.text.empty() ? std::to_string(cmd.param1) : cmd.text,
                    (float)cmd.param1, (float)cmd.param2 + 1.0f, (float)cmd.param3,
                    1.0f, 0.2f, 0.2f, 1.5f);
            }
            return true;
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
        case EventCommandCode::ChangeItems: {
            if (onChangeItems) onChangeItems(cmd.param1, cmd.param2);
            return true;
        }
        case EventCommandCode::ChangeActorHP: {
            if (onChangeActorHP) onChangeActorHP(cmd.param1, cmd.param2);
            return true;
        }
        case EventCommandCode::ChangeSelfSwitch: {
            char ch = 'A';
            if (!cmd.text.empty()) ch = (char)std::toupper((unsigned char)cmd.text[0]);
            else if (cmd.param2 >= 0 && cmd.param2 <= 3) ch = (char)('A' + cmd.param2);
            bool val = cmd.param1 != 0;
            if (onChangeSelfSwitch) onChangeSelfSwitch(mEventId, ch, val);
            return true;
        }
        case EventCommandCode::SetMoveRoute: {
            // text: "UULDRW10"  U/D/L/R/F=forward/T=toward/A=away/X=random/W=wait frames in digits
            MoveRoute route;
            route.repeat = cmd.param1 != 0;
            route.skippable = cmd.param2 != 0;
            std::string s = cmd.text;
            for (size_t i = 0; i < s.size(); ++i) {
                char c = (char)std::toupper((unsigned char)s[i]);
                MoveRouteStep st;
                if (c == 'U') st.code = MoveRouteCode::MoveUp;
                else if (c == 'D') st.code = MoveRouteCode::MoveDown;
                else if (c == 'L') st.code = MoveRouteCode::MoveLeft;
                else if (c == 'R') st.code = MoveRouteCode::MoveRight;
                else if (c == 'F') st.code = MoveRouteCode::MoveForward;
                else if (c == 'T') st.code = MoveRouteCode::TowardPlayer;
                else if (c == 'A') st.code = MoveRouteCode::AwayFromPlayer;
                else if (c == 'X') st.code = MoveRouteCode::Random;
                else if (c == 'W') {
                    st.code = MoveRouteCode::Wait;
                    int frames = 20;
                    if (i+1 < s.size() && std::isdigit((unsigned char)s[i+1])) {
                        frames = 0;
                        while (i+1 < s.size() && std::isdigit((unsigned char)s[i+1])) {
                            frames = frames*10 + (s[++i]-'0');
                        }
                    }
                    st.param = frames;
                } else continue;
                route.list.push_back(st);
            }
            if (route.list.empty()) {
                // default patrol
                route.list.push_back({MoveRouteCode::MoveRight,0});
                route.list.push_back({MoveRouteCode::Wait,30});
                route.list.push_back({MoveRouteCode::MoveLeft,0});
                route.list.push_back({MoveRouteCode::Wait,30});
            }
            if (onSetMoveRoute) onSetMoveRoute(mEventId > 0 ? mEventId : cmd.param3, route);
            return true;
        }
        case EventCommandCode::TransferPlayer: {
            if (onTransferPlayer)
                onTransferPlayer(cmd.param1, cmd.param2, cmd.param3,
                    cmd.parameters.empty() ? 0 : 0);
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
        case EventCommandCode::BattleProcessing: {
            // param1 = troopId (0 = text parse / random troop 1)
            int troopId = cmd.param1 > 0 ? cmd.param1 : 1;
            if (!cmd.text.empty()) {
                try { troopId = std::stoi(cmd.text); } catch (...) {}
            }
            if (onBattleProcessing) onBattleProcessing(troopId);
            return true;
        }
        case EventCommandCode::ShopProcessing: {
            std::vector<int> items;
            if (!cmd.text.empty()) {
                std::stringstream ss(cmd.text);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    try { items.push_back(std::stoi(item)); } catch (...) {}
                }
            }
            if (items.empty()) {
                if (cmd.param1 > 0) items.push_back(cmd.param1);
                if (cmd.param2 > 0) items.push_back(cmd.param2);
                if (cmd.param3 > 0) items.push_back(cmd.param3);
            }
            if (items.empty()) items = {1, 2}; // default potions
            if (onShopProcessing) onShopProcessing(items);
            return true;
        }
        case EventCommandCode::RecoverAll: {
            if (onRecoverAll) onRecoverAll(cmd.param1 > 0 ? cmd.param1 : 1);
            return true;
        }
        case EventCommandCode::ChangeExp: {
            if (onChangeExp) onChangeExp(cmd.param1 > 0 ? cmd.param1 : 1, cmd.param2);
            return true;
        }
        case EventCommandCode::ChangeLevel: {
            if (onChangeLevel) onChangeLevel(cmd.param1 > 0 ? cmd.param1 : 1, cmd.param2);
            return true;
        }
        case EventCommandCode::Comment:
            return true;
        case EventCommandCode::ConditionalBranch: {
            // param1=switchId, param2=expected (0/1). text="var:ID:OP:VAL" optional
            mBranchDepth++;
            mBranchResult = true;
            if (cmd.param1 > 0) {
                bool sw = Game::Get().Switches().Get(cmd.param1);
                mBranchResult = (cmd.param2 != 0) ? sw : !sw;
            }
            if (!cmd.text.empty() && cmd.text.rfind("var:", 0) == 0) {
                // var:ID:>=:VAL
                int vid = 0, val = 0;
                char op[4] = {0};
                if (sscanf(cmd.text.c_str(), "var:%d:%2[^:]:%d", &vid, op, &val) >= 3) {
                    int cur = Game::Get().Variables().Get(vid);
                    if (std::string(op) == ">=") mBranchResult = cur >= val;
                    else if (std::string(op) == "<=") mBranchResult = cur <= val;
                    else if (std::string(op) == "==") mBranchResult = cur == val;
                    else if (std::string(op) == "!=") mBranchResult = cur != val;
                    else if (std::string(op) == ">") mBranchResult = cur > val;
                    else if (std::string(op) == "<") mBranchResult = cur < val;
                }
            }
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

void EventSystem::SetChoiceResult(int index) { mLastChoice = index; }
int EventSystem::ConsumeChoiceResult() {
    int v = mLastChoice;
    mLastChoice = -1;
    return v;
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
    interp.onPlayBGM = [](const std::string& p, bool loop) {
        RPG_LOG_INFO("[Event] BGM: " + p + (loop ? " (loop)" : ""));
    };
    interp.onPlaySE = [](const std::string& p) {
        RPG_LOG_INFO("[Event] SE: " + p);
    };
    interp.onChangeGold = [](int gold) {
        Game::Get().Party().GainGold(gold);
        RPG_LOG_INFO("[Event] Gold += " + std::to_string(gold) +
                     " (now " + std::to_string(Game::Get().Party().GetGold()) + ")");
        // Gold popup as world text
        Vec3 pp = Game::Get().Player().GetPosition();
        GameUI::Get().AddWorldText("Gold +" + std::to_string(gold), pp + Vec3(0,1.2f,0), Color(1.0f, 0.9f, 0.2f, 1.0f), 2.0f);
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
        // Ausfuehrung ueber globalen Ruby-Hook (Engine setzt ihn)
        if (s_scriptRunner) s_scriptRunner(code);
    };
    interp.onBattleProcessing = [](int troopId) {
        std::vector<int> enemies;
        if (const auto* troop = Database::Get().GetTroop(troopId)) {
            enemies = troop->members;
        } else {
            enemies = {1}; // fallback slime
        }
        BattleSystem::Get().Setup(enemies, true, false);
        BattleSystem::Get().onMessage = [](const std::string& m) {
            GameUI::Get().ShowMessage(m);
        };
        BattleSystem::Get().onVictory = []() {
            GameUI::Get().ShowMessage("Sieg!");
            GameUI::Get().AddScreenText("VICTORY", Vec2(0.5f, 0.4f), Color(1,0.9f,0.2f,1), 3.0f);
        };
        BattleSystem::Get().onDefeat = []() {
            GameUI::Get().ShowMessage("Niederlage...");
        };
        // Auto-Angriff falls keine UI: erster Input-Frame Attack
        BattleAction act;
        act.type = BattleActionType::Attack;
        act.subjectIndex = 0;
        act.targetIndex = 0;
        BattleSystem::Get().SetAction(act);
        RPG_LOG_INFO("[Event] Battle troop=" + std::to_string(troopId) +
                     " enemies=" + std::to_string(enemies.size()));
        GameUI::Get().ShowMessage("Kampf startet! (Troop " + std::to_string(troopId) + ")");
    };
    interp.onShopProcessing = [](const std::vector<int>& itemIds) {
        std::string list = "Shop: ";
        for (size_t i = 0; i < itemIds.size(); ++i) {
            const auto* it = Database::Get().GetItem(itemIds[i]);
            if (i) list += ", ";
            list += it ? it->name : ("#" + std::to_string(itemIds[i]));
            if (it) list += " (" + std::to_string(it->price) + "G)";
        }
        list += "\n(E kauft erstes Item wenn genug Gold)";
        GameUI::Get().ShowMessage(list);
        // Einfacher Auto-Kauf: erstes Item wenn Gold reicht
        if (!itemIds.empty()) {
            const auto* it = Database::Get().GetItem(itemIds[0]);
            if (it && Game::Get().Party().GetGold() >= it->price) {
                Game::Get().Party().GainGold(-it->price);
                Game::Get().Party().GainItem(it->id, 1);
                RPG_LOG_INFO("[Shop] Bought " + it->name);
            }
        }
    };
    interp.onRecoverAll = [](int actorId) {
        auto* a = Game::Get().Party().GetActor(actorId);
        if (a) { a->RecoverAll(); RPG_LOG_INFO("[Event] RecoverAll actor " + std::to_string(actorId)); }
        else {
            for (auto& m : Game::Get().Party().Members()) m.RecoverAll();
        }
        GameUI::Get().ShowMessage("HP/MP vollstaendig wiederhergestellt!");
    };
    interp.onChangeExp = [](int actorId, int exp) {
        auto* a = Game::Get().Party().GetActor(actorId);
        if (a) {
            a->exp += exp;
            RPG_LOG_INFO("[Event] EXP +" + std::to_string(exp));
            GameUI::Get().AddScreenText("EXP +" + std::to_string(exp), Vec2(0.5f, 0.2f), Color(0.5f,1,0.5f,1), 2.0f);
        }
    };
    interp.onChangeLevel = [](int actorId, int level) {
        auto* a = Game::Get().Party().GetActor(actorId);
        if (a) {
            a->level = std::max(1, level);
            RPG_LOG_INFO("[Event] Level -> " + std::to_string(a->level));
            GameUI::Get().ShowMessage(a->name + " ist nun Level " + std::to_string(a->level) + "!");
        }
    };
    interp.onOpenSave = [](int slot) {
        if (Game::Get().Save(slot > 0 ? slot : 1))
            GameUI::Get().ShowMessage("Spiel gespeichert (Slot " + std::to_string(slot > 0 ? slot : 1) + ").");
        else
            GameUI::Get().ShowMessage("Speichern fehlgeschlagen.");
    };
    interp.onOpenLoad = [](int slot) {
        if (Game::Get().Load(slot > 0 ? slot : 1))
            GameUI::Get().ShowMessage("Spiel geladen (Slot " + std::to_string(slot > 0 ? slot : 1) + ").");
        else
            GameUI::Get().ShowMessage("Kein Spielstand gefunden.");
    };
    // NEW: Screen text callbacks
    interp.onShowScreenText = [](const std::string& txt, float x, float y, float r, float g, float b, float dur) {
        float nx = x > 1.0f ? x / 100.0f : x;
        float ny = y > 1.0f ? y / 100.0f : y;
        if (nx <= 0.0f) nx = 0.5f;
        if (ny <= 0.0f) ny = 0.2f;
        GameUI::Get().AddScreenText(txt, Vec2(nx, ny), Color(r,g,b,1.0f), dur > 0 ? dur : 3.0f);
        RPG_LOG_INFO("[Event] ScreenText: " + txt);
    };
    interp.onShowWorldText = [](const std::string& txt, float x, float y, float z, float r, float g, float b, float dur) {
        Vec3 pos(x,y,z);
        // If x,y,z are 0, use player pos
        if (glm::length(pos) < 0.01f) {
            pos = Game::Get().Player().GetPosition() + Vec3(0,1.0f,0);
        }
        GameUI::Get().AddWorldText(txt, pos, Color(r,g,b,1.0f), dur > 0 ? dur : 2.5f);
        RPG_LOG_INFO("[Event] WorldText: " + txt);
    };
    interp.onClearScreenTexts = []() {
        GameUI::Get().ClearScreenTexts();
    };
    interp.onChangeItems = [](int itemId, int amount) {
        Game::Get().Party().GainItem(itemId, amount);
        RPG_LOG_INFO("[Event] Item " + std::to_string(itemId) + " x" + std::to_string(amount));
    };
    interp.onChangeActorHP = [](int actorId, int hpChange) {
        auto* actor = Game::Get().Party().GetActor(actorId);
        if (actor) {
            int old = actor->hp;
            actor->hp += hpChange;
            if (actor->hp < 0) actor->hp = 0;
            RPG_LOG_INFO("[Event] Actor " + std::to_string(actorId) + " HP " + std::to_string(old) + " -> " + std::to_string(actor->hp));
            if (hpChange < 0) {
                Vec3 pp = Game::Get().Player().GetPosition();
                GameUI::Get().AddWorldText(std::to_string(hpChange) + " HP", pp + Vec3(0,1.0f,0), Color(1,0.2f,0.2f,1), 1.5f);
            }
        }
    };
    interp.onChangeSelfSwitch = [](int eventId, char ch, bool value) {
        int mapId = EventSystem::Get().GetCurrentMapId();
        Game::Get().SelfSwitches().Set(mapId, eventId, ch, value);
        RPG_LOG_INFO(std::string("[Event] SelfSwitch ") + ch + " event " + std::to_string(eventId) +
                     " = " + (value ? "ON" : "OFF"));
    };
    interp.onSetMoveRoute = [](int eventId, const MoveRoute& route) {
        auto* ev = EventSystem::Get().GetEvent(eventId);
        if (!ev) return;
        ev->moveRoute = route;
        ev->moveRoute.stepIndex = 0;
        ev->moveRoute.waitTimer = 0;
        ev->hasMoveRoute = !route.list.empty();
        RPG_LOG_INFO("[Event] MoveRoute set on " + std::to_string(eventId) +
                     " steps=" + std::to_string(route.list.size()));
    };
    interp.onShowChoices = [](const std::string& txt, int) {
        // text: "Frage|OptionA|OptionB|OptionC"
        std::vector<std::string> opts;
        std::string prompt = txt;
        size_t p = txt.find('|');
        if (p != std::string::npos) {
            prompt = txt.substr(0, p);
            std::string rest = txt.substr(p+1);
            size_t start = 0;
            while (start <= rest.size()) {
                size_t n = rest.find('|', start);
                if (n == std::string::npos) { opts.push_back(rest.substr(start)); break; }
                opts.push_back(rest.substr(start, n-start));
                start = n+1;
            }
        }
        if (opts.empty()) opts = {"Ja", "Nein"};
        GameUI::Get().ShowChoices(prompt, opts, [](int idx) {
            EventSystem::Get().SetChoiceResult(idx);
            Game::Get().Variables().Set(0, idx); // last choice in var 0
        });
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
    int best = 0;
    for (int i = 0; i < (int)ev.pages.size(); ++i) {
        const auto& c = ev.pages[i].condition;
        bool ok = ConditionsMet(c);
        if (ok && c.selfSwitchValid) {
            char ch = c.selfSwitchCh ? c.selfSwitchCh : 'A';
            if (!Game::Get().SelfSwitches().Get(mCurrentMapId, ev.id, ch))
                ok = false;
        }
        if (ok) best = i; // highest page that matches (RM style: last matching)
    }
    ev.currentPage = best;
}

void EventSystem::Update(float dt, const Vec3& playerPos) {
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

    // Common Events (Autorun/Parallel) – RPG Maker Style
    for (auto& ce : mCommonEvents) {
        if (ce.list.empty()) continue;
        bool run = false;
        if (ce.trigger == EventTrigger::Autorun || ce.trigger == EventTrigger::Parallel) {
            if (ce.switchId <= 0 || Game::Get().Switches().Get(ce.switchId))
                run = true;
        }
        if (!run) continue;
        // Avoid stacking same common event if already running (id as negative)
        int cid = -ce.id;
        bool already = false;
        for (auto& it : mInterpreters)
            if (it->GetEventId() == cid && it->IsRunning()) { already = true; break; }
        if (already && ce.trigger != EventTrigger::Parallel) continue;
        if (already && ce.trigger == EventTrigger::Parallel) continue; // one instance
        auto interpreter = std::make_unique<EventInterpreter>();
        WireInterpreter(*interpreter);
        interpreter->Setup(ce.list, cid);
        mInterpreters.push_back(std::move(interpreter));
        RPG_LOG_INFO("CommonEvent " + std::to_string(ce.id) + " (" + ce.name + ")");
    }

    UpdateMoveRoutes(dt, playerPos);

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


void EventSystem::UpdateMoveRoutes(float dt, const Vec3& playerPos) {
    for (auto& ev : mEvents) {
        if (!ev.hasMoveRoute || ev.moveRoute.list.empty()) continue;
        auto& mr = ev.moveRoute;
        if (mr.waitTimer > 0) {
            mr.waitTimer -= dt;
            continue;
        }
        if (mr.stepIndex < 0 || mr.stepIndex >= (int)mr.list.size()) {
            if (mr.repeat) mr.stepIndex = 0;
            else { ev.hasMoveRoute = false; continue; }
        }
        const auto& step = mr.list[mr.stepIndex];
        Vec3& pos = ev.worldPos;
        if (glm::length(pos) < 0.001f)
            pos = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
        const float speed = 2.0f * dt;
        auto applyDir = [&](Vec3 d) {
            if (glm::length(d) > 1e-5f) d = glm::normalize(d);
            pos += d * speed * 20.0f; // step roughly one cell over ~0.5s - use fixed step
        };
        // fixed cell step
        const float cell = 1.0f;
        Vec3 delta(0);
        switch (step.code) {
            case MoveRouteCode::MoveUp: delta = Vec3(0,0,-cell); break;
            case MoveRouteCode::MoveDown: delta = Vec3(0,0,cell); break;
            case MoveRouteCode::MoveLeft: delta = Vec3(-cell,0,0); break;
            case MoveRouteCode::MoveRight: delta = Vec3(cell,0,0); break;
            case MoveRouteCode::MoveForward: {
                auto* page = ev.GetCurrentPage();
                Vec3 d = page ? page->direction : Vec3(0,0,-1);
                delta = glm::normalize(d) * cell;
                break;
            }
            case MoveRouteCode::TowardPlayer: {
                Vec3 d = playerPos - pos; d.y = 0;
                if (glm::length(d) > 0.1f) {
                    if (std::fabs(d.x) > std::fabs(d.z))
                        delta = Vec3(d.x > 0 ? cell : -cell, 0, 0);
                    else
                        delta = Vec3(0, 0, d.z > 0 ? cell : -cell);
                }
                break;
            }
            case MoveRouteCode::AwayFromPlayer: {
                Vec3 d = pos - playerPos; d.y = 0;
                if (glm::length(d) > 0.1f) {
                    if (std::fabs(d.x) > std::fabs(d.z))
                        delta = Vec3(d.x > 0 ? cell : -cell, 0, 0);
                    else
                        delta = Vec3(0, 0, d.z > 0 ? cell : -cell);
                }
                break;
            }
            case MoveRouteCode::Random: {
                int r = rand() % 4;
                if (r==0) delta=Vec3(cell,0,0);
                else if (r==1) delta=Vec3(-cell,0,0);
                else if (r==2) delta=Vec3(0,0,cell);
                else delta=Vec3(0,0,-cell);
                break;
            }
            case MoveRouteCode::Wait:
                mr.waitTimer = step.param > 0 ? step.param / 60.0f : 0.3f;
                mr.stepIndex++;
                continue;
            default:
                mr.stepIndex++;
                continue;
        }
        // passability
        Vec3 next = pos + delta;
        if (Game::Get().Map().IsPassableWorld(next.x, next.z) || mr.skippable) {
            if (Game::Get().Map().IsPassableWorld(next.x, next.z)) {
                pos = next;
                ev.x = (int)std::round(pos.x);
                ev.z = (int)std::round(pos.z);
                if (glm::length(delta) > 0.01f && !ev.pages.empty()) {
                    int pi = ev.currentPage;
                    if (pi < 0 || pi >= (int)ev.pages.size()) pi = 0;
                    ev.pages[pi].direction = glm::normalize(delta);
                }
            }
        }
        mr.waitTimer = 0.25f; // pause between steps
        mr.stepIndex++;
        if (mr.stepIndex >= (int)mr.list.size()) {
            if (mr.repeat) mr.stepIndex = 0;
            else ev.hasMoveRoute = false;
        }
    }
}

void EventSystem::EnsureDemoEvent() {
    // Sample Common Event (switch 1 triggers parallel? autorun when switch 1 ON)
    if (mCommonEvents.empty()) {
        CommonEvent ce;
        ce.id = 1;
        ce.name = "Welcome CE";
        ce.trigger = EventTrigger::None; // call manually / switch later
        ce.switchId = 0;
        EventCommand c;
        c.code = EventCommandCode::ShowText;
        c.text = "Common Event: Willkommen! (Scripts steuern das Spiel)";
        ce.list.push_back(c);
        mCommonEvents.push_back(ce);
    }
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
    wait.param1 = 30;
    page.list.push_back(wait);

    EventCommand bye;
    bye.code = EventCommandCode::ShowText;
    bye.text = "Viel Erfolg bei deinem Abenteuer!";
    page.list.push_back(bye);

    ev.pages.push_back(page);
    mEvents.push_back(ev);

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
    // Schild bleibt; Elder bekommt Patrol-MoveRoute
    {
        MoveRoute mr;
        mr.repeat = true;
        mr.list.push_back({MoveRouteCode::MoveRight, 0});
        mr.list.push_back({MoveRouteCode::Wait, 45});
        mr.list.push_back({MoveRouteCode::MoveLeft, 0});
        mr.list.push_back({MoveRouteCode::Wait, 45});
        // apply to elder (id 1) already pushed - fix elder before push instead
    }
    mEvents.push_back(sign);

    // Patrol auf Village Elder
    if (auto* elder = GetEvent(1)) {
        MoveRoute mr;
        mr.repeat = true;
        mr.skippable = true;
        mr.list.push_back({MoveRouteCode::MoveRight, 0});
        mr.list.push_back({MoveRouteCode::Wait, 40});
        mr.list.push_back({MoveRouteCode::MoveLeft, 0});
        mr.list.push_back({MoveRouteCode::Wait, 40});
        elder->moveRoute = mr;
        elder->hasMoveRoute = true;
    }

    RPG_LOG_INFO("Demo events created (Elder + Sign + MoveRoute)");
}

// ==================== JSON Event Persistence ====================

namespace {

std::string ReadFileToString(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string FormatMapEventFileName(int mapId, bool padded) {
    if (padded) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Map%03d_events.json", mapId);
        return std::string(buf);
    } else {
        return "map" + std::to_string(mapId) + "_events.json";
    }
}

std::vector<std::string> FindEventFiles(const std::string& projectPath, int mapId) {
    std::vector<std::string> candidates;
    candidates.push_back(projectPath + "/maps/" + FormatMapEventFileName(mapId, true));
    candidates.push_back(projectPath + "/maps/" + FormatMapEventFileName(mapId, false));
    candidates.push_back(projectPath + "/maps/Map" + std::to_string(mapId) + "_events.json");
    candidates.push_back(projectPath + "/" + FormatMapEventFileName(mapId, true));
    candidates.push_back(projectPath + "/" + FormatMapEventFileName(mapId, false));
    candidates.push_back(projectPath + "/maps/map" + std::to_string(mapId) + ".json");
    return candidates;
}

EventCommand ParseCommandObject(const std::string& obj) {
    using namespace JsonUtils;
    EventCommand cmd;
    int code = 0;
    if (TryParseInt(obj, "code", 0, code)) cmd.code = static_cast<EventCommandCode>(code);
    int iv = 0;
    if (TryParseInt(obj, "indent", 0, iv)) cmd.indent = iv;
    if (TryParseInt(obj, "param1", 0, iv)) cmd.param1 = iv;
    if (TryParseInt(obj, "p1", 0, iv)) cmd.param1 = iv;
    if (TryParseInt(obj, "param2", 0, iv)) cmd.param2 = iv;
    if (TryParseInt(obj, "p2", 0, iv)) cmd.param2 = iv;
    if (TryParseInt(obj, "param3", 0, iv)) cmd.param3 = iv;
    if (TryParseInt(obj, "p3", 0, iv)) cmd.param3 = iv;
    std::string txt;
    if (TryParseString(obj, "text", 0, txt)) cmd.text = txt;
    else if (TryParseString(obj, "t", 0, txt)) cmd.text = txt;

    std::string paramsArr;
    if (FindArrayForKey(obj, "parameters", 0, paramsArr)) {
        // parse string array
        size_t pos = 0;
        while (true) {
            size_t q1 = paramsArr.find('\"', pos);
            if (q1 == std::string::npos) break;
            size_t q2 = paramsArr.find('\"', q1+1);
            if (q2 == std::string::npos) break;
            std::string raw = paramsArr.substr(q1+1, q2-q1-1);
            // handle escaped? simple unescape
            cmd.parameters.push_back(JsonUtils::Unescape(raw));
            pos = q2+1;
        }
    }
    return cmd;
}

EventPage::Condition ParseConditionObject(const std::string& obj) {
    using namespace JsonUtils;
    EventPage::Condition c;
    bool b = false;
    int iv = 0;
    if (TryParseBool(obj, "switch1Valid", 0, b)) c.switch1Valid = b;
    if (TryParseInt(obj, "switch1Id", 0, iv)) c.switch1Id = iv;
    if (TryParseBool(obj, "switch2Valid", 0, b)) c.switch2Valid = b;
    if (TryParseInt(obj, "switch2Id", 0, iv)) c.switch2Id = iv;
    if (TryParseBool(obj, "variableValid", 0, b)) c.variableValid = b;
    if (TryParseInt(obj, "variableId", 0, iv)) c.variableId = iv;
    if (TryParseInt(obj, "variableValue", 0, iv)) c.variableValue = iv;
    if (TryParseBool(obj, "selfSwitchValid", 0, b)) c.selfSwitchValid = b;
    std::string s;
    if (TryParseString(obj, "selfSwitchCh", 0, s) && !s.empty()) c.selfSwitchCh = s[0];
    if (TryParseBool(obj, "itemValid", 0, b)) c.itemValid = b;
    if (TryParseInt(obj, "itemId", 0, iv)) c.itemId = iv;
    if (TryParseBool(obj, "actorValid", 0, b)) c.actorValid = b;
    if (TryParseInt(obj, "actorId", 0, iv)) c.actorId = iv;
    return c;
}

EventPage ParsePageObject(const std::string& obj) {
    using namespace JsonUtils;
    EventPage page;
    int iv = 0;
    if (TryParseInt(obj, "id", 0, iv)) page.id = iv;
    if (TryParseInt(obj, "trigger", 0, iv)) page.trigger = static_cast<EventTrigger>(iv);
    bool b = false;
    if (TryParseBool(obj, "walkAnime", 0, b)) page.walkAnime = b;
    if (TryParseBool(obj, "stepAnime", 0, b)) page.stepAnime = b;
    if (TryParseBool(obj, "directionFix", 0, b)) page.directionFix = b;
    if (TryParseBool(obj, "through", 0, b)) page.through = b;
    if (TryParseInt(obj, "moveType", 0, iv)) page.moveType = iv;
    if (TryParseInt(obj, "moveSpeed", 0, iv)) page.moveSpeed = iv;
    if (TryParseInt(obj, "moveFrequency", 0, iv)) page.moveFrequency = iv;
    std::string s;
    if (TryParseString(obj, "graphicName", 0, s)) page.graphicName = s;
    if (TryParseInt(obj, "graphicIndex", 0, iv)) page.graphicIndex = iv;
    Vec3 dir;
    if (ParseVec3(obj, "direction", 0, dir)) page.direction = dir;

    std::string condObj;
    if (FindObjectForKey(obj, "condition", 0, condObj)) {
        page.condition = ParseConditionObject(condObj);
    }
    std::string listArr;
    if (FindArrayForKey(obj, "list", 0, listArr)) {
        auto cmdObjs = ExtractObjectsFromArray(listArr);
        for (auto& co : cmdObjs) {
            page.list.push_back(ParseCommandObject(co));
        }
    }
    return page;
}

MapEvent ParseMapEventObject(const std::string& obj) {
    using namespace JsonUtils;
    MapEvent ev;
    int iv = 0;
    if (TryParseInt(obj, "id", 0, iv)) ev.id = iv;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) ev.name = name;
    if (TryParseInt(obj, "x", 0, iv)) ev.x = iv;
    if (TryParseInt(obj, "y", 0, iv)) ev.y = iv;
    if (TryParseInt(obj, "z", 0, iv)) ev.z = iv;
    Vec3 wp;
    if (ParseVec3(obj, "worldPos", 0, wp)) ev.worldPos = wp;
    else {
        // try worldPos as separate x,y,z? fallback to x,y,z as world
        ev.worldPos = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
    }
    bool en = true;
    if (TryParseBool(obj, "enabled", 0, en)) ev.enabled = en;

    std::string pagesArr;
    if (FindArrayForKey(obj, "pages", 0, pagesArr)) {
        auto pageObjs = ExtractObjectsFromArray(pagesArr);
        for (auto& po : pageObjs) {
            ev.pages.push_back(ParsePageObject(po));
        }
    }
    return ev;
}

} // anon

void EventSystem::LoadMapEvents(int mapId, const std::string& projectPath) {
    mCurrentMapId = mapId;
    mEvents.clear();
    mInterpreters.clear();

    std::string loadedPath;
    std::string content;
    for (auto& cand : FindEventFiles(projectPath, mapId)) {
        if (std::filesystem::exists(cand)) {
            std::string c = ReadFileToString(cand);
            if (!c.empty()) {
                loadedPath = cand;
                content = c;
                break;
            }
        }
    }

    if (content.empty()) {
        RPG_LOG_INFO("No event file found for map " + std::to_string(mapId) + " in " + projectPath + " - using demo events");
        EnsureDemoEvent();
        return;
    }

    try {
        using namespace JsonUtils;
        std::string eventsArr;
        if (!FindArrayForKey(content, "events", 0, eventsArr)) {
            // Maybe file itself is array of events directly
            if (content.find('\"') != std::string::npos && content.find('[') != std::string::npos) {
                size_t start = content.find('[');
                size_t end;
                std::string full;
                if (ExtractArray(content, start, full, end)) {
                    eventsArr = full;
                }
            }
        }

        if (eventsArr.empty()) {
            RPG_LOG_WARN("Events array not found in " + loadedPath + " - using demo");
            EnsureDemoEvent();
            return;
        }

        auto evObjs = ExtractObjectsFromArray(eventsArr);
        if (evObjs.empty()) {
            RPG_LOG_WARN("No events parsed from " + loadedPath);
            EnsureDemoEvent();
            return;
        }

        for (auto& eo : evObjs) {
            MapEvent ev = ParseMapEventObject(eo);
            if (ev.id != 0 && !ev.pages.empty()) {
                mEvents.push_back(std::move(ev));
            }
        }

        if (mEvents.empty()) {
            RPG_LOG_WARN("Parsed 0 valid events from " + loadedPath + " - using demo");
            EnsureDemoEvent();
        } else {
            RPG_LOG_INFO("LoadMapEvents map " + std::to_string(mapId) + " loaded " + std::to_string(mEvents.size()) + " events from " + loadedPath);
            // CommonEvents
            try {
                std::string cpath = projectPath + "/maps/CommonEvents.json";
                if (std::filesystem::exists(cpath)) {
                    std::ifstream cf(cpath);
                    std::stringstream ss; ss << cf.rdbuf();
                    std::string content = ss.str();
                    mCommonEvents.clear();
                    size_t pos = 0;
                    while ((pos = content.find("\"id\"", pos)) != std::string::npos) {
                        CommonEvent ce;
                        int id=0, trig=0, sw=0;
                        // naive parse
                        try {
                            size_t c = content.find(':', pos);
                            id = std::stoi(content.substr(c+1));
                        } catch(...) {}
                        ce.id = id;
                        size_t np = content.find("\"name\"", pos);
                        size_t next = content.find("\"id\"", pos+4);
                        if (np != std::string::npos && (next==std::string::npos || np < next)) {
                            size_t q1 = content.find('"', content.find(':', np)+1);
                            size_t q2 = content.find('"', q1+1);
                            if (q1!=std::string::npos && q2!=std::string::npos)
                                ce.name = content.substr(q1+1, q2-q1-1);
                        }
                        size_t tp = content.find("\"trigger\"", pos);
                        if (tp != std::string::npos && (next==std::string::npos || tp < next)) {
                            try { ce.trigger = (EventTrigger)std::stoi(content.substr(content.find(':',tp)+1)); } catch(...) {}
                        }
                        size_t sp = content.find("\"switchId\"", pos);
                        if (sp != std::string::npos && (next==std::string::npos || sp < next)) {
                            try { ce.switchId = std::stoi(content.substr(content.find(':',sp)+1)); } catch(...) {}
                        }
                        // commands: find "list"
                        size_t lp = content.find("\"list\"", pos);
                        if (lp != std::string::npos && (next==std::string::npos || lp < next)) {
                            size_t b = content.find('[', lp);
                            size_t e = content.find(']', b);
                            // parse code entries
                            size_t cp = b;
                            while (cp != std::string::npos && cp < e) {
                                cp = content.find("\"code\"", cp);
                                if (cp==std::string::npos || cp > e) break;
                                EventCommand cmd;
                                try { cmd.code = (EventCommandCode)std::stoi(content.substr(content.find(':',cp)+1)); } catch(...) {}
                                size_t tx = content.find("\"text\"", cp);
                                if (tx != std::string::npos && tx < e) {
                                    size_t q1 = content.find('"', content.find(':', tx)+1);
                                    size_t q2 = q1+1;
                                    std::string text;
                                    while (q2 < content.size() && content[q2] != '"') {
                                        if (content[q2]=='\\' && q2+1<content.size()) { text.push_back(content[q2+1]); q2+=2; }
                                        else { text.push_back(content[q2]); q2++; }
                                    }
                                    cmd.text = text;
                                }
                                auto grabP = [&](const char* k, int& out) {
                                    size_t p = content.find(k, cp);
                                    if (p!=std::string::npos && p < (cp+200)) {
                                        try { out = std::stoi(content.substr(content.find(':',p)+1)); } catch(...) {}
                                    }
                                };
                                grabP("\"p1\"", cmd.param1);
                                grabP("\"p2\"", cmd.param2);
                                grabP("\"p3\"", cmd.param3);
                                ce.list.push_back(cmd);
                                cp = content.find("\"code\"", cp+6);
                            }
                        }
                        if (ce.id > 0) mCommonEvents.push_back(ce);
                        pos += 4;
                    }
                    RPG_LOG_INFO("Loaded CommonEvents: " + std::to_string(mCommonEvents.size()));
                }
            } catch (...) {}
        }
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("LoadMapEvents failed for map ") + std::to_string(mapId) + ": " + e.what() + " - using demo");
        EnsureDemoEvent();
    }
}

void EventSystem::SaveMapEvents(int mapId, const std::string& projectPath) const {
    try {
        using namespace JsonUtils;
        std::string mapsDir = projectPath + "/maps";
        std::filesystem::create_directories(mapsDir);
        std::string path = mapsDir + "/" + FormatMapEventFileName(mapId, true);

        std::ofstream f(path);
        if (!f) {
            RPG_LOG_ERROR("Failed to open event file for saving: " + path);
            return;
        }

        f << "{\n";
        f << "  \"mapId\": " << mapId << ",\n";
        f << "  \"events\": [\n";
        for (size_t ei = 0; ei < mEvents.size(); ++ei) {
            const auto& ev = mEvents[ei];
            f << "    {\n";
            f << "      \"id\": " << ev.id << ",\n";
            f << "      \"name\": \"" << Escape(ev.name) << "\",\n";
            f << "      \"x\": " << ev.x << ", \"y\": " << ev.y << ", \"z\": " << ev.z << ",\n";
            f << "      \"worldPos\": [" << ev.worldPos.x << "," << ev.worldPos.y << "," << ev.worldPos.z << "],\n";
            f << "      \"enabled\": " << (ev.enabled ? "true" : "false") << ",\n";
            f << "      \"pages\": [\n";
            for (size_t pi = 0; pi < ev.pages.size(); ++pi) {
                const auto& pg = ev.pages[pi];
                f << "        {\n";
                f << "          \"id\": " << pg.id << ",\n";
                f << "          \"trigger\": " << static_cast<int>(pg.trigger) << ",\n";
                f << "          \"walkAnime\": " << (pg.walkAnime ? "true" : "false") << ",\n";
                f << "          \"stepAnime\": " << (pg.stepAnime ? "true" : "false") << ",\n";
                f << "          \"directionFix\": " << (pg.directionFix ? "true" : "false") << ",\n";
                f << "          \"through\": " << (pg.through ? "true" : "false") << ",\n";
                f << "          \"moveType\": " << pg.moveType << ",\n";
                f << "          \"moveSpeed\": " << pg.moveSpeed << ",\n";
                f << "          \"moveFrequency\": " << pg.moveFrequency << ",\n";
                f << "          \"graphicName\": \"" << Escape(pg.graphicName) << "\",\n";
                f << "          \"graphicIndex\": " << pg.graphicIndex << ",\n";
                f << "          \"direction\": [" << pg.direction.x << "," << pg.direction.y << "," << pg.direction.z << "],\n";
                f << "          \"condition\": {\n";
                f << "            \"switch1Valid\": " << (pg.condition.switch1Valid ? "true" : "false") << ",\n";
                f << "            \"switch1Id\": " << pg.condition.switch1Id << ",\n";
                f << "            \"switch2Valid\": " << (pg.condition.switch2Valid ? "true" : "false") << ",\n";
                f << "            \"switch2Id\": " << pg.condition.switch2Id << ",\n";
                f << "            \"variableValid\": " << (pg.condition.variableValid ? "true" : "false") << ",\n";
                f << "            \"variableId\": " << pg.condition.variableId << ",\n";
                f << "            \"variableValue\": " << pg.condition.variableValue << ",\n";
                f << "            \"selfSwitchValid\": " << (pg.condition.selfSwitchValid ? "true" : "false") << ",\n";
                f << "            \"selfSwitchCh\": \"" << pg.condition.selfSwitchCh << "\",\n";
                f << "            \"itemValid\": " << (pg.condition.itemValid ? "true" : "false") << ",\n";
                f << "            \"itemId\": " << pg.condition.itemId << ",\n";
                f << "            \"actorValid\": " << (pg.condition.actorValid ? "true" : "false") << ",\n";
                f << "            \"actorId\": " << pg.condition.actorId << "\n";
                f << "          },\n";
                f << "          \"list\": [\n";
                for (size_t ci = 0; ci < pg.list.size(); ++ci) {
                    const auto& cmd = pg.list[ci];
                    f << "            {\"code\": " << static_cast<int>(cmd.code)
                      << ", \"indent\": " << cmd.indent
                      << ", \"param1\": " << cmd.param1
                      << ", \"param2\": " << cmd.param2
                      << ", \"param3\": " << cmd.param3
                      << ", \"text\": \"" << Escape(cmd.text) << "\"";
                    if (!cmd.parameters.empty()) {
                        f << ", \"parameters\": [";
                        for (size_t ppi = 0; ppi < cmd.parameters.size(); ++ppi) {
                            if (ppi) f << ",";
                            f << "\"" << Escape(cmd.parameters[ppi]) << "\"";
                        }
                        f << "]";
                    }
                    f << "}";
                    if (ci + 1 < pg.list.size()) f << ",";
                    f << "\n";
                }
                f << "          ]\n";
                f << "        }";
                if (pi + 1 < ev.pages.size()) f << ",";
                f << "\n";
            }
            f << "      ]\n";
            f << "    }";
            if (ei + 1 < mEvents.size()) f << ",";
            f << "\n";
        }
        f << "  ]\n";
        f << "}\n";
        f.close();
        RPG_LOG_INFO("SaveMapEvents map " + std::to_string(mapId) + " saved " + std::to_string(mEvents.size()) + " events to " + path);
        // Common Events (projektweit)
        try {
            std::filesystem::create_directories(projectPath + "/maps");
            std::string cpath = projectPath + "/maps/CommonEvents.json";
            std::ofstream cf(cpath);
            cf << "[\n";
            for (size_t i = 0; i < mCommonEvents.size(); ++i) {
                const auto& ce = mCommonEvents[i];
                cf << "  {\"id\":" << ce.id << ",\"name\":\"" << ce.name
                   << "\",\"trigger\":" << (int)ce.trigger
                   << ",\"switchId\":" << ce.switchId
                   << ",\"list\":[";
                for (size_t j = 0; j < ce.list.size(); ++j) {
                    const auto& c = ce.list[j];
                    if (j) cf << ",";
                    cf << "{\"code\":" << (int)c.code << ",\"text\":\"";
                    for (char ch : c.text) {
                        if (ch=='\\'||ch=='"') cf << '\\';
                        cf << ch;
                    }
                    cf << "\",\"p1\":" << c.param1 << ",\"p2\":" << c.param2
                       << ",\"p3\":" << c.param3 << "}";
                }
                cf << "]}";
                if (i+1<mCommonEvents.size()) cf << ",";
                cf << "\n";
            }
            cf << "]\n";
            RPG_LOG_INFO("Saved CommonEvents: " + std::to_string(mCommonEvents.size()));
        } catch (...) {}
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("SaveMapEvents failed: ") + e.what());
    }
}

} // namespace rpg
