#pragma once
// RPG Maker 3D - Game Runtime (Player, Map, Party etc.)

#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>
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
    /// Direkter Zugriff fuer das Savegame-System (Key: "mapId_eventId_ch")
    const std::unordered_map<std::string, bool>& Data() const { return mData; }
    /// Key aus Savegame ("1_3_A") wiederherstellen; false bei ungueltigem Key
    bool SetFromKey(const std::string& key, bool val);
private:
    // key: mapId_eventId_ch
    std::unordered_map<std::string, bool> mData;
    static std::string Key(int mapId, int eventId, char ch);
};

// == Game System (Timer, Zugriffs-Flags, BGM-Merker) ==
class GameSystem {
public:
    // Timer (Control Timer, 124)
    void StartTimer(int seconds) { mTimerRemaining = (float)seconds; mTimerWorking = true; }
    void StopTimer() { mTimerWorking = false; mTimerRemaining = 0.0f; }
    bool IsTimerWorking() const { return mTimerWorking; }
    int GetTimerSeconds() const { return (int)(mTimerRemaining + 0.999f); }
    void Update(float dt) {
        if (mTimerWorking && mTimerRemaining > 0.0f) {
            mTimerRemaining -= dt;
            if (mTimerRemaining <= 0.0f) { mTimerRemaining = 0.0f; mTimerWorking = false; }
        }
    }

    // Zugriffs-Flags (134/135/136)
    void SetSaveAccess(bool v) { mSaveAccess = v; }
    bool HasSaveAccess() const { return mSaveAccess; }
    void SetMenuAccess(bool v) { mMenuAccess = v; }
    bool HasMenuAccess() const { return mMenuAccess; }
    void SetEncounterEnabled(bool v) { mEncounterEnabled = v; }
    bool IsEncounterEnabled() const { return mEncounterEnabled; }

    // Windowskin / Battle BGM / ME (131/132/133/247/248)
    void SetWindowskin(const std::string& n) { mWindowskin = n; }
    const std::string& GetWindowskin() const { return mWindowskin; }
    void SetBattleBgm(const std::string& n) { mBattleBgm = n; }
    const std::string& GetBattleBgm() const { return mBattleBgm; }
    void SetBattleEndMe(const std::string& n) { mBattleEndMe = n; }
    const std::string& GetBattleEndMe() const { return mBattleEndMe; }
    void MemorizeBgm(bool memorize) {
        if (memorize) mMemorizedBgm = mBattleBgm;
        else mBattleBgm = mMemorizedBgm;
    }

    void SetMenuCalling(bool v) { mMenuCalling = v; }
    bool IsMenuCalling() const { return mMenuCalling; }
    void Reset() {
        mTimerWorking = false; mTimerRemaining = 0.0f;
        mSaveAccess = mMenuAccess = mEncounterEnabled = true;
    }

private:
    float mTimerRemaining = 0.0f;
    bool mTimerWorking = false;
    bool mSaveAccess = true;
    bool mMenuAccess = true;
    bool mEncounterEnabled = true;
    bool mMenuCalling = false;
    std::string mWindowskin;
    std::string mBattleBgm;
    std::string mBattleEndMe;
    std::string mMemorizedBgm;
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
    // XP-Erweiterungen (fuer Event-Befehle 31x/32x)
    int classId = 1;
    int weaponId = 0;
    std::vector<int> armors;
    std::vector<int> states;  // Status-Ids
    std::vector<int> skills;  // Fertigkeits-Ids
    std::string graphicName;

    void Setup(int id);
    void RecoverAll();
    bool IsDead() const { return hp <= 0; }

