#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/JsonUtils.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm> // std::clamp (MSVC: nicht transitiv vorhanden)

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
        hero.finalStats   = {1500, 480, 120, 110, 40, 40, 120, 60};
        hero.equips       = {1, 1}; // Iron Sword + Wooden Shield
        mActors.push_back(hero);

        ActorData mage;
        mage.id = 2;
        mage.name = "Mage";
        mage.className = "Mage";
        mage.initialStats = {70, 60, 8, 6, 18, 15, 10, 7};
        mage.finalStats   = {1050, 950, 60, 65, 170, 160, 95, 50};
        mActors.push_back(mage);
    }

    // Classes
    if (mClasses.empty()) {
        ClassData warrior; warrior.id=1; warrior.name="Warrior";
        // Erste Hilfe ab 1 (PAKET 21), Heal ab 2, Giftstich ab 3 (PAKET 17)
        warrior.learnings = {{1, 4}, {2, 2}, {3, 3}};
        mClasses.push_back(warrior);
        ClassData mage; mage.id=2; mage.name="Mage";
        // Fire ab 1, Heal ab 4, Auferstehung ab 5 (PAKET 22)
        mage.learnings = {{1, 1}, {4, 2}, {5, 5}};
        // PAKET 23: XP-Ausruestungs-Set — Magier duerfen nur Staebe (id 3)
        mage.weaponSet = {3};
        mClasses.push_back(mage);
    }

    // Items
    if (mItems.empty()) {
        ItemData potion; potion.id=1; potion.name="Potion"; potion.price=50; potion.hpRecovery=100;
        mItems.push_back(potion);
        ItemData ether; ether.id=2; ether.name="Ether"; ether.price=100; ether.mpRecovery=50; ether.hpRecovery=0;
        mItems.push_back(ether);
        // PAKET 20: Demo-Item mit Zustands-Heilung (XP minus_state_set)
        ItemData anti; anti.id=3; anti.name="Gegengift"; anti.price=30;
        anti.hpRecovery=0; anti.minusStates={1}; // heilt „Poison"
        mItems.push_back(anti);
        // PAKET 22: Wiederbelebungs-Item (XP scope 5 = ein Verbuendeter
        // (tot); die HP-Genesung gilt dort als PROZENT der max. HP).
        ItemData fenix; fenix.id=4; fenix.name="Phönixfeder"; fenix.price=500;
        fenix.hpRecovery=100; fenix.scope=rpg::ItemData::Scope::OneAllyDead;
        mItems.push_back(fenix);
    }

    // Weapons
    if (mWeapons.empty()) {
        WeaponData sword; sword.id=1; sword.name="Iron Sword"; sword.atk=10; sword.price=200;
        mWeapons.push_back(sword);
        // PAKET 22: Demo-Waffe mit XP-Zustands-Effekt (vergiftet beim Treffer)
        WeaponData fang; fang.id=2; fang.name="Giftklinge"; fang.atk=6; fang.price=800;
        fang.plusStates={1};
        mWeapons.push_back(fang);
        // PAKET 23: Magier-Waffe (Demo fuer Klassen-Ausruestungs-Sets:
        // die Mage-Klasse darf NUR Staebe anlegen)
        WeaponData staff; staff.id=3; staff.name="Holzstab"; staff.atk=3; staff.price=80;
        staff.agiPlus=2;
        mWeapons.push_back(staff);
    }

    // Armors
    if (mArmors.empty()) {
        ArmorData shield; shield.id=1; shield.name="Wooden Shield"; shield.def=5; shield.price=100;
        mArmors.push_back(shield);
        // PAKET 23: Demo fuer XP guard_state_set — „verfluchter" Ring,
        // der den Traeger dauerhaft vergiftet (solange angelegt).
        ArmorData ring; ring.id=2; ring.name="Fluchring"; ring.def=1; ring.price=1;
        ring.armorType=rpg::ArmorData::Type::Accessory; ring.guardStates={1};
        mArmors.push_back(ring);
        // PAKET 24: Demo fuer XP guard_element_set — halbiert Feuer-Schaden
        ArmorData cape; cape.id=3; cape.name="Feuerumhang"; cape.def=3; cape.price=250;
        cape.armorType=rpg::ArmorData::Type::Body; cape.guardElements={1};
        mArmors.push_back(cape);
    }

    // Skills
    if (mSkills.empty()) {
        // PAKET 24: Demo-Element — Feuer (Element-ID 1 aus System.elements)
        SkillData fire; fire.id=1; fire.name="Fire"; fire.mpCost=5; fire.power=50;
        fire.elementId=1;
        mSkills.push_back(fire);
        // PAKET 21: Heal hatte scope=1 (Gegner!) — konnte so nie im Menue
        // wirken und „heilte" im Kampf Gegner. XP-konform: scope 3 = ein
        // Verbuendeter, positiver Stärke-Wert (Engine nimmt |power|).
        SkillData heal; heal.id=2; heal.name="Heal"; heal.mpCost=8; heal.scope=3; heal.power=30;
        mSkills.push_back(heal);
        // PAKET 17: Demo-Skill mit XP-Zustands-Effekt (verhaengt „Poison", id 1)
        SkillData toxin; toxin.id=3; toxin.name="Giftstich"; toxin.mpCost=4;
        toxin.power=20; toxin.plusStates={1};
        mSkills.push_back(toxin);
        // PAKET 21: Zustands-Heilskill a la Esuna (heilt „Poison" sicher,
        // kein Staerke-Wert) — im Menue UND Kampf benutzbar.
        SkillData aid; aid.id=4; aid.name="Erste Hilfe"; aid.mpCost=3;
        aid.scope=3; aid.power=0; aid.minusStates={1};
        mSkills.push_back(aid);
        // PAKET 22: Wiederbelebungs-Skill (XP scope 5 = ein Verbuendeter
        // (tot); power gilt dort als PROZENT der max. HP — 100 = volle HP).
        SkillData rev; rev.id=5; rev.name="Auferstehung"; rev.mpCost=20;
        rev.scope=5; rev.power=100;
        mSkills.push_back(rev);
    }

    // Enemies
    if (mEnemies.empty()) {
        EnemyData slime; slime.id=1; slime.name="Slime"; slime.maxHp=40; slime.maxMp=10; slime.atk=10; slime.def=3; slime.exp=8; slime.gold=5;
        // PAKET 18: einfache Verhaltenstabelle (nur Angriff, wie bisher)
        slime.actions.push_back({}); // ein Standardangriff, rating 5
        mEnemies.push_back(slime);
        EnemyData bat; bat.id=2; bat.name="Bat"; bat.maxHp=60; bat.maxMp=10; bat.atk=15; bat.agi=14; bat.exp=15; bat.gold=10;
        // PAKET 24: Demo — Fledermaus ist feueranfaellig (Rang A = 200 %)
        bat.elementRanks={0};
        // PAKET 18: Bat nutzt „Giftstich" (Skill 3), wenn HP <= 80 % —
        // Demo fuer die XP-Aktionstabelle (rating 4 < Angriff 5)
        {
            EnemyData::Action atk; atk.kind=0; atk.basic=0; atk.rating=5;
            bat.actions.push_back(atk);
            EnemyData::Action tox; tox.kind=1; tox.skillId=3; tox.rating=4; tox.hpBelow=80;
            bat.actions.push_back(tox);
            EnemyData::Action def; def.kind=0; def.basic=1; def.rating=3;
            bat.actions.push_back(def);
        }
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
        // PAKET 25: Standard-Passagen fuer das Demo-Raster (8 Spalten x
        // 6 Zeilen, ID = zeile*8 + spalte): Stein (Spalte 2) und Wasser
        // (Spalte 3) blockieren; die unteren drei Gras-Zeilen (Spalte 0)
        // sind Durchwiese (bush -> "im Gras", hoehere Begegnungsrate).
        td.flags.assign(48, 0);
        td.bushFlags.assign(48, 0);
        for (int row = 0; row < 6; ++row) {
            td.flags[row * 8 + 2] = 1; // Stein: blockiert
            td.flags[row * 8 + 3] = 1; // Wasser: blockiert
        }
        td.bushFlags[3 * 8 + 0] = 1;   // hohes Gras (Zeilen 3..5)
        td.bushFlags[4 * 8 + 0] = 1;
        td.bushFlags[5 * 8 + 0] = 1;
        // Terrain-Tag 4 ("hohes Gras", Paket-6-Belegung) ebenfalls setzen,
        // damit Schritt-SE-System + Encounter-Regel konsistent greifen.
        td.terrainTags.assign(48, 0);
        td.terrainTags[3 * 8 + 0] = 4;
        td.terrainTags[4 * 8 + 0] = 4;
        td.terrainTags[5 * 8 + 0] = 4;
        mTilesets.push_back(td);
    }

    // Animationen (XP-Paket 5): ein kleiner Standard-Satz als Startpunkt
    if (mAnimations.empty()) {
        AnimationData a1;
        a1.id = 1; a1.name = "Standard-Treffer"; a1.position = 2;
        AnimFrame f1;
        f1.cells.push_back(AnimCell{0, 0, 0, 100, 0, 255});
        f1.flashScope = 1; f1.flashR = 255; f1.flashG = 255; f1.flashB = 255;
        f1.flashDuration = 3;
        a1.frames.push_back(f1);
        a1.frames.push_back(AnimFrame{}); // leerer Ausklang-Frame
        mAnimations.push_back(a1);
        AnimationData a2;
        a2.id = 2; a2.name = "Aufleveln"; a2.position = 1;
        AnimFrame f2;
        f2.cells.push_back(AnimCell{0, 0, 0, 100, 0, 200});
        mAnimations.push_back(a2);
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
        map2.encounterStep = 25;
        map2.encounterList[0] = 2; // troop 2 (Slime + Bat)
        map2.encounterList[1] = 3; // troop 3 (Bat x2)
        map2.backgroundColor = Color(0, 0, 0, 1);
        map2.fogColor = Color(0.5f, 0.5f, 0.5f, 1.0f);
        mMapInfos.push_back(map2);
    }

    // System defaults
    mSystem.switches.resize(100);
    for (int i=0;i<100;++i) mSystem.switches[i]="Switch "+std::to_string(i+1);
    mSystem.variables.resize(100);
    for (int i=0;i<100;++i) mSystem.variables[i]="Variable "+std::to_string(i+1);
    // XP-Standard-Elemente (wie im XP Datenbank-System-Tab)
    if (mSystem.elements.empty()) {
        mSystem.elements = {"Feuer","Eis","Donner","Wasser","Erde","Wind",
                            "Licht","Dunkelheit","gegen Untote","gegen Schlangen",
                            "gegen Wassertiere","gegen Bestien","gegen Kobolde",
                            "gegen Vögel","gegen Teufel","gegen Engel"};
    }
    if (mSystem.initialParty.empty()) mSystem.initialParty = {1};
    if (mSystem.animations.empty()) {
        mSystem.animations.resize(10);
        for (int i = 0; i < 10; ++i)
            mSystem.animations[i] = "Animation " + std::to_string(i + 1);
    }
}

