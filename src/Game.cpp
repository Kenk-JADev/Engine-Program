#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Database.h"
#include <fstream>
#include <filesystem>

namespace rpg {

// --- Self Switches ---
std::string GameSelfSwitches::Key(int mapId, int eventId, char ch) {
    return std::to_string(mapId) + "_" + std::to_string(eventId) + "_" + ch;
}
void GameSelfSwitches::Set(int mapId, int eventId, char ch, bool val) {
    mData[Key(mapId,eventId,ch)] = val;
}
bool GameSelfSwitches::Get(int mapId, int eventId, char ch) const {
    auto it = mData.find(Key(mapId,eventId,ch));
    if (it!=mData.end()) return it->second;
    return false;
}

// --- GameActor ---
void GameActor::Setup(int id) {
    actorId = id;
    const auto* data = Database::Get().GetActor(id);
    if (!data) {
        name = "Actor"+std::to_string(id);
        hp = 100; mp = 30;
        return;
    }
    name = data->name;
    level = data->initialLevel;
    hp = data->initialStats.mhp;
    mp = data->initialStats.mmp;
}
void GameActor::RecoverAll() {
    const auto* data = Database::Get().GetActor(actorId);
    if (data) {
        hp = data->initialStats.mhp;
        mp = data->initialStats.mmp;
    } else {
        hp = 100; mp = 30;
    }
}

// --- GameParty ---
void GameParty::SetupStartingMembers() {
    Clear();
    AddActor(1);
    mGold = 500;
}
void GameParty::AddActor(int actorId) {
    if (HasActor(actorId)) return;
    GameActor ga; ga.Setup(actorId);
    mActors.push_back(ga);
}
void GameParty::RemoveActor(int actorId) {
    mActors.erase(std::remove_if(mActors.begin(), mActors.end(),
        [&](const GameActor& a){return a.actorId==actorId;}), mActors.end());
}
bool GameParty::HasActor(int actorId) const {
    for (auto& a : mActors) if (a.actorId==actorId) return true;
    return false;
}
void GameParty::GainItem(int itemId, int amount) {
    mItems[itemId] += amount;
    if (mItems[itemId]<=0) mItems.erase(itemId);
}
int GameParty::GetItemCount(int itemId) const {
    auto it = mItems.find(itemId);
    if (it!=mItems.end()) return it->second;
    return 0;
}
GameActor* GameParty::GetActor(int actorId) {
    for (auto& a : mActors) if (a.actorId==actorId) return &a;
    return nullptr;
}

// --- GamePlayer ---
void GamePlayer::Move(const Vec3& delta) {
    mPosition += delta;
    if (glm::length(delta)>0.001f) {
        mDirection = glm::normalize(delta);
        mIsMoving = true;
    }
}
void GamePlayer::Update(float dt, Input& input) {
    if (mLocked) {
        mIsMoving = false;
        return;
    }
    Vec3 move{0,0,0};
    float speed = mMoveSpeed * dt;
    // Sprint
    if (input.IsKeyDown(Key::LShift)) speed *= 1.75f;

    if (input.IsKeyDown(Key::W) || input.IsKeyDown(Key::Up))    move.z -= speed;
    if (input.IsKeyDown(Key::S) || input.IsKeyDown(Key::Down))  move.z += speed;
    if (input.IsKeyDown(Key::A) || input.IsKeyDown(Key::Left))  move.x -= speed;
    if (input.IsKeyDown(Key::D) || input.IsKeyDown(Key::Right)) move.x += speed;

    if (glm::length(move) > 0.001f) {
        Move(move);
    } else {
        mIsMoving = false;
    }
}

// --- GameMap ---
void GameMap::Setup(int mapId) {
    mMapId = mapId;
    RPG_LOG_INFO("GameMap setup mapId="+std::to_string(mapId));
}
void GameMap::Update(float dt) {
    (void)dt;
}
bool GameMap::IsPassable(int x, int z) const {
    (void)x; (void)z;
    return true; // TODO: check tileset flags
}

// --- Game ---
Game& Game::Get() {
    static Game instance;
    return instance;
}

void Game::NewGame() {
    NewGameAt(Vec3(
        static_cast<float>(Database::Get().System().startX),
        0.0f,
        static_cast<float>(Database::Get().System().startY)),
        Database::Get().System().startMapId);
}

void Game::NewGameAt(const Vec3& worldPos, int mapId) {
    mSwitches.Clear();
    mVariables.Clear();
    mSelfSwitches.Clear();
    mParty.SetupStartingMembers();
    int mid = mapId > 0 ? mapId : Database::Get().System().startMapId;
    mMap.Setup(mid);
    mPlayer.SetPosition(worldPos);
    mPlayer.SetLocked(false);
    mGameStarted = true;
    RPG_LOG_INFO("New Game at (" + std::to_string(worldPos.x) + ", " +
                 std::to_string(worldPos.y) + ", " + std::to_string(worldPos.z) +
                 ") map=" + std::to_string(mid));
}

bool Game::Save(int slot) {
    try {
        std::filesystem::create_directories("saves");
        std::string path = "saves/save" + std::to_string(slot) + ".sav";
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        int gold = mParty.GetGold();
        f.write((char*)&gold, sizeof(gold));
        Vec3 pos = mPlayer.GetPosition();
        f.write((char*)&pos, sizeof(pos));
        f.close();
        RPG_LOG_INFO("Game saved to "+path);
        return true;
    } catch (...) { return false; }
}
bool Game::Load(int slot) {
    try {
        std::string path = "saves/save" + std::to_string(slot) + ".sav";
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        int gold; f.read((char*)&gold, sizeof(gold));
        mParty.GainGold(gold - mParty.GetGold());
        Vec3 pos; f.read((char*)&pos, sizeof(pos));
        mPlayer.SetPosition(pos);
        f.close();
        mGameStarted = true;
        RPG_LOG_INFO("Game loaded from "+path);
        return true;
    } catch (...) { return false; }
}

void Game::Update(float dt) {
    if (!mGameStarted) return;
    mMap.Update(dt);

    // Lock player while a message/event is blocking
    bool busy = EventSystem::Get().IsWaitingForMessage() ||
                GameUI::Get().Message().IsBusy();
    mPlayer.SetLocked(busy);

    EventSystem::Get().Update(dt, mPlayer.GetPosition());
}

} // namespace rpg