    // --- Parameter-Kurven A..E: Interpolation Startwert -> Endwert
    // (Akteure-Tab der Datenbank). Steigen nie unter den aktuellen Ist-Wert. ---
    int MaxHp() const;
    int MaxMp() const;
    int Atk() const;  // Basis-Kurve + Waffen-Bonus
    int Def() const;  // Basis-Kurve + Ruestungs-Bonus (alle Slots) + Waffen-defPlus
    int Agi() const;  // Basis-Kurve + agiPlus von Waffe und Ruestungen
    /// PAKET 23 (XP auto_state): Zustaende der angelegten Ruestungen
    /// (ArmorData.guardStates) mit der Zustandsliste abgleichen — fehlende
    /// hinzufuegen, nicht mehr angelegte Ruestungs-Zustaende entfernen.
    /// Aufruf nach jedem Ausruestungs-Wechsel und in Setup().
    void SyncArmorStates();
    /// Benoetigte Gesamt-EXP fuer (level+1) nach der Klassen-Kurve
    /// (ClassData: expBase/expExtra/expAccA/expAccB, Formel wie VX Ace).
    /// -1 wenn das Max-Level erreicht ist.
    int ExpForNextLevel() const;
    /// EXP gutschreiben; gibt die Anzahl der Level-Aufstiege zurueck.
    /// Bei jedem Aufstieg werden Klassen-Fertigkeiten nachgelernt; die
    /// Namen neu gelernter Fertigkeiten landen optional in learnedNames.
    int AddExp(int amount, std::vector<std::string>* learnedNames = nullptr);
    /// Lernt alle Klassen-Fertigkeiten der Actor-Klasse bis Level 'lvl'
    /// nach (keine Duplikate). Namen neutgelernter Fertigkeiten optional.
    void LearnSkillsUpToLevel(int lvl, std::vector<std::string>* learnedNames = nullptr);
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

    // Waffen/Ruestungen (Event-Befehle 127/128)
    void GainWeapon(int weaponId, int amount) { mWeapons[weaponId] += amount; if (mWeapons[weaponId] <= 0) mWeapons.erase(weaponId); }
    int GetWeaponCount(int weaponId) const { auto it = mWeapons.find(weaponId); return it != mWeapons.end() ? it->second : 0; }
    void GainArmor(int armorId, int amount) { mArmors[armorId] += amount; if (mArmors[armorId] <= 0) mArmors.erase(armorId); }
    int GetArmorCount(int armorId) const { auto it = mArmors.find(armorId); return it != mArmors.end() ? it->second : 0; }

    std::vector<GameActor>& Members() { return mActors; }
    const std::vector<GameActor>& Members() const { return mActors; }

    /// Read-Only-Zugriff fuer das Savegame-System
    const std::unordered_map<int,int>& Items() const { return mItems; }
    const std::unordered_map<int,int>& Weapons() const { return mWeapons; }
    const std::unordered_map<int,int>& Armors() const { return mArmors; }
    /// Direktes Setzen beim Laden (ohne Delta-Rechnung)
    void SetItemCount(int id, int n) { if (n <= 0) mItems.erase(id); else mItems[id] = n; }
    void SetWeaponCount(int id, int n) { if (n <= 0) mWeapons.erase(id); else mWeapons[id] = n; }
    void SetArmorCount(int id, int n) { if (n <= 0) mArmors.erase(id); else mArmors[id] = n; }

    GameActor* GetActor(int actorId);
    void Clear() { mActors.clear(); mGold=0; mItems.clear(); mWeapons.clear(); mArmors.clear(); }

private:
    std::vector<GameActor> mActors;
    int mGold = 500;
    std::unordered_map<int,int> mItems; // itemId -> count
    std::unordered_map<int,int> mWeapons;
    std::unordered_map<int,int> mArmors;
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

    /// Transparenz (Event-Befehl 208)
    void SetTransparent(bool v) { mTransparent = v; }
    bool IsTransparent() const { return mTransparent; }

private:
    Vec3 mPosition{0,0,0};
    Vec3 mDirection{0,0,-1};
    Vec3 mVelocity{0,0,0};
    // PAKET 29: 5.0 Kacheln/s = XP-Geschwindigkeitsstufe 4 ("Normal"),
    // siehe CharacterMotion.h (xp::TilesPerSecondForSpeed).
    float mMoveSpeed = 5.0f;
    bool mIsMoving = false;
    bool mLocked = false;
    bool mTransparent = false;
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
    // dirBit: XP-Richtungsbit (TilesetData::DirDown=1, Left=2, Right=4, Up=8),
    // 0 = richtungslos (nur passage-Flag + solid)
    bool IsPassable(int x, int z) const { return IsPassable(x, z, 0); }
    bool IsPassable(int x, int z, int dirBit) const;
    // World-space passability (world coordinates as used by player)
    bool IsPassableWorld(float worldX, float worldZ) const;
    bool IsPassableWorld(const Vec3& worldPos) const { return IsPassableWorld(worldPos.x, worldPos.z); }
    bool IsPassableWithRadius(const Vec3& pos, float radius = 0.35f) const;
    bool IsPassableWithRadius(const Vec3& pos, float radius, int dirBit) const;

