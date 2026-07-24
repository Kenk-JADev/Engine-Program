#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
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
static std::function<int()> s_buttonProvider;

void EventSystem_SetScriptRunner(std::function<void(const std::string&)> fn) {
    s_scriptRunner = std::move(fn);
}
void EventSystem_SetButtonProvider(std::function<int()> fn) {
    s_buttonProvider = std::move(fn);
}

// Audio-Bruecke fuer Event-Befehle + Karten-Autoplay (Engine injiziert in
// Initialize; PlayAudio ist no-op, wenn nichts injiziert wurde).
static std::function<void(const std::string&, int, bool)> s_audioPlayer;

void EventSystem_SetAudioPlayer(
    std::function<void(const std::string&, int, bool)> fn) {
    s_audioPlayer = std::move(fn);
}

void EventSystem_PlayAudio(const std::string& name, int kind, bool loop) {
    if (s_audioPlayer) s_audioPlayer(name, kind, loop);
}

// Map-Wechsel-Bruecke (Engine injiziert in Initialize).
static std::function<void(int)> s_mapChangeHandler;

void EventSystem_SetMapChangeHandler(std::function<void(int)> fn) {
    s_mapChangeHandler = std::move(fn);
}

void EventSystem_NotifyMapChanged(int mapId) {
    if (s_mapChangeHandler) s_mapChangeHandler(mapId);
}

// ============================================================================
// Screen Effects (223 / 224 / 225)
// ============================================================================
void ScreenEffects::Update(float dt) {
    if (flashTimer > 0.0f) {
        flashTimer -= dt;
        if (flashTimer < 0.0f) { flashTimer = 0.0f; flashColor.a = 0.0f; }
    }
    if (shakeTimer > 0.0f) {
        shakeTimer -= dt;
        if (shakeTimer < 0.0f) shakeTimer = 0.0f;
    }
    if (toneElapsed < toneDuration && toneDuration > 0.0f) {
        toneElapsed += dt;
        float t = toneElapsed / toneDuration;
        if (t > 1.0f) t = 1.0f;
        toneCurrent.r = toneCurrent.r + (toneTarget.r - toneCurrent.r) * t;
        toneCurrent.g = toneCurrent.g + (toneTarget.g - toneCurrent.g) * t;
        toneCurrent.b = toneCurrent.b + (toneTarget.b - toneCurrent.b) * t;
        toneCurrent.a = toneCurrent.a + (toneTarget.a - toneCurrent.a) * t;
    }
    // PAKET 11: Wetter-Staerke sanft auf das Ziel rampen (XP aendert die
    // Staerke ueber eine Dauer — bei uns fest weatherRamp Sekunden).
    {
        const float target = (weatherType > 0) ? (float)weatherPowerTarget : 0.0f;
        const float rate = (weatherRamp > 0.01f) ? (9.0f / weatherRamp) : 1000.0f;
        if (weatherPower < target)
            weatherPower = std::min(target, weatherPower + rate * dt);
        else if (weatherPower > target)
            weatherPower = std::max(target, weatherPower - rate * dt);
    }
}
ScreenEffects& GetScreenEffects() {
    static ScreenEffects fx;
    return fx;
}

// ============================================================================
// MapEvent
// ============================================================================
const EventPage* MapEvent::GetCurrentPage() const {
    if (pages.empty()) return nullptr;
    if (currentPage < 0 || currentPage >= (int)pages.size()) return nullptr;
    return &pages[currentPage];
}
EventPage* MapEvent::GetCurrentPage() {
    if (pages.empty()) return nullptr;
    if (currentPage < 0 || currentPage >= (int)pages.size()) return nullptr;
    return &pages[currentPage];
}

// ============================================================================
// EventInterpreter - XP-kompatible Abarbeitung (siehe XP_Scripts/Interpreter)
// ============================================================================
EventInterpreter::EventInterpreter() = default;
EventInterpreter::~EventInterpreter() = default;

void EventInterpreter::Setup(const std::vector<EventCommand>& list, int eventId, int mapId) {
    mList = list;
    mIndex = 0;
    mEventId = eventId;
    mMapId = mapId;
    mWaitTime = 0.0f;
    mMessageWaiting = false;
    mChoiceWaiting = false;
    mNumberWaiting = false;
    mNameWaiting = false;
    mShopWaiting = false;
    mSaveWaiting = false;
    mMoveRouteWaiting = false;
    mButtonInputVariableId = 0;
    mBranch.clear();
    mChild.reset();
}

void EventInterpreter::Clear() {
    mList.clear();
    mIndex = 0;
    mWaitTime = 0.0f;
    mMessageWaiting = false;
    mChoiceWaiting = false;
    mChild.reset();
}

int EventInterpreter::CurrentIndent() const {
    if (mIndex < mList.size()) return mList[mIndex].indent;
    return 0;
}

void EventInterpreter::SetChoiceResult(int index) {
    if (!mChoiceWaiting) return;
    mBranch[mChoiceIndent] = index;
    mChoiceWaiting = false;
}

void EventInterpreter::SetNumberResult(int value) {
    (void)value;
    mNumberWaiting = false;
}

// XP command_skip: ueberspringe Befehle, bis einer mit gleichem Indent kommt
bool EventInterpreter::CommandSkip() {
    const int indent = CurrentIndent();
    for (;;) {
        if (mIndex + 1 >= mList.size()) return true;
        if (mList[mIndex + 1].indent == indent) return true;
        mIndex++;
    }
}

int EventInterpreter::FindLabel(const std::string& name) const {
    for (size_t i = 0; i < mList.size(); ++i) {
        if (mList[i].code == EventCommandCode::Label && mList[i].text == name)
            return static_cast<int>(i);
    }
    return -1;
}

void EventInterpreter::Update(float dt) {
    mLoopSafety = 0;
    for (;;) {
        mLoopSafety++;
        if (mLoopSafety > 100) return; // Freeze-Schutz wie XP

        // Child-Interpreter (Call Common Event)
        if (mChild) {
            mChild->Update(dt);
            if (!mChild->IsRunning()) mChild.reset();
            if (mChild) return;
        }

        // Warte auf Nachrichten-Fenster (Show Text / Choices block-Fenster)
        if (mMessageWaiting) {
            if (GameUI::Get().Message().IsBusy()) return;
            mMessageWaiting = false;
        }
        // Warte auf Auswahl-Ergebnis
        if (mChoiceWaiting) return;
        // Warte auf Zahleneingabe / Namenseingabe
        if (mNumberWaiting || mNameWaiting) return;
        // Warte auf Shop- / Speicherbildschirm (302 / 352)
        if (mShopWaiting || mSaveWaiting) return;
        // Warte auf Move-Completion
        if (mMoveRouteWaiting) {
            if (isAnyRouteForcing && isAnyRouteForcing()) return;
            mMoveRouteWaiting = false;
        }
        // Warte auf Tasteneingabe (105)
        if (mButtonInputVariableId > 0) {
            int code = pollButtonCode ? pollButtonCode() : 0;
            if (code > 0) {
                Game::Get().Variables().Set(mButtonInputVariableId, code);
                mButtonInputVariableId = 0;
            } else {
                return;
            }
        }
        // Wartezaehler
        if (mWaitTime > 0.0f) {
            mWaitTime -= dt;
            if (mWaitTime > 0.0f) return;
            mWaitTime = 0.0f;
        }
        // Liste fertig?
        if (mList.empty() || mIndex >= mList.size()) {
            mList.clear();
            return;
        }
        // Befehl ausfuehren; false = Pause (nächster Frame gleiche Stelle)
        if (!ExecuteCommand()) return;
        mIndex++;
        if (mIndex >= mList.size()) {
            mList.clear();
            return;
        }
    }
}

void EventInterpreter::ApplyToActors(int actorIdOrAll, const std::function<void(int)>& fn) {
    if (actorIdOrAll == 0) {
        for (auto& a : Game::Get().Party().Members()) fn(a.actorId);
    } else {
        fn(actorIdOrAll);
    }
}

int EventInterpreter::ResolveOperand(const EventCommand& cmd, int index0) const {
    // parameters[index0+0] = art ("0"=Konstante, "1"=Variable, "2"=Zufall a..b)
    // parameters[index0+1] = wert / variablenId / min
    // parameters[index0+2] = (nur Zufall) max
    try {
        const std::string& kind = cmd.parameters.at(index0);
        const std::string& v = cmd.parameters.at(index0 + 1);
        if (kind == "1") {
            return Game::Get().Variables().Get(std::stoi(v));
        } else if (kind == "2") {
            int lo = std::stoi(v);
            int hi = cmd.parameters.size() > (size_t)(index0 + 2)
                     ? std::stoi(cmd.parameters[index0 + 2]) : lo;
            if (hi < lo) std::swap(hi, lo);
            return lo + (hi > lo ? (std::rand() % (hi - lo + 1)) : 0);
        }
        return std::stoi(v);
    } catch (...) {
        return 0;
    }
}