namespace {
std::string ReadFileToString(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// "1, 2,3" -> {1,2,3} (Leerzeichen/unerlaubte Eintrae robust uebersprungen)
std::vector<int> ParseIntCsv(const std::string& s) {
    std::vector<int> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        const size_t b = tok.find_first_not_of(" \t");
        if (b == std::string::npos) continue;
        const size_t e = tok.find_last_not_of(" \t");
        try {
            const int v = std::stoi(tok.substr(b, e - b + 1));
            if (v > 0) out.push_back(v);
        } catch (...) {}
    }
    return out;
}

// PAKET 17: Vorwaertsdeklarationen — die Helfer sind weiter unten definiert
// (ParseIntArrayInto liest "key":[1,2,...], WriteIntArray schreibt es).
static bool ParseIntArrayInto(const std::string& obj, const char* key,
                              std::vector<int>& out);
static void WriteIntArray(std::ofstream& f, const char* key,
                          const std::vector<int>& v);

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
    if (TryParseInt(obj, "fmhp", 0, v)) a.finalStats.mhp = v;
    if (TryParseInt(obj, "fmmp", 0, v)) a.finalStats.mmp = v;
    if (TryParseInt(obj, "fatk", 0, v)) a.finalStats.atk = v;
    if (TryParseInt(obj, "fdef", 0, v)) a.finalStats.def = v;
    if (TryParseInt(obj, "fmat", 0, v)) a.finalStats.mat = v;
    if (TryParseInt(obj, "fmdf", 0, v)) a.finalStats.mdf = v;
    if (TryParseInt(obj, "fagi", 0, v)) a.finalStats.agi = v;
    if (TryParseInt(obj, "fluk", 0, v)) a.finalStats.luk = v;
    {
        std::string cr;
        const auto norm = [](const std::string& s) -> char {
            const char c = s.empty() ? 'C' : s[0];
            return (c >= 'A' && c <= 'E') ? c : 'C';
        };
        if (TryParseString(obj, "curveHp", 0, cr)) a.curveHp = norm(cr);
        if (TryParseString(obj, "curveMp", 0, cr)) a.curveMp = norm(cr);
        if (TryParseString(obj, "curveAtk", 0, cr)) a.curveAtk = norm(cr);
        if (TryParseString(obj, "curveDef", 0, cr)) a.curveDef = norm(cr);
        if (TryParseString(obj, "curveAgi", 0, cr)) a.curveAgi = norm(cr);
    }

