#include "rpgmaker3d/BattleSystem.h"
#include "rpgmaker3d/EventSystem.h" // EventCommand fuer ApplyEventCommand
#include "rpgmaker3d/Logger.h"
#include <algorithm>
#include <random>

namespace rpg {

BattleSystem& BattleSystem::Get() {
    static BattleSystem instance;
    return instance;
}

void Battler::ApplyDamage(int dmg) {
    hp -= dmg;
    if (hp <= 0) { hp = 0; isDead = true; }
}
void Battler::Recover(int h, int m) {
    hp += h; if (hp > maxHp) hp = maxHp;
    mp += m; if (mp > maxMp) mp = maxMp;
    if (hp > 0) isDead = false;
}

void BattleSystem::Setup(const std::vector<int>& enemyIds, bool canEscape, bool canLose) {
    Clear();
    mCanEscape = canEscape;
    mCanLose = canLose;
    mLastOutcome = 0; // Ergebnis fuer IfWin/IfEscape/IfLose zuruecksetzen

    // Actors from Party
    auto& party = Game::Get().Party().Members();
    mActors.clear();
    for (size_t i=0;i<party.size();++i) {
        Battler b;
        b.isActor = true;
        b.id = party[i].actorId;
        b.index = (int)i;
        b.hp = party[i].hp;
        b.maxHp = 100;
        b.maxMp = 30;
        b.name = party[i].name;
        b.atk = 20; b.def = 10; b.agi = 12;
        mActors.push_back(b);
    }
    if (mActors.empty()) {
        Battler b; b.isActor=true; b.id=1; b.name="Hero"; b.hp=100; b.maxHp=100;
        mActors.push_back(b);
    }

    // Enemies
    mEnemies.clear();
    for (size_t i=0;i<enemyIds.size();++i) {
        const auto* data = Database::Get().GetEnemy(enemyIds[i]);
        Battler b;
        b.isActor = false;
        b.id = enemyIds[i];
        b.index = (int)i;
        if (data) {
            b.name = data->name;
            b.hp = data->maxHp;
            b.maxHp = data->maxHp;
            b.atk = data->atk;
            b.def = data->def;
            b.agi = data->agi;
        } else {
            b.name = "Enemy"+std::to_string(enemyIds[i]);
            b.hp = 50; b.maxHp = 50; b.atk=10; b.def=5; b.agi=8;
        }
        mEnemies.push_back(b);
    }

    mState = BattleState::Start;
    mTurn = 0;
    mTimer = 0.0f;
    RPG_LOG_INFO("Battle started with "+std::to_string(mEnemies.size())+" enemies");
    if (onMessage) onMessage("Battle Start!");
}

void BattleSystem::Clear() {
    mState = BattleState::None;
    mActors.clear();
    mEnemies.clear();
    mTurn = 0;
    mTimer = 0.0f;
    mLastExp = 0;
    mLastGold = 0;
}

void BattleSystem::Update(float dt) {
    if (mState==BattleState::None || mState==BattleState::End) return;

    mTimer += dt;

    switch (mState) {
        case BattleState::Start:
            if (mTimer > 1.0f) {
                mState = BattleState::Input;
                mTimer = 0;
                if (onMessage) onMessage("Choose action");
            }
            break;
        case BattleState::Input:
            // waiting for external SetAction
            if (mNextAction.type != BattleActionType::None) {
                mState = BattleState::Turn;
                mTimer = 0;
            }
            break;
        case BattleState::Turn:
            ProcessTurn();
            break;
        case BattleState::Action:
            if (mTimer > 0.8f) {
                mState = BattleState::Turn;
                mTimer = 0;
                mTurn++;
                if (mTurn >= (int)(mActors.size()+mEnemies.size())) {
                    mTurn = 0;
                    CheckVictory();
                }
            }
            break;
        case BattleState::Victory:
            if (mTimer > 2.0f) {
                mState = BattleState::End;
                mLastOutcome = 1;
                if (onVictory) onVictory();
            }
            break;
        case BattleState::Defeat:
            if (mTimer > 2.0f) {
                mState = BattleState::End;
                mLastOutcome = 3;
                if (onDefeat) onDefeat();
            }
            break;
        default: break;
    }
}

void BattleSystem::ProcessTurn() {
    // Simple: if all actors acted, enemies act automatically
    CheckVictory();
    if (mState!=BattleState::Turn) return;

    // Determine subject
    bool isActorTurn = mTurn < (int)mActors.size();
    Battler* subject = nullptr;
    if (isActorTurn) {
        if (mTurn < (int)mActors.size()) subject = &mActors[mTurn];
    } else {
        int eIdx = mTurn - (int)mActors.size();
        if (eIdx < (int)mEnemies.size()) subject = &mEnemies[eIdx];
    }

    if (!subject || subject->isDead) {
        mTurn++;
        return;
    }

    // If actor and no action set -> wait for input
    if (isActorTurn && mNextAction.type==BattleActionType::None) {
        mState = BattleState::Input;
        return;
    }

    // Execute action
    BattleAction action = mNextAction;
    if (!isActorTurn) {
        // Enemy AI: attack random actor
        action.type = BattleActionType::Attack;
        std::random_device rd; std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, (int)mActors.size()-1);
        action.targetIndex = dist(gen);
    }

