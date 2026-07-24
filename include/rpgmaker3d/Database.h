#pragma once
// RPG Maker 3D - Datenbanksystem
// Ähnlich RPG Maker: Actors, Classes, Items, Weapons, Skills, Enemies etc.

#include <string>
#include <vector>
#include <unordered_map>
#include "Types.h"

namespace rpg {

struct ActorData {
    int id = 0;
    std::string name = "Hero";
    std::string nickname;
    std::string className = "Warrior";
    int initialLevel = 1;
    int maxLevel = 99;
    std::string characterName; // sprite / model name
    int characterIndex = 0;
    std::string faceName;
    int faceIndex = 0;
    std::string battlerName;
    // Stats pro Level
    struct Stats {
        int mhp = 100;
        int mmp = 30;
        int atk = 10;
        int def = 10;
        int mat = 5;
        int mdf = 5;
        int agi = 10;
        int luk = 10;
    };
    Stats initialStats;
    Stats finalStats; // Endwerte bei maxLevel (mhp<=0 => alte lineare Kurve)
    // XP-Parameterkurven: A = sehr schnelles Wachstum .. E = sehr langsames
    char curveHp = 'C';
    char curveMp = 'C';
    char curveAtk = 'C';
    char curveDef = 'C';
    char curveAgi = 'C';
    std::vector<int> equips; // item ids
    // PAKET 17: XP state_ranks — index = Zustands-ID - 1, Wert 0..5 =
    // Rang A..F (Trefferchance 100/80/60/40/20/0 %). Fehlender Eintrag = C.
    std::vector<int> stateRanks;
};

struct ClassData {
    int id = 0;
    std::string name = "Warrior";
    // Exp curve params
    int expBase = 30;
    int expExtra = 20;
    float expAccA = 30;
    float expAccB = 20;
    // Fertigkeiten, die ab einem Level automatisch gelernt werden (XP-Stil)
    struct Learning { int level = 1; int skillId = 1; };
    std::vector<Learning> learnings;
};

struct ItemData {
    enum class Type { Regular, KeyItem, HiddenA, HiddenB };
    enum class Scope { None, OneEnemy, AllEnemies, OneAlly, AllAllies, OneAllyDead, AllAlliesDead, User };
    int id = 0;
    std::string name = "Potion";
    std::string description;
    std::string iconName;
    int iconIndex = 0;
    Type itemType = Type::Regular;
    int price = 50;
    bool consumable = true;
    Scope scope = Scope::OneAlly;
    int hpRecovery = 100;
    int mpRecovery = 0;
    int animationId = 0;
    // PAKET 20: XP plus_state_set / minus_state_set — Zustands-IDs, die
    // das Item bei Benutzung verhaengt (Kampf: Wurf gegen Resistenz-Rang)
    // bzw. heilt (Kampf UND Menue, z. B. Gegengift).
    std::vector<int> plusStates;
    std::vector<int> minusStates;
};

struct WeaponData {
    int id = 0;
    std::string name = "Sword";
    std::string description;
    int iconIndex = 0;
    int price = 100;
    int atk = 10;
    int animationId = 0;
    // PAKET 22: XP plus_state_set / minus_state_set der Waffe — die IDs
    // werden beim Standardangriff auf das getroffene Ziel gewuerfelt
    // (Resistenz-Rang) bzw. sicher geheilt.
    std::vector<int> plusStates;
    std::vector<int> minusStates;
};

struct ArmorData {
    int id = 0;
    std::string name = "Shield";
    std::string description;
    int iconIndex = 0;
    int price = 100;
    int def = 10;
    int mdf = 5;
    enum class Type { Shield, Helmet, Body, Accessory } armorType = Type::Shield;
};

struct SkillData {
    int id = 0;
    std::string name = "Fire";
    std::string description;
    int iconIndex = 0;
    int mpCost = 10;
    int scope = 1; // 1=enemy
    int power = 100;
    std::string animation = "fire";
    // PAKET 12: XP animation_id (Animations-Tab) fuer die Kampf-Animation.
    // 0 = Fallback: obiger Namens-String per Datenbank-Abgleich aufloesen.
    int animationId = 0;
    // PAKET 17: XP plus_state_set / minus_state_set — Zustands-IDs, die der
    // Skill beim Treffer verhaengt (Wurf gegen den Resistenz-Rang des Ziels)
    // bzw. sicher heilt (z. B. Esuna).
    std::vector<int> plusStates;
    std::vector<int> minusStates;
    // PAKET 21: XP occasion — 0=Immer, 1=Nur im Kampf, 2=Nur im Menue,
    // 3=Nie. Steuert die Benutzbarkeit in den jeweiligen Skill-Listen
    // (XP Game_Actor#skill_can_use?): Kampf filtert 2 und 3 heraus,
    // Menue 1 und 3 (Eintrag bleibt sichtbar, aber deaktiviert).
    int occasion = 0;
};

struct EnemyData {
    int id = 0;
    std::string name = "Slime";
    std::string battlerName = "slime";
    int battlerHue = 0;
    int maxHp = 100;
    int maxMp = 10;
    int atk = 15;
    int def = 5;
    int mat = 5;
    int mdf = 5;
    int agi = 8;
    int luk = 5;
    int exp = 10;
    int gold = 5;
    std::vector<int> dropItems; // item ids
    // PAKET 17: XP state_ranks — index = Zustands-ID - 1, Wert 0..5 =
    // Rang A..F (Trefferchance 100/80/60/40/20/0 %). Fehlender Eintrag = C.
    std::vector<int> stateRanks;
    // PAKET 18: XP RPG::Enemy.actions — Verhaltenstabelle des Gegners.
    // kind 0 = Basis-Aktion (basic: 0 Angriff, 1 Verteidigen, 2 Flucht,
    // 3 Nichtstun), kind 1 = Fertigkeit (skillId). Gegner ohne Eintraege
    // fallen auf den bisherigen Standardangriff zurueck.
    // Bedingungen pro Aktion (alle erfuellt, XP): Runde == turnA + turnB*x
    // (x>=0), eigene HP <= hpBelow %, hoechstes Party-Level >= level,
    // Schalter switchId AN (0 = egal). rating (1..10) gewichtet die Wahl —
    // XP laesst nur Aktionen mit rating > max-3 in den Lostopf.
    // Skills zaehlen nur, wenn das Ziel sie sich mp-maessig leisten kann.
    struct Action {
        int kind = 0;      // 0 = Basis, 1 = Fertigkeit
        int basic = 0;     // kind 0: 0 Angriff 1 Verteidigen 2 Flucht 3 Nichts
        int skillId = 0;   // kind 1: Fertigkeits-ID
        int rating = 5;    // 1..10
        int turnA = 0;     // Runde turnA + turnB*x (0/0 = immer)
        int turnB = 0;
        int hpBelow = 100; // eigene HP <= x % (100 = immer)
        int level = 1;     // hoechstes Party-Level >= x (1 = immer)
        int switchId = 0;  // 0 = keine Schalter-Bedingung
    };
    std::vector<Action> actions;
};

// Truppe (Gegner-Gruppe fuer Random Encounters / Battle Processing)
// Kampf-Ereignis-Seite eines Trupps (XP-Stil). Alle aktivierten Bedingungen
// muessen erfuellt sein; die Befehle laufen ueber ein Gemeinsames Ereignis
// (commonEventId, Tab "Gem. Events"), damit der volle Befehlsumfang direkt
// editierbar ist.
struct TroopPage {
    bool switchValid = false; int switchId = 1;                     // Schalter AN
    bool turnValid = false;   int turnA = 0; int turnB = 0;         // Runde turnA + turnB*x
    bool actorValid = false;  int actorIndex = 1; int actorHpBelow = 50; // Party-Platz (1-basiert), HP <= x%
    bool enemyValid = false;  int enemyIndex = 1; int enemyHpBelow = 50; // Trupp-Platz (1-basiert), HP <= x%
    int span = 0;             // 0=Kampf (1x je Kampf), 1=Runde (1x je Runde), 2=Moment (sofort, neu bei Nicht-Erfuellung)
    int commonEventId = 0;    // 0 = nichts ausfuehren
};

struct TroopData {
    int id = 0;
    std::string name = "Troop";
    std::vector<int> members; // enemy ids
    std::vector<TroopPage> pages; // Kampf-Ereignisse (XP)
};

// Status-Effekt (Poison, Sleep, ...)
struct StateData {
    int id = 0;
    std::string name = "Poison";
    std::string description;
    int restriction = 0; // 0=none 1=attack enemy 2=attack anyone 3=attack ally 4=cannot move
    int priority = 50;
    bool removeAtBattleEnd = true;
    int autoRemovalTiming = 0; // 0=none 1=action end 2=turn end
    int holdTurn = 0;
    float hpDrainRate = 0.0f; // 0..1 per turn
};

struct TilesetData {
    int id = 0;
    std::string name = "World";
    std::string tilesetName = "tileset_demo.png";

