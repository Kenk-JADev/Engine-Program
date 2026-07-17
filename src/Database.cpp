#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/JsonUtils.h"
#include <fstream>
#include <filesystem>
#include <sstream>

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
    mTroops.clear();
    mStates.clear();
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

    // Troops (Gegner-Gruppen)
    if (mTroops.empty()) {
        TroopData t1; t1.id=1; t1.name="Slime x1"; t1.members={1};
        mTroops.push_back(t1);
        TroopData t2; t2.id=2; t2.name="Slime + Bat"; t2.members={1,2};
        mTroops.push_back(t2);
        TroopData t3; t3.id=3; t3.name="Bat x2"; t3.members={2,2};
        mTroops.push_back(t3);
    }

    // States
    if (mStates.empty()) {
        StateData poison; poison.id=1; poison.name="Poison"; poison.hpDrainRate=0.05f; poison.removeAtBattleEnd=true;
        mStates.push_back(poison);
        StateData sleep; sleep.id=2; sleep.name="Sleep"; sleep.restriction=4; sleep.removeAtBattleEnd=true;
        mStates.push_back(sleep);
        StateData guard; guard.id=3; guard.name="Guard"; guard.restriction=0; guard.holdTurn=1;
        mStates.push_back(guard);
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
        map1.encounterList[0] = 1; // troop 1
        map1.encounterList[1] = 2;
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

namespace {
std::string ReadFileToString(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

rpg::ActorData ParseActorObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::ActorData a;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) a.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) a.name = name;
    std::string className;
    if (TryParseString(obj, "class", 0, className)) a.className = className;
    if (TryParseString(obj, "className", 0, className)) a.className = className;
    int level = 0;
    if (TryParseInt(obj, "level", 0, level)) a.initialLevel = level;
    if (TryParseInt(obj, "initialLevel", 0, level)) a.initialLevel = level;

    int v = 0;
    if (TryParseInt(obj, "mhp", 0, v)) a.initialStats.mhp = v;
    if (TryParseInt(obj, "hp", 0, v)) a.initialStats.mhp = v;
    if (TryParseInt(obj, "maxHp", 0, v)) a.initialStats.mhp = v;
    if (TryParseInt(obj, "mmp", 0, v)) a.initialStats.mmp = v;
    if (TryParseInt(obj, "mp", 0, v)) a.initialStats.mmp = v;
    if (TryParseInt(obj, "atk", 0, v)) a.initialStats.atk = v;
    if (TryParseInt(obj, "def", 0, v)) a.initialStats.def = v;
    if (TryParseInt(obj, "mat", 0, v)) a.initialStats.mat = v;
    if (TryParseInt(obj, "mdf", 0, v)) a.initialStats.mdf = v;
    if (TryParseInt(obj, "agi", 0, v)) a.initialStats.agi = v;
    if (TryParseInt(obj, "luk", 0, v)) a.initialStats.luk = v;

    if (TryParseString(obj, "characterName", 0, name)) a.characterName = name;
    if (TryParseString(obj, "faceName", 0, name)) a.faceName = name;
    if (TryParseString(obj, "battlerName", 0, name)) a.battlerName = name;
    if (TryParseString(obj, "nickname", 0, name)) a.nickname = name;

    int idx = 0;
    if (TryParseInt(obj, "characterIndex", 0, idx)) a.characterIndex = idx;
    if (TryParseInt(obj, "faceIndex", 0, idx)) a.faceIndex = idx;

    return a;
}

rpg::ItemData ParseItemObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::ItemData it;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) it.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) it.name = name;
    if (TryParseString(obj, "description", 0, name)) it.description = name;
    int price = 0;
    if (TryParseInt(obj, "price", 0, price)) it.price = price;
    int rec = 0;
    if (TryParseInt(obj, "hpRecovery", 0, rec)) it.hpRecovery = rec;
    if (TryParseInt(obj, "mpRecovery", 0, rec)) it.mpRecovery = rec;
    if (TryParseInt(obj, "iconIndex", 0, rec)) it.iconIndex = rec;
    bool cons = true;
    if (TryParseBool(obj, "consumable", 0, cons)) it.consumable = cons;
    if (TryParseString(obj, "iconName", 0, name)) it.iconName = name;

    std::string typeStr;
    if (TryParseString(obj, "type", 0, typeStr)) {
        if (typeStr == "KeyItem") it.itemType = rpg::ItemData::Type::KeyItem;
        else if (typeStr == "HiddenA") it.itemType = rpg::ItemData::Type::HiddenA;
        else if (typeStr == "HiddenB") it.itemType = rpg::ItemData::Type::HiddenB;
        else it.itemType = rpg::ItemData::Type::Regular;
    }
    return it;
}