    if (TryParseString(obj, "characterName", 0, name)) a.characterName = name;
    if (TryParseString(obj, "faceName", 0, name)) a.faceName = name;
    if (TryParseString(obj, "battlerName", 0, name)) a.battlerName = name;
    if (TryParseString(obj, "nickname", 0, name)) a.nickname = name;

    int idx = 0;
    if (TryParseInt(obj, "characterIndex", 0, idx)) a.characterIndex = idx;
    if (TryParseInt(obj, "faceIndex", 0, idx)) a.faceIndex = idx;

    // Start-Ausruestung als CSV-String "1,2" (Waffen-/Ruestungs-IDs)
    if (TryParseString(obj, "equips", 0, name))
        a.equips = ParseIntCsv(name);

    // PAKET 17: XP state_ranks [0..5] (Index = Zustands-ID-1, fehlt = C)
    ParseIntArrayInto(obj, "stateRanks", a.stateRanks);
    // PAKET 24: XP element_raten [0..5=A..F] (Index = Element-ID-1, fehlt = C)
    ParseIntArrayInto(obj, "elementRanks", a.elementRanks);

    return a;
}

// "2:2, 4:3" -> [{2,2},{4,3}] (Level:Fertigkeits-ID Paare, XP-Learnings)
std::vector<rpg::ClassData::Learning> ParseLearningsCsv(const std::string& s) {
    std::vector<rpg::ClassData::Learning> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        const auto c = tok.find(':');
        if (c == std::string::npos) continue;
        try {
            const int lv = std::stoi(tok.substr(0, c));
            const int sk = std::stoi(tok.substr(c + 1));
            if (lv >= 1 && sk > 0) out.push_back({lv, sk});
        } catch (...) {}
    }
    return out;
}

rpg::ClassData ParseClassObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::ClassData c;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) c.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) c.name = name;
    int v = 0;
    if (TryParseInt(obj, "expBase", 0, v)) c.expBase = v;
    if (TryParseInt(obj, "expExtra", 0, v)) c.expExtra = v;
    float f = 0.f;
    if (TryParseFloat(obj, "expAccA", 0, f)) c.expAccA = f;
    if (TryParseFloat(obj, "expAccB", 0, f)) c.expAccB = f;
    if (TryParseString(obj, "learnings", 0, name))
        c.learnings = ParseLearningsCsv(name);
    // PAKET 23: XP-Ausruestungs-Sets (leer = alles erlaubt)
    ParseIntArrayInto(obj, "weaponSet", c.weaponSet);
    ParseIntArrayInto(obj, "armorSet", c.armorSet);
    return c;
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
    if (TryParseInt(obj, "animationId", 0, rec)) it.animationId = rec; // PAKET 12
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
    // PAKET 20: scope wurde bisher weder gelesen noch geschrieben (Luecke)
    if (TryParseInt(obj, "scope", 0, rec))
        it.scope = (rpg::ItemData::Scope)std::clamp(rec, 0, 7);
    // PAKET 20: XP plus_state_set / minus_state_set (Zustands-IDs)
    ParseIntArrayInto(obj, "plusStates", it.plusStates);
    ParseIntArrayInto(obj, "minusStates", it.minusStates);
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
    // PAKET 17: XP state_ranks [0..5] (Index = Zustands-ID-1, fehlt = C)
    ParseIntArrayInto(obj, "stateRanks", e.stateRanks);
    // PAKET 24: XP element_raten [0..5=A..F] (Index = Element-ID-1, fehlt = C)
    ParseIntArrayInto(obj, "elementRanks", e.elementRanks);
    // PAKET 18: XP RPG::Enemy.actions (Objekt-Array, optional)
    {
        std::string arr;
        if (FindArrayForKey(obj, "actions", 0, arr)) {
            for (const auto& ao : ExtractObjectsFromArray(arr)) {
                rpg::EnemyData::Action act;
                if (TryParseInt(ao, "kind", 0, v)) act.kind = v;
                if (TryParseInt(ao, "basic", 0, v)) act.basic = v;
                if (TryParseInt(ao, "skillId", 0, v)) act.skillId = v;
                if (TryParseInt(ao, "rating", 0, v)) act.rating = v;
                if (TryParseInt(ao, "turnA", 0, v)) act.turnA = v;
                if (TryParseInt(ao, "turnB", 0, v)) act.turnB = v;
                if (TryParseInt(ao, "hpBelow", 0, v)) act.hpBelow = v;
                if (TryParseInt(ao, "level", 0, v)) act.level = v;
                if (TryParseInt(ao, "switchId", 0, v)) act.switchId = v;
                e.actions.push_back(act);
            }
        }
    }
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
    // PAKET 23: XP pdef_plus u. a. Parameter-Boni der Waffe
    if (TryParseInt(obj, "defPlus", 0, v)) w.defPlus = v;
    if (TryParseInt(obj, "agiPlus", 0, v)) w.agiPlus = v;
    // PAKET 22: XP plus_state_set / minus_state_set (Zustands-IDs)
    ParseIntArrayInto(obj, "plusStates", w.plusStates);
    ParseIntArrayInto(obj, "minusStates", w.minusStates);
    // PAKET 24: XP element_set (Element-IDs des Waffenangriffs)
    ParseIntArrayInto(obj, "elementSet", w.elementSet);
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
    if (TryParseInt(obj, "agiPlus", 0, v)) a.agiPlus = v; // PAKET 23
    // PAKET 23 (Mitfund): armorType wurde bisher weder gelesen noch
    // geschrieben — jede geladene Ruestung galt als Schild (Slotfehler).
    if (TryParseInt(obj, "armorType", 0, v))
        a.armorType = (rpg::ArmorData::Type)std::clamp(v, 0, 3);
    // PAKET 23: XP guard_state_set (passive Zustaende am Traeger)
    ParseIntArrayInto(obj, "guardStates", a.guardStates);
    // PAKET 24: XP guard_element_set (Element-Schutz, halbiert Schaden)
    ParseIntArrayInto(obj, "guardElements", a.guardElements);
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
    if (TryParseInt(obj, "animationId", 0, v)) s.animationId = v; // PAKET 12 (XP animation_id)
    if (TryParseInt(obj, "mpCost", 0, v)) s.mpCost = v;
    if (TryParseInt(obj, "power", 0, v)) s.power = v;
    if (TryParseInt(obj, "iconIndex", 0, v)) s.iconIndex = v;
    if (TryParseInt(obj, "scope", 0, v)) s.scope = v;
    // PAKET 21: XP occasion (0=immer 1=nur Kampf 2=nur Menue 3=nie)
    if (TryParseInt(obj, "occasion", 0, v)) s.occasion = std::clamp(v, 0, 3);
    // PAKET 24: XP element_id (0 = physisch/kein Element)
    if (TryParseInt(obj, "elementId", 0, v)) s.elementId = std::max(0, v);
    // PAKET 17: XP plus_state_set / minus_state_set (Zustands-IDs)
    ParseIntArrayInto(obj, "plusStates", s.plusStates);
    ParseIntArrayInto(obj, "minusStates", s.minusStates);
    return s;
}