    // ---- XP Tileset-Tab (Paket 1/2, TODO_XP_PARITY.md) ----
    // Grafik-Zuordnungen (XP legt diese am TILESET fest, nicht an der Map)
    std::string autotileNames[7];     // 7 Autotile-Slots ("" = leer)
    std::string panoramaName;         // Panorama-Grafik
    std::string fogName;              // Nebel-Grafik
    std::string battlebackName;       // Kampfhintergrund

    // Flag-Tabellen, Index = Tile-ID (= Zeilenindex im Tileset-Grid).
    // Kurze/leere Vektoren sind erlaubt: Zugriff immer ueber die Getter
    // (Default 0), niemals direkt per [] auf evtl. fehlende Eintraege!
    std::vector<int> flags;          // "passage": 0 = begehbar, 1 = blockiert
    std::vector<int> passage4dir;    // Bits 1=unten,2=links,4=rechts,8=oben;
                                     // 0 = Default (alle Richtungen frei)
    std::vector<int> priority;       // XP-Prioritaet 0..5
    std::vector<int> bushFlags;      // 0/1 Durchwiese ("im Gras stehen")
    std::vector<int> counterFlags;   // 0/1 Tresen (Event darueber hinweg ausloesen)
    std::vector<int> terrainTags;    // 0..7 frei verwendbarer Boden-Tag