rpg::EnemyData ParseEnemyObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::EnemyData e;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) e.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) e.name = name;
    if (TryParseString(obj, "battlerName", 0, name)) e.battlerName = name;
    int v = 0;
    if (TryParseInt(obj, "hp", 0, v)) e.maxHp = v;
    if (TryParseInt(obj, "maxHp", 0, v)) e.maxHp = v;
    if (TryParseInt(obj, "maxMp", 0, v)) e.maxMp = v;
    if (TryParseInt(obj, "atk", 0, v)) e.atk = v;
    if (TryParseInt(obj, "def", 0, v)) e.def = v;
    if (TryParseInt(obj, "mat", 0, v)) e.mat = v;
    if (TryParseInt(obj, "mdf", 0, v)) e.mdf = v;
    if (TryParseInt(obj, "agi", 0, v)) e.agi = v;
    if (TryParseInt(obj, "luk", 0, v)) e.luk = v;
    if (TryParseInt(obj, "exp", 0, v)) e.exp = v;
    if (TryParseInt(obj, "gold", 0, v)) e.gold = v;
    if (TryParseInt(obj, "battlerHue", 0, v)) e.battlerHue = v;
    return e;
}

rpg::WeaponData ParseWeaponObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::WeaponData w;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) w.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) w.name = name;
    if (TryParseString(obj, "description", 0, name)) w.description = name;
    int v = 0;
    if (TryParseInt(obj, "price", 0, v)) w.price = v;
    if (TryParseInt(obj, "atk", 0, v)) w.atk = v;
    if (TryParseInt(obj, "animationId", 0, v)) w.animationId = v;
    if (TryParseInt(obj, "iconIndex", 0, v)) w.iconIndex = v;
    return w;
}

rpg::ArmorData ParseArmorObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::ArmorData a;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) a.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) a.name = name;
    if (TryParseString(obj, "description", 0, name)) a.description = name;
    int v = 0;
    if (TryParseInt(obj, "price", 0, v)) a.price = v;
    if (TryParseInt(obj, "def", 0, v)) a.def = v;
    if (TryParseInt(obj, "mdf", 0, v)) a.mdf = v;
    if (TryParseInt(obj, "iconIndex", 0, v)) a.iconIndex = v;
    return a;
}

rpg::SkillData ParseSkillObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::SkillData s;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) s.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) s.name = name;
    if (TryParseString(obj, "description", 0, name)) s.description = name;
    if (TryParseString(obj, "animation", 0, name)) s.animation = name;
    int v = 0;
    if (TryParseInt(obj, "mpCost", 0, v)) s.mpCost = v;
    if (TryParseInt(obj, "power", 0, v)) s.power = v;
    if (TryParseInt(obj, "iconIndex", 0, v)) s.iconIndex = v;
    if (TryParseInt(obj, "scope", 0, v)) s.scope = v;
    return s;
}

rpg::TilesetData ParseTilesetObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::TilesetData t;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) t.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) t.name = name;
    if (TryParseString(obj, "tilesetName", 0, name)) t.tilesetName = name;
    else if (TryParseString(obj, "image", 0, name)) t.tilesetName = name;
    // flags array
    std::string flagsArr;
    if (FindArrayForKey(obj, "flags", 0, flagsArr)) {
        // parse ints inside
        size_t pos = 0;
        // Remove brackets
        size_t start = flagsArr.find('[');
        size_t end = flagsArr.rfind(']');
        if (start != std::string::npos && end != std::string::npos) {
            std::string inner = flagsArr.substr(start+1, end-start-1);
            std::stringstream ss(inner);
            std::string token;
            while (std::getline(ss, token, ',')) {
                try {
                    // trim
                    size_t s = 0;
                    while (s < token.size() && std::isspace((unsigned char)token[s])) ++s;
                    size_t e = token.size();
                    while (e > s && std::isspace((unsigned char)token[e-1])) --e;
                    if (s < e) t.flags.push_back(std::stoi(token.substr(s, e-s)));
                } catch (...) {}
            }
        }
    }
    return t;
}