// Liest ein Int-Array ("key":[1,2,...]) aus einem JSON-Objekt in out.
// Gibt false zurueck, wenn der Schluessel fehlt (out bleibt dann leer).
static bool ParseIntArrayInto(const std::string& obj, const char* key,
                              std::vector<int>& out) {
    using namespace rpg::JsonUtils;
    std::string arr;
    if (!FindArrayForKey(obj, key, 0, arr)) return false;
    size_t start = arr.find('[');
    size_t end = arr.rfind(']');
    if (start == std::string::npos || end == std::string::npos) return false;
    std::string inner = arr.substr(start + 1, end - start - 1);
    std::stringstream ss(inner);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try {
            size_t s = 0;
            while (s < token.size() && std::isspace((unsigned char)token[s])) ++s;
            size_t e = token.size();
            while (e > s && std::isspace((unsigned char)token[e - 1])) --e;
            if (s < e) out.push_back(std::stoi(token.substr(s, e - s)));
        } catch (...) {}
    }
    return true;
}

// Schreibt ein Int-Array-Feld: ,"key":[1,2,...]
static void WriteIntArray(std::ofstream& f, const char* key,
                          const std::vector<int>& v) {
    f << ",\"" << key << "\":[";
    for (size_t j = 0; j < v.size(); ++j) {
        if (j) f << ",";
        f << v[j];
    }
    f << "]";
}

// ---------------------------------------------------------------------------
// XP-Animationen (Paket 5) - Parse/Schreib-Helfer
// ---------------------------------------------------------------------------

static rpg::AnimCell ParseAnimCellObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::AnimCell c;
    TryParseInt(obj, "id", 0, c.cellId);
    TryParseInt(obj, "x", 0, c.x);
    TryParseInt(obj, "y", 0, c.y);
    TryParseInt(obj, "scale", 0, c.scale);
    TryParseInt(obj, "rot", 0, c.rotation);
    TryParseInt(obj, "opacity", 0, c.opacity);
    return c;
}

static rpg::AnimFrame ParseAnimFrameObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::AnimFrame fr;
    std::string cellsArr;
    if (FindArrayForKey(obj, "cells", 0, cellsArr)) {
        for (const auto& co : ExtractObjectsFromArray(cellsArr))
            fr.cells.push_back(ParseAnimCellObject(co));
    }
    // SE: {"name":..,"volume":..,"pitch":..}
    std::string seObj;
    if (FindObjectForKey(obj, "se", 0, seObj)) {
        TryParseString(seObj, "name", 0, fr.seName);
        TryParseInt(seObj, "volume", 0, fr.seVolume);
        TryParseInt(seObj, "pitch", 0, fr.sePitch);
    }
    // Flash: {"scope":..,"r":..,"g":..,"b":..,"duration":..}
    std::string flashObj;
    if (FindObjectForKey(obj, "flash", 0, flashObj)) {
        TryParseInt(flashObj, "scope", 0, fr.flashScope);
        TryParseInt(flashObj, "r", 0, fr.flashR);
        TryParseInt(flashObj, "g", 0, fr.flashG);
        TryParseInt(flashObj, "b", 0, fr.flashB);
        TryParseInt(flashObj, "duration", 0, fr.flashDuration);
    }
    return fr;
}

rpg::AnimationData ParseAnimationObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::AnimationData a;
    TryParseInt(obj, "id", 0, a.id);
    TryParseString(obj, "name", 0, a.name);
    TryParseString(obj, "file", 0, a.file);
    TryParseInt(obj, "position", 0, a.position);
    std::string framesArr;
    if (FindArrayForKey(obj, "frames", 0, framesArr)) {
        for (const auto& fo : ExtractObjectsFromArray(framesArr))
            a.frames.push_back(ParseAnimFrameObject(fo));
    }
    return a;
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
    // ---- XP-Flag-Tabellen (Paket 1) ----
    ParseIntArrayInto(obj, "flags", t.flags);
    ParseIntArrayInto(obj, "flags4dir", t.passage4dir);
    ParseIntArrayInto(obj, "priority", t.priority);
    ParseIntArrayInto(obj, "bush", t.bushFlags);
    ParseIntArrayInto(obj, "counter", t.counterFlags);
    ParseIntArrayInto(obj, "terrain", t.terrainTags);
    // ---- XP-Grafik-Zuordnungen ----
    for (int i = 0; i < 7; ++i) {
        std::string key = "autotile" + std::to_string(i + 1);
        if (TryParseString(obj, key.c_str(), 0, name)) t.autotileNames[i] = name;
    }
    if (TryParseString(obj, "panorama", 0, name)) t.panoramaName = name;
    if (TryParseString(obj, "fog", 0, name)) t.fogName = name;
    if (TryParseString(obj, "battleback", 0, name)) t.battlebackName = name;
    return t;
}