    // Sichere Getter (Default wenn Vektor zu kurz)
    static int VecGet(const std::vector<int>& v, int tileId, int def = 0) {
        if (tileId < 0 || (size_t)tileId >= v.size()) return def;
        return v[(size_t)tileId];
    }
    int GetPassage(int tileId)    const { return VecGet(flags, tileId); }
    int GetPassage4Dir(int tileId)const { return VecGet(passage4dir, tileId); }
    int GetPriority(int tileId)   const { return VecGet(priority, tileId); }
    int GetBush(int tileId)       const { return VecGet(bushFlags, tileId); }
    int GetCounter(int tileId)    const { return VecGet(counterFlags, tileId); }
    int GetTerrainTag(int tileId) const { return VecGet(terrainTags, tileId); }

    // Richtungsbits fuer passage4dir
    enum DirBit { DirDown = 1, DirLeft = 2, DirRight = 4, DirUp = 8 };

    // XP-Regel: 4Dir==0 -> alle Richtungen frei; sonst muss das Bit gesetzt sein
    bool IsPassableDir(int tileId, int dirBit) const {
        if (GetPassage(tileId) != 0) return false; // komplett blockiert
        int d = GetPassage4Dir(tileId);
        if (d == 0) return true;
        return (d & dirBit) != 0;
    }