rpg::MapInfo ParseMapInfoObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::MapInfo m;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) m.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) m.name = name;
    int v = 0;
    if (TryParseInt(obj, "width", 0, v)) m.width = v;
    if (TryParseInt(obj, "height", 0, v)) m.height = v;
    if (TryParseInt(obj, "tilesetId", 0, v)) m.tilesetId = v;
    if (TryParseInt(obj, "scrollType", 0, v)) m.scrollType = v;
    if (TryParseInt(obj, "encounterStep", 0, v)) m.encounterStep = v;
    bool b = false;
    if (TryParseBool(obj, "bgmAutoPlay", 0, b)) m.bgmAutoPlay = b;
    if (TryParseBool(obj, "bgsAutoPlay", 0, b)) m.bgsAutoPlay = b;
    if (TryParseString(obj, "bgmName", 0, name)) m.bgmName = name;
    if (TryParseString(obj, "bgsName", 0, name)) m.bgsName = name;
    return m;
}

template<typename T, typename ParseFunc>
bool LoadArrayFile(const std::string& filepath, std::vector<T>& out, ParseFunc parser, const char* typeName) {
    if (!std::filesystem::exists(filepath)) {
        return false;
    }
    std::string content = ReadFileToString(filepath);
    if (content.empty()) {
        rpg::Logger::Get().Warning(std::string(typeName) + " file empty: " + filepath);
        return false;
    }
    auto objs = rpg::JsonUtils::ExtractObjectsFromArray(content);
    // Also handle if file itself is an array via FindArray? ExtractObjectsFromArray expects array string
    // If content is array, it works. If content is not array but single object list? Try fallback
    if (objs.empty()) {
        // Try to see if content is array with brackets
        std::string arr;
        size_t pos = content.find('[');
        if (pos != std::string::npos) {
            size_t end;
            std::string fullArr;
            if (rpg::JsonUtils::ExtractArray(content, pos, fullArr, end)) {
                objs = rpg::JsonUtils::ExtractObjectsFromArray(fullArr);
            }
        }
    }
    if (objs.empty()) {
        // File might be empty array []
        return true;
    }
    out.clear();
    for (auto& o : objs) {
        try {
            T data = parser(o);
            if (data.id != 0 || !objs.empty()) {
                out.push_back(std::move(data));
            }
        } catch (...) {}
    }
    rpg::Logger::Get().Info(std::string("Loaded ") + std::to_string(out.size()) + " " + typeName + " from " + filepath);
    return true;
}

} // anonymous namespace