    if (action.type==BattleActionType::Attack) {
        Battler* target = nullptr;
        if (isActorTurn) {
            if (action.targetIndex >=0 && action.targetIndex < (int)mEnemies.size())
                target = &mEnemies[action.targetIndex];
        } else {
            if (action.targetIndex >=0 && action.targetIndex < (int)mActors.size())
                target = &mActors[action.targetIndex];
        }
        if (target && !target->isDead) {
            int dmg = std::max(1, subject->atk - target->def/2);
            target->ApplyDamage(dmg);
            if (onMessage) onMessage(subject->name + " attacks " + target->name + " for " + std::to_string(dmg) + " damage!");
            if (target->isDead && onEnemyDefeated && !target->isActor) onEnemyDefeated(target->id);
        }
    } else if (action.type==BattleActionType::Skill) {
        Battler* target = nullptr;
        if (isActorTurn && action.targetIndex >= 0 && action.targetIndex < (int)mEnemies.size())
            target = &mEnemies[action.targetIndex];
        int power = 40;
        std::string sname = "Skill";
        if (const auto* sk = Database::Get().GetSkill(action.skillId)) {
            power = sk->power; sname = sk->name;
            subject->mp = std::max(0, subject->mp - sk->mpCost);
        }
        if (target && !target->isDead) {
            if (power >= 0) {
                int dmg = std::max(1, power + subject->atk/2 - target->def/2);
                target->ApplyDamage(dmg);
                if (onMessage) onMessage(subject->name + " uses " + sname + " for " + std::to_string(dmg) + "!");
            } else {
                // heal
                int heal = -power;
                subject->Recover(heal, 0);
                if (onMessage) onMessage(subject->name + " uses " + sname + " +" + std::to_string(heal) + " HP");
            }
        }
    } else if (action.type==BattleActionType::Item) {
        if (const auto* it = Database::Get().GetItem(action.itemId)) {
            subject->Recover(it->hpRecovery, it->mpRecovery);
            Game::Get().Party().GainItem(it->id, -1);
            if (onMessage) onMessage(subject->name + " uses " + it->name);
        }
    } else if (action.type==BattleActionType::Guard) {
        if (onMessage) onMessage(subject->name + " guards!");
    } else if (action.type==BattleActionType::Escape) {
        if (mCanEscape) {
            mState = BattleState::End;
            mLastOutcome = 2; // Flucht (Event-Bedingung IfEscape)
            if (onMessage) onMessage("Escaped!");
            return;
        }
    }

    mNextAction.type = BattleActionType::None;
    mState = BattleState::Action;
    mTimer = 0.0f;
}

