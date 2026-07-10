#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Logger.h"
#include <fstream>
#include <algorithm>

namespace rpg {

// --- MapEvent ---
const EventPage* MapEvent::GetCurrentPage() const {
    if (pages.empty()) return nullptr;
    if (currentPage <0 || currentPage >= (int)pages.size()) return nullptr;
    return &pages[currentPage];
}

// --- EventInterpreter ---
EventInterpreter::EventInterpreter() = default;

void EventInterpreter::Setup(const std::vector<EventCommand>& list, int eventId) {
    mList = list;
    mIndex = 0;
    mEventId = eventId;
    mRunning = !list.empty();
    mWaitTime = 0.0f;
    mBranchDepth = 0;
    mBranchResult = true;
}

void EventInterpreter::Clear() {
    mList.clear();
    mIndex = 0;
    mRunning = false;
    mWaitTime = 0.0f;
}

void EventInterpreter::Update(float dt) {
    if (!mRunning) return;

    if (mWaitTime > 0.0f) {
        mWaitTime -= dt;
        if (mWaitTime > 0) return;
        mWaitTime = 0.0f;
    }

    // Execute up to some commands per frame to avoid blocking
    int executed = 0;
    while (mRunning && mIndex < mList.size() && executed < 10) {
        const auto& cmd = mList[mIndex];
        if (!ExecuteCommand(cmd)) {
            // Wait triggered inside ExecuteCommand
            mIndex++;
            return;
        }
        mIndex++;
        executed++;
        if (mIndex >= mList.size()) {
            mRunning = false;
        }
    }
}

bool EventInterpreter::ExecuteCommand(const EventCommand& cmd) {
    switch (cmd.code) {
        case EventCommandCode::ShowText: {
            if (onShowText) onShowText(cmd.text);
            SetWait(0.1f);
            return true;
        }
        case EventCommandCode::ShowChoices: {
            if (onShowChoices) onShowChoices(cmd.text, cmd.param1);
            SetWait(0.1f);
            return true;
        }
        case EventCommandCode::Wait: {
            float secs = cmd.param1 / 60.0f; // RPG Maker wait is in frames (60 fps)
            if (secs <= 0) secs = cmd.param1 * 0.01f;
            SetWait(secs);
            return false; // triggers wait
        }
        case EventCommandCode::PlayBGM: {
            if (onPlayBGM) onPlayBGM(cmd.text, true);
            return true;
        }
        case EventCommandCode::PlaySE: {
            if (onPlaySE) onPlaySE(cmd.text);
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
        case EventCommandCode::ConditionalBranch: {
            mBranchDepth++;
            // Stub: always true
            mBranchResult = true;
            return true;
        }
        case EventCommandCode::EndBranch: {
            if (mBranchDepth>0) mBranchDepth--;
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
}

void EventSystem::AddEvent(const MapEvent& ev) {
    // replace if exists
    for (auto& e : mEvents) if (e.id==ev.id) { e=ev; return; }
    mEvents.push_back(ev);
}
void EventSystem::RemoveEvent(int id) {
    mEvents.erase(std::remove_if(mEvents.begin(), mEvents.end(), [&](const MapEvent& e){return e.id==id;}), mEvents.end());
}
MapEvent* EventSystem::GetEvent(int id) {
    for (auto& e : mEvents) if (e.id==id) return &e;
    return nullptr;
}

void EventSystem::AddCommonEvent(const CommonEvent& ev) {
    for (auto& e : mCommonEvents) if (e.id==ev.id) { e=ev; return; }
    mCommonEvents.push_back(ev);
}
CommonEvent* EventSystem::GetCommonEvent(int id) {
    for (auto& e : mCommonEvents) if (e.id==id) return &e;
    return nullptr;
}

void EventSystem::Update(float dt, const Vec3& playerPos) {
    (void)playerPos;
    for (auto& interp : mInterpreters) {
        if (interp->IsRunning()) interp->Update(dt);
    }
    // remove finished
    mInterpreters.erase(std::remove_if(mInterpreters.begin(), mInterpreters.end(),
        [](const std::unique_ptr<EventInterpreter>& i){return !i->IsRunning();}), mInterpreters.end());
}

void EventSystem::StartEvent(int eventId) {
    auto* ev = GetEvent(eventId);
    if (!ev || !ev->IsValid()) return;
    const EventPage* page = ev->GetCurrentPage();
    if (!page) return;

    auto interpreter = std::make_unique<EventInterpreter>();
    interpreter->onShowText = [](const std::string& txt){ RPG_LOG_INFO("[Event] ShowText: "+txt); };
    interpreter->onPlayBGM = [](const std::string& p, bool loop){ RPG_LOG_INFO("[Event] PlayBGM: "+p); };
    interpreter->onPlaySE = [](const std::string& p){ RPG_LOG_INFO("[Event] PlaySE: "+p); };
    interpreter->Setup(page->list, eventId);
    mInterpreters.push_back(std::move(interpreter));
}

bool EventSystem::IsEventRunning(int eventId) const {
    for (auto& it : mInterpreters) if (it->GetEventId()==eventId && it->IsRunning()) return true;
    return false;
}

void EventSystem::LoadMapEvents(int mapId, const std::string& projectPath) {
    (void)mapId; (void)projectPath;
    RPG_LOG_INFO("LoadMapEvents stub for map "+std::to_string(mapId));
}
void EventSystem::SaveMapEvents(int mapId, const std::string& projectPath) const {
    (void)mapId; (void)projectPath;
    RPG_LOG_INFO("SaveMapEvents stub for map "+std::to_string(mapId));
}

} // namespace rpg