bool Database::Load(const std::string& projectPath) {
    std::string dbDir = projectPath + "/database";
    if (!std::filesystem::exists(dbDir)) {
        RPG_LOG_INFO("Database folder not found, using defaults: " + dbDir);
        return false;
    }

    bool anyLoaded = false;

    // Actors
    {
        std::vector<ActorData> tmp;
        if (LoadArrayFile(dbDir + "/Actors.json", tmp, ParseActorObject, "Actors")) {
            if (!tmp.empty()) { mActors = std::move(tmp); anyLoaded = true; }
        }
    }
    // Items
    {
        std::vector<ItemData> tmp;
        if (LoadArrayFile(dbDir + "/Items.json", tmp, ParseItemObject, "Items")) {
            if (!tmp.empty()) { mItems = std::move(tmp); anyLoaded = true; }
        }
    }
    // Enemies
    {
        std::vector<EnemyData> tmp;
        if (LoadArrayFile(dbDir + "/Enemies.json", tmp, ParseEnemyObject, "Enemies")) {
            if (!tmp.empty()) { mEnemies = std::move(tmp); anyLoaded = true; }
        }
    }
    // Weapons
    {
        std::vector<WeaponData> tmp;
        if (LoadArrayFile(dbDir + "/Weapons.json", tmp, ParseWeaponObject, "Weapons")) {
            if (!tmp.empty()) { mWeapons = std::move(tmp); anyLoaded = true; }
        }
    }
    // Armors
    {
        std::vector<ArmorData> tmp;
        if (LoadArrayFile(dbDir + "/Armors.json", tmp, ParseArmorObject, "Armors")) {
            if (!tmp.empty()) { mArmors = std::move(tmp); anyLoaded = true; }
        }
    }
    // Skills
    {
        std::vector<SkillData> tmp;
        if (LoadArrayFile(dbDir + "/Skills.json", tmp, ParseSkillObject, "Skills")) {
            if (!tmp.empty()) { mSkills = std::move(tmp); anyLoaded = true; }
        }
    }
    // Tilesets
    {
        std::vector<TilesetData> tmp;
        if (LoadArrayFile(dbDir + "/Tilesets.json", tmp, ParseTilesetObject, "Tilesets")) {
            if (!tmp.empty()) { mTilesets = std::move(tmp); anyLoaded = true; }
        }
    }
    // MapInfos
    {
        std::vector<MapInfo> tmp;
        if (LoadArrayFile(dbDir + "/MapInfos.json", tmp, ParseMapInfoObject, "MapInfos")) {
            if (!tmp.empty()) { mMapInfos = std::move(tmp); anyLoaded = true; }
        }
        // Also try MapInfos.json inside maps? Some projects have MapInfos in database as mapinfos
        if (tmp.empty()) {
            LoadArrayFile(dbDir + "/MapInfos.json", tmp, ParseMapInfoObject, "MapInfos");
        }
    }
    // System.json
    {
        std::string sysPath = dbDir + "/System.json";
        if (std::filesystem::exists(sysPath)) {
            std::string content = ReadFileToString(sysPath);
            using namespace JsonUtils;
            std::string name;
            if (TryParseString(content, "gameTitle", 0, name)) mSystem.gameTitle = name;
            if (TryParseString(content, "currencyUnit", 0, name)) mSystem.currencyUnit = name;
            int v = 0;
            if (TryParseInt(content, "startMapId", 0, v)) mSystem.startMapId = v;
            if (TryParseInt(content, "startX", 0, v)) mSystem.startX = v;
            if (TryParseInt(content, "startY", 0, v)) mSystem.startY = v;
            // switches / variables arrays
            std::string arr;
            if (FindArrayForKey(content, "switches", 0, arr)) {
                // extract strings from array - simple split by quote
                mSystem.switches.clear();
                size_t pos = 0;
                while (true) {
                    size_t q1 = arr.find('\"', pos);
                    if (q1 == std::string::npos) break;
                    size_t q2 = arr.find('\"', q1+1);
                    if (q2 == std::string::npos) break;
                    std::string s = arr.substr(q1+1, q2-q1-1);
                    mSystem.switches.push_back(JsonUtils::Unescape(s));
                    pos = q2+1;
                }
            }
            if (FindArrayForKey(content, "variables", 0, arr)) {
                mSystem.variables.clear();
                size_t pos = 0;
                while (true) {
                    size_t q1 = arr.find('\"', pos);
                    if (q1 == std::string::npos) break;
                    size_t q2 = arr.find('\"', q1+1);
                    if (q2 == std::string::npos) break;
                    std::string s = arr.substr(q1+1, q2-q1-1);
                    mSystem.variables.push_back(JsonUtils::Unescape(s));
                    pos = q2+1;
                }
            }
            anyLoaded = true;
            RPG_LOG_INFO("Loaded System.json from " + sysPath);
        }
    }

    if (anyLoaded) {
        RPG_LOG_INFO("Database loaded from " + dbDir);
        return true;
    } else {
        RPG_LOG_INFO("Database folder exists but no valid files, keeping defaults: " + dbDir);
        return false;
    }
}