    // Vektor auf mindestens count Eintraege bringen (Editor-Komfort)
    static void EnsureSize(std::vector<int>& v, size_t count) {
        if (v.size() < count) v.resize(count, 0);
    }
    void EnsureFlagSizes(size_t count) {
        EnsureSize(flags, count);        EnsureSize(passage4dir, count);
        EnsureSize(priority, count);     EnsureSize(bushFlags, count);
        EnsureSize(counterFlags, count); EnsureSize(terrainTags, count);
    }
};

// ---------------------------------------------------------------------------
// XP-Animationen (Paket 5, TODO_XP_PARITY.md)
// Spritesheet: Zellen 192x192 px, 5 Zellen pro Zeile, Index 0..95
// (XP: cell_max 100; wir erlauben 0..95 = 4 Zeilen + Reserve).
// ---------------------------------------------------------------------------
struct AnimCell {
    int cellId = 0;      // Bildzelle 0..95 (x = id%5, y = id/5 im Sheet)
    int x = 0;           // Position relativ zum Zielzentrum (px, XP-2D)
    int y = 0;
    int scale = 100;     // %
    int rotation = 0;    // Grad (0..360)
    int opacity = 255;   // 0..255
};

struct AnimFrame {
    std::vector<AnimCell> cells;
    // pro Frame optional: Sound + Bildschirmblitz (XP-Animation-Timing)
    std::string seName;
    int seVolume = 100;  // 0..100
    int sePitch = 100;   // 50..150
    int flashScope = 0;  // 0=keiner 1=Ziel 2=Bildschirm
    int flashR = 255, flashG = 255, flashB = 255; // Farbe
    int flashDuration = 5;                        // in Frames
};

struct AnimationData {
    int id = 0;
    std::string name = "Animation";
    std::string file;                 // Grafik unter Graphics/Animations/ o. <projekt>/Graphics/Animations/
    std::vector<AnimFrame> frames;    // Ablauffolge, ~15 fps wie XP
    int position = 2;                 // 0=Oben 1=Mitte 2=Unten (rel. zum Ziel)
};

struct MapInfo {
    int id = 0;
    std::string name = "Map";
    bool expanded = false;
    int parentId = 0;
    int order = 0;
    int scrollX = 0;
    int scrollY = 0;
    
    // RPG Maker Map Properties
    int width = 20;
    int height = 20;
    int tilesetId = 1;
    std::string bgmName = "";
    bool bgmAutoPlay = true;
    std::string bgsName = "";
    bool bgsAutoPlay = true;
    bool disableDashing = false;
    std::string battleback1Name = "";
    std::string battleback2Name = "";
    int displayX = 0;
    int displayY = 0;
    int parallaxLoopX = 0;
    int parallaxLoopY = 0;
    std::string parallaxName = "";
    bool parallaxShow = true;
    int parallaxSx = 0;
    int parallaxSy = 0;
    int scrollType = 0; // 0=No Loop, 1=Vertical Loop, 2=Horizontal Loop, 3=Both Loop
    int specifyBattleback = 0;
    int backgroundType = 1; // 1=Parallax, 2=Color
    Color backgroundColor = Color(0, 0, 0, 1);
    bool fogEnabled = false;
    std::string fogName = "";
    int fogBlendMode = 0;
    Color fogColor = Color(0.5f, 0.5f, 0.5f, 1.0f);
    int fogOpacity = 64;
    int fogZoom = 100;
    int fogSx = 0;
    int fogSy = 0;
    int encounterList[8] = {0};
    int encounterStep = 30;
    int noteId = 0; // für zukünftige Notizen
};

struct SystemData {
    std::string gameTitle = "RPG Maker 3D Game";
    std::string currencyUnit = "G";
    int startMapId = 1;
    int startX = 0;
    int startY = 0;

    // ---- XP System-Tab ----
    // Anfaengliche Gruppe (Akteur-IDs)
    std::vector<int> initialParty;
    // Element-Namen (Liste wie im XP-System-Tab)
    std::vector<std::string> elements;
    // Animations-Namen (IDs = Index+1; Dateiname wird am Befehl angegeben)
    std::vector<std::string> animations;

