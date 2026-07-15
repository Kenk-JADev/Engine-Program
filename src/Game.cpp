#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Database.h"
#include <fstream>
#include <filesystem>
#include <cmath>

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
        // Keep Y component out of direction
        Vec3 dir = delta;
        dir.y = 0.0f;
        if (glm::length(dir) > 0.001f) {
            mDirection = glm::normalize(dir);
        }
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

    if (glm::length(move) < 0.001f) {
        mIsMoving = false;
        return;
    }

    // Collision handling via GameMap
    const GameMap& gameMap = Game::Get().Map();

    Vec3 current = mPosition;
    Vec3 desired = current + move;

    // Try full move first
    if (gameMap.IsPassableWithRadius(desired)) {
        Move(move);
        return;
    }

    // Slide: try X only
    Vec3 testX = Vec3(current.x + move.x, current.y, current.z);
    bool xPassable = gameMap.IsPassableWithRadius(testX);
    // Try Z only
    Vec3 testZ = Vec3(current.x, current.y, current.z + move.z);
    bool zPassable = gameMap.IsPassableWithRadius(testZ);

    if (xPassable && !zPassable) {
        Move(Vec3(move.x, 0, 0));
        return;
    }
    if (zPassable && !xPassable) {
        Move(Vec3(0, 0, move.z));
        return;
    }
    if (xPassable && zPassable) {
        // Both individually passable but diagonal blocked by corner
        // Choose larger component
        if (std::abs(move.x) > std::abs(move.z)) {
            Move(Vec3(move.x, 0, 0));
        } else {
            Move(Vec3(0, 0, move.z));
        }
        return;
    }

    // Completely blocked
    mIsMoving = false;
    // Still update direction for facing even when blocked
    Vec3 dir = move;
    dir.y = 0;
    if (glm::length(dir) > 0.001f) {
        mDirection = glm::normalize(dir);
    }
}

// --- GameMap ---
void GameMap::Setup(int mapId) {
    mMapId = mapId;
    mVisible = true; // beim Map Wechsel sichtbar machen
    RPG_LOG_INFO("GameMap setup mapId="+std::to_string(mapId));
}

bool GameMap::LoadFromFile(const std::string& path) {
    if (!mBoundMap) {
        RPG_LOG_WARN("GameMap::LoadFromFile - no bound map, cannot load " + path);
        return false;
    }
    // Versuche Editor Map zu laden (const_cast, da BoundMap eigentlich editierbar ist während Playtest)
    Map* mutableMap = const_cast<Map*>(mBoundMap);
    if (mutableMap->Load(path)) {
        RPG_LOG_INFO("GameMap loaded from file: " + path);
        return true;
    }
    RPG_LOG_WARN("GameMap failed to load: " + path);
    return false;
}

void GameMap::Update(float dt) {
    (void)dt;
}

bool GameMap::WorldToMap(float worldX, float worldZ, int& outX, int& outZ) const {
    if (!mBoundMap) {
        // Fallback using default 20x20 assumption
        int w = 20, h = 20;
        outX = static_cast<int>(std::floor(worldX + w * 0.5f));
        outZ = static_cast<int>(std::floor(worldZ + h * 0.5f));
        return true;
    }
    int w = mBoundMap->GetWidth();
    int h = mBoundMap->GetHeight();
    outX = static_cast<int>(std::floor(worldX + w * 0.5f));
    outZ = static_cast<int>(std::floor(worldZ + h * 0.5f));
    return true;
}

bool GameMap::IsPassable(int x, int z) const {
    if (!mBoundMap) {
        // No map bound -> always passable (editor startup)
        return true;
    }
    int w = mBoundMap->GetWidth();
    int h = mBoundMap->GetHeight();
    if (x < 0 || x >= w || z < 0 || z >= h) {
        // Outside map bounds = blocked
        return false;
    }

    const auto& layers = mBoundMap->GetLayers();
    auto tileset = mBoundMap->GetTileset();

    bool hasTile = false;
    for (const auto& layer : layers) {
        // Skip empty layers? Check all for collision
        if (x >= layer.width || z >= layer.height) continue;
        int idx = z * layer.width + x;
        if (idx < 0 || idx >= (int)layer.tiles.size()) continue;
        int tileId = layer.tiles[idx];
        if (tileId < 0) continue; // empty

        hasTile = true;

        if (tileset) {
            const TileInfo* info = tileset->GetTileInfo(tileId);
            if (info && info->solid) {
                return false; // blocked by solid flag
            }
        }

        // Additional check via Database TilesetData flags if available
        // For now, TileInfo solid is authoritative
    }

    // If no tile at all (void), treat as blocked to prevent falling off map
    // But allow if map is empty (during initialization)
    if (!hasTile && !layers.empty()) {
        // Check if ground layer exists and is empty -> consider blocked unless map is in initial state
        // For safety, if there is at least one layer with data elsewhere, empty spot is blocked
        // Here we assume empty = passable for flexibility (mapper can leave holes)
        // Change to false if you want strict blocking:
        return true;
    }

    return true;
}