void BattleSystem::CheckVictory() {
    bool allEnemiesDead = true;
    for (auto& e : mEnemies) if (!e.isDead) { allEnemiesDead = false; break; }
    if (allEnemiesDead) {
        mState = BattleState::Victory;
        mTimer = 0;
        mLastExp = 0; mLastGold = 0;
        for (auto& e : mEnemies) {
            if (const auto* d = Database::Get().GetEnemy(e.id)) {
                mLastExp += d->exp;
                mLastGold += d->gold;
            } else {
                mLastExp += 5; mLastGold += 3;
            }
        }
        Game::Get().Party().GainGold(mLastGold);
        for (auto& a : Game::Get().Party().Members()) a.exp += mLastExp;
        if (onMessage) onMessage("Victory! EXP +" + std::to_string(mLastExp) +
                                 " Gold +" + std::to_string(mLastGold));
        return;
    }
    bool allActorsDead = true;
    for (auto& a : mActors) if (!a.isDead) { allActorsDead = false; break; }
    if (allActorsDead) {
        mState = BattleState::Defeat;
        mTimer = 0;
        if (onMessage) onMessage("Defeat...");
    }
}

// ---------------------------------------------------------------------------
// Event-Befehle im Kampf (331..340) - XP "Kampf"-Befehle
// ---------------------------------------------------------------------------
void BattleSystem::Abort() {
    if (IsInBattle()) {
        mState = BattleState::End;
        if (onMessage) onMessage("Kampf abgebrochen.");
    }
}

void BattleSystem::ApplyEventCommand(const EventCommand& cmd) {
    using CC = EventCommandCode;
    auto forEnemies = [&](int index, const std::function<void(Battler&)>& fn) {
        if (index <= 0) { // 0/-1 = ganze Truppe
            for (auto& e : mEnemies) fn(e);
        } else if (index - 1 < (int)mEnemies.size()) {
            fn(mEnemies[index - 1]);
        }
    };
    switch (cmd.code) {
        case CC::ChangeEnemyHP:
            forEnemies(cmd.param1, [&](Battler& b) { b.ApplyDamage(-cmd.param2); });
            CheckVictory();
            break;
        case CC::ChangeEnemySP:
            forEnemies(cmd.param1, [&](Battler& b) {
                b.mp += cmd.param2;
                if (b.mp < 0) b.mp = 0;
                if (b.mp > b.maxMp) b.mp = b.maxMp;
            });
            break;
        case CC::ChangeEnemyState:
            RPG_LOG_INFO("[Battle] ChangeEnemyState index=" + std::to_string(cmd.param1) +
                         " (Battler-Status ist einfach gehalten)");
            break;
        case CC::EnemyRecoverAll:
            forEnemies(cmd.param1, [&](Battler& b) {
                b.hp = b.maxHp; b.mp = b.maxMp; b.isDead = false;
            });
            break;
        case CC::EnemyAppearance:
            forEnemies(cmd.param1, [&](Battler& b) {
                b.isDead = false;
                if (b.hp <= 0) b.hp = 1;
            });
            break;
        case CC::EnemyTransform:
            forEnemies(cmd.param1, [&](Battler& b) {
                b.id = cmd.param2;
                if (const auto* d = Database::Get().GetEnemy(b.id)) {
                    b.name = d->name;
                    b.maxHp = d->maxHp; b.hp = d->maxHp;
                    b.maxMp = d->maxMp; b.mp = d->maxMp;
                    b.atk = d->atk; b.def = d->def; b.agi = d->agi;
                    b.isDead = false;
                }
            });
            break;
        case CC::DealDamage: {
            // param1: 0=Gegner, 1=Akteur; param2: Index (0=alle), param3: Schaden
            int dmg = cmd.param3;
            if (cmd.param1 == 1) {
                if (cmd.param2 <= 0) {
                    for (auto& a : mActors) a.ApplyDamage(dmg);
                } else if (cmd.param2 - 1 < (int)mActors.size()) {
                    mActors[cmd.param2 - 1].ApplyDamage(dmg);
                }
            } else {
                forEnemies(cmd.param2, [&](Battler& b) { b.ApplyDamage(dmg); });
            }
            CheckVictory();
            break;
        }
        case CC::ForceAction:
            RPG_LOG_INFO("[Battle] ForceAction (naechste Aktion wird erzwungen)");
            break;
        default:
            break;
    }
}

} // namespace rpg