    // Grafiken
    std::string windowskinName = "001-Blue01";
    std::string titleGraphicName = "001-Title01";
    std::string gameoverGraphicName = "001-Gameover01";
    std::string battleTransitionName = "003-Blind03";

    // BGM / ME
    std::string battleBgm = "battle.ogg";
    std::string titleBgm = "title.ogg";
    std::string gameoverMe = "gameover.ogg";
    std::string battleEndMe = "001-Victory01";

    // Sound-Effekte (SE-Namen)
    std::string cursorSe = "001-System01";
    std::string decisionSe = "002-System02";
    std::string cancelSe = "003-System03";
    std::string buzzerSe = "004-System04";
    std::string equipSe = "005-System05";
    std::string shopSe = "006-System06";
    std::string saveSe = "007-System07";
    std::string loadSe = "008-System08";
    std::string battleStartSe = "009-System09";
    std::string escapeSe = "010-System10";
    std::string actorCollapseSe = "011-System11";
    std::string enemyCollapseSe = "012-System12";

    // Woerter / Begriffe ("Words" im XP-System-Tab)
    std::string wordWeapon = "Weapon";
    std::string wordShield = "Shield";
    std::string wordHelmet = "Helmet";
    std::string wordBodyArmor = "Body Armor";
    std::string wordAccessory = "Accessory";
    std::string wordHp = "HP";
    std::string wordSp = "SP";
    std::string wordStr = "STR";
    std::string wordDex = "DEX";
    std::string wordAgi = "AGI";
    std::string wordInt = "INT";
    std::string wordAtk = "ATK";
    std::string wordPdef = "PDEF";
    std::string wordMdef = "MDEF";
    std::string wordAttack = "Attack";
    std::string wordSkill = "Skill";
    std::string wordDefend = "Defend";
    std::string wordItem = "Item";
    std::string wordEquip = "Equip";

    std::vector<std::string> switches;
    std::vector<std::string> variables;
};

class Database {
public:
    static Database& Get();

    void Clear();
    bool Load(const std::string& projectPath);
    bool Save(const std::string& projectPath) const;

    // Accessors
    std::vector<ActorData>& Actors() { return mActors; }
    std::vector<ClassData>& Classes() { return mClasses; }
    std::vector<ItemData>& Items() { return mItems; }
    std::vector<WeaponData>& Weapons() { return mWeapons; }
    std::vector<ArmorData>& Armors() { return mArmors; }
    std::vector<SkillData>& Skills() { return mSkills; }
    std::vector<EnemyData>& Enemies() { return mEnemies; }
    std::vector<TroopData>& Troops() { return mTroops; }
    std::vector<StateData>& States() { return mStates; }
    std::vector<TilesetData>& Tilesets() { return mTilesets; }
    std::vector<AnimationData>& AnimationSet() { return mAnimations; }
    std::vector<MapInfo>& MapInfos() { return mMapInfos; }
    SystemData& System() { return mSystem; }

    const ActorData* GetActor(int id) const;
    const ClassData* GetClass(const std::string& name) const;
    const ItemData* GetItem(int id) const;
    const EnemyData* GetEnemy(int id) const;
    const SkillData* GetSkill(int id) const;
    const TroopData* GetTroop(int id) const;
    const StateData* GetState(int id) const;
    const AnimationData* GetAnimation(int id) const;

    void CreateDefaults();

private:
    Database() { CreateDefaults(); }
    std::vector<ActorData> mActors;
    std::vector<ClassData> mClasses;
    std::vector<ItemData> mItems;
    std::vector<WeaponData> mWeapons;
    std::vector<ArmorData> mArmors;
    std::vector<SkillData> mSkills;
    std::vector<EnemyData> mEnemies;
    std::vector<TroopData> mTroops;
    std::vector<StateData> mStates;
    std::vector<TilesetData> mTilesets;
    std::vector<AnimationData> mAnimations;   // XP-Animationen (Paket 5)
    std::vector<MapInfo> mMapInfos;
    SystemData mSystem;
};

} // namespace rpg
