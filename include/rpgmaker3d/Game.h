#pragma once
// RPG Maker 3D - Game Runtime (Player, Map, Party etc.)

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include "Types.h"
#include "Config.h"
#include "Database.h"

namespace rpg {

class Engine;

// == Switches & Variables (global) ==
class GameSwitches {
public:
    GameSwitches() { mData.resize(EngineConfig::MAX_SWITCHES, false); }
    void Set(int id, bool value) { if (id >=0 && id < (int)mData.size()) mData[id]=value; }
    bool Get(int id) const { if (id>=0 && id<(int)mData.size()) return mData[id]; return false; }
    void Clear() { std::fill(mData.begin(), mData.end(), false); }
    size_t Size() const { return mData.size(); }
private:
    std::vector<bool> mData;
};

class GameVariables {
public:
    GameVariables() { mData.resize(EngineConfig::MAX_VARIABLES, 0); }
    void Set(int id, int value) { if (id>=0 && id<(int)mData.size()) mData[id]=value; }
    int Get(int id) const { if (id>=0 && id<(int)mData.size()) return mData[id]; return 0; }
    void Clear() { std::fill(mData.begin(), mData.end(), 0); }
private:
    std::vector<int> mData;
};

class GameSelfSwitches {
public:
    void Set(int mapId, int eventId, char ch, bool val);
    bool Get(int mapId, int eventId, char ch) const;
    void Clear() { mData.clear(); }
private:
    // key: mapId_eventId_ch
    std::unordered_map<std::string, bool> mData;
    static std::string Key(int mapId, int eventId, char ch);
};

// == Game Actor (Instanz eines Datenbank-Aktors) ==
struct GameActor {
    int actorId = 1;
    int level = 1;
    int hp = 100;
    int mp = 30;
    int exp = 0;
    std::string name;
    int faceIndex = 0;
    std::vector<int> equips;

    void Setup(int id);
    void RecoverAll();
    bool IsDead() const { return hp <= 0; }
};

// == Game Party ==
class GameParty {
public:
    void SetupStartingMembers();
    void AddActor(int actorId);
    void RemoveActor(int actorId);
    bool HasActor(int actorId) const;

    void GainGold(int amount) { mGold += amount; if (mGold<0) mGold=0; }
    int GetGold() const { return mGold; }

    void GainItem(int itemId, int amount);
    int GetItemCount(int itemId) const;

    std::vector<GameActor>& Members() { return mActors; }
    const std::vector<GameActor>& Members() const { return mActors; }

    GameActor* GetActor(int actorId);
    void Clear() { mActors.clear(); mGold=0; mItems.clear(); }

private:
    std::vector<GameActor> mActors;
    int mGold = 500;
    std::unordered_map<int,int> mItems; // itemId -> count
};

// == Game Player (3D) ==
class GamePlayer {
public:
    GamePlayer() = default;

    void SetPosition(const Vec3& pos) { mPosition = pos; }
    const Vec3& GetPosition() const { return mPosition; }

    void SetDirection(const Vec3& dir) {
        if (glm::length(dir) > 1e-5f) mDirection = glm::normalize(dir);
    }
    const Vec3& GetDirection() const { return mDirection; }

    void Move(const Vec3& delta);
    void Update(float dt, class Input& input);

    float GetMoveSpeed() const { return mMoveSpeed; }
    void SetMoveSpeed(float s) { mMoveSpeed = s; }

    bool IsMoving() const { return mIsMoving; }
    /// When true, player input is ignored (dialog / cutscene)
    void SetLocked(bool locked) { mLocked = locked; }
    bool IsLocked() const { return mLocked; }

private:
    Vec3 mPosition{0,0,0};
    Vec3 mDirection{0,0,-1};
    Vec3 mVelocity{0,0,0};
    float mMoveSpeed = 4.5f;
    bool mIsMoving = false;
    bool mLocked = false;
};

// == Forward for Map binding ==
class Map;
class Tileset;

// == Game Map (Runtime) ==
class GameMap {
public:
    void Setup(int mapId);
    void Update(float dt);

    int GetMapId() const { return mMapId; }

    // Bind editor/runtime Map for collision checks
    void BindMap(const Map* map) { mBoundMap = map; }
    const Map* GetBoundMap() const { return mBoundMap; }

    // Grid-based passability (0..width-1, 0..height-1)
    bool IsPassable(int x, int z) const;
    // World-space passability (world coordinates as used by player)
    bool IsPassableWorld(float worldX, float worldZ) const;
    bool IsPassableWorld(const Vec3& worldPos) const { return IsPassableWorld(worldPos.x, worldPos.z); }
    bool IsPassableWithRadius(const Vec3& pos, float radius = 0.35f) const;

    // Convert world position to map grid coordinates
    bool WorldToMap(float worldX, float worldZ, int& outX, int& outZ) const;

    // Map border handling - strict enforcement based on width/height
    bool IsInsideMapBounds(const Vec3& pos, float margin = 0.0f) const;
    bool IsInsideMapBounds(float worldX, float worldZ, float margin = 0.0f) const;
    Vec3 ClampToBounds(const Vec3& pos, float radius = 0.35f) const;
    void GetWorldBounds(float& minX, float& maxX, float& minZ, float& maxZ) const;
    void GetWorldBoundsWithMargin(float& minX, float& maxX, float& minZ, float& maxZ, float radius) const;

    int GetWidth() const;
    int GetHeight() const;

    // Visibility for Scene system - wenn Scenes switchen, kann Map ausgeblendet werden
    void SetVisible(bool v) { mVisible = v; }
    bool IsVisible() const { return mVisible; }
    // Load map data via script
    bool LoadFromFile(const std::string& path);

    void SetDisplayPos(const Vec3& pos) { mDisplayPos = pos; }
    const Vec3& GetDisplayPos() const { return mDisplayPos; }

private:
    int mMapId = 1;
    Vec3 mDisplayPos{0,0,0};
    const Map* mBoundMap = nullptr;
    bool mVisible = true;
};

// == Overall Game ==
class Game {
public:
    static Game& Get();

    void NewGame();
    /// Start playtest at a custom world position (editor "play from here")
    void NewGameAt(const Vec3& worldPos, int mapId = -1);
    bool Save(int slot);
    bool Load(int slot);

    void Update(float dt);

    GameSwitches& Switches() { return mSwitches; }
    GameVariables& Variables() { return mVariables; }
    GameSelfSwitches& SelfSwitches() { return mSelfSwitches; }
    GameParty& Party() { return mParty; }
    GamePlayer& Player() { return mPlayer; }
    GameMap& Map() { return mMap; }

    bool IsGameStarted() const { return mGameStarted; }
    void SetGameStarted(bool v) { mGameStarted = v; }

private:
    Game() = default;
    GameSwitches mSwitches;
    GameVariables mVariables;
    GameSelfSwitches mSelfSwitches;
    GameParty mParty;
    GamePlayer mPlayer;
    GameMap mMap;
    bool mGameStarted = false;
};

} // namespace rpg