bool GameMap::IsPassableWorld(float worldX, float worldZ) const {
    int mx, mz;
    WorldToMap(worldX, worldZ, mx, mz);
    return IsPassable(mx, mz);
}

bool GameMap::IsPassableWithRadius(const Vec3& pos, float radius) const {
    // First check map border - strict enforcement
    if (!IsInsideMapBounds(pos, radius)) return false;

    // Check center and 4 cardinal points plus diagonals for robust collision
    if (!IsPassableWorld(pos.x, pos.z)) return false;
    if (radius <= 0.0f) return true;

    // Cardinal checks
    if (!IsPassableWorld(pos.x + radius, pos.z)) return false;
    if (!IsPassableWorld(pos.x - radius, pos.z)) return false;
    if (!IsPassableWorld(pos.x, pos.z + radius)) return false;
    if (!IsPassableWorld(pos.x, pos.z - radius)) return false;

    // Diagonal checks for corner clipping (half radius)
    float diag = radius * 0.7071f;
    if (!IsPassableWorld(pos.x + diag, pos.z + diag)) return false;
    if (!IsPassableWorld(pos.x - diag, pos.z + diag)) return false;
    if (!IsPassableWorld(pos.x + diag, pos.z - diag)) return false;
    if (!IsPassableWorld(pos.x - diag, pos.z - diag)) return false;

    return true;
}

int GameMap::GetWidth() const {
    if (mBoundMap) return mBoundMap->GetWidth();
    return 20;
}

int GameMap::GetHeight() const {
    if (mBoundMap) return mBoundMap->GetHeight();
    return 20;
}

void GameMap::GetWorldBounds(float& minX, float& maxX, float& minZ, float& maxZ) const {
    int w = GetWidth();
    int h = GetHeight();
    // Map is centered at 0,0, tiles are 1x1, so world goes from -w*0.5 to +w*0.5
    // But with floor conversion, max valid world is w*0.5 - epsilon
    minX = -w * 0.5f;
    maxX = w * 0.5f;
    minZ = -h * 0.5f;
    maxZ = h * 0.5f;
}

void GameMap::GetWorldBoundsWithMargin(float& minX, float& maxX, float& minZ, float& maxZ, float radius) const {
    GetWorldBounds(minX, maxX, minZ, maxZ);
    minX += radius;
    maxX -= radius;
    minZ += radius;
    maxZ -= radius;
}

bool GameMap::IsInsideMapBounds(float worldX, float worldZ, float margin) const {
    float minX, maxX, minZ, maxZ;
    GetWorldBoundsWithMargin(minX, maxX, minZ, maxZ, margin);
    return worldX >= minX && worldX <= maxX && worldZ >= minZ && worldZ <= maxZ;
}

bool GameMap::IsInsideMapBounds(const Vec3& pos, float margin) const {
    return IsInsideMapBounds(pos.x, pos.z, margin);
}

Vec3 GameMap::ClampToBounds(const Vec3& pos, float radius) const {
    float minX, maxX, minZ, maxZ;
    GetWorldBoundsWithMargin(minX, maxX, minZ, maxZ, radius);
    Vec3 clamped = pos;
    clamped.x = std::clamp(clamped.x, minX, maxX);
    clamped.z = std::clamp(clamped.z, minZ, maxZ);
    return clamped;
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
    // Clamp spawn to map bounds - enforce map border
    Vec3 clampedPos = worldPos;
    if (mMap.GetBoundMap()) {
        clampedPos = mMap.ClampToBounds(worldPos, 0.4f);
        if (glm::length(clampedPos - worldPos) > 0.01f) {
            RPG_LOG_INFO("Spawn clamped from (" + std::to_string(worldPos.x) + "," + std::to_string(worldPos.z) +
                         ") to (" + std::to_string(clampedPos.x) + "," + std::to_string(clampedPos.z) + ") to fit map bounds");
        }
        // Also ensure spawn point itself is passable, otherwise find nearest passable
        if (!mMap.IsPassableWithRadius(clampedPos, 0.4f)) {
            // Search nearby for passable spot
            bool found = false;
            for (float r = 0.5f; r < 5.0f && !found; r += 0.5f) {
                for (int angle = 0; angle < 360 && !found; angle += 45) {
                    float rad = glm::radians((float)angle);
                    Vec3 test = clampedPos + Vec3(std::cos(rad) * r, 0, std::sin(rad) * r);
                    if (mMap.IsPassableWithRadius(test, 0.4f)) {
                        clampedPos = test;
                        found = true;
                        RPG_LOG_INFO("Found alternative passable spawn at (" + std::to_string(test.x) + "," + std::to_string(test.z) + ")");
                    }
                }
            }
        }
    }
    mPlayer.SetPosition(clampedPos);
    mPlayer.SetLocked(false);
    mGameStarted = true;
    RPG_LOG_INFO("New Game at (" + std::to_string(clampedPos.x) + ", " +
                 std::to_string(clampedPos.y) + ", " + std::to_string(clampedPos.z) +
                 ") map=" + std::to_string(mid) + " (requested " +
                 std::to_string(worldPos.x) + "," + std::to_string(worldPos.z) + ")");
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