bool EventInterpreter::EvalCondition(const EventCommand& cmd) {
    // Kodiert wie im Qt-Editor-Katalog (docs/EVENTS-XP.md):
    // param1 = Bedingungstyp
    switch (cmd.param1) {
        case 0: { // Schalter: param2=id, param3=0(ON)/1(OFF)
            bool on = Game::Get().Switches().Get(cmd.param2);
            return cmd.param3 == 0 ? on : !on;
        }
        case 1: { // Variable: param2=id, param3=Vergleich(0==,1>=,2<=,3>,4<,5!=),
                  // parameters[0]=Operandart, [1]=Wert/VarId
            int cur = Game::Get().Variables().Get(cmd.param2);
            int val = ResolveOperand(cmd, 0);
            switch (cmd.param3) {
                case 0: return cur == val;
                case 1: return cur >= val;
                case 2: return cur <= val;
                case 3: return cur > val;
                case 4: return cur < val;
                case 5: return cur != val;
            }
            return false;
        }
        case 2: { // Selbstschalter: text=Buchstabe, param3=0(ON)/1(OFF)
            char ch = cmd.text.empty() ? 'A' : cmd.text[0];
            bool on = Game::Get().SelfSwitches().Get(mMapId, mEventId, ch);
            return cmd.param3 == 0 ? on : !on;
        }
        case 3: { // Timer: param2=Sekunden, param3=0(>=)/1(<=)
            int sec = Game::Get().System().GetTimerSeconds();
            return cmd.param3 == 0 ? (sec >= cmd.param2) : (sec <= cmd.param2);
        }
        case 4: { // Akteur: param2=actorId, param3=Art (0 Party,1 Name,2 Fertigkeit,
                  //                          3 Waffe,4 Ruestung,5 Status)
            auto& party = Game::Get().Party();
            switch (cmd.param3) {
                case 0: return party.HasActor(cmd.param2);
                case 1: {
                    if (auto* a = party.GetActor(cmd.param2))
                        return !cmd.parameters.empty() && a->name == cmd.parameters[0];
                    return false;
                }
                case 2: {
                    if (auto* a = party.GetActor(cmd.param2)) {
                        int sid = cmd.parameters.empty() ? 0 : atoi(cmd.parameters[0].c_str());
                        for (int s : a->skills) if (s == sid) return true;
                    }
                    return false;
                }
                case 3: {
                    if (auto* a = party.GetActor(cmd.param2))
                        return a->weaponId == (cmd.parameters.empty() ? 0 : atoi(cmd.parameters[0].c_str()));
                    return false;
                }
                case 4: {
                    if (auto* a = party.GetActor(cmd.param2)) {
                        int id = cmd.parameters.empty() ? 0 : atoi(cmd.parameters[0].c_str());
                        for (int ar : a->armors) if (ar == id) return true;
                    }
                    return false;
                }
                case 5: {
                    if (auto* a = party.GetActor(cmd.param2)) {
                        int sid = cmd.parameters.empty() ? 0 : atoi(cmd.parameters[0].c_str());
                        for (int st : a->states) if (st == sid) return true;
                    }
                    return false;
                }
            }
            return false;
        }
        case 5: { // Gegner: param2=Index, param3=0(erschienen)/1(Status)
            if (cmd.param3 == 0) {
                if (isEnemyAppeared) return isEnemyAppeared(cmd.param2);
                return false;
            }
            return false; // Status der Gegner: Kampfsystem-Erweiterung (siehe Doku)
        }
        case 6: { // Event-Richtung: param2=eventId(0=dieses), param3=2/4/6/8
            int dir = -1;
            if (getEventDirection) dir = getEventDirection(cmd.param2 > 0 ? cmd.param2 : mEventId);
            return dir == cmd.param3;
        }
        case 7: { // Gold: param2=Betrag, param3=0(>=)/1(<=)
            int gold = Game::Get().Party().GetGold();
            return cmd.param3 == 0 ? (gold >= cmd.param2) : (gold <= cmd.param2);
        }
        case 8: { // Gegenstand: param2=itemId
            return Game::Get().Party().GetItemCount(cmd.param2) > 0;
        }
        case 9:  // Waffe: param2=waffeId
            return Game::Get().Party().GetWeaponCount(cmd.param2) > 0;
        case 10: // Ruestung: param2=ruestungId
            return Game::Get().Party().GetArmorCount(cmd.param2) > 0;
        case 11: { // Taste: param2=XP-Tastencode
            int code = s_buttonProvider ? s_buttonProvider() : 0;
            return code == cmd.param2;
        }
        case 12: { // Script: text = Ruby-Ausdruck -> jeder nicht-leere/"true"-Rueckgabewert zaehlt
            // Script-Bedingungen laufen ueber den Script-Runner; Ergebnis landet in Variable 0
            if (s_scriptRunner && !cmd.text.empty()) {
                s_scriptRunner("$__cond = (" + cmd.text + ")\nGame.set_variable(0, $__cond ? 1 : 0)");
                return Game::Get().Variables().Get(0) != 0;
            }
            return false;
        }
    }
    return false;
}

