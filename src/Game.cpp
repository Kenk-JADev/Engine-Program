#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/UI.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/BattleSystem.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Database.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace rpg {

// --- Actor-Helfer (Party oder einzeln) ---
void ApplyToActorOrParty(int actorId, const std::function<void(GameActor&)>& fn) {
    auto& party = Game::Get().Party();
    if (actorId > 0) {
        if (auto* a = party.GetActor(actorId)) fn(*a);
    } else {
        for (auto& a : party.Members()) fn(a);
    }
}

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
bool GameSelfSwitches::SetFromKey(const std::string& key, bool val) {
    // Format "mapId_eventId_ch" (z.B. "1_3_A")
    const auto p1 = key.find('_');
    const auto p2 = p1 == std::string::npos ? std::string::npos : key.find('_', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos || p2 + 1 >= key.size())
        return false;
    try {
        const int mapId = std::stoi(key.substr(0, p1));
        const int eventId = std::stoi(key.substr(p1 + 1, p2 - p1 - 1));
        const char ch = key[p2 + 1];
        Set(mapId, eventId, ch, val);
        return true;
    } catch (...) {
        return false;
    }
}

// --- GameActor ---
void GameActor::Setup(int id) {
    actorId = id;
    const auto* data = Database::Get().GetActor(id);
    if (!data) {
        name = "Akteur "+std::to_string(id);
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

// ---------------------------------------------------------------------------
// Provisorische Stat-Kurven (initialStats + ~5%/Level bis Kurven-Editor)
// ---------------------------------------------------------------------------
namespace {
int CurveFor(int base, int level, int current) {
    base = std::max(1, base);
    return std::max(current, base + (level - 1) * std::max(1, base / 20));
}
} // namespace

int GameActor::MaxHp() const {
    if (const auto* ad = Database::Get().GetActor(actorId))
        return CurveFor(ad->initialStats.mhp, level, hp);
    return std::max(hp, 100);
}
int GameActor::MaxMp() const {
    if (const auto* ad = Database::Get().GetActor(actorId))
        return CurveFor(ad->initialStats.mmp, level, mp);
    return std::max(mp, 30);
}
int GameActor::Atk() const {
    int v = 10;
    if (const auto* ad = Database::Get().GetActor(actorId))
        v = CurveFor(ad->initialStats.atk, level, 0);
    // Waffen-Bonus
    if (weaponId > 0)
        for (const auto& w : Database::Get().Weapons())
            if (w.id == weaponId) { v += w.atk; break; }
    return v;
}
int GameActor::Def() const {
    int v = 10;
    if (const auto* ad = Database::Get().GetActor(actorId))
        v = CurveFor(ad->initialStats.def, level, 0);
    // Ruestungs-Bonus aller angelegten Slots
    for (int armorId : armors)
        for (const auto& a : Database::Get().Armors())
            if (a.id == armorId) { v += a.def; break; }
    return v;
}
int GameActor::Agi() const {
    if (const auto* ad = Database::Get().GetActor(actorId))
        return CurveFor(ad->initialStats.agi, level, 0);
    return 10;
}

// ---------------------------------------------------------------------------
// EXP-Kurve aus ClassData (Formel wie RPG Maker VX Ace)
// ---------------------------------------------------------------------------
int GameActor::ExpForNextLevel() const {
    int maxLv = 99;
    if (const auto* ad = Database::Get().GetActor(actorId))
        maxLv = std::max(1, ad->maxLevel);
    if (level >= maxLv) return -1;
    int basis = 30, extra = 20;
    double accA = 30.0, accB = 20.0;
    for (const auto& c : Database::Get().Classes()) {
        if (c.id == classId) {
            basis = c.expBase; extra = c.expExtra;
            accA = c.expAccA; accB = c.expAccB;
            break;
        }
    }
    const double lv = (double)(level + 1);
    double result = basis * std::pow(lv - 1.0, 0.9 + accA / 250.0) * lv * (lv + 1.0);
    result /= (6.0 + lv * lv) / 50.0 / (accB * accB);
    result += extra * (lv - 1.0);
    return (int)std::llround(result);
}

int GameActor::AddExp(int amount) {
    if (amount <= 0) return 0;
    int maxLv = 99;
    if (const auto* ad = Database::Get().GetActor(actorId))
        maxLv = std::max(1, ad->maxLevel);
    if (level >= maxLv) return 0;
    exp += amount;
    int ups = 0;
    while (level < maxLv) {
        const int need = ExpForNextLevel();
        if (need < 0 || exp < need) break;
        ++level;
        ++ups;
    }
    return ups;
}

// --- GameParty ---
void GameParty::SetupStartingMembers() {
    Clear();
    // XP: Anfangsgruppe kommt aus Datenbank -> System -> "Initial Party"
    const auto& party = Database::Get().System().initialParty;
    if (!party.empty()) {
        for (int id : party) if (id > 0) AddActor(id);
    } else {
        AddActor(1);
    }
    if (mActors.empty()) AddActor(1); // Sicherheitsnetz
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

    // XP: Karten-BGM/BGS automatisch abspielen (Karteneigenschaften-Dialog:
    // „Automatisch abspielen" + Dateiname). Die Audio-Bruecke loest den
    // Dateinamen gegen <Projekt>/Audio/BGM|BGS auf; ohne Engine-Injection
    // ist der Aufruf ein no-op.
    for (const auto& mi : Database::Get().MapInfos()) {
        if (mi.id != mapId) continue;
        if (mi.bgmAutoPlay)
            EventSystem_PlayAudio(mi.bgmName, 0, true); // leer = FadeOut (XP)
        if (mi.bgsAutoPlay)
            EventSystem_PlayAudio(mi.bgsName, 1, true);
        break;
    }
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
    mSystem.Reset();
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

void Game::SetSaveDirectory(const std::string& dir) {
    mSaveDirectory = dir.empty() ? "saves" : dir;
}

std::string Game::SavePath(int slot) const {
    if (slot < 1) slot = 1;
    return mSaveDirectory + "/save" + std::to_string(slot) + ".json";
}

// ---------------------------------------------------------------------------
// SaveSlotInfo (XP-Speicherbildschirm): liest nur Kopfdaten der JSON-Datei.
// ---------------------------------------------------------------------------
bool Game::GetSaveSlotInfo(int slot, SaveSlotInfo& out) const {
    out = SaveSlotInfo{};
    const std::string path = SavePath(slot);
    if (!std::filesystem::exists(path)) return false;
    std::ifstream in(path);
    if (!in) return false;
    std::stringstream ss; ss << in.rdbuf();
    const std::string c = ss.str();

    auto findInt = [&](const char* key, int def, size_t from = 0) {
        const auto p = c.find(key, from);
        if (p == std::string::npos) return def;
        const auto col = c.find(':', p);
        if (col == std::string::npos) return def;
        try { return std::stoi(c.substr(col + 1)); } catch (...) { return def; }
    };

    out.exists = true;
    out.saveCount = findInt("\"saveCount\"", 1);
    out.gold = findInt("\"gold\"", 0);
    out.mapId = findInt("\"mapId\"", 0);

    // Erstes Gruppenmitglied (Name + Level) aus dem actors-Block
    const auto actorsPos = c.find("\"actors\"");
    if (actorsPos != std::string::npos) {
        const auto nameKey = c.find("\"name\":\"", actorsPos);
        if (nameKey != std::string::npos) {
            const size_t start = nameKey + 8;
            const size_t end = c.find('"', start);
            if (end != std::string::npos)
                out.actorName = c.substr(start, end - start);
        }
        out.actorLevel = findInt("\"level\"", 1, actorsPos);
    }

    // Kartenname aus der Datenbank aufloesen (fallback: "Karte <id>")
    for (const auto& mi : Database::Get().MapInfos()) {
        if (mi.id == out.mapId) { out.mapName = mi.name; break; }
    }
    if (out.mapName.empty())
        out.mapName = "Karte " + std::to_string(out.mapId);
    return true;
}

// ---------------------------------------------------------------------------
// Kampfstart gegen Trupp (Ruby Game.start_battle, Player --battletest)
// ---------------------------------------------------------------------------
void Game::StartBattleByTroop(int troopId, bool canEscape) {
    std::vector<int> enemies;
    if (const auto* tr = Database::Get().GetTroop(troopId))
        enemies = tr->members;
    if (enemies.empty()) enemies = {1}; // Fallback, damit der Test nie leer ist
    BattleSystem::Get().Setup(enemies, canEscape, false);
    BattleSystem::Get().onMessage = [](const std::string& msg) {
        GameUI::Get().ShowMessage(msg);
    };
    // Keine vorbelegte Aktion mehr: Die Engine oeffnet bei NeedsInput() das
    // XP-Kampfmenue (Angriff/Fertigkeit/Gegenstand/Verteidigen/Flucht).
    GameUI::Get().ShowMessage("Kampf!");
    RPG_LOG_INFO("Kampf gestartet: Trupp " + std::to_string(troopId));
}

bool Game::Save(int slot) {
    try {
        if (slot < 1) slot = 1;
        // Savegames landen IMMER im Save-Verzeichnis des Projekts (vom Player/
        // Editor auf "<Projekt>/saves" gesetzt) – nie relativ zum aktuellen
        // Arbeitsverzeichnis, sonst gehen Spielstaende je nach Startordner
        // verloren oder landen im falschen Projekt.
        std::filesystem::create_directories(mSaveDirectory);
        const std::string path = SavePath(slot);

        // saveCount aus einer vorhandenen Datei weiterzaehlen (XP-Stil)
        int saveCount = 1;
        if (std::filesystem::exists(path)) {
            std::ifstream old(path);
            std::stringstream oss; oss << old.rdbuf();
            const std::string oc = oss.str();
            const auto p = oc.find("\"saveCount\"");
            if (p != std::string::npos) {
                const auto col = oc.find(':', p);
                try { saveCount = std::stoi(oc.substr(col + 1)) + 1; }
                catch (...) { saveCount = 1; }
            }
        }

        std::ofstream f(path);
        if (!f) return false;
        const Vec3 pos = mPlayer.GetPosition();
        f << "{\n";
        f << "  \"version\": 2,\n";
        f << "  \"saveCount\": " << saveCount << ",\n";
        f << "  \"gold\": " << mParty.GetGold() << ",\n";
        f << "  \"mapId\": " << mMap.GetMapId() << ",\n";
        f << "  \"pos\": [" << pos.x << "," << pos.y << "," << pos.z << "],\n";
        // Party-Mitglieder komplett (Level/HP/MP/EXP)
        f << "  \"actors\": [\n";
        auto& members = mParty.Members();
        for (size_t i = 0; i < members.size(); ++i) {
            const auto& a = members[i];
            f << "    {\"id\":" << a.actorId << ",\"name\":\"" << a.name
              << "\",\"level\":" << a.level << ",\"hp\":" << a.hp
              << ",\"mp\":" << a.mp << ",\"exp\":" << a.exp << "}";
            if (i + 1 < members.size()) f << ",";
            f << "\n";
        }
        f << "  ],\n";
        // ALLE Switches/Variables (MAX_SWITCHES/MAX_VARIABLES, nicht nur 64)
        f << "  \"switches\": [";
        for (size_t i = 0; i < mSwitches.Size(); ++i) {
            if (i) f << ",";
            f << (mSwitches.Get((int)i) ? "1" : "0");
        }
        f << "],\n";
        f << "  \"variables\": [";
        for (size_t i = 0; i < EngineConfig::MAX_VARIABLES; ++i) {
            if (i) f << ",";
            f << mVariables.Get((int)i);
        }
        f << "],\n";
        // Self-Switches (A/B/C/D pro Event – nur gesetzte Eintraege)
        f << "  \"selfSwitches\": {";
        bool first = true;
        for (const auto& kv : mSelfSwitches.Data()) {
            if (!kv.second) continue;
            if (!first) f << ",";
            f << "\"" << kv.first << "\":1";
            first = false;
        }
        f << "},\n";
        // Inventar: Items, Waffen, Ruestungen
        auto writeBag = [&](const char* key, const std::unordered_map<int,int>& bag) {
            f << "  \"" << key << "\": {";
            bool fst = true;
            for (const auto& kv : bag) {
                if (!fst) f << ",";
                f << "\"" << kv.first << "\":" << kv.second;
                fst = false;
            }
            f << "}";
        };
        writeBag("items", mParty.Items());
        f << ",\n";
        writeBag("weapons", mParty.Weapons());
        f << ",\n";
        writeBag("armors", mParty.Armors());
        f << "\n}\n";
        f.close();
        RPG_LOG_INFO("Game saved to " + path);
        return true;
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("Save failed: ") + e.what());
        return false;
    }
}

bool Game::Load(int slot) {
    try {
        if (slot < 1) slot = 1;
        std::string path = SavePath(slot);
        // fallback: altes binaeres Format (Legacy)
        if (!std::filesystem::exists(path)) {
            const std::string legacy = mSaveDirectory + "/save" + std::to_string(slot) + ".sav";
            std::ifstream fb(legacy, std::ios::binary);
            if (!fb) return false;
            int gold; fb.read((char*)&gold, sizeof(gold));
            mParty.GainGold(gold - mParty.GetGold());
            Vec3 pos; fb.read((char*)&pos, sizeof(pos));
            mPlayer.SetPosition(pos);
            mGameStarted = true;
            RPG_LOG_INFO("Game loaded (legacy) from " + legacy);
            return true;
        }
        std::ifstream f(path);
        if (!f) return false;
        std::stringstream ss; ss << f.rdbuf();
        std::string c = ss.str();
        auto findNumIn = [&](const std::string& hay, const char* key, float def) -> float {
            auto p = hay.find(std::string("\"") + key + "\"");
            if (p == std::string::npos) return def;
            auto col = hay.find(':', p);
            try { return std::stof(hay.substr(col + 1)); } catch (...) { return def; }
        };
        auto section = [&](const char* key, char open, char close) -> std::string {
            // liefert den Text zwischen { .. } bzw. [ .. ] nach "key"
            auto p = c.find(std::string("\"") + key + "\"");
            if (p == std::string::npos) return {};
            auto b = c.find(open, p);
            if (b == std::string::npos) return {};
            int depth = 0;
            for (size_t i = b; i < c.size(); ++i) {
                if (c[i] == open) ++depth;
                else if (c[i] == close) { if (--depth == 0) return c.substr(b + 1, i - b - 1); }
            }
            return {};
        };
        // {"id": n}-Objekt in int-Map parsen
        auto parseBag = [&](const std::string& body) -> std::vector<std::pair<int,int>> {
            std::vector<std::pair<int,int>> out;
            size_t i = 0;
            while (i < body.size()) {
                auto q1 = body.find('"', i);
                if (q1 == std::string::npos) break;
                auto q2 = body.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                auto col = body.find(':', q2 + 1);
                if (col == std::string::npos) break;
                try {
                    const int id = std::stoi(body.substr(q1 + 1, q2 - q1 - 1));
                    const int n = std::stoi(body.substr(col + 1));
                    out.emplace_back(id, n);
                } catch (...) {}
                i = col + 1;
            }
            return out;
        };

        const int gold = (int)findNumIn(c, "gold", (float)mParty.GetGold());
        const int mapId = (int)findNumIn(c, "mapId", 1);
        Vec3 pos = mPlayer.GetPosition();
        {
            const std::string body = section("pos", '[', ']');
            if (!body.empty()) {
                std::stringstream ps(body);
                char sep; float x=pos.x,y=pos.y,z=pos.z;
                ps >> x >> sep >> y >> sep >> z;
                pos = Vec3(x, y, z);
            }
        }

        // --- Party komplett neu aufbauen (wie XP: Spielstand ersetzt Stand) ---
        struct ActorData { int id; std::string name; int level, hp, mp, exp; };
        std::vector<ActorData> savedActors;
        {
            const std::string body = section("actors", '[', ']');
            size_t i = 0;
            while (i < body.size()) {
                auto b = body.find('{', i);
                if (b == std::string::npos) break;
                auto e = body.find('}', b);
                if (e == std::string::npos) break;
                const std::string blk = body.substr(b, e - b);
                ActorData a{};
                a.id = (int)findNumIn(blk, "id", 0);
                a.level = (int)findNumIn(blk, "level", 1);
                a.hp = (int)findNumIn(blk, "hp", 100);
                a.mp = (int)findNumIn(blk, "mp", 30);
                a.exp = (int)findNumIn(blk, "exp", 0);
                auto n1 = blk.find("\"name\"");
                if (n1 != std::string::npos) {
                    auto q1 = blk.find('"', n1 + 6);
                    auto q2 = q1 == std::string::npos ? std::string::npos : blk.find('"', q1 + 1);
                    if (q2 != std::string::npos) a.name = blk.substr(q1 + 1, q2 - q1 - 1);
                }
                if (a.id > 0) savedActors.push_back(a);
                i = e + 1;
            }
        }

        mParty.Clear(); // setzt Members/Inventar zurueck (Gold=0, unten neu)
        for (const auto& sa : savedActors) {
            mParty.AddActor(sa.id);
            if (auto* ga = mParty.GetActor(sa.id)) {
                ga->level = sa.level;
                ga->hp = sa.hp;
                ga->mp = sa.mp;
                ga->exp = sa.exp;
                if (!sa.name.empty()) ga->name = sa.name;
            }
        }
        mParty.GainGold(gold); // Gold von 0 auf Zielwert

        // Items/Waffen/Ruestungen
        for (const auto& kv : parseBag(section("items", '{', '}')))
            mParty.SetItemCount(kv.first, kv.second);
        for (const auto& kv : parseBag(section("weapons", '{', '}')))
            mParty.SetWeaponCount(kv.first, kv.second);
        for (const auto& kv : parseBag(section("armors", '{', '}')))
            mParty.SetArmorCount(kv.first, kv.second);

        // Karte + Position
        if (mapId > 0) mMap.Setup(mapId);
        mPlayer.SetPosition(pos);

        // Switches / Variables (Arraylaenge flexibel, v1=64, v2=alle)
        {
            const std::string body = section("switches", '[', ']');
            if (!body.empty()) {
                std::stringstream ss2(body);
                std::string item; int idx=0;
                while (std::getline(ss2, item, ',') && idx < (int)mSwitches.Size()) {
                    mSwitches.Set(idx, item.find('1') != std::string::npos);
                    ++idx;
                }
            }
        }
        {
            const std::string body = section("variables", '[', ']');
            if (!body.empty()) {
                std::stringstream ss2(body);
                std::string item; int idx=0;
                while (std::getline(ss2, item, ',') && idx < EngineConfig::MAX_VARIABLES) {
                    try { mVariables.Set(idx, std::stoi(item)); } catch (...) {}
                    ++idx;
                }
            }
        }
        // Self-Switches (Keys sind Strings "mapId_eventId_ch", keine Zahlen!)
        mSelfSwitches.Clear();
        {
            const std::string body = section("selfSwitches", '{', '}');
            size_t i = 0;
            while (i < body.size()) {
                auto q1 = body.find('"', i);
                if (q1 == std::string::npos) break;
                auto q2 = body.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                auto col = body.find(':', q2 + 1);
                if (col == std::string::npos) break;
                const std::string key = body.substr(q1 + 1, q2 - q1 - 1);
                bool on = false;
                try { on = std::stoi(body.substr(col + 1)) != 0; } catch (...) {}
                if (!key.empty()) mSelfSwitches.SetFromKey(key, on);
                i = col + 1;
            }
        }

        mGameStarted = true;
        // Welt nachziehen: Karte (Visual) + Events der geladenen Karten-ID
        // laden (Engine-Hook; no-op, wenn keine Engine injiziert hat).
        EventSystem_NotifyMapChanged(mMap.GetMapId());
        RPG_LOG_INFO("Game loaded from " + path);
        return true;
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("Load failed: ") + e.what());
        return false;
    }
}

void Game::Update(float dt) {
    if (!mGameStarted) return;
    mMap.Update(dt);
    mSystem.Update(dt); // Timer (Control Timer)

    // Lock player while a message/event is blocking or battle is running
    bool busy = EventSystem::Get().IsWaitingForMessage() ||
                EventSystem::Get().IsBlockingEventRunning() ||
                GameUI::Get().Message().IsBusy() ||
                GameUI::Get().IsNumberInputActive() ||
                GameUI::Get().IsNameInputActive() ||
                BattleSystem::Get().IsInBattle();
    mPlayer.SetLocked(busy);

    EventSystem::Get().Update(dt, mPlayer.GetPosition());

    // Random Encounter (RPG Maker Style): Schritte zaehlen wenn Map encounterStep > 0
    // ChangeEncounter (136) deaktiviert Zufallskaempfe komplett
    if (!busy && !BattleSystem::Get().IsInBattle() && mSystem.IsEncounterEnabled()) {
        static Vec3 s_lastPos(0,0,0);
        static float s_stepAccum = 0.f;
        static int s_stepsToEncounter = 0;
        const Vec3 pos = mPlayer.GetPosition();
        float moved = glm::length(Vec3(pos.x - s_lastPos.x, 0, pos.z - s_lastPos.z));
        s_lastPos = pos;
        if (moved > 0.001f) {
            s_stepAccum += moved;
            // ~1 "Schritt" pro 1 Welt-Einheit
            while (s_stepAccum >= 1.0f) {
                s_stepAccum -= 1.0f;
                if (s_stepsToEncounter <= 0) {
                    int step = 30;
                    int mapId = mMap.GetMapId();
                    for (const auto& mi : Database::Get().MapInfos()) {
                        if (mi.id == mapId) {
                            step = mi.encounterStep > 0 ? mi.encounterStep : 0;
                            break;
                        }
                    }
                    if (step <= 0) break; // keine Encounters auf dieser Map
                    // Zufalls-Abstand 50%..150% von encounterStep
                    s_stepsToEncounter = step / 2 + (int)(step * (0.5f + (float)(rand() % 100) / 100.f));
                }
                s_stepsToEncounter--;
                if (s_stepsToEncounter <= 0) {
                    // Troop aus Map encounterList oder Default-Troop 1
                    int troopId = 1;
                    int mapId = mMap.GetMapId();
                    for (const auto& mi : Database::Get().MapInfos()) {
                        if (mi.id == mapId) {
                            for (int k = 0; k < 8; ++k) {
                                if (mi.encounterList[k] > 0) { troopId = mi.encounterList[k]; break; }
                            }
                            break;
                        }
                    }
                    std::vector<int> enemies;
                    if (const auto* tr = Database::Get().GetTroop(troopId))
                        enemies = tr->members;
                    if (enemies.empty()) enemies = {1};
                    BattleSystem::Get().Setup(enemies, true, false);
                    BattleSystem::Get().onMessage = [](const std::string& m) {
                        GameUI::Get().ShowMessage(m);
                    };
                    BattleSystem::Get().onVictory = []() {
                        GameUI::Get().ShowMessage("Sieg! Enemies besiegt.");
                    };
                    // Aktionswahl laeuft ueber das XP-Kampfmenue (Engine).
                    GameUI::Get().ShowMessage("Ein Kampf beginnt!");
                    RPG_LOG_INFO("Random encounter troop=" + std::to_string(troopId));
                    s_stepsToEncounter = 0;
                    break;
                }
            }
        }
    }
}

} // namespace rpg
