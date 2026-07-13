#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Logger.h"
#include <fstream>
#include <filesystem>

namespace rpg {

Database& Database::Get() {
    static Database instance;
    return instance;
}

void Database::Clear() {
    mActors.clear();
    mClasses.clear();
    mItems.clear();
    mWeapons.clear();
    mArmors.clear();
    mSkills.clear();
    mEnemies.clear();
    mTilesets.clear();
    mMapInfos.clear();
    mSystem = SystemData();
    CreateDefaults();
}

void Database::CreateDefaults() {
    // Actors
    if (mActors.empty()) {
        ActorData hero;
        hero.id = 1;
        hero.name = "Hero";
        hero.className = "Warrior";
        hero.initialStats = {100, 30, 15, 10, 5, 5, 12, 8};
        mActors.push_back(hero);

        ActorData mage;
        mage.id = 2;
        mage.name = "Mage";
        mage.className = "Mage";
        mage.initialStats = {70, 60, 8, 6, 18, 15, 10, 7};
        mActors.push_back(mage);
    }

    // Classes
    if (mClasses.empty()) {
        ClassData warrior; warrior.id=1; warrior.name="Warrior";
        mClasses.push_back(warrior);
        ClassData mage; mage.id=2; mage.name="Mage";
        mClasses.push_back(mage);
    }

    // Items
    if (mItems.empty()) {
        ItemData potion; potion.id=1; potion.name="Potion"; potion.price=50; potion.hpRecovery=100;
        mItems.push_back(potion);
        ItemData ether; ether.id=2; ether.name="Ether"; ether.price=100; ether.mpRecovery=50; ether.hpRecovery=0;
        mItems.push_back(ether);
    }

    // Weapons
    if (mWeapons.empty()) {
        WeaponData sword; sword.id=1; sword.name="Iron Sword"; sword.atk=10; sword.price=200;
        mWeapons.push_back(sword);
    }

    // Armors
    if (mArmors.empty()) {
        ArmorData shield; shield.id=1; shield.name="Wooden Shield"; shield.def=5; shield.price=100;
        mArmors.push_back(shield);
    }

    // Skills
    if (mSkills.empty()) {
        SkillData fire; fire.id=1; fire.name="Fire"; fire.mpCost=5; fire.power=50;
        mSkills.push_back(fire);
        SkillData heal; heal.id=2; heal.name="Heal"; heal.mpCost=8; heal.power=-30;
        mSkills.push_back(heal);
    }

    // Enemies
    if (mEnemies.empty()) {
        EnemyData slime; slime.id=1; slime.name="Slime"; slime.maxHp=40; slime.atk=10; slime.def=3; slime.exp=8; slime.gold=5;
        mEnemies.push_back(slime);
        EnemyData bat; bat.id=2; bat.name="Bat"; bat.maxHp=60; bat.atk=15; bat.agi=14; bat.exp=15; bat.gold=10;
        mEnemies.push_back(bat);
    }

    // Tilesets
    if (mTilesets.empty()) {
        TilesetData td; td.id=1; td.name="World"; td.tilesetName="tileset_demo.png";
        mTilesets.push_back(td);
    }

    // MapInfos - RPG Maker Standard Maps
    if (mMapInfos.empty()) {
        MapInfo map1;
        map1.id = 1;
        map1.name = "Karte 001";
        map1.width = 20;
        map1.height = 15;
        map1.tilesetId = 1;
        map1.bgmAutoPlay = true;
        map1.bgsAutoPlay = true;
        map1.scrollType = 0;
        map1.encounterStep = 30;
        map1.backgroundColor = Color(0, 0, 0, 1);
        map1.fogColor = Color(0.5f, 0.5f, 0.5f, 1.0f);
        mMapInfos.push_back(map1);

        MapInfo map2;
        map2.id = 2;
        map2.name = "Karte 002";
        map2.width = 20;
        map2.height = 15;
        map2.tilesetId = 1;
        map2.bgmAutoPlay = true;
        map2.bgsAutoPlay = true;
        map2.scrollType = 0;
        map2.encounterStep = 30;
        map2.backgroundColor = Color(0, 0, 0, 1);
        map2.fogColor = Color(0.5f, 0.5f, 0.5f, 1.0f);
        mMapInfos.push_back(map2);
    }

    // System defaults
    mSystem.switches.resize(100);
    for (int i=0;i<100;++i) mSystem.switches[i]="Switch "+std::to_string(i+1);
    mSystem.variables.resize(100);
    for (int i=0;i<100;++i) mSystem.variables[i]="Variable "+std::to_string(i+1);
}

bool Database::Load(const std::string& projectPath) {
    // Simplified: JSON is expected at projectPath/database/*.json
    // For now just check existence and keep defaults if not found
    std::string dbDir = projectPath + "/database";
    if (!std::filesystem::exists(dbDir)) {
        RPG_LOG_INFO("Database folder not found, using defaults: " + dbDir);
        return false;
    }
    // TODO: proper JSON loading
    RPG_LOG_INFO("Database loaded (stub) from " + projectPath);
    return true;
}

bool Database::Save(const std::string& projectPath) const {
    try {
        std::string dbDir = projectPath + "/database";
        std::filesystem::create_directories(dbDir);

        // Actors.json
        {
            std::ofstream f(dbDir + "/Actors.json");
            f << "[\n";
            for (size_t i=0;i<mActors.size();++i) {
                const auto& a = mActors[i];
                f << "  {\"id\":" << a.id << ",\"name\":\"" << a.name << "\",\"class\":\"" << a.className << "\"}";
                if (i+1<mActors.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // Items.json
        {
            std::ofstream f(dbDir + "/Items.json");
            f << "[\n";
            for (size_t i=0;i<mItems.size();++i) {
                const auto& it = mItems[i];
                f << "  {\"id\":" << it.id << ",\"name\":\"" << it.name << "\"}";
                if (i+1<mItems.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }

        RPG_LOG_INFO("Database saved to " + dbDir);
        return true;
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("Database save failed: ") + e.what());
        return false;
    }
}

const ActorData* Database::GetActor(int id) const {
    for (const auto& a : mActors) if (a.id==id) return &a;
    return nullptr;
}
const ItemData* Database::GetItem(int id) const {
    for (const auto& it : mItems) if (it.id==id) return &it;
    return nullptr;
}
const EnemyData* Database::GetEnemy(int id) const {
    for (const auto& e : mEnemies) if (e.id==id) return &e;
    return nullptr;
}

} // namespace rpg