bool EventInterpreter::ExecuteCommand() {
    const EventCommand& cmd = mList[mIndex];
    using CC = EventCommandCode;

    switch (cmd.code) {
    // ------------------------------------------------------------------
    // Seite 1
    // ------------------------------------------------------------------
    case CC::ShowText: {
        // Wenn noch eine Nachricht offen ist: spaeter erneut versuchen (XP)
        if (GameUI::Get().Message().IsBusy()) return false;
        mMessageWaiting = true;
        std::string msg = cmd.text;
        int lineCount = msg.empty() ? 0 : 1;
        // Folgezeilen (401) + evtl. anschliessende Choices (102) einsammeln
        for (;;) {
            if (mIndex + 1 < mList.size() && mList[mIndex + 1].code == CC::TextLine) {
                mIndex++;
                if (!msg.empty()) msg += "\n";
                msg += mList[mIndex].text;
                lineCount++;
            } else {
                if (mIndex + 1 < mList.size() && mList[mIndex + 1].code == CC::ShowChoices
                    && lineCount < 4) {
                    // Choices werden direkt im selben Fenster gezeigt:
                    // 102 einmalig vorspulen, sein Handler sammelt sie ein.
                    // (Der 102 darunter wartet nicht erneut auf busy.)
                }
                break;
            }
        }
        if (onShowText) onShowText(msg);
        return true;
    }
    case CC::TextLine:
        return true; // wurde schon von ShowText konsumiert

    case CC::ShowChoices: {
        if (GameUI::Get().Message().IsBusy()) return false;
        // Optionen aus parameters[0..3], Argument: param2 = Abbruchverhalten
        if (onShowChoices) onShowChoices(cmd.text, cmd.param2);
        mMessageWaiting = true;
        mChoiceWaiting = true;
        mChoiceIndent = cmd.indent;
        return true;
    }
    case CC::WhenChoice: { // "Wenn [x]"
        int idx = cmd.param1; // 0-basierter Optionsindex
        auto it = mBranch.find(cmd.indent);
        if (it != mBranch.end() && it->second == idx) {
            mBranch.erase(it);
            return true;
        }
        return CommandSkip();
    }
    case CC::WhenCancel: { // "Wenn Abbruch"
        auto it = mBranch.find(cmd.indent);
        if (it != mBranch.end() && it->second == 4) {
            mBranch.erase(it);
            return true;
        }
        return CommandSkip();
    }
    case CC::ChoicesEnd:
        return true;

    case CC::InputNumber: {
        if (GameUI::Get().Message().IsBusy()) return false;
        int varId = cmd.param1;
        int digits = cmd.param2 > 0 ? cmd.param2 : 4;
        if (showNumberInput) {
            mNumberWaiting = true;
            showNumberInput(digits, Game::Get().Variables().Get(varId),
                [this, varId](int value) {
                    Game::Get().Variables().Set(varId, value);
                    mNumberWaiting = false;
                });
        } else {
            RPG_LOG_WARN("[Event] Zahleneingabe ohne UI-Provider (Variable " +
                         std::to_string(varId) + " bleibt)");
        }
        return true;
    }

    case CC::ChangeTextOptions:
        // Position/Hintergrund des Nachrichtenfensters (kosmetisch)
        return true;

    case CC::ButtonInputProcessing: {
        mButtonInputVariableId = cmd.param1;
        return true;
    }

    case CC::Wait: {
        // XP: Frames @ 40 fps. Editor-Katalog erlaubt auch Sekundenangaben.
        SetWait(cmd.param1 > 0 ? cmd.param1 / 40.0f : 0.1f);
        return true;
    }

    case CC::Comment:
    case CC::CommentLine:
        return true;

    case CC::ConditionalBranch: {
        bool result = EvalCondition(cmd);
        mBranch[cmd.indent] = result ? 1 : 0;
        if (!result) return CommandSkip();
        return true;
    }
    case CC::Else: {
        auto it = mBranch.find(cmd.indent);
        if (it != mBranch.end() && it->second == 0) {
            mBranch.erase(it);
            return true; // Bedingung war falsch -> Else-Zweig ausfuehren
        }
        return CommandSkip();
    }
    case CC::BranchEnd: {
        mBranch.erase(cmd.indent);
        return true;
    }
    case CC::Loop:
        return true;
    case CC::RepeatAbove: {
        // Rueckwaerts zum passenden Loop gleicher Einrückung
        const int indent = cmd.indent;
        for (;;) {
            if (mIndex == 0) return true;
            if (mList[mIndex].indent == indent && mList[mIndex].code == CC::Loop)
                return true;
            mIndex--;
        }
    }
    case CC::BreakLoop: {
        const int indent = cmd.indent;
        size_t i = mIndex;
        for (;;) {
            i++;
            if (i >= mList.size()) { mIndex = mList.size(); return true; }
            if (mList[i].code == CC::RepeatAbove && mList[i].indent < indent) {
                mIndex = i;
                return true;
            }
        }
    }
    case CC::ExitEventProcessing:
        mList.clear();
        return false;
    case CC::EraseEvent:
        if (eraseEvent) eraseEvent(mEventId);
        return true;
    case CC::CallCommonEvent: {
        // Sync wichtig: keine Rekursion ueber 100 Ebenen
        static thread_local int s_depth = 0;
        if (s_depth >= 100) {
            RPG_LOG_ERROR("[Event] Common-Event-Aufruf ueberschreitet Maximal-Tiefe!");
            return true;
        }
        const CommonEvent* ce = nullptr;
        for (auto& c : EventSystem::Get().GetCommonEvents())
            if (c.id == cmd.param1) { ce = &c; break; }
        if (!ce) {
            RPG_LOG_WARN("[Event] Common Event " + std::to_string(cmd.param1) + " nicht gefunden");
            return true;
        }
        s_depth++;
        mChild = std::make_unique<EventInterpreter>();
        // Child bekommt dieselbe Verdrahtung
        EventSystem::Get().WireInterpreter(*mChild);
        mChild->Setup(ce->list, 0, mMapId);
        s_depth--;
        return true;
    }
    case CC::Label:
        return true;
    case CC::JumpToLabel: {
        int pos = FindLabel(cmd.text);
        if (pos >= 0) mIndex = (size_t)pos;
        return true;
    }

    case CC::ControlSwitches: {
        int from = cmd.param1, to = cmd.param2;
        if (to < from) std::swap(from, to);
        bool val = cmd.param3 != 0;
        for (int id = from; id <= to; ++id)
            if (onChangeSwitch) onChangeSwitch(id, val); else Game::Get().Switches().Set(id, val);
        EventSystem::Get().RefreshAllPages();
        return true;
    }
    case CC::ControlVariables: {
        int from = cmd.param1, to = cmd.param2;
        if (to < from) std::swap(from, to);
        int value = ResolveOperand(cmd, 0);
        for (int id = from; id <= to; ++id) {
            // Bei Bereich+Zufall: pro Variable neu wuerfeln
            if (!cmd.parameters.empty() && cmd.parameters[0] == "2" && to > from)
                value = ResolveOperand(cmd, 0);
            int cur = Game::Get().Variables().Get(id);
            int nv = cur;
            switch (cmd.param3) {
                case 0: nv = value; break;
                case 1: nv = cur + value; break;
                case 2: nv = cur - value; break;
                case 3: nv = cur * value; break;
                case 4: nv = value != 0 ? cur / value : 0; break;
                case 5: nv = value != 0 ? cur % value : 0; break;
            }
            if (onChangeVariable) onChangeVariable(id, nv); else Game::Get().Variables().Set(id, nv);
        }
        EventSystem::Get().RefreshAllPages();
        return true;
    }
    case CC::ControlSelfSwitch: {
        char ch = cmd.text.empty() ? 'A' : cmd.text[0];
        bool val = cmd.param3 != 0;
        if (onChangeSelfSwitch) onChangeSelfSwitch(mEventId, ch, val);
        EventSystem::Get().RefreshAllPages();
        return true;
    }
    case CC::ControlTimer: {
        if (cmd.param1 == 0) Game::Get().System().StartTimer(std::max(0, cmd.param2));
        else Game::Get().System().StopTimer();
        return true;
    }
    case CC::ChangeGold:
        if (onChangeGold) onChangeGold(cmd.param1);
        else Game::Get().Party().GainGold(cmd.param1);
        EventSystem::Get().RefreshAllPages();
        return true;
    case CC::ChangeItems:
        if (onChangeItems) onChangeItems(cmd.param1, cmd.param2);
        else Game::Get().Party().GainItem(cmd.param1, cmd.param2);
        EventSystem::Get().RefreshAllPages();
        return true;
    case CC::ChangeWeapons:
        Game::Get().Party().GainWeapon(cmd.param1, cmd.param2);
        return true;
    case CC::ChangeArmor:
        Game::Get().Party().GainArmor(cmd.param1, cmd.param2);
        return true;
    case CC::ChangePartyMember: {
        if (cmd.param3 == 0) Game::Get().Party().AddActor(cmd.param1);
        else Game::Get().Party().RemoveActor(cmd.param1);
        EventSystem::Get().RefreshAllPages();
        return true;
    }
    case CC::ChangeWindowskin:
        Game::Get().System().SetWindowskin(cmd.text);
        return true;
    case CC::ChangeBattleBGM:
        Game::Get().System().SetBattleBgm(cmd.text);
        return true;
    case CC::ChangeBattleEndME:
        Game::Get().System().SetBattleEndMe(cmd.text);
        return true;
    case CC::ChangeSaveAccess:
        Game::Get().System().SetSaveAccess(cmd.param1 != 0);
        return true;
    case CC::ChangeMenuAccess:
        Game::Get().System().SetMenuAccess(cmd.param1 != 0);
        return true;
    case CC::ChangeEncounter:
        Game::Get().System().SetEncounterEnabled(cmd.param1 != 0);
        return true;

    // ------------------------------------------------------------------
    // Seite 2
    // ------------------------------------------------------------------
    case CC::TransferPlayer:
        if (onTransferPlayer) onTransferPlayer(cmd.param1, 0, cmd.param2, cmd.param3);
        return true;
    case CC::SetEventLocation:
        if (setEventLocation) setEventLocation(cmd.param1, cmd.param2, cmd.param3);
        return true;
    case CC::ScrollMap:
        return true; // 2D-spezifisch (Kamera folgt in 3D dem Spieler)
    case CC::ChangeMapSettings:
        return true;
    case CC::ChangeFogColorTone:
    case CC::ChangeFogOpacity:
        // Nebel-Farbton/Deckkraft: Renderer-Fog-Uniforms (Engine-Hook vorhanden)
        return true;
    case CC::ShowAnimation:
    case CC::ShowBattleAnimation:
    case CC::PlayAnimation: {
        // XP-Animations-Playback (Paket 5): Sequenz aus Data/Animations.json
        // als RGSS-Sprite-Gruppe. Ziel (Paket 6): param1 -> -1 Spieler,
        // 0 dieses Event, >0 Event-ID; Weltposition wird von der Engine als
        // Canvas-Position projiziert (Fallback: Canvas-Mitte).
        int animId = cmd.param2 > 0 ? cmd.param2 : 0;
        if (animId == 0 && !cmd.text.empty()) {
            // Hilfsweg: Animation per NAME finden (Editor-Textfeld)
            for (const auto& a : Database::Get().AnimationSet())
                if (a.name == cmd.text) { animId = a.id > 0 ? a.id : 1; break; }
        }
        if (animId <= 0) animId = 1;
        Vec3 target = Game::Get().Player().GetPosition();
        const int targetId = (cmd.param1 == 0) ? mEventId : cmd.param1;
        if (targetId > 0) {
            if (MapEvent* ev = EventSystem::Get().GetEvent(targetId)) {
                Vec3 ep = ev->worldPos;
                if (glm::length(ep) < 0.001f)
                    ep = Vec3((float)ev->x, (float)ev->y, (float)ev->z);
                target = ep;
            }
        }
        Game::Get().StartMapAnimationAt(animId, target);
        return true;
    }
    case CC::ChangeTransparentFlag:
        Game::Get().Player().SetTransparent(cmd.param1 != 0);
        return true;
    case CC::SetMoveRoute: {
        MoveRoute route;
        route.repeat = (cmd.param2 & 1) != 0;
        route.skippable = (cmd.param2 & 2) != 0;
        const bool wait = (cmd.param2 & 4) != 0;
        // Routen-Text parsen: U D L R F T A X S(n) W(n) J(x,y) TD TL TR TU
        std::stringstream ss(cmd.text);
        std::string tok;
        while (ss >> tok) {
            MoveRouteStep st;
            char c = (char)std::toupper(tok[0]);
            if (c == 'U') st.code = MoveRouteCode::MoveUp;
            else if (c == 'D') st.code = MoveRouteCode::MoveDown;
            else if (c == 'L') st.code = MoveRouteCode::MoveLeft;
            else if (c == 'R') st.code = MoveRouteCode::MoveRight;
            else if (c == 'F') st.code = MoveRouteCode::MoveForward;
            else if (c == 'T' && tok.size() > 1 && std::toupper(tok[1]) == 'D') { st.code = MoveRouteCode::TurnDown; }
            else if (c == 'T' && tok.size() > 1 && std::toupper(tok[1]) == 'L') { st.code = MoveRouteCode::TurnLeft; }
            else if (c == 'T' && tok.size() > 1 && std::toupper(tok[1]) == 'R') { st.code = MoveRouteCode::TurnRight; }
            else if (c == 'T' && tok.size() > 1 && std::toupper(tok[1]) == 'U') { st.code = MoveRouteCode::TurnUp; }
            else if (c == 'T') st.code = MoveRouteCode::TowardPlayer;
            else if (c == 'A') st.code = MoveRouteCode::AwayFromPlayer;
            else if (c == 'X') st.code = MoveRouteCode::Random;
            else if (c == 'W') {
                st.code = MoveRouteCode::Wait;
                st.param = tok.size() > 1 ? atoi(tok.c_str() + 1) : 20;
            } else continue;
            route.list.push_back(st);
        }
        route.list.push_back({MoveRouteCode::End, 0});
        int target = cmd.param1; // 0=dieses Event, >0 Event-Id
        if (target < 0) target = mEventId;
        if (onSetMoveRoute) onSetMoveRoute(target, route);
        if (wait && !route.list.empty()) mMoveRouteWaiting = true;
        return true;
    }
    case CC::WaitForMoveCompletion:
        mMoveRouteWaiting = true;
        return true;
    case CC::PrepareTransition:
    case CC::ExecuteTransition:
        return true;
    case CC::ChangeScreenColorTone: {
        auto& fx = GetScreenEffects();
        float r = (float)(cmd.param1), g = (float)(cmd.param2), b = (float)(cmd.param3);
        float grey = cmd.parameters.empty() ? 0.f : (float)atof(cmd.parameters[0].c_str());
        float secs = cmd.parameters.size() > 1 ? (float)atof(cmd.parameters[1].c_str()) : 0.5f;
        fx.toneTarget = Color(r / 255.0f, g / 255.0f, b / 255.0f, grey / 255.0f);
        fx.toneElapsed = 0.0f;
        fx.toneDuration = std::max(0.01f, secs);
        return true;
    }
    case CC::ScreenFlash: {
        auto& fx = GetScreenEffects();
        float r = (float)(cmd.param1), g = (float)(cmd.param2), b = (float)(cmd.param3);
        float pwr = cmd.parameters.empty() ? 160.f : (float)atof(cmd.parameters[0].c_str());
        float secs = cmd.parameters.size() > 1 ? (float)atof(cmd.parameters[1].c_str()) : 0.3f;
        fx.flashColor = Color(r / 255.0f, g / 255.0f, b / 255.0f, pwr / 255.0f);
        fx.flashDuration = secs;
        fx.flashTimer = secs;
        return true;
    }
    case CC::ScreenShake: {
        auto& fx = GetScreenEffects();
        fx.shakePower = cmd.param1 > 0 ? cmd.param1 : 5;
        fx.shakeSpeed = cmd.param2 > 0 ? cmd.param2 : 10;
        float secs = cmd.parameters.empty() ? 0.5f : (float)atof(cmd.parameters[0].c_str());
        fx.shakeDuration = secs;
        fx.shakeTimer = secs;
        if (cmd.param3 != 0) SetWait(secs); // "Warten bis fertig"
        return true;
    }
    case CC::ShowPicture: {
        if (!cmd.text.empty()) {
            float x = (float)cmd.param1, y = (float)cmd.param2;
            float normX = x > 1.0f ? x / 640.0f : x;
            float normY = y > 1.0f ? y / 480.0f : y;
            GameUI::Get().ShowPicture(cmd.text, Vec2(normX, normY));
        }
        return true;
    }
    case CC::MovePicture:
        GameUI::Get().MovePicture(cmd.param1 > 0 ? cmd.param1 : 1,
            Vec2((float)cmd.param2 / 640.0f, (float)cmd.param3 / 480.0f));
        return true;
    case CC::RotatePicture:
        GameUI::Get().SetPictureRotation(cmd.param1 > 0 ? cmd.param1 : 1, (float)cmd.param2);
        return true;
    case CC::ChangePictureColorTone:
        if (cmd.parameters.size() >= 1)
            GameUI::Get().SetPictureOpacity(cmd.param1 > 0 ? cmd.param1 : 1,
                (float)atof(cmd.parameters[0].c_str()));
        return true;
    case CC::ErasePicture:
        GameUI::Get().RemovePicture(cmd.param1 > 0 ? cmd.param1 : 1);
        return true;
    case CC::SetWeatherEffects:
    case CC::SetWeather: {
        // PAKET 11: XP-Wetter (Typ 0 Keins / 1 Regen / 2 Sturm / 3 Schnee,
        // Staerke 1-9). Zustand hier; die Anzeige liegt bei
        // GameUI::DrawWeather (ImGui-Overlay, Karte UND Kampf).
        auto& fx = GetScreenEffects();
        fx.weatherType = cmd.param1;
        fx.weatherPowerTarget = std::clamp(cmd.param2, 0, 9);
        if (fx.weatherType <= 0 || fx.weatherType > 3) {
            fx.weatherType = 0;
            fx.weatherPowerTarget = 0; // „Keins" faehrt sanft herunter
        }
        return true;
    }
    case CC::SetTimeOfDay:
        return true; // Tageszeit: Renderer-Hook (bleibt reserviert)
    case CC::PlayBGM:
        if (!cmd.text.empty()) { EventSystem_PlayAudio(cmd.text, 0, true); return true; }
        if (onPlayBGM) onPlayBGM(cmd.text, true);
        return true;
    case CC::FadeOutBGM:
        EventSystem_PlayAudio("", 0, false);
        if (!s_audioPlayer && onPlayBGM) onPlayBGM("", false);
        return true;
    case CC::PlayBGS:
        if (!cmd.text.empty()) { EventSystem_PlayAudio(cmd.text, 1, true); return true; }
        if (onPlaySE) onPlaySE(cmd.text);
        return true;
    case CC::PlayME:
        if (!cmd.text.empty()) { EventSystem_PlayAudio(cmd.text, 2, false); return true; }
        if (onPlaySE) onPlaySE(cmd.text);
        return true;
    case CC::FadeOutBGS:
        EventSystem_PlayAudio("", 1, false);
        if (!s_audioPlayer && onPlaySE) onPlaySE("");
        return true;
    case CC::StopSE:
        EventSystem_PlayAudio("", 3, false);
        if (!s_audioPlayer && onPlaySE) onPlaySE("");
        return true;
    case CC::MemorizeBGM:
    case CC::RestoreBGM:
        Game::Get().System().MemorizeBgm(cmd.code == CC::MemorizeBGM);
        return true;
    case CC::PlaySE:
        if (!cmd.text.empty()) { EventSystem_PlayAudio(cmd.text, 3, false); return true; }
        if (onPlaySE) onPlaySE(cmd.text);
        return true;

    // ------------------------------------------------------------------
    // Seite 3
    // ------------------------------------------------------------------
    case CC::BattleProcessing: {
        int troopId = cmd.param1 > 0 ? cmd.param1 : 1;
        if (!cmd.text.empty()) { try { troopId = std::stoi(cmd.text); } catch (...) {} }
        if (onBattleProcessing) onBattleProcessing(troopId, (cmd.param2 & 1) != 0, (cmd.param2 & 2) != 0);
        return true;
    }
    case CC::IfWin:
    case CC::IfEscape:
    case CC::IfLose: {
        int outcome = BattleSystem::Get().GetLastOutcome(); // 0=keiner,1=Sieg,2=Flucht,3=Niederlage
        int expected = cmd.code == CC::IfWin ? 1 : (cmd.code == CC::IfEscape ? 2 : 3);
        if (outcome == expected) {
            mBranch.erase(cmd.indent);
            return true;
        }
        return CommandSkip();
    }
    case CC::ShopProcessing: {
        // Waren-Text: "1,2,w3,a1" - Zahl = Item, w<ID> = Waffe, a<ID> = Ruestung
        std::vector<ShopGood> goods;
        if (!cmd.text.empty()) {
            std::stringstream ss(cmd.text);
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                // Leerzeichen trimmen
                while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
                while (!tok.empty() && tok.back() == ' ') tok.pop_back();
                if (tok.empty()) continue;
                try {
                    if (tok[0] == 'w' || tok[0] == 'W')
                        goods.push_back({ShopGood::Kind::Weapon, std::stoi(tok.substr(1))});
                    else if (tok[0] == 'a' || tok[0] == 'A')
                        goods.push_back({ShopGood::Kind::Armor, std::stoi(tok.substr(1))});
                    else
                        goods.push_back({ShopGood::Kind::Item, std::stoi(tok)});
                } catch (...) {}
            }
        }
        if (goods.empty()) {
            if (cmd.param1 > 0) goods.push_back({ShopGood::Kind::Item, cmd.param1});
            if (cmd.param2 > 0) goods.push_back({ShopGood::Kind::Item, cmd.param2});
            if (cmd.param3 > 0) goods.push_back({ShopGood::Kind::Item, cmd.param3});
        }
        if (goods.empty()) goods = {{ShopGood::Kind::Item, 1}, {ShopGood::Kind::Item, 2}};
        if (onShopProcessing) {
            // Externer Override bekommt weiterhin nur die Item-IDs (Kompat.)
            std::vector<int> items;
            for (const auto& g : goods)
                if (g.kind == ShopGood::Kind::Item) items.push_back(g.id);
            onShopProcessing(items);
        } else {
            // XP-Shopfenster (RmlUi): Interpreter wartet bis zum Schliessen
            mShopWaiting = true;
            GameUI::Get().ShowShopGoods(goods, [this]() { mShopWaiting = false; });
        }
        return true;
    }
    case CC::NameInputProcessing: {
        int actorId = cmd.param1 > 0 ? cmd.param1 : 1;
        int maxChars = cmd.param2 > 0 ? cmd.param2 : 8;
        if (showNameInput) {
            mNameWaiting = true;
            showNameInput(actorId, maxChars, [this](const std::string&) { mNameWaiting = false; });
        }
        return true;
    }
    case CC::ChangeHP:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (onChangeActorHP) onChangeActorHP(actorId, cmd.param2);
        });
        return true;
    case CC::ChangeSP:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) {
                a->mp += cmd.param2;
                if (a->mp < 0) a->mp = 0;
            }
        });
        return true;
    case CC::ChangeState:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) {
                if (cmd.param3 == 0) { // hinzufuegen
                    bool has = false;
                    for (int s : a->states) if (s == cmd.param2) has = true;
                    if (!has) a->states.push_back(cmd.param2);
                } else {
                    a->states.erase(std::remove(a->states.begin(), a->states.end(), cmd.param2), a->states.end());
                }
            }
        });
        return true;
    case CC::RecoverAll:
        if (onRecoverAll) onRecoverAll(cmd.param1);
        return true;
    case CC::ChangeEXP:
        if (onChangeExp) onChangeExp(cmd.param1, cmd.param2);
        return true;
    case CC::ChangeLevel:
        if (onChangeLevel) onChangeLevel(cmd.param1, cmd.param2);
        return true;
    case CC::ChangeParameters:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) {
                // param2 = Stat (0 MaxHP,1 MaxSP,2 ATK...), param3 = Delta (nur Basiswert-Notiz)
                (void)a; // Stats werden primär über Level/EXP skaliert
                RPG_LOG_INFO("[Event] ChangeParameters actor " + std::to_string(actorId));
            }
        });
        return true;
    case CC::ChangeSkills:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) {
                if (cmd.param3 == 0) {
                    bool has = false;
                    for (int s : a->skills) if (s == cmd.param2) has = true;
                    if (!has) a->skills.push_back(cmd.param2);
                } else {
                    a->skills.erase(std::remove(a->skills.begin(), a->skills.end(), cmd.param2), a->skills.end());
                }
            }
        });
        return true;
    case CC::ChangeEquipment:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) {
                if (cmd.param2 == 0) a->weaponId = cmd.param3;
                else {
                    bool has = false;
                    for (int ar : a->armors) if (ar == cmd.param3) has = true;
                    if (!has && cmd.param3 > 0) a->armors.push_back(cmd.param3);
                }
            }
        });
        return true;
    case CC::ChangeActorName:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) a->name = cmd.text;
        });
        return true;
    case CC::ChangeActorClass:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) a->classId = cmd.param2;
        });
        return true;
    case CC::ChangeActorGraphic:
        ApplyToActors(cmd.param1, [&](int actorId) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) a->graphicName = cmd.text;
        });
        return true;
    case CC::ChangeEnemyHP:
    case CC::ChangeEnemySP:
    case CC::ChangeEnemyState:
    case CC::EnemyRecoverAll:
    case CC::EnemyAppearance:
    case CC::EnemyTransform:
    case CC::DealDamage:
    case CC::ForceAction:
        BattleSystem::Get().ApplyEventCommand(cmd);
        return true;
    case CC::AbortBattle:
        BattleSystem::Get().Abort();
        return true;
    case CC::OpenMenuScreen:
        GameUI::Get().Pause().Show();
        return true;
    case CC::OpenSaveScreen:
        if (onOpenSave) {
            onOpenSave(1); // externer Override
        } else {
            // XP-Speicherbildschirm (4 Slots): Interpreter wartet bis zum Ende
            mSaveWaiting = true;
            GameUI::Get().ShowSaveScreen(true, [this]() { mSaveWaiting = false; });
        }
        return true;
    case CC::GameOver:
        if (onGameOver) onGameOver();
        return true;
    case CC::ReturnToTitle:
        if (onReturnToTitle) onReturnToTitle();
        return true;
    case CC::Script: {
        std::string code = cmd.text;
        while (mIndex + 1 < mList.size() && mList[mIndex + 1].code == CC::ScriptLine) {
            mIndex++;
            code += "\n";
            code += mList[mIndex].text;
        }
        if (onScript) onScript(code);
        return true;
    }
    case CC::ScriptLine:
        return true;

    // ------------------------------------------------------------------
    // Engine-3D-Befehle
    // ------------------------------------------------------------------
    case CC::ShowScreenText: {
        if (onShowScreenText) {
            float x = cmd.param1 / 100.0f;
            float y = cmd.param2 / 100.0f;
            float dur = cmd.param3 > 0 ? cmd.param3 / 10.0f : 3.0f;
            onShowScreenText(cmd.text, x, y, 1.0f, 1.0f, 0.8f, dur);
        }
        return true;
    }
    case CC::ShowWorldText: {
        if (onShowWorldText) {
            onShowWorldText(cmd.text, (float)cmd.param1, (float)cmd.param2, (float)cmd.param3,
                            1.0f, 1.0f, 0.2f, 2.5f);
        }
        return true;
    }
    case CC::ClearScreenTexts:
        if (onClearScreenTexts) onClearScreenTexts();
        return true;
    case CC::ShowFloatingDamage:
        if (onShowWorldText) {
            onShowWorldText(cmd.text.empty() ? std::to_string(cmd.param1) : cmd.text,
                Game::Get().Player().GetPosition().x,
                Game::Get().Player().GetPosition().y + 1.0f,
                Game::Get().Player().GetPosition().z,
                1.0f, 0.3f, 0.3f, 1.5f);
        }
        return true;
    case CC::SpawnEntity:
    case CC::MoveEntity:
    case CC::RotateEntity:
    case CC::PlayParticle:
        // 3D-Szenerie-Befehle: ueber Script-Kanal (Ruby: Engine.spawn etc.)
        if (onScript && !cmd.text.empty()) onScript(cmd.text);
        return true;
    default:
        RPG_LOG_WARN("[Event] Unbekannter Befehlscode: " + std::to_string((int)cmd.code));
        return true;
    }
}

