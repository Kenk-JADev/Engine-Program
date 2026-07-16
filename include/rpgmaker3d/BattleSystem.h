#pragma once
// RPG Maker 3D - Einfaches rundenbasiertes Kampfsystem

#include <vector>
#include <functional>
#include "Types.h"
#include "Database.h"
#include "Game.h"

namespace rpg {

enum class BattleState {
    None,
    Start,
    Input,
    Turn,
    Action,
    Victory,
    Defeat,
    End
};

enum class BattleActionType {
    None,
    Attack,
    Guard,
    Skill,
    Item,
    Escape
};

struct BattleAction {
    BattleActionType type = BattleActionType::None;
    int subjectIndex = 0; // who acts
    int targetIndex = 0; // who receives
    int skillId = 0;
    int itemId = 0;
};

struct Battler {
    bool isActor = true;
    int id = 0; // actorId or enemyId
    int index = 0;
    int hp = 100;
    int mp = 10;
    int maxHp = 100;
    int maxMp = 10;
    int atk = 10;
    int def = 10;
    int agi = 10;
    bool isDead = false;
    Vec3 position{0,0,0};
    std::string name;

    void ApplyDamage(int dmg);
    void Recover(int hp, int mp);
};

class BattleSystem {
public:
    static BattleSystem& Get();

    void Setup(const std::vector<int>& enemyIds, bool canEscape = true, bool canLose = false);
    void Update(float dt);
    void Clear();

    BattleState GetState() const { return mState; }
    bool IsInBattle() const { return mState != BattleState::None && mState != BattleState::End; }

    void SetAction(const BattleAction& action) { mNextAction = action; }
    bool NeedsInput() const { return mState == BattleState::Input; }
    int GetTurn() const { return mTurn; }
    std::vector<Battler>& Actors() { return mActors; }
    std::vector<Battler>& Enemies() { return mEnemies; }
    int LastExp() const { return mLastExp; }
    int LastGold() const { return mLastGold; }

    // Callbacks für UI/Audio
    std::function<void(const std::string&)> onMessage;
    std::function<void(int enemyId)> onEnemyDefeated;
    std::function<void()> onVictory;
    std::function<void()> onDefeat;

private:
    BattleSystem() = default;
    void ProcessTurn();
    void CheckVictory();

    BattleState mState = BattleState::None;
    std::vector<Battler> mActors;
    std::vector<Battler> mEnemies;
    BattleAction mNextAction;
    float mTimer = 0.0f;
    int mTurn = 0;
    bool mCanEscape = true;
    bool mCanLose = false;
    int mLastExp = 0;
    int mLastGold = 0;
};

} // namespace rpg