    // XP-Busch-/Terrain-Abfrage unter einer Weltposition (Paket 6 Folge):
    // IsBushAt = irgendeine Ebene des Tiles traegt das Busch-Flag
    // (TilesetData::bushFlags, Paket 1) -> Charakter steht "im Gras".
    // GetTerrainTagAt = Tag des obersten Tiles mit Tag != 0 (0 = keiner).
    // Grid-Varianten: fuer Ruby-Bindings ($game_map.bush?(x, y) u.ae.).
    bool IsBushAt(const Vec3& worldPos) const;
    bool IsBushAt(int x, int z) const;
    int GetTerrainTagAt(const Vec3& worldPos) const;
    int GetTerrainTagAt(int x, int z) const;

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

/// Hilfsfunktion: Aktion auf einen Akteur (id>0) oder die ganze Party (id==0)
void ApplyToActorOrParty(int actorId, const std::function<void(GameActor&)>& fn);

// == Overall Game ==
class Game {
public:
    static Game& Get();

    void NewGame();
    /// Start playtest at a custom world position (editor "play from here")
    void NewGameAt(const Vec3& worldPos, int mapId = -1);
    // PAKET 27: Encounter-Laufzaehler (Schritte/Distanz/Ziel) zuruecksetzen
    // - wird aus NewGameAt und nach Kampfende gerufen, damit kein
    // Positions-Sprung als Schritte zaehlt (Sofort-Kampf-Bug).
    void ResetEncounterSteps();
    bool Save(int slot);
    bool Load(int slot);

    /// Kurzinfo eines Savegame-Slots fuer den XP-Speicherbildschirm
    /// (wird aus der JSON-Datei gelesen, OHNE sie zu laden).
    struct SaveSlotInfo {
        bool exists = false;
        int saveCount = 0;      // wie oft gespeichert wurde (XP-Zaehler)
        int gold = 0;
        int mapId = 0;
        std::string mapName;    // aus der Datenbank aufgeloest
        std::string actorName;  // erstes Gruppenmitglied
        int actorLevel = 0;
    };
    bool GetSaveSlotInfo(int slot, SaveSlotInfo& out) const;

    /// Kampf gegen einen Trupp aus der Datenbank starten (Ruby Game.start_battle,
    /// Player --battletest). canEscape = XP „Flucht erlauben".
    void StartBattleByTroop(int troopId, bool canEscape = true);
    /// Ordner fuer Spielstaende (Standard "saves" relativ zum Arbeits-
    /// verzeichnis). Player/Editor setzen hier "<Projekt>/saves", damit
    /// Savegames IMMER im Projektordner landen – egal von wo die exe
    /// gestartet wird. Zusaetzlich existiert (XP-RMXP-Stil) der Ordner im
    /// Projekt, damit Savegames mit ausgeliefert werden koennen.
    void SetSaveDirectory(const std::string& dir);
    const std::string& GetSaveDirectory() const { return mSaveDirectory; }
    /// Vollstaendiger Pfad zu "save<slot>.json" im Save-Verzeichnis
    std::string SavePath(int slot) const;

    void Update(float dt);