rpg::TroopData ParseTroopObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::TroopData t;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) t.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) t.name = name;
    std::string arr;
    // members: Array aus Gegner-IDs
    if (FindArrayForKey(obj, "members", 0, arr)) {
        size_t start = arr.find('[');
        size_t end = arr.rfind(']');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string inner = arr.substr(start + 1, end - start - 1);
            std::stringstream ss(inner);
            std::string token;
            while (std::getline(ss, token, ',')) {
                size_t s = 0;
                while (s < token.size() && std::isspace((unsigned char)token[s])) ++s;
                size_t e = token.size();
                while (e > s && std::isspace((unsigned char)token[e - 1])) --e;
                if (s < e) { try { t.members.push_back(std::stoi(token.substr(s, e - s))); } catch (...) {} }
            }
        }
    }
    // pages: Kampf-Ereignis-Seiten (Array von Sub-Objekten)
    std::string pagesArr;
    if (FindArrayForKey(obj, "pages", 0, pagesArr)) {
        auto pageObjs = ExtractObjectsFromArray(pagesArr);
        for (const auto& po : pageObjs) {
            rpg::TroopPage p;
            int iv = 0; bool bv = false;
            if (TryParseBool(po, "switchValid", 0, bv)) p.switchValid = bv;
            if (TryParseInt(po, "switchId", 0, iv)) p.switchId = iv;
            if (TryParseBool(po, "turnValid", 0, bv)) p.turnValid = bv;
            if (TryParseInt(po, "turnA", 0, iv)) p.turnA = iv;
            if (TryParseInt(po, "turnB", 0, iv)) p.turnB = iv;
            if (TryParseBool(po, "actorValid", 0, bv)) p.actorValid = bv;
            if (TryParseInt(po, "actorIndex", 0, iv)) p.actorIndex = iv;
            if (TryParseInt(po, "actorHpBelow", 0, iv)) p.actorHpBelow = iv;
            if (TryParseBool(po, "enemyValid", 0, bv)) p.enemyValid = bv;
            if (TryParseInt(po, "enemyIndex", 0, iv)) p.enemyIndex = iv;
            if (TryParseInt(po, "enemyHpBelow", 0, iv)) p.enemyHpBelow = iv;
            if (TryParseInt(po, "span", 0, iv)) p.span = iv;
            if (TryParseInt(po, "commonEventId", 0, iv)) p.commonEventId = iv;
            t.pages.push_back(p);
        }
    }
    return t;
}