bool Database::Save(const std::string& projectPath) const {
    try {
        std::string dbDir = projectPath + "/database";
        std::filesystem::create_directories(dbDir);

        using namespace JsonUtils;

        // Actors.json - full format
        {
            std::ofstream f(dbDir + "/Actors.json");
            f << "[\n";
            for (size_t i=0;i<mActors.size();++i) {
                const auto& a = mActors[i];
                f << "  {\"id\":" << a.id
                  << ",\"name\":\"" << Escape(a.name) << "\""
                  << ",\"className\":\"" << Escape(a.className) << "\""
                  << ",\"nickname\":\"" << Escape(a.nickname) << "\""
                  << ",\"initialLevel\":" << a.initialLevel
                  << ",\"maxLevel\":" << a.maxLevel
                  << ",\"mhp\":" << a.initialStats.mhp
                  << ",\"mmp\":" << a.initialStats.mmp
                  << ",\"atk\":" << a.initialStats.atk
                  << ",\"def\":" << a.initialStats.def
                  << ",\"mat\":" << a.initialStats.mat
                  << ",\"mdf\":" << a.initialStats.mdf
                  << ",\"agi\":" << a.initialStats.agi
                  << ",\"luk\":" << a.initialStats.luk
                  << ",\"characterName\":\"" << Escape(a.characterName) << "\""
                  << ",\"characterIndex\":" << a.characterIndex
                  << ",\"faceName\":\"" << Escape(a.faceName) << "\""
                  << ",\"faceIndex\":" << a.faceIndex
                  << ",\"battlerName\":\"" << Escape(a.battlerName) << "\""
                  << "}";
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
                std::string typeStr = "Regular";
                if (it.itemType == ItemData::Type::KeyItem) typeStr = "KeyItem";
                else if (it.itemType == ItemData::Type::HiddenA) typeStr = "HiddenA";
                else if (it.itemType == ItemData::Type::HiddenB) typeStr = "HiddenB";
                f << "  {\"id\":" << it.id
                  << ",\"name\":\"" << Escape(it.name) << "\""
                  << ",\"description\":\"" << Escape(it.description) << "\""
                  << ",\"type\":\"" << typeStr << "\""
                  << ",\"price\":" << it.price
                  << ",\"consumable\":" << (it.consumable?"true":"false")
                  << ",\"hpRecovery\":" << it.hpRecovery
                  << ",\"mpRecovery\":" << it.mpRecovery
                  << ",\"iconName\":\"" << Escape(it.iconName) << "\""
                  << ",\"iconIndex\":" << it.iconIndex
                  << "}";
                if (i+1<mItems.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // Enemies.json
        {
            std::ofstream f(dbDir + "/Enemies.json");
            f << "[\n";
            for (size_t i=0;i<mEnemies.size();++i) {
                const auto& e = mEnemies[i];
                f << "  {\"id\":" << e.id
                  << ",\"name\":\"" << Escape(e.name) << "\""
                  << ",\"battlerName\":\"" << Escape(e.battlerName) << "\""
                  << ",\"maxHp\":" << e.maxHp
                  << ",\"maxMp\":" << e.maxMp
                  << ",\"atk\":" << e.atk
                  << ",\"def\":" << e.def
                  << ",\"mat\":" << e.mat
                  << ",\"mdf\":" << e.mdf
                  << ",\"agi\":" << e.agi
                  << ",\"luk\":" << e.luk
                  << ",\"exp\":" << e.exp
                  << ",\"gold\":" << e.gold
                  << "}";
                if (i+1<mEnemies.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // Troops.json
        {
            std::ofstream f(dbDir + "/Troops.json");
            f << "[\n";
            for (size_t i=0;i<mTroops.size();++i) {
                const auto& tr = mTroops[i];
                f << "  {\"id\":" << tr.id
                  << ",\"name\":\"" << Escape(tr.name) << "\""
                  << ",\"members\":[";
                for (size_t j=0;j<tr.members.size();++j) {
                    if (j) f << ",";
                    f << tr.members[j];
                }
                f << "]}";
                if (i+1<mTroops.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // States.json
        {
            std::ofstream f(dbDir + "/States.json");
            f << "[\n";
            for (size_t i=0;i<mStates.size();++i) {
                const auto& s = mStates[i];
                f << "  {\"id\":" << s.id
                  << ",\"name\":\"" << Escape(s.name) << "\""
                  << ",\"restriction\":" << s.restriction
                  << ",\"priority\":" << s.priority
                  << ",\"hpDrainRate\":" << s.hpDrainRate
                  << "}";
                if (i+1<mStates.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }

        // Weapons.json
        {
            std::ofstream f(dbDir + "/Weapons.json");
            f << "[\n";
            for (size_t i=0;i<mWeapons.size();++i) {
                const auto& w = mWeapons[i];
                f << "  {\"id\":" << w.id
                  << ",\"name\":\"" << Escape(w.name) << "\""
                  << ",\"description\":\"" << Escape(w.description) << "\""
                  << ",\"price\":" << w.price
                  << ",\"atk\":" << w.atk
                  << ",\"iconIndex\":" << w.iconIndex
                  << "}";
                if (i+1<mWeapons.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // Armors.json
        {
            std::ofstream f(dbDir + "/Armors.json");
            f << "[\n";
            for (size_t i=0;i<mArmors.size();++i) {
                const auto& a = mArmors[i];
                f << "  {\"id\":" << a.id
                  << ",\"name\":\"" << Escape(a.name) << "\""
                  << ",\"description\":\"" << Escape(a.description) << "\""
                  << ",\"price\":" << a.price
                  << ",\"def\":" << a.def
                  << ",\"mdf\":" << a.mdf
                  << ",\"iconIndex\":" << a.iconIndex
                  << "}";
                if (i+1<mArmors.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // Skills.json
        {
            std::ofstream f(dbDir + "/Skills.json");
            f << "[\n";
            for (size_t i=0;i<mSkills.size();++i) {
                const auto& s = mSkills[i];
                f << "  {\"id\":" << s.id
                  << ",\"name\":\"" << Escape(s.name) << "\""
                  << ",\"description\":\"" << Escape(s.description) << "\""
                  << ",\"mpCost\":" << s.mpCost
                  << ",\"power\":" << s.power
                  << ",\"iconIndex\":" << s.iconIndex
                  << ",\"scope\":" << s.scope
                  << ",\"animation\":\"" << Escape(s.animation) << "\""
                  << "}";
                if (i+1<mSkills.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // Tilesets.json
        {
            std::ofstream f(dbDir + "/Tilesets.json");
            f << "[\n";
            for (size_t i=0;i<mTilesets.size();++i) {
                const auto& t = mTilesets[i];
                f << "  {\"id\":" << t.id
                  << ",\"name\":\"" << Escape(t.name) << "\""
                  << ",\"tilesetName\":\"" << Escape(t.tilesetName) << "\""
                  << ",\"flags\":[";
                for (size_t j=0;j<t.flags.size();++j) {
                    if (j) f << ",";
                    f << t.flags[j];
                }
                f << "]}";
                if (i+1<mTilesets.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // MapInfos.json
        {
            std::ofstream f(dbDir + "/MapInfos.json");
            f << "[\n";
            for (size_t i=0;i<mMapInfos.size();++i) {
                const auto& m = mMapInfos[i];
                f << "  {\"id\":" << m.id
                  << ",\"name\":\"" << Escape(m.name) << "\""
                  << ",\"width\":" << m.width
                  << ",\"height\":" << m.height
                  << ",\"tilesetId\":" << m.tilesetId
                  << ",\"bgmName\":\"" << Escape(m.bgmName) << "\""
                  << "}";
                if (i+1<mMapInfos.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // System.json
        {
            std::ofstream f(dbDir + "/System.json");
            f << "{\n";
            f << "  \"gameTitle\":\"" << Escape(mSystem.gameTitle) << "\",\n";
            f << "  \"currencyUnit\":\"" << Escape(mSystem.currencyUnit) << "\",\n";
            f << "  \"startMapId\":" << mSystem.startMapId << ",\n";
            f << "  \"startX\":" << mSystem.startX << ",\n";
            f << "  \"startY\":" << mSystem.startY << ",\n";
            f << "  \"switches\":[";
            for (size_t i=0;i<mSystem.switches.size();++i) {
                if (i) f << ",";
                f << "\"" << Escape(mSystem.switches[i]) << "\"";
            }
            f << "],\n";
            f << "  \"variables\":[";
            for (size_t i=0;i<mSystem.variables.size();++i) {
                if (i) f << ",";
                f << "\"" << Escape(mSystem.variables[i]) << "\"";
            }
            f << "]\n";
            f << "}\n";
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

const SkillData* Database::GetSkill(int id) const {
    for (const auto& e : mSkills) if (e.id==id) return &e;
    return nullptr;
}

const TroopData* Database::GetTroop(int id) const {
    for (const auto& e : mTroops) if (e.id==id) return &e;
    return nullptr;
}

const StateData* Database::GetState(int id) const {
    for (const auto& e : mStates) if (e.id==id) return &e;
    return nullptr;
}

} // namespace rpg