    // ---- XP-Animation-Playback (Paket 5/6, TODO_XP_PARITY.md) ----
    // Läuft komplett über die RGSS-Spriteschicht (kein 3D-Partikelsystem).
    /// Startet Animation <animId> aus der Datenbank beim Spieler
    /// (XP-Referenz -1 = Spieler).
    void StartMapAnimation(int animId);
    /// Startet Animation <animId> an einer Weltposition (XP-„Ziel-Event“):
    /// Engine projiziert ueber worldToScreenHook in den RGSS-Canvas; ohne
    /// Hook/Kamera bleibt das alte Zentrum-Verhalten als Fallback.
    void StartMapAnimationAt(int animId, const Vec3& worldPos);
    /// PAKET 12: Startet Animation <animId> direkt an einer Canvas-Position
    /// (RGSS 640x480, top-origin). Fuer Ziele, die keinen 3D-Weltpunkt haben
    /// — XP-Kampf: Waffen-/Skill-/Item-Animation am Ziel-Battler
    /// (Battler-Bilder/Statuszeile sind normierte Bildschirmpositionen).
    void StartAnimationAtCanvas(int animId, int canvasX, int canvasY);
    bool IsAnimationPlaying() const { return mRunningAnim.active; }
    void UpdateAnimations(float dt);
    /// Wire-once-Hook (Engine): SE-Abspielen zu Frame-Wechseln.
    /// (name wie in Data/Animations.json, vol 0..100, pitch 50..150)
    std::function<void(const std::string& seName, int vol, int pitch)> playSeHook;
    /// Wire-once-Hook (Engine, Paket 6): Weltposition -> RGSS-Canvas (0..640
    /// x 0..480). false = nicht projizierbar (hinter Kamera/kein Fenster).
    std::function<bool(const Vec3& worldPos, float& outCanvasX, float& outCanvasY)> worldToScreenHook;
    /// XP-Bruecke (Stufe 4g, $game_troop): wird bei JEDEM Kampfstart mit der
    /// Truppen-ID aufgerufen — sowohl aus Skripten (Game.start_battle /
    /// StartBattleByTroop) als auch aus dem Event-Befehl „Kampf"
    /// (EventSystem WireInterpreter, troopId aus dem Befehl). Ausgeloest
    /// NACH BattleSystem::Setup, d.h. die Battler stehen bereits. Die
    /// RubyVM verdrahtet damit $game_troop.setup(troop_id).
    std::function<void(int troopId)> onBattleStarted;

    GameSwitches& Switches() { return mSwitches; }
    GameVariables& Variables() { return mVariables; }
    GameSelfSwitches& SelfSwitches() { return mSelfSwitches; }
    GameParty& Party() { return mParty; }
    GamePlayer& Player() { return mPlayer; }
    GameMap& Map() { return mMap; }
    GameSystem& System() { return mSystem; }

    bool IsGameStarted() const { return mGameStarted; }
    void SetGameStarted(bool v) { mGameStarted = v; }

    struct RunningAnimation {
        int animId = 0;
        int frameIdx = -1;              // aktueller Frame in der Sequenz
        float frameTimer = 0.0f;
        int bmpId = 0;                  // RGSS-Spritesheet (0 = Ersatz-Pixel)
        std::vector<int> spriteIds;     // RGSS-Zellen-Sprite-Pool
        int flashSpriteId = 0;
        int flashFramesTotal = 0, flashFramesLeft = 0;
        float flashTimer = 0.0f;
        int flashR = 255, flashG = 255, flashB = 255;
        bool active = false;
        int baseX = 320, baseY = 240;   // Zielzentrum im RGSS-Canvas (640x480)
    };
    RunningAnimation mRunningAnim;
    int mFallbackCellBmpId = 0;         // 8x8-Rund als Ersatzzelle
    void ApplyAnimFrame();              // baut Sprite-Pool auf mRunningAnim.frameIdx
    /// Gemeinsamer Start (Karten-/Kampf-Animation): vorige Sequenz beenden,
    /// Sheet laden/cachen, Zielzentrum setzen, ersten Frame anzeigen.
    void InitRunningAnimation(int animId, int baseX, int baseY);

private:
    Game() = default;
    GameSwitches mSwitches;
    GameVariables mVariables;
    GameSelfSwitches mSelfSwitches;
    GameParty mParty;
    GamePlayer mPlayer;
    GameMap mMap;
    GameSystem mSystem;
    std::string mSaveDirectory = "saves";
    bool mGameStarted = false;
};

} // namespace rpg