rpg::StateData ParseStateObject(const std::string& obj) {
    using namespace rpg::JsonUtils;
    rpg::StateData s;
    int id = 0;
    if (TryParseInt(obj, "id", 0, id)) s.id = id;
    std::string name;
    if (TryParseString(obj, "name", 0, name)) s.name = name;
    if (TryParseString(obj, "description", 0, name)) s.description = name;
    int v = 0;
    if (TryParseInt(obj, "restriction", 0, v)) s.restriction = v;
    if (TryParseInt(obj, "priority", 0, v)) s.priority = v;
    if (TryParseInt(obj, "autoRemovalTiming", 0, v)) s.autoRemovalTiming = v;
    if (TryParseInt(obj, "holdTurn", 0, v)) s.holdTurn = v;
    bool b = false;
    if (TryParseBool(obj, "removeAtBattleEnd", 0, b)) s.removeAtBattleEnd = b;
    float f = 0.f;
    if (TryParseFloat(obj, "hpDrainRate", 0, f)) s.hpDrainRate = f;
    return s;
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
    // PAKET 25: Trupp-Liste der Zufallsbegegnungen mit einlesen
    // (fehlte bisher komplett - MapInfos.json verlor die Encounter-Liste)
    {
        std::vector<int> enc;
        ParseIntArrayInto(obj, "encounterList", enc);
        for (size_t i = 0; i < enc.size() && i < 8; ++i) m.encounterList[i] = enc[i];
    }
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
    // Classes
    {
        std::vector<ClassData> tmp;
        if (LoadArrayFile(dbDir + "/Classes.json", tmp, ParseClassObject, "Classes")) {
            if (!tmp.empty()) { mClasses = std::move(tmp); anyLoaded = true; }
        }
    }
    // Tilesets
    {
        std::vector<TilesetData> tmp;
        if (LoadArrayFile(dbDir + "/Tilesets.json", tmp, ParseTilesetObject, "Tilesets")) {
            if (!tmp.empty()) { mTilesets = std::move(tmp); anyLoaded = true; }
        }
    }
    // Animationen (XP-Paket 5)
    {
        std::vector<AnimationData> tmp;
        if (LoadArrayFile(dbDir + "/Animations.json", tmp, ParseAnimationObject, "Animations")) {
            if (!tmp.empty()) { mAnimations = std::move(tmp); anyLoaded = true; }
        }
    }
    // Troops
    {
        std::vector<TroopData> tmp;
        if (LoadArrayFile(dbDir + "/Troops.json", tmp, ParseTroopObject, "Troops")) {
            if (!tmp.empty()) { mTroops = std::move(tmp); anyLoaded = true; }
        }
    }
    // States
    {
        std::vector<StateData> tmp;
        if (LoadArrayFile(dbDir + "/States.json", tmp, ParseStateObject, "States")) {
            if (!tmp.empty()) { mStates = std::move(tmp); anyLoaded = true; }
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

            // ---- XP System-Tab (Strings) ----
            const auto readStr = [&](const char* key, std::string& dst) {
                std::string s;
                if (TryParseString(content, key, 0, s)) dst = s;
            };
            const auto readStrArray = [&](const char* key, std::vector<std::string>& dst) {
                std::string arr2;
                if (!FindArrayForKey(content, key, 0, arr2)) return;
                std::vector<std::string> tmp;
                size_t pos = 0;
                while (true) {
                    size_t q1 = arr2.find('\"', pos);
                    if (q1 == std::string::npos) break;
                    size_t q2 = arr2.find('\"', q1 + 1);
                    if (q2 == std::string::npos) break;
                    tmp.push_back(JsonUtils::Unescape(arr2.substr(q1 + 1, q2 - q1 - 1)));
                    pos = q2 + 1;
                }
                dst = std::move(tmp);
            };
            const auto readIntArray = [&](const char* key, std::vector<int>& dst) {
                std::string arr2;
                if (!FindArrayForKey(content, key, 0, arr2)) return;
                std::vector<int> tmp;
                std::stringstream ss(arr2);
                std::string num;
                while (std::getline(ss, num, ',')) {
                    try { tmp.push_back(std::stoi(num)); } catch (...) {}
                }
                dst = std::move(tmp);
            };
            readStrArray("elements", mSystem.elements);
            readStrArray("animations", mSystem.animations);
            readIntArray("initialParty", mSystem.initialParty);
            readStr("windowskinName", mSystem.windowskinName);
            readStr("titleGraphicName", mSystem.titleGraphicName);
            readStr("gameoverGraphicName", mSystem.gameoverGraphicName);
            readStr("battleTransitionName", mSystem.battleTransitionName);
            readStr("battleBgm", mSystem.battleBgm);
            readStr("titleBgm", mSystem.titleBgm);
            readStr("gameoverMe", mSystem.gameoverMe);
            readStr("battleEndMe", mSystem.battleEndMe);
            readStr("cursorSe", mSystem.cursorSe);
            readStr("decisionSe", mSystem.decisionSe);
            readStr("cancelSe", mSystem.cancelSe);
            readStr("buzzerSe", mSystem.buzzerSe);
            readStr("equipSe", mSystem.equipSe);
            readStr("shopSe", mSystem.shopSe);
            readStr("saveSe", mSystem.saveSe);
            readStr("loadSe", mSystem.loadSe);
            readStr("battleStartSe", mSystem.battleStartSe);
            readStr("escapeSe", mSystem.escapeSe);
            readStr("actorCollapseSe", mSystem.actorCollapseSe);
            readStr("enemyCollapseSe", mSystem.enemyCollapseSe);
            readStr("wordWeapon", mSystem.wordWeapon);
            readStr("wordShield", mSystem.wordShield);
            readStr("wordHelmet", mSystem.wordHelmet);
            readStr("wordBodyArmor", mSystem.wordBodyArmor);
            readStr("wordAccessory", mSystem.wordAccessory);
            readStr("wordHp", mSystem.wordHp);
            readStr("wordSp", mSystem.wordSp);
            readStr("wordStr", mSystem.wordStr);
            readStr("wordDex", mSystem.wordDex);
            readStr("wordAgi", mSystem.wordAgi);
            readStr("wordInt", mSystem.wordInt);
            readStr("wordAtk", mSystem.wordAtk);
            readStr("wordPdef", mSystem.wordPdef);
            readStr("wordMdef", mSystem.wordMdef);
            readStr("wordAttack", mSystem.wordAttack);
            readStr("wordSkill", mSystem.wordSkill);
            readStr("wordDefend", mSystem.wordDefend);
            readStr("wordItem", mSystem.wordItem);
            readStr("wordEquip", mSystem.wordEquip);

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
                std::string eq;
                for (int e : a.equips) {
                    if (!eq.empty()) eq += ",";
                    eq += std::to_string(e);
                }
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
                  << ",\"fmhp\":" << a.finalStats.mhp
                  << ",\"fmmp\":" << a.finalStats.mmp
                  << ",\"fatk\":" << a.finalStats.atk
                  << ",\"fdef\":" << a.finalStats.def
                  << ",\"fmat\":" << a.finalStats.mat
                  << ",\"fmdf\":" << a.finalStats.mdf
                  << ",\"fagi\":" << a.finalStats.agi
                  << ",\"fluk\":" << a.finalStats.luk
                  << ",\"curveHp\":\"" << a.curveHp << "\""
                  << ",\"curveMp\":\"" << a.curveMp << "\""
                  << ",\"curveAtk\":\"" << a.curveAtk << "\""
                  << ",\"curveDef\":\"" << a.curveDef << "\""
                  << ",\"curveAgi\":\"" << a.curveAgi << "\""
                  << ",\"characterName\":\"" << Escape(a.characterName) << "\""
                  << ",\"characterIndex\":" << a.characterIndex
                  << ",\"faceName\":\"" << Escape(a.faceName) << "\""
                  << ",\"faceIndex\":" << a.faceIndex
                  << ",\"battlerName\":\"" << Escape(a.battlerName) << "\""
                  << ",\"equips\":\"" << eq << "\"";
                WriteIntArray(f, "stateRanks", a.stateRanks);   // PAKET 17
                WriteIntArray(f, "elementRanks", a.elementRanks); // PAKET 24
                f << "}";
                if (i+1<mActors.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // Classes.json
        {
            std::ofstream f(dbDir + "/Classes.json");
            f << "[\n";
            for (size_t i=0;i<mClasses.size();++i) {
                const auto& c = mClasses[i];
                std::string ls;
                for (const auto& lrn : c.learnings) {
                    if (!ls.empty()) ls += ",";
                    ls += std::to_string(lrn.level) + ":" + std::to_string(lrn.skillId);
                }
                f << "  {\"id\":" << c.id
                  << ",\"name\":\"" << Escape(c.name) << "\""
                  << ",\"expBase\":" << c.expBase
                  << ",\"expExtra\":" << c.expExtra
                  << ",\"expAccA\":" << c.expAccA
                  << ",\"expAccB\":" << c.expAccB
                  << ",\"learnings\":\"" << ls << "\"";
                WriteIntArray(f, "weaponSet", c.weaponSet); // PAKET 23
                WriteIntArray(f, "armorSet", c.armorSet);   // PAKET 23
                f << "}";
                if (i+1<mClasses.size()) f << ",";
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
                  << ",\"scope\":" << (int)it.scope // PAKET 20 (war ungespeichert)
                  << ",\"hpRecovery\":" << it.hpRecovery
                  << ",\"mpRecovery\":" << it.mpRecovery
                  << ",\"iconName\":\"" << Escape(it.iconName) << "\""
                  << ",\"iconIndex\":" << it.iconIndex
                  << ",\"animationId\":" << it.animationId;
                WriteIntArray(f, "plusStates", it.plusStates);   // PAKET 20
                WriteIntArray(f, "minusStates", it.minusStates); // PAKET 20
                f << "}";
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
                  << ",\"battlerHue\":" << e.battlerHue
                  << ",\"maxHp\":" << e.maxHp
                  << ",\"maxMp\":" << e.maxMp
                  << ",\"atk\":" << e.atk
                  << ",\"def\":" << e.def
                  << ",\"mat\":" << e.mat
                  << ",\"mdf\":" << e.mdf
                  << ",\"agi\":" << e.agi
                  << ",\"luk\":" << e.luk
                  << ",\"exp\":" << e.exp
                  << ",\"gold\":" << e.gold;
                WriteIntArray(f, "stateRanks", e.stateRanks);   // PAKET 17
                WriteIntArray(f, "elementRanks", e.elementRanks); // PAKET 24
                // PAKET 18: XP actions-Objekt-Array
                f << ",\"actions\":[";
                for (size_t j = 0; j < e.actions.size(); ++j) {
                    const auto& ac = e.actions[j];
                    if (j) f << ",";
                    f << "{\"kind\":" << ac.kind
                      << ",\"basic\":" << ac.basic
                      << ",\"skillId\":" << ac.skillId
                      << ",\"rating\":" << ac.rating
                      << ",\"turnA\":" << ac.turnA
                      << ",\"turnB\":" << ac.turnB
                      << ",\"hpBelow\":" << ac.hpBelow
                      << ",\"level\":" << ac.level
                      << ",\"switchId\":" << ac.switchId << "}";
                }
                f << "]}";
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
                f << "]";
                if (!tr.pages.empty()) {
                    f << ",\"pages\":[";
                    for (size_t j=0;j<tr.pages.size();++j) {
                        const auto& p = tr.pages[j];
                        if (j) f << ",";
                        f << "{\"span\":" << p.span
                          << ",\"switchValid\":" << (p.switchValid ? "true" : "false")
                          << ",\"switchId\":" << p.switchId
                          << ",\"turnValid\":" << (p.turnValid ? "true" : "false")
                          << ",\"turnA\":" << p.turnA
                          << ",\"turnB\":" << p.turnB
                          << ",\"actorValid\":" << (p.actorValid ? "true" : "false")
                          << ",\"actorIndex\":" << p.actorIndex
                          << ",\"actorHpBelow\":" << p.actorHpBelow
                          << ",\"enemyValid\":" << (p.enemyValid ? "true" : "false")
                          << ",\"enemyIndex\":" << p.enemyIndex
                          << ",\"enemyHpBelow\":" << p.enemyHpBelow
                          << ",\"commonEventId\":" << p.commonEventId
                          << "}";
                    }
                    f << "]";
                }
                f << "}";
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
                  << ",\"description\":\"" << Escape(s.description) << "\""
                  << ",\"restriction\":" << s.restriction
                  << ",\"priority\":" << s.priority
                  << ",\"removeAtBattleEnd\":" << (s.removeAtBattleEnd ? "true" : "false")
                  << ",\"autoRemovalTiming\":" << s.autoRemovalTiming
                  << ",\"holdTurn\":" << s.holdTurn
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
                  << ",\"defPlus\":" << w.defPlus // PAKET 23
                  << ",\"agiPlus\":" << w.agiPlus // PAKET 23
                  << ",\"iconIndex\":" << w.iconIndex
                  << ",\"animationId\":" << w.animationId;
                WriteIntArray(f, "plusStates", w.plusStates);   // PAKET 22
                WriteIntArray(f, "minusStates", w.minusStates); // PAKET 22
                WriteIntArray(f, "elementSet", w.elementSet);   // PAKET 24
                f << "}";
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
                  << ",\"agiPlus\":" << a.agiPlus        // PAKET 23
                  << ",\"armorType\":" << (int)a.armorType // PAKET 23 (war ungespeichert)
                  << ",\"iconIndex\":" << a.iconIndex;
                WriteIntArray(f, "guardStates", a.guardStates); // PAKET 23
                WriteIntArray(f, "guardElements", a.guardElements); // PAKET 24
                f << "}";
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
                  << ",\"occasion\":" << s.occasion // PAKET 21 (XP occasion)
                  << ",\"elementId\":" << s.elementId // PAKET 24 (XP element_id)
                  << ",\"animation\":\"" << Escape(s.animation) << "\""
                  << ",\"animationId\":" << s.animationId;
                WriteIntArray(f, "plusStates", s.plusStates);   // PAKET 17
                WriteIntArray(f, "minusStates", s.minusStates); // PAKET 17
                f << "}";
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
                  << ",\"tilesetName\":\"" << Escape(t.tilesetName) << "\"";
                // XP-Grafik-Zuordnungen (nur nicht-leere schreiben)
                for (int a = 0; a < 7; ++a) {
                    if (!t.autotileNames[a].empty())
                        f << ",\"autotile" << (a + 1) << "\":\"" << Escape(t.autotileNames[a]) << "\"";
                }
                if (!t.panoramaName.empty())
                    f << ",\"panorama\":\"" << Escape(t.panoramaName) << "\"";
                if (!t.fogName.empty())
                    f << ",\"fog\":\"" << Escape(t.fogName) << "\"";
                if (!t.battlebackName.empty())
                    f << ",\"battleback\":\"" << Escape(t.battlebackName) << "\"";
                // XP-Flag-Tabellen
                WriteIntArray(f, "flags", t.flags);
                WriteIntArray(f, "flags4dir", t.passage4dir);
                WriteIntArray(f, "priority", t.priority);
                WriteIntArray(f, "bush", t.bushFlags);
                WriteIntArray(f, "counter", t.counterFlags);
                WriteIntArray(f, "terrain", t.terrainTags);
                f << "}";
                if (i+1<mTilesets.size()) f << ",";
                f << "\n";
            }
            f << "]\n";
        }
        // Animations.json (XP-Paket 5)
        {
            std::ofstream f(dbDir + "/Animations.json");
            f << "[\n";
            for (size_t i = 0; i < mAnimations.size(); ++i) {
                const auto& a = mAnimations[i];
                f << "  {\"id\":" << a.id
                  << ",\"name\":\"" << Escape(a.name) << "\""
                  << ",\"file\":\"" << Escape(a.file) << "\""
                  << ",\"position\":" << a.position
                  << ",\"frames\":[";
                for (size_t fi = 0; fi < a.frames.size(); ++fi) {
                    const auto& fr = a.frames[fi];
                    f << "{\"cells\":[";
                    for (size_t ci = 0; ci < fr.cells.size(); ++ci) {
                        const auto& c = fr.cells[ci];
                        f << "{\"id\":" << c.cellId << ",\"x\":" << c.x << ",\"y\":" << c.y
                          << ",\"scale\":" << c.scale << ",\"rot\":" << c.rotation
                          << ",\"opacity\":" << c.opacity << "}";
                        if (ci + 1 < fr.cells.size()) f << ",";
                    }
                    f << "]";
                    if (!fr.seName.empty()) {
                        f << ",\"se\":{\"name\":\"" << Escape(fr.seName) << "\""
                          << ",\"volume\":" << fr.seVolume
                          << ",\"pitch\":" << fr.sePitch << "}";
                    }
                    if (fr.flashScope != 0) {
                        f << ",\"flash\":{\"scope\":" << fr.flashScope
                          << ",\"r\":" << fr.flashR << ",\"g\":" << fr.flashG
                          << ",\"b\":" << fr.flashB << ",\"duration\":" << fr.flashDuration << "}";
                    }
                    f << "}";
                    if (fi + 1 < a.frames.size()) f << ",";
                }
                f << "]}";
                if (i + 1 < mAnimations.size()) f << ",";
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
                  << ",\"encounterStep\":" << m.encounterStep;
                // PAKET 25: Encounter-Trupp-Liste persistieren (Round-Trip)
                {
                    std::vector<int> enc(std::begin(m.encounterList), std::end(m.encounterList));
                    WriteIntArray(f, "encounterList", enc);
                }
                f << ",\"bgmName\":\"" << Escape(m.bgmName) << "\""
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
            f << "],\n";

            // ---- XP System-Tab ----
            const auto writeString = [&f](const char* key, const std::string& val, bool comma) {
                f << "  \"" << key << "\":\"" << Escape(val) << "\"" << (comma ? ",\n" : "\n");
            };
            const auto writeStrArray = [&f](const char* key, const std::vector<std::string>& arr) {
                f << "  \"" << key << "\":[";
                for (size_t i=0;i<arr.size();++i) {
                    if (i) f << ",";
                    f << "\"" << Escape(arr[i]) << "\"";
                }
                f << "],\n";
            };
            writeStrArray("elements", mSystem.elements);
            writeStrArray("animations", mSystem.animations);
            f << "  \"initialParty\":[";
            for (size_t i=0;i<mSystem.initialParty.size();++i) {
                if (i) f << ",";
                f << mSystem.initialParty[i];
            }
            f << "],\n";

            writeString("windowskinName", mSystem.windowskinName, true);
            writeString("titleGraphicName", mSystem.titleGraphicName, true);
            writeString("gameoverGraphicName", mSystem.gameoverGraphicName, true);
            writeString("battleTransitionName", mSystem.battleTransitionName, true);
            writeString("battleBgm", mSystem.battleBgm, true);
            writeString("titleBgm", mSystem.titleBgm, true);
            writeString("gameoverMe", mSystem.gameoverMe, true);
            writeString("battleEndMe", mSystem.battleEndMe, true);
            writeString("cursorSe", mSystem.cursorSe, true);
            writeString("decisionSe", mSystem.decisionSe, true);
            writeString("cancelSe", mSystem.cancelSe, true);
            writeString("buzzerSe", mSystem.buzzerSe, true);
            writeString("equipSe", mSystem.equipSe, true);
            writeString("shopSe", mSystem.shopSe, true);
            writeString("saveSe", mSystem.saveSe, true);
            writeString("loadSe", mSystem.loadSe, true);
            writeString("battleStartSe", mSystem.battleStartSe, true);
            writeString("escapeSe", mSystem.escapeSe, true);
            writeString("actorCollapseSe", mSystem.actorCollapseSe, true);
            writeString("enemyCollapseSe", mSystem.enemyCollapseSe, true);
            writeString("wordWeapon", mSystem.wordWeapon, true);
            writeString("wordShield", mSystem.wordShield, true);
            writeString("wordHelmet", mSystem.wordHelmet, true);
            writeString("wordBodyArmor", mSystem.wordBodyArmor, true);
            writeString("wordAccessory", mSystem.wordAccessory, true);
            writeString("wordHp", mSystem.wordHp, true);
            writeString("wordSp", mSystem.wordSp, true);
            writeString("wordStr", mSystem.wordStr, true);
            writeString("wordDex", mSystem.wordDex, true);
            writeString("wordAgi", mSystem.wordAgi, true);
            writeString("wordInt", mSystem.wordInt, true);
            writeString("wordAtk", mSystem.wordAtk, true);
            writeString("wordPdef", mSystem.wordPdef, true);
            writeString("wordMdef", mSystem.wordMdef, true);
            writeString("wordAttack", mSystem.wordAttack, true);
            writeString("wordSkill", mSystem.wordSkill, true);
            writeString("wordDefend", mSystem.wordDefend, true);
            writeString("wordItem", mSystem.wordItem, true);
            writeString("wordEquip", mSystem.wordEquip, false); // letztes Feld (kein Komma)
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

const ClassData* Database::GetClass(const std::string& name) const {
    for (const auto& c : mClasses) if (c.name == name) return &c;
    return nullptr;
}

const TroopData* Database::GetTroop(int id) const {
    for (const auto& e : mTroops) if (e.id==id) return &e;
    return nullptr;
}

const AnimationData* Database::GetAnimation(int id) const {
    for (const auto& a : mAnimations) if (a.id == id) return &a;
    return nullptr;
}

const StateData* Database::GetState(int id) const {
    for (const auto& e : mStates) if (e.id==id) return &e;
    return nullptr;
}

} // namespace rpg
