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
    std::vector<int> equips; // item ids
};

struct ClassData {
    int id = 0;
    std::string name = "Warrior";
    // Exp curve params
    int expBase = 30;
    int expExtra = 20;
    float expAccA = 30;
    float expAccB = 20;
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
};

struct WeaponData {
    int id = 0;
    std::string name = "Sword";
    std::string description;
    int iconIndex = 0;
    int price = 100;
    int atk = 10;
    int animationId = 0;
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
};

struct TilesetData {
    int id = 0;
    std::string name = "World";
    std::string tilesetName = "tileset_demo.png";
    std::vector<int> flags; // passability flags per tile id
};

struct MapInfo {
    int id = 0;
    std::string name = "Map";
    bool expanded = false;
    int parentId = 0;
    int order = 0;
    int scrollX = 0;
    int scrollY = 0;
};

struct SystemData {
    std::string gameTitle = "RPG Maker 3D Game";
    std::string currencyUnit = "G";
    int startMapId = 1;
    int startX = 0;
    int startY = 0;
    std::vector<std::string> switches;
    std::vector<std::string> variables;
    std::string battleBgm = "battle.ogg";
    std::string titleBgm = "title.ogg";
    std::string gameoverMe = "gameover.ogg";
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
    std::vector<TilesetData>& Tilesets() { return mTilesets; }
    std::vector<MapInfo>& MapInfos() { return mMapInfos; }
    SystemData& System() { return mSystem; }

    const ActorData* GetActor(int id) const;
    const ItemData* GetItem(int id) const;
    const EnemyData* GetEnemy(int id) const;

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
    std::vector<TilesetData> mTilesets;
    std::vector<MapInfo> mMapInfos;
    SystemData mSystem;
};

} // namespace rpg