// ============================================================================
// EventSystem
// ============================================================================
EventSystem& EventSystem::Get() {
    static EventSystem instance;
    return instance;
}

void EventSystem::SetChoiceResult(int index) {
    mLastChoice = index;
    for (auto& it : mInterpreters)
        if (it->IsWaitingForChoice()) it->SetChoiceResult(index);
}
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

void EventSystem::EraseEvent(int eventId) {
    if (auto* ev = GetEvent(eventId)) {
        ev->erased = true;
        RPG_LOG_INFO("[Event] Event " + std::to_string(eventId) + " geloescht (bis Map-Reload)");
    }
}

void EventSystem::SetEventLocation(int eventId, int x, int z) {
    MapEvent* ev = GetEvent(eventId > 0 ? eventId : 0);
    if (!ev) return;
    ev->x = x; ev->z = z;
    ev->worldPos = Vec3((float)x, ev->worldPos.y, (float)z);
}

bool EventSystem::IsAnyRouteForcing() const {
    for (const auto& ev : mEvents)
        if (ev.routeForcing) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Callback-Verdrahtung
// ---------------------------------------------------------------------------
void EventSystem::WireInterpreter(EventInterpreter& interp) {
    interp.onShowText = [](const std::string& txt) {
        GameUI::Get().ShowMessage(txt);
        RPG_LOG_INFO(std::string("[Event] ") + txt);
    };
    interp.onShowChoices = [this](const std::string& txt, int cancel) {
        // text: "Frage|OptionA|OptionB|OptionC|OptionD"
        std::vector<std::string> opts;
        std::string prompt = txt;
        size_t p = txt.find('|');
        if (p != std::string::npos) {
            prompt = txt.substr(0, p);
            std::string rest = txt.substr(p + 1);
            size_t start = 0;
            while (start <= rest.size()) {
                size_t n = rest.find('|', start);
                if (n == std::string::npos) { opts.push_back(rest.substr(start)); break; }
                opts.push_back(rest.substr(start, n - start));
                start = n + 1;
            }
        }
        if (opts.empty()) opts = {"Ja", "Nein"};
        // Abbruchverhalten: 0=nicht erlaubt, 1..4=Index waehlen, 5=Abbruchzweig
        GameUI::Get().ShowChoices(prompt, opts, [this, cancel, opts](int idx) {
            if (idx < 0) { // Abbrechen gedrueckt
                if (cancel >= 1 && cancel <= 4 && cancel <= (int)opts.size()) idx = cancel - 1;
                else if (cancel == 5) idx = 4; // Abbruch-Zweig
                else idx = 0;
            }
            EventSystem::Get().SetChoiceResult(idx);
        }, cancel != 0); // cancel==0 -> Escape gesperrt (XP)
    };
    interp.onPlayBGM = [](const std::string& p, bool loop) {
        RPG_LOG_INFO("[Event] BGM: " + p + (loop ? " (loop)" : ""));
    };
    interp.onPlaySE = [](const std::string& p) {
        if (!p.empty()) RPG_LOG_INFO("[Event] SE: " + p);
    };
    interp.onTransferPlayer = [](int x, int y, int z, int mapId) {
        Game::Get().Player().SetPosition(Vec3((float)x, (float)y + 0.05f, (float)z));
        // XP: Karte wirklich wechseln (Visual + Events + BGM via Engine-Hook)
        if (mapId > 0) EventSystem_NotifyMapChanged(mapId);
        RPG_LOG_INFO("[Event] Transfer: Map " + std::to_string(mapId) +
                     " (" + std::to_string(x) + "," + std::to_string(z) + ")");
    };
    interp.onScript = [](const std::string& code) {
        RPG_LOG_INFO("[Event] Script: " + code);
        if (s_scriptRunner) s_scriptRunner(code);
    };
    interp.onChangeGold = [](int gold) {
        Game::Get().Party().GainGold(gold);
        Vec3 pp = Game::Get().Player().GetPosition();
        GameUI::Get().AddWorldText("Gold " + std::string(gold >= 0 ? "+" : "") + std::to_string(gold),
                                   pp + Vec3(0, 1.2f, 0), Color(1.0f, 0.9f, 0.2f, 1.0f), 2.0f);
        RPG_LOG_INFO("[Event] Gold += " + std::to_string(gold));
    };
    interp.onChangeSwitch = [](int id, bool val) {
        Game::Get().Switches().Set(id, val);
    };
    interp.onChangeVariable = [](int id, int val) {
        Game::Get().Variables().Set(id, val);
    };
    interp.onChangeItems = [](int itemId, int amount) {
        Game::Get().Party().GainItem(itemId, amount);
        RPG_LOG_INFO("[Event] Item " + std::to_string(itemId) + " x" + std::to_string(amount));
    };
    interp.onChangeActorHP = [](int actorId, int hpChange) {
        if (auto* actor = Game::Get().Party().GetActor(actorId)) {
            int old = actor->hp;
            actor->hp += hpChange;
            if (actor->hp < 0) actor->hp = 0;
            if (const auto* data = Database::Get().GetActor(actorId))
                if (actor->hp > data->initialStats.mhp) actor->hp = data->initialStats.mhp;
            RPG_LOG_INFO("[Event] HP " + std::to_string(actorId) + ": " +
                         std::to_string(old) + " -> " + std::to_string(actor->hp));
        }
    };
    interp.onBattleProcessing = [](int troopId, bool canEscape, bool canLose) {
        std::vector<int> enemies;
        std::vector<TroopPage> pages;
        if (const auto* troop = Database::Get().GetTroop(troopId)) {
            enemies = troop->members;
            pages = troop->pages; // XP-Kampfereignis-Seiten
        } else enemies = {1};
        BattleSystem::Get().Setup(enemies, canEscape, canLose, pages);
        // XP-Bruecke (Stufe 4g): auch der Event-Befehl „Kampf" meldet die
        // Truppen-ID an $game_troop.setup (Battler stehen bereits).
        if (Game::Get().onBattleStarted) Game::Get().onBattleStarted(troopId);
        BattleSystem::Get().onMessage = [](const std::string& m) {
            GameUI::Get().ShowMessage(m);
        };
        BattleSystem::Get().onVictory = []() {
            GameUI::Get().ShowMessage("Sieg!");
            GameUI::Get().AddScreenText("SIEG", Vec2(0.5f, 0.4f), Color(1, 0.9f, 0.2f, 1), 3.0f);
        };
        BattleSystem::Get().onDefeat = []() {
            GameUI::Get().ShowMessage("Niederlage...");
        };
        RPG_LOG_INFO("[Event] Kampf gestartet: Troop " + std::to_string(troopId));
    };
    // Hinweis: Der Laden (302) laeuft INTERNE ueber GameUI::ShowShop
    // (XP-Shopfenster mit Kaufen/Verkaufen). onShopProcessing bleibt als
    // externer Override erhalten und ist hier bewusst NICHT vorbelegt.
    interp.onRecoverAll = [](int actorId) {
        if ((int)actorId > 0) {
            if (auto* a = Game::Get().Party().GetActor(actorId)) a->RecoverAll();
        } else {
            for (auto& m : Game::Get().Party().Members()) m.RecoverAll();
        }
        GameUI::Get().ShowMessage("HP/MP vollstaendig wiederhergestellt!");
    };
    interp.onChangeExp = [](int actorId, int exp) {
        ApplyToActorOrParty(actorId, [&](GameActor& a) {
            if (exp >= 0) {
                // EXP-Kurve der Klasse (GameActor::AddExp), Level-Up-Meldung
                // inkl. neu gelernter Klassen-Fertigkeiten
                std::vector<std::string> learned;
                if (a.AddExp(exp, &learned) > 0) {
                    std::string msg = a.name + " erreicht Level " +
                                      std::to_string(a.level) + "!";
                    for (const auto& s : learned)
                        msg += "\n" + a.name + " hat [" + s + "] gelernt!";
                    GameUI::Get().ShowMessage(msg);
                }
            } else {
                // Reduzieren senkt nicht das Level (XP-Verhalten)
                a.exp = std::max(0, a.exp + exp);
            }
        });
    };
    interp.onChangeLevel = [](int actorId, int level) {
        ApplyToActorOrParty(actorId, [&](GameActor& a) {
            int maxLv = 99;
            if (const auto* ad = Database::Get().GetActor(a.actorId))
                maxLv = std::max(1, ad->maxLevel);
            a.level = std::max(1, std::min(level, maxLv));
            // Fertigkeiten bis zum neuen Level nachlernen (nur Meldung wenn neu)
            std::vector<std::string> learned;
            a.LearnSkillsUpToLevel(a.level, &learned);
            for (const auto& s : learned)
                GameUI::Get().ShowMessage(a.name + " hat [" + s + "] gelernt!");
        });
    };
    // Hinweis: "Speicherbildschirm aufrufen" (352) oeffnet INTERNE
    // GameUI::ShowSaveScreen (4 XP-Slots). onOpenSave bleibt Override-Hook
    // fuer externe UIs und ist hier bewusst NICHT vorbelegt.
    interp.onOpenLoad = [](int slot) {
        if (Game::Get().Load(slot > 0 ? slot : 1))
            GameUI::Get().ShowMessage("Spiel geladen (Slot " + std::to_string(slot > 0 ? slot : 1) + ").");
        else
            GameUI::Get().ShowMessage("Kein Spielstand gefunden.");
    };
    interp.onGameOver = []() {
        GameUI::Get().ShowMessage("GAME OVER");
        RPG_LOG_INFO("[Event] Game Over");
    };
    interp.onReturnToTitle = []() {
        RPG_LOG_INFO("[Event] Zurück zum Titel");
    };
    interp.onCallCommonEvent = nullptr; // intern (child interpreter)
    interp.onChangeSelfSwitch = [](int eventId, char ch, bool value) {
        int mapId = EventSystem::Get().GetCurrentMapId();
        Game::Get().SelfSwitches().Set(mapId, eventId, ch, value);
        RPG_LOG_INFO(std::string("[Event] SelfSwitch ") + ch + " @Event " +
                     std::to_string(eventId) + " = " + (value ? "AN" : "AUS"));
    };
    interp.onSetMoveRoute = [](int eventId, const MoveRoute& route) {
        auto* ev = EventSystem::Get().GetEvent(eventId);
        if (!ev) return;
        ev->moveRoute = route;
        ev->moveRoute.stepIndex = 0;
        ev->moveRoute.waitTimer = 0;
        ev->hasMoveRoute = !route.list.empty();
        ev->routeForcing = !route.repeat; // einmalige Route = "forcing" bis fertig
    };
    interp.onShowScreenText = [](const std::string& txt, float x, float y, float r, float g, float b, float dur) {
        float nx = x > 1.0f ? x / 100.0f : x;
        float ny = y > 1.0f ? y / 100.0f : y;
        if (nx <= 0.0f) nx = 0.5f;
        if (ny <= 0.0f) ny = 0.2f;
        GameUI::Get().AddScreenText(txt, Vec2(nx, ny), Color(r, g, b, 1.0f), dur > 0 ? dur : 3.0f);
    };
    interp.onShowWorldText = [](const std::string& txt, float x, float y, float z, float r, float g, float b, float dur) {
        Vec3 pos(x, y, z);
        if (glm::length(pos) < 0.01f)
            pos = Game::Get().Player().GetPosition() + Vec3(0, 1.0f, 0);
        GameUI::Get().AddWorldText(txt, pos, Color(r, g, b, 1.0f), dur > 0 ? dur : 2.5f);
    };
    interp.onClearScreenTexts = []() {
        GameUI::Get().ClearScreenTexts();
    };

    // ---- Provider ----
    interp.pollButtonCode = []() { return s_buttonProvider ? s_buttonProvider() : 0; };
    interp.isAnyRouteForcing = [this]() { return IsAnyRouteForcing(); };
    interp.eraseEvent = [this](int id) { EraseEvent(id); };
    interp.setEventLocation = [this](int id, int x, int z) { SetEventLocation(id, x, z); };
    interp.showNumberInput = [](int digits, int initial, std::function<void(int)> cb) -> int {
        GameUI::Get().ShowNumberInput("", digits, initial, std::move(cb));
        return 0;
    };
    interp.showNameInput = [](int actorId, int maxChars, std::function<void(const std::string&)> cb) {
        std::string initial;
        if (auto* a = Game::Get().Party().GetActor(actorId)) initial = a->name;
        GameUI::Get().ShowNameInput("", initial, maxChars,
            [actorId, cb = std::move(cb)](const std::string& name) {
                if (auto* a = Game::Get().Party().GetActor(actorId)) a->name = name;
                cb(name);
            });
    };
    interp.getEventDirection = [this](int eventId) {
        if (auto* ev = GetEvent(eventId)) return ev->direction;
        return (int)DIR_DOWN;
    };
    interp.isEnemyAppeared = [this](int) { return BattleSystem::Get().IsInBattle(); };
}

void EventSystem::BindRuntimeCallbacks() {
    mCallbacksBound = true;
}

// ---------------------------------------------------------------------------
// Seiten-Bedingungen (XP refresh)
// ---------------------------------------------------------------------------
bool EventSystem::ConditionsMet(const EventPage::Condition& c, int eventId) const {
    if (c.switch1Valid && !Game::Get().Switches().Get(c.switch1Id)) return false;
    if (c.switch2Valid && !Game::Get().Switches().Get(c.switch2Id)) return false;
    if (c.variableValid && Game::Get().Variables().Get(c.variableId) < c.variableValue) return false;
    if (c.selfSwitchValid) {
        char ch = c.selfSwitchCh ? c.selfSwitchCh : 'A';
        if (!Game::Get().SelfSwitches().Get(mCurrentMapId, eventId, ch)) return false;
    }
    if (c.itemValid && Game::Get().Party().GetItemCount(c.itemId) <= 0) return false;
    if (c.actorValid && !Game::Get().Party().HasActor(c.actorId)) return false;
    return true;
}

void EventSystem::RefreshEventPage(MapEvent& ev) {
    int best = -1;
    if (!ev.erased) {
        for (int i = 0; i < (int)ev.pages.size(); ++i) {
            if (ConditionsMet(ev.pages[i].condition, ev.id)) best = i; // letzte erfuellte Seite (XP)
        }
    }
    if (ev.currentPage == best) return;
    ev.currentPage = best;

    // Seitenwechsel: Bewegung/Grafik anwenden (XP refresh)
    ev.hasMoveRoute = false;
    ev.routeForcing = false;
    if (auto* page = ev.GetCurrentPage()) {
        ev.direction = page->direction2D;
        if (page->moveType == (int)EventMoveType::Custom && !page->customRoute.empty()) {
            StartCustomRoute(ev, page->customRoute, page->routeRepeat, page->routeSkippable);
        }
    }
}

void EventSystem::StartCustomRoute(MapEvent& ev, const std::string& routeText, bool repeat, bool skippable) {
    MoveRoute route;
    route.repeat = repeat;
    route.skippable = skippable;
    std::stringstream ss(routeText);
    std::string tok;
    while (ss >> tok) {
        MoveRouteStep st;
        char c = (char)std::toupper(tok[0]);
        if (c == 'U') st.code = MoveRouteCode::MoveUp;
        else if (c == 'D') st.code = MoveRouteCode::MoveDown;
        else if (c == 'L') st.code = MoveRouteCode::MoveLeft;
        else if (c == 'R') st.code = MoveRouteCode::MoveRight;
        else if (c == 'F') st.code = MoveRouteCode::MoveForward;
        else if (c == 'T') st.code = MoveRouteCode::TowardPlayer;
        else if (c == 'A') st.code = MoveRouteCode::AwayFromPlayer;
        else if (c == 'X') st.code = MoveRouteCode::Random;
        else if (c == 'W') { st.code = MoveRouteCode::Wait; st.param = tok.size() > 1 ? atoi(tok.c_str() + 1) : 20; }
        else continue;
        route.list.push_back(st);
    }
    if (route.list.empty()) return;
    route.list.push_back({MoveRouteCode::End, 0});
    ev.moveRoute = route;
    ev.hasMoveRoute = true;
}

void EventSystem::RefreshAllPages() {
    for (auto& ev : mEvents) RefreshEventPage(ev);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void EventSystem::Update(float dt, const Vec3& playerPos) {
    // Interpreter laufen lassen (sie verlassen Pausen selbst ueber GameUI-Zustand)
    for (auto& interp : mInterpreters) {
        if (interp->IsRunning()) interp->Update(dt);
    }
    mInterpreters.erase(std::remove_if(mInterpreters.begin(), mInterpreters.end(),
        [](const std::unique_ptr<EventInterpreter>& i) { return !i->IsRunning(); }),
        mInterpreters.end());

    // Screen-Effekte ticken
    GetScreenEffects().Update(dt);

    // Common Events (Autorun/Parallel)
    for (auto& ce : mCommonEvents) {
        if (ce.list.empty()) continue;
        bool run = false;
        if (ce.trigger == EventTrigger::Autorun || ce.trigger == EventTrigger::Parallel)
            run = (ce.switchId <= 0 || Game::Get().Switches().Get(ce.switchId));
        if (!run) continue;
        int cid = -ce.id;
        bool already = false;
        for (auto& it : mInterpreters)
            if (it->GetEventId() == cid && it->IsRunning()) { already = true; break; }
        if (already) continue;
        auto interpreter = std::make_unique<EventInterpreter>();
        WireInterpreter(*interpreter);
        interpreter->Setup(ce.list, cid, mCurrentMapId);
        interpreter->SetBlocking(ce.trigger != EventTrigger::Parallel);
        mInterpreters.push_back(std::move(interpreter));
    }

    UpdateMoveRoutes(dt, playerPos);

    // Autorun / Parallel / Touch triggern
    for (auto& ev : mEvents) {
        if (!ev.enabled || ev.erased || !ev.IsValid()) continue;
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
            if (glm::length(ep) < 0.001f) ep = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
            float dist = glm::length(Vec3(playerPos.x - ep.x, 0.0f, playerPos.z - ep.z));
            if (dist < 1.1f) StartEvent(ev.id);
        }
    }
}

void EventSystem::TryInteract(const Vec3& playerPos, float radius) {
    if (IsAnyEventRunning()) return;
    float best = radius;
    int bestId = -1;

    for (auto& ev : mEvents) {
        if (!ev.enabled || ev.erased || !ev.IsValid()) continue;
        RefreshEventPage(ev);
        const EventPage* page = ev.GetCurrentPage();
        if (!page || page->trigger != EventTrigger::ActionButton) continue;
        Vec3 ep = ev.worldPos;
        if (glm::length(ep) < 0.001f) ep = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
        float dist = glm::length(Vec3(playerPos.x - ep.x, 0.0f, playerPos.z - ep.z));
        if (dist < best) { best = dist; bestId = ev.id; }
    }
    if (bestId >= 0) { StartEvent(bestId); return; }

    // XP-Tresen (Counter-Flag, Paket 1/6): Wenn der Spieler einem Tresen-
    // Tile (Verkaufstresen/Theke) gegenuebersteht, darf das ActionButton-
    // Event EIN Feld dahinter ausgeloest werden.
    const auto& gm = Game::Get().Map();
    const Map* bound = gm.GetBoundMap();
    if (!bound) return;
    auto tileset = bound->GetTileset();
    if (!tileset || !tileset->HasTilesetData()) return;

    const float reach = radius + 1.0f; // zusaetzliche Kachel
    float bestC = reach;
    for (auto& ev : mEvents) {
        if (!ev.enabled || ev.erased || !ev.IsValid()) continue;
        RefreshEventPage(ev);
        const EventPage* page = ev.GetCurrentPage();
        if (!page || page->trigger != EventTrigger::ActionButton) continue;
        Vec3 ep = ev.worldPos;
        if (glm::length(ep) < 0.001f) ep = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
        const float dist = glm::length(Vec3(playerPos.x - ep.x, 0.0f, playerPos.z - ep.z));
        if (dist >= reach || dist <= radius) continue; // nur die neu erschlossene Zone
        // Mittelpunkt zwischen Spieler und Event: dort muss ein Tresen-Tile liegen
        const Vec3 mid((playerPos.x + ep.x) * 0.5f, 0.0f, (playerPos.z + ep.z) * 0.5f);
        int mx, mz;
        if (!gm.WorldToMap(mid.x, mid.z, mx, mz)) continue;
        bool counter = false;
        for (const auto& layer : bound->GetLayers()) {
            if (mx < 0 || mz < 0 || mx >= layer.width || mz >= layer.height) continue;
            const int idx = mz * layer.width + mx;
            if (idx >= 0 && idx < (int)layer.tiles.size()) {
                const int tid = layer.tiles[idx];
                if (tid >= 0 && tileset->GetCounter(tid) != 0) { counter = true; break; }
            }
        }
        if (counter && dist < bestC) { bestC = dist; bestId = ev.id; }
    }
    if (bestId >= 0) StartEvent(bestId);
}

bool EventSystem::StartCommonEventById(int commonEventId, int runtimeEventId, bool blocking) {
    const CommonEvent* ce = nullptr;
    for (const auto& e : mCommonEvents)
        if (e.id == commonEventId) { ce = &e; break; }
    if (!ce || ce->list.empty()) return false;
    // Laeuft diese Runtime-Instanz schon? (Doppelstart verhindern)
    for (auto& it : mInterpreters)
        if (it->GetEventId() == runtimeEventId && it->IsRunning()) return true;
    auto interpreter = std::make_unique<EventInterpreter>();
    WireInterpreter(*interpreter);
    interpreter->Setup(ce->list, runtimeEventId, mCurrentMapId);
    interpreter->SetBlocking(blocking);
    mInterpreters.push_back(std::move(interpreter));
    return true;
}

void EventSystem::StartEvent(int eventId) {
    auto* ev = GetEvent(eventId);
    if (!ev || !ev->IsValid() || ev->erased) return;
    RefreshEventPage(*ev);
    const EventPage* page = ev->GetCurrentPage();
    if (!page || page->list.empty()) return;
    if (IsEventRunning(eventId) && page->trigger != EventTrigger::Parallel) return;

    auto interpreter = std::make_unique<EventInterpreter>();
    WireInterpreter(*interpreter);
    interpreter->Setup(page->list, eventId, mCurrentMapId);
    interpreter->SetBlocking(page->trigger != EventTrigger::Parallel);
    mInterpreters.push_back(std::move(interpreter));
    RPG_LOG_INFO("Event " + std::to_string(eventId) + " (\"" + ev->name + "\") gestartet");
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

bool EventSystem::IsBlockingEventRunning() const {
    for (auto& it : mInterpreters)
        if (it->IsRunning() && it->IsBlocking()) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Autonome Bewegung + Move Routes
// ---------------------------------------------------------------------------
void EventSystem::UpdateMoveRoutes(float dt, const Vec3& playerPos) {
    for (auto& ev : mEvents) {
        if (!ev.enabled || ev.erased) continue;
        const EventPage* page = nullptr;
        if (ev.currentPage >= 0 && ev.currentPage < (int)ev.pages.size())
            page = &ev.pages[ev.currentPage];

        Vec3& pos = ev.worldPos;
        if (glm::length(pos) < 0.001f)
            pos = Vec3((float)ev.x, (float)ev.y, (float)ev.z);

        // ---- Autonome Bewegung der aktiven Seite (Fixed/Random/Approach) ----
        if (page && !ev.hasMoveRoute && page->moveType != (int)EventMoveType::Custom) {
            // Frequenz 1..6 -> Intervall 1.5s .. 0.25s
            float interval = 1.75f - page->moveFrequency * 0.25f;
            ev.moveTimer += dt;
            if (ev.moveTimer >= interval) {
                ev.moveTimer = 0.0f;
                Vec3 delta(0.0f);
                if (page->moveType == (int)EventMoveType::Random) {
                    switch (std::rand() % 4) {
                        case 0: delta = Vec3(1, 0, 0); break;
                        case 1: delta = Vec3(-1, 0, 0); break;
                        case 2: delta = Vec3(0, 0, 1); break;
                        default: delta = Vec3(0, 0, -1); break;
                    }
                } else if (page->moveType == (int)EventMoveType::Approach) {
                    Vec3 to = playerPos - pos;
                    to.y = 0.0f;
                    if (glm::length(to) > 1.2f) {
                        delta = (std::fabs(to.x) > std::fabs(to.z))
                            ? Vec3(to.x > 0 ? 1.f : -1.f, 0, 0)
                            : Vec3(0, 0, to.z > 0 ? 1.f : -1.f);
                    }
                }
                if (glm::length(delta) > 0.001f) {
                    pos += delta; // ganzzellig (XP-Kachel)
                    ev.x = (int)std::round(pos.x);
                    ev.z = (int)std::round(pos.z);
                    if (delta.x > 0) ev.direction = DIR_RIGHT;
                    else if (delta.x < 0) ev.direction = DIR_LEFT;
                    else if (delta.z > 0) ev.direction = DIR_DOWN;
                    else if (delta.z < 0) ev.direction = DIR_UP;
                }
            }
        }

        // ---- Explizite Move Routes (SetMoveRoute / Custom Page Route) ----
        if (!ev.hasMoveRoute || ev.moveRoute.list.empty()) continue;
        auto& mr = ev.moveRoute;
        if (mr.waitTimer > 0) { mr.waitTimer -= dt; continue; }
        if (mr.stepIndex < 0 || mr.stepIndex >= (int)mr.list.size()) {
            if (mr.repeat) { mr.stepIndex = 0; continue; }
            ev.hasMoveRoute = false;
            ev.routeForcing = false;
            continue;
        }
        const auto& step = mr.list[mr.stepIndex];
        const float cell = 1.0f;
        Vec3 delta(0.0f);
        int dir = -1;
        switch (step.code) {
            case MoveRouteCode::MoveUp:    delta = Vec3(0, 0, -cell); dir = DIR_UP; break;
            case MoveRouteCode::MoveDown:  delta = Vec3(0, 0, cell);  dir = DIR_DOWN; break;
            case MoveRouteCode::MoveLeft:  delta = Vec3(-cell, 0, 0); dir = DIR_LEFT; break;
            case MoveRouteCode::MoveRight: delta = Vec3(cell, 0, 0);  dir = DIR_RIGHT; break;
            case MoveRouteCode::MoveForward: {
                switch (ev.direction) {
                    case DIR_UP: delta = Vec3(0, 0, -cell); break;
                    case DIR_DOWN: delta = Vec3(0, 0, cell); break;
                    case DIR_LEFT: delta = Vec3(-cell, 0, 0); break;
                    case DIR_RIGHT: delta = Vec3(cell, 0, 0); break;
                    default: delta = Vec3(0, 0, cell); break;
                }
                break;
            }
            case MoveRouteCode::TowardPlayer: {
                Vec3 to = playerPos - pos; to.y = 0.0f;
                if (glm::length(to) > 0.4f) {
                    delta = (std::fabs(to.x) > std::fabs(to.z))
                        ? Vec3(to.x > 0 ? cell : -cell, 0, 0)
                        : Vec3(0, 0, to.z > 0 ? cell : -cell);
                }
                break;
            }
            case MoveRouteCode::AwayFromPlayer: {
                Vec3 to = pos - playerPos; to.y = 0.0f;
                delta = (std::fabs(to.x) > std::fabs(to.z))
                    ? Vec3(to.x > 0 ? cell : -cell, 0, 0)
                    : Vec3(0, 0, to.z > 0 ? cell : -cell);
                break;
            }
            case MoveRouteCode::Random: {
                switch (std::rand() % 4) {
                    case 0: delta = Vec3(cell, 0, 0); break;
                    case 1: delta = Vec3(-cell, 0, 0); break;
                    case 2: delta = Vec3(0, 0, cell); break;
                    default: delta = Vec3(0, 0, -cell); break;
                }
                break;
            }
            case MoveRouteCode::Wait:
                mr.waitTimer = step.param > 0 ? step.param / 40.0f : 0.5f;
                mr.stepIndex++;
                continue;
            case MoveRouteCode::TurnDown:  ev.direction = DIR_DOWN;  mr.stepIndex++; continue;
            case MoveRouteCode::TurnLeft:  ev.direction = DIR_LEFT;  mr.stepIndex++; continue;
            case MoveRouteCode::TurnRight: ev.direction = DIR_RIGHT; mr.stepIndex++; continue;
            case MoveRouteCode::TurnUp:    ev.direction = DIR_UP;    mr.stepIndex++; continue;
            case MoveRouteCode::End:
                mr.stepIndex++;
                continue;
            default:
                mr.stepIndex++;
                continue;
        }
        mr.stepIndex++;
        if (dir >= 0) ev.direction = dir;
        else if (glm::length(delta) > 0.001f) {
            if (delta.x > 0) ev.direction = DIR_RIGHT;
            else if (delta.x < 0) ev.direction = DIR_LEFT;
            else if (delta.z > 0) ev.direction = DIR_DOWN;
            else if (delta.z < 0) ev.direction = DIR_UP;
        }
        pos += delta;
        ev.x = (int)std::round(pos.x);
        ev.z = (int)std::round(pos.z);
        mr.waitTimer = 0.05f; // kleine Schritt-Pause, damit Bewegung sichtbar ist
    }
}

// ---------------------------------------------------------------------------
// Demo-Event (Playtest ohne Projektdatei)
// ---------------------------------------------------------------------------
void EventSystem::EnsureDemoEvent() {
    if (!mEvents.empty()) return;

    MapEvent elder;
    elder.id = 1;
    elder.name = "Dorfaeltester";
    elder.worldPos = Vec3(2.0f, 0.0f, 2.0f);
    elder.x = 2; elder.y = 0; elder.z = 2;
    EventPage page;
    page.trigger = EventTrigger::ActionButton;
    page.id = 0;
    {
        EventCommand t; t.code = EventCommandCode::ShowText;
        t.text = "Willkommen in RPG Maker 3D! Ich bin der Dorfaelteste.";
        page.list.push_back(t);
        EventCommand c; c.code = EventCommandCode::ShowChoices;
        c.text = "Was moechtest du wissen?|Gold geben|Nichts";
        c.param1 = 2; c.param2 = 0;
        page.list.push_back(c);
        // XP-Konvention: "Wenn"-Koepfe haben denselben Einzug wie ShowChoices,
        // nur der Body ist tiefer eingerueckt.
        EventCommand w0; w0.code = EventCommandCode::WhenChoice; w0.param1 = 0; w0.indent = 0;
        page.list.push_back(w0);
        EventCommand g; g.code = EventCommandCode::ChangeGold; g.param1 = 50; g.indent = 1;
        page.list.push_back(g);
        EventCommand gm; gm.code = EventCommandCode::ShowText; gm.text = "Hier, nimm 50 Gold."; gm.indent = 1;
        page.list.push_back(gm);
        EventCommand w1; w1.code = EventCommandCode::WhenChoice; w1.param1 = 1; w1.indent = 0;
        page.list.push_back(w1);
        EventCommand nm; nm.code = EventCommandCode::ShowText; nm.text = "Komm bald wieder!"; nm.indent = 1;
        page.list.push_back(nm);
        EventCommand ce; ce.code = EventCommandCode::ChoicesEnd; ce.indent = 0;
        page.list.push_back(ce);
    }
    elder.pages.push_back(page);

    // Seite 2: falls SelfSwitch A an -> nur kurze Grussformel
    EventPage p2;
    p2.id = 1;
    p2.trigger = EventTrigger::ActionButton;
    p2.condition.selfSwitchValid = true;
    p2.condition.selfSwitchCh = 'A';
    EventCommand t2; t2.code = EventCommandCode::ShowText;
    t2.text = "Schoen, dich wiederzusehen!";
    p2.list.push_back(t2);
    elder.pages.push_back(p2);
    mEvents.push_back(elder);

    MapEvent sign;
    sign.id = 2;
    sign.name = "Schild";
    sign.worldPos = Vec3(-2.0f, 0.0f, 1.0f);
    sign.x = -2; sign.z = 1;
    EventPage sp;
    sp.trigger = EventTrigger::ActionButton;
    EventCommand st; st.code = EventCommandCode::ShowText;
    st.text = "(Schild) Norden: Dorf  |  Sueden: Wald";
    sp.list.push_back(st);
    sign.pages.push_back(sp);
    mEvents.push_back(sign);

    // Patrol fuer den Aeltesten
    if (auto* e = GetEvent(1)) {
        e->pages[0].moveType = (int)EventMoveType::Custom;
        e->pages[0].customRoute = "R W40 L W40";
        e->pages[0].routeRepeat = true;
    }

    RPG_LOG_INFO("Demo-Events erstellt (Dorfaeltester + Schild, XP-Struktur)");
}

// ============================================================================
// JSON Event Persistence (formatVersion 2, XP-Codes; liest v1 alt)
// ============================================================================
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
    }
    return "map" + std::to_string(mapId) + "_events.json";
}

std::vector<std::string> FindEventFiles(const std::string& projectPath, int mapId) {
    return {
        projectPath + "/maps/" + FormatMapEventFileName(mapId, true),
        projectPath + "/maps/" + FormatMapEventFileName(mapId, false),
        projectPath + "/maps/Map" + std::to_string(mapId) + "_events.json",
        projectPath + "/" + FormatMapEventFileName(mapId, true),
        projectPath + "/" + FormatMapEventFileName(mapId, false),
        projectPath + "/maps/map" + std::to_string(mapId) + ".json"
    };
}

// Legacy: alte Dateien (formatVersion < 2 bzw. fehlend) hatten andere Codes:
//  104=ShowScreenText -> 181, 105=ShowWorldText -> 182, 106=ClearScreenTexts -> 183,
//  205=SetMoveRoute -> 209, 230=Wait -> 106
EventCommandCode RemapLegacyCode(int code, bool legacy) {
    if (!legacy) return static_cast<EventCommandCode>(code);
    switch (code) {
        case 104: return EventCommandCode::ShowScreenText;
        case 105: return EventCommandCode::ShowWorldText;
        case 106: return EventCommandCode::ClearScreenTexts;
        case 205: return EventCommandCode::SetMoveRoute;
        case 230: return EventCommandCode::Wait;
        default: return static_cast<EventCommandCode>(code);
    }
}

EventCommand ParseCommandObject(const std::string& obj, bool legacy) {
    using namespace JsonUtils;
    EventCommand cmd;
    int code = 0;
    if (TryParseInt(obj, "code", 0, code)) cmd.code = RemapLegacyCode(code, legacy);
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
        size_t pos = 0;
        while (true) {
            size_t q1 = paramsArr.find('\"', pos);
            if (q1 == std::string::npos) break;
            // escapes beruecksichtigen
            std::string raw;
            size_t q2 = q1 + 1;
            while (q2 < paramsArr.size() && paramsArr[q2] != '\"') {
                if (paramsArr[q2] == '\\' && q2 + 1 < paramsArr.size()) { raw.push_back(paramsArr[q2 + 1]); q2 += 2; }
                else { raw.push_back(paramsArr[q2]); q2++; }
            }
            if (q2 >= paramsArr.size()) break;
            cmd.parameters.push_back(raw);
            pos = q2 + 1;
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

EventPage ParsePageObject(const std::string& obj, bool legacy) {
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
    if (TryParseBool(obj, "alwaysOnTop", 0, b)) page.alwaysOnTop = b;
    if (TryParseInt(obj, "moveType", 0, iv)) page.moveType = iv;
    if (TryParseInt(obj, "moveSpeed", 0, iv)) page.moveSpeed = iv;
    if (TryParseInt(obj, "moveFrequency", 0, iv)) page.moveFrequency = iv;
    if (TryParseInt(obj, "direction2D", 0, iv) && iv >= 2 && iv <= 8) page.direction2D = iv;
    std::string s;
    if (TryParseString(obj, "graphicName", 0, s)) page.graphicName = s;
    if (TryParseInt(obj, "graphicIndex", 0, iv)) page.graphicIndex = iv;
    if (TryParseString(obj, "customRoute", 0, s)) page.customRoute = s;
    if (TryParseBool(obj, "routeRepeat", 0, b)) page.routeRepeat = b;
    if (TryParseBool(obj, "routeSkippable", 0, b)) page.routeSkippable = b;
    Vec3 dir;
    if (ParseVec3(obj, "direction", 0, dir)) page.direction = dir;

    std::string condObj;
    if (FindObjectForKey(obj, "condition", 0, condObj))
        page.condition = ParseConditionObject(condObj);
    std::string listArr;
    if (FindArrayForKey(obj, "list", 0, listArr)) {
        auto cmdObjs = ExtractObjectsFromArray(listArr);
        for (auto& co : cmdObjs)
            page.list.push_back(ParseCommandObject(co, legacy));
    }
    return page;
}

MapEvent ParseMapEventObject(const std::string& obj, bool legacy) {
    using namespace JsonUtils;
    MapEvent ev;
    int iv = 0;
    if (TryParseInt(obj, "id", 0, iv)) ev.id = iv;
    if (TryParseInt(obj, "x", 0, iv)) ev.x = iv;
    if (TryParseInt(obj, "y", 0, iv)) ev.y = iv;
    if (TryParseInt(obj, "z", 0, iv)) ev.z = iv;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) ev.name = name;
    Vec3 wp;
    if (ParseVec3(obj, "worldPos", 0, wp)) ev.worldPos = wp;
    else ev.worldPos = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
    bool en = true;
    if (TryParseBool(obj, "enabled", 0, en)) ev.enabled = en;

    std::string pagesArr;
    if (FindArrayForKey(obj, "pages", 0, pagesArr)) {
        auto pageObjs = ExtractObjectsFromArray(pagesArr);
        for (auto& po : pageObjs)
            ev.pages.push_back(ParsePageObject(po, legacy));
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
            if (!c.empty()) { loadedPath = cand; content = c; break; }
        }
    }

    if (content.empty()) {
        RPG_LOG_INFO("Keine Event-Datei fuer Map " + std::to_string(mapId) + " - Demo-Events");
        EnsureDemoEvent();
        return;
    }

    try {
        using namespace JsonUtils;
        bool legacy = true;
        int fv = 0;
        if (TryParseInt(content, "formatVersion", 0, fv) && fv >= 2) legacy = false;

        std::string eventsArr;
        if (!FindArrayForKey(content, "events", 0, eventsArr)) {
            size_t start = content.find('[');
            if (start != std::string::npos) {
                size_t end;
                std::string full;
                if (ExtractArray(content, start, full, end)) eventsArr = full;
            }
        }
        if (eventsArr.empty()) {
            RPG_LOG_WARN("Events-Array nicht gefunden in " + loadedPath + " - Demo-Events");
            EnsureDemoEvent();
            return;
        }
        auto evObjs = ExtractObjectsFromArray(eventsArr);
        for (auto& eo : evObjs) {
            MapEvent ev = ParseMapEventObject(eo, legacy);
            if (ev.id != 0 && !ev.pages.empty())
                mEvents.push_back(std::move(ev));
        }
        if (mEvents.empty()) {
            RPG_LOG_WARN("0 gueltige Events in " + loadedPath + " - Demo-Events");
            EnsureDemoEvent();
        } else {
            RPG_LOG_INFO("Map " + std::to_string(mapId) + ": " +
                         std::to_string(mEvents.size()) + " Events aus " + loadedPath +
                         (legacy ? " (Legacy-Format konvertiert)" : ""));
        }

        // CommonEvents (Projekt-weit) laden falls vorhanden
        try {
            std::string cpath = projectPath + "/maps/CommonEvents.json";
            if (std::filesystem::exists(cpath)) {
                std::string cc = ReadFileToString(cpath);
                if (!cc.empty()) {
                    mCommonEvents.clear();
                    bool cLegacy = legacy;
                    int cfv = 0;
                    if (TryParseInt(cc, "formatVersion", 0, cfv) && cfv >= 2) cLegacy = false;
                    size_t pos = 0;
                    while ((pos = cc.find("\"id\"", pos)) != std::string::npos) {
                        CommonEvent ce;
                        try { ce.id = std::stoi(cc.substr(cc.find(':', pos) + 1)); } catch (...) {}
                        size_t next = cc.find("\"id\"", pos + 4);
                        auto inRange = [&](size_t p) {
                            return p != std::string::npos && (next == std::string::npos || p < next);
                        };
                        size_t np = cc.find("\"name\"", pos);
                        if (inRange(np)) {
                            size_t q1 = cc.find('"', cc.find(':', np) + 1);
                            size_t q2 = cc.find('"', q1 + 1);
                            if (q1 != std::string::npos && q2 != std::string::npos)
                                ce.name = cc.substr(q1 + 1, q2 - q1 - 1);
                        }
                        size_t tp = cc.find("\"trigger\"", pos);
                        if (inRange(tp)) { try { ce.trigger = (EventTrigger)std::stoi(cc.substr(cc.find(':', tp) + 1)); } catch (...) {} }
                        size_t sp = cc.find("\"switchId\"", pos);
                        if (inRange(sp)) { try { ce.switchId = std::stoi(cc.substr(cc.find(':', sp) + 1)); } catch (...) {} }
                        std::string listArr;
                        if (FindArrayForKey(cc, "list", (int)pos, listArr)) {
                            auto cmdObjs = ExtractObjectsFromArray(listArr);
                            for (auto& co : cmdObjs)
                                ce.list.push_back(ParseCommandObject(co, cLegacy));
                        }
                        if (ce.id > 0) mCommonEvents.push_back(ce);
                        pos += 4;
                    }
                    RPG_LOG_INFO("CommonEvents geladen: " + std::to_string(mCommonEvents.size()));
                }
            }
        } catch (...) {}
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("LoadMapEvents fehlgeschlagen: ") + e.what() + " - Demo-Events");
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
            RPG_LOG_ERROR("Event-Datei kann nicht geschrieben werden: " + path);
            return;
        }

        f << "{\n";
        f << "  \"formatVersion\": 2,\n";
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
                f << "          \"alwaysOnTop\": " << (pg.alwaysOnTop ? "true" : "false") << ",\n";
                f << "          \"moveType\": " << pg.moveType << ",\n";
                f << "          \"moveSpeed\": " << pg.moveSpeed << ",\n";
                f << "          \"moveFrequency\": " << pg.moveFrequency << ",\n";
                f << "          \"direction2D\": " << pg.direction2D << ",\n";
                f << "          \"graphicName\": \"" << Escape(pg.graphicName) << "\",\n";
                f << "          \"graphicIndex\": " << pg.graphicIndex << ",\n";
                f << "          \"customRoute\": \"" << Escape(pg.customRoute) << "\",\n";
                f << "          \"routeRepeat\": " << (pg.routeRepeat ? "true" : "false") << ",\n";
                f << "          \"routeSkippable\": " << (pg.routeSkippable ? "true" : "false") << ",\n";
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
        RPG_LOG_INFO("Events gespeichert: " + path + " (" + std::to_string(mEvents.size()) + " Events)");

        // Common Events
        try {
            std::string cpath = projectPath + "/maps/CommonEvents.json";
            std::ofstream cf(cpath);
            cf << "{\n  \"formatVersion\": 2,\n  \"commonEvents\": [\n";
            for (size_t i = 0; i < mCommonEvents.size(); ++i) {
                const auto& ce = mCommonEvents[i];
                cf << "  {\"id\":" << ce.id << ",\"name\":\"" << Escape(ce.name)
                   << "\",\"trigger\":" << (int)ce.trigger
                   << ",\"switchId\":" << ce.switchId
                   << ",\"list\":[";
                for (size_t j = 0; j < ce.list.size(); ++j) {
                    const auto& c = ce.list[j];
                    if (j) cf << ",";
                    cf << "{\"code\":" << (int)c.code
                       << ",\"indent\":" << c.indent
                       << ",\"text\":\"" << Escape(c.text)
                       << "\",\"param1\":" << c.param1
                       << ",\"param2\":" << c.param2
                       << ",\"param3\":" << c.param3 << "}";
                }
                cf << "]}";
                if (i + 1 < mCommonEvents.size()) cf << ",";
                cf << "\n";
            }
            cf << "  ]\n}\n";
        } catch (...) {}
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("SaveMapEvents fehlgeschlagen: ") + e.what());
    }
}

} // namespace rpg
