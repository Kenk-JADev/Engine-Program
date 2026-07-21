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

    // Actors from Party - echte Werte aus der Datenbank (Kurven + Ausruestung)
    auto& party = Game::Get().Party().Members();
    mActors.clear();
    for (size_t i=0;i<party.size();++i) {
        Battler b;
        b.isActor = true;
        b.id = party[i].actorId;
        b.index = (int)i;
        b.maxHp = party[i].MaxHp();
        b.maxMp = party[i].MaxMp();
        b.hp = std::max(0, std::min(party[i].hp, b.maxHp));
        b.mp = std::max(0, std::min(party[i].mp, b.maxMp));
        b.isDead = (b.hp <= 0);
        b.name = party[i].name;
        b.atk = party[i].Atk();
        b.def = party[i].Def();
        b.agi = party[i].Agi();
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
            b.mp = data->maxMp;
            b.maxMp = data->maxMp;
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
                SyncBackToParty();
                if (onVictory) onVictory();
            }
            break;
        case BattleState::Defeat:
            if (mTimer > 2.0f) {
                mState = BattleState::End;
                mLastOutcome = 3;
                SyncBackToParty();
                if (onDefeat) onDefeat();
                // XP: Game Over nur wenn "Niederlage moeglich" NICHT gesetzt
                if (!mCanLose && onGameOver) onGameOver();
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
        // Enemy AI: einen zufaelligen LEBENDEN Akteur angreifen
        action.type = BattleActionType::Attack;
        action.targetIsActor = true;
        std::vector<int> alive;
        for (size_t i=0;i<mActors.size();++i)
            if (!mActors[i].isDead) alive.push_back((int)i);
        if (!alive.empty()) {
            std::random_device rd; std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, (int)alive.size()-1);
            action.targetIndex = alive[(size_t)dist(gen)];
        }
    }

    // Verteidigen endet, sobald der Kaempfer wieder handelt
    subject->isGuarding = false;

    // Zielauflösung: targetIsActor entscheidet ueber die Seite
    auto targetOf = [&](const BattleAction& a) -> Battler* {
        auto& side = a.targetIsActor ? mActors : mEnemies;
        if (a.targetIndex >= 0 && a.targetIndex < (int)side.size())
            return &side[(size_t)a.targetIndex];
        return nullptr;
    };

    if (action.type==BattleActionType::Attack) {
        Battler* target = targetOf(action);
        if (target && !target->isDead) {
            int dmg = std::max(1, subject->atk - target->def/2);
            if (target->isGuarding) dmg = std::max(1, dmg/2);
            target->ApplyDamage(dmg);
            std::string msg = subject->name + " greift " + target->name + " an: " +
                              std::to_string(dmg) + " Schaden!";
            if (target->isDead) msg += " " + target->name + " wurde besiegt!";
            if (onMessage) onMessage(msg);
            if (target->isDead && onEnemyDefeated && !target->isActor) onEnemyDefeated(target->id);
        } else if (onMessage) onMessage(subject->name + " greift an... aber da ist niemand!");
    } else if (action.type==BattleActionType::Skill) {
        // XP-Semantik: scope>=3 = eigene Seite (Heilung um |power|),
        // sonst Schaden am Gegner (power + atk/2 - def/2).
        const SkillData* sk = Database::Get().GetSkill(action.skillId);
        const int power = sk ? (sk->power < 0 ? -sk->power : sk->power) : 40;
        const bool allyScope = sk && sk->scope >= 3;
        const std::string sname = sk ? sk->name : "Fertigkeit";
        if (sk) subject->mp = std::max(0, subject->mp - sk->mpCost);
        if (!allyScope) {
            // Schadens-Skill - Zielseite steht in action.targetIsActor
            Battler* target = targetOf(action);
            if (target && !target->isDead) {
                int dmg = std::max(1, power + subject->atk/2 - target->def/2);
                if (target->isGuarding) dmg = std::max(1, dmg/2);
                target->ApplyDamage(dmg);
                std::string msg = subject->name + " setzt " + sname + " ein: " +
                                  std::to_string(dmg) + " Schaden!";
                if (target->isDead) msg += " " + target->name + " wurde besiegt!";
                if (onMessage) onMessage(msg);
                if (target->isDead && onEnemyDefeated && !target->isActor) onEnemyDefeated(target->id);
            }
        } else {
            // Heil-Skill: Ziel = Verbuendeter (Standard: Anwender selbst)
            Battler* target = subject;
            if (action.targetIsActor) {
                if (Battler* t = targetOf(action)) target = t;
            }
            target->Recover(power, 0);
            if (onMessage) onMessage(subject->name + " setzt " + sname + " ein: " +
                                     target->name + " +" + std::to_string(power) + " HP");
        }
    } else if (action.type==BattleActionType::Item) {
        if (const auto* it = Database::Get().GetItem(action.itemId)) {
            if (it->hpRecovery < 0) {
                // Schadens-Item (z. B. Bombe) - Zielseite aus der Aktion
                Battler* target = targetOf(action);
                if (target && !target->isDead) {
                    const int dmg = -it->hpRecovery;
                    target->ApplyDamage(dmg);
                    std::string msg = subject->name + " benutzt " + it->name + ": " +
                                      std::to_string(dmg) + " Schaden!";
                    if (target->isDead) msg += " " + target->name + " wurde besiegt!";
                    if (onMessage) onMessage(msg);
                    if (target->isDead && onEnemyDefeated && !target->isActor) onEnemyDefeated(target->id);
                }
            } else {
                // Heil-Item auf Verbuendeten (Standard: Anwender selbst)
                Battler* target = subject;
                if (action.targetIsActor) {
                    if (Battler* t = targetOf(action)) target = t;
                }
                target->Recover(it->hpRecovery, it->mpRecovery);
                if (onMessage) onMessage(subject->name + " benutzt " + it->name +
                                         " auf " + target->name + " (+" +
                                         std::to_string(it->hpRecovery) + " HP)");
            }
            Game::Get().Party().GainItem(it->id, -1);
        }
    } else if (action.type==BattleActionType::Guard) {
        subject->isGuarding = true;
        if (onMessage) onMessage(subject->name + " verteidigt sich!");
    } else if (action.type==BattleActionType::Escape) {
        if (mCanEscape) {
            mState = BattleState::End;
            mLastOutcome = 2; // Flucht (Event-Bedingung IfEscape)
            SyncBackToParty();
            if (onMessage) onMessage("Die Flucht ist gelungen!");
            return;
        }
        if (onMessage) onMessage("Flucht nicht moeglich!");
    }

    mNextAction.type = BattleActionType::None;
    mState = BattleState::Action;
    mTimer = 0.0f;
}

void BattleSystem::CheckVictory() {
    // Wiedereintritt verhindern (wird aus mehreren Pfaden aufgerufen):
    // EXP/Gold duerfen nur genau einmal gutgeschrieben werden.
    if (mState == BattleState::Victory || mState == BattleState::Defeat ||
        mState == BattleState::End || mState == BattleState::None)
        return;

    bool allEnemiesDead = true;
    for (auto& e : mEnemies) if (!e.isDead) { allEnemiesDead = false; break; }
    if (allEnemiesDead && !mEnemies.empty()) {
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
        // EXP nur an lebende Mitglieder (XP-Verhalten) + Level-Aufstiege
        std::string msg = "Sieg! +" + std::to_string(mLastExp) + " EXP, +" +
                          std::to_string(mLastGold) + " G";
        for (auto& a : Game::Get().Party().Members()) {
            if (a.IsDead()) continue;
            std::vector<std::string> learned;
            const int ups = a.AddExp(mLastExp, &learned);
            if (ups > 0) {
                msg += "\n" + a.name + " erreicht Level " + std::to_string(a.level) + "!";
                for (const auto& s : learned)
                    msg += "\n" + a.name + " hat [" + s + "] gelernt!";
            }
        }
        if (onMessage) onMessage(msg);
        return;
    }
    bool allActorsDead = true;
    for (auto& a : mActors) if (!a.isDead) { allActorsDead = false; break; }
    if (allActorsDead) {
        mState = BattleState::Defeat;
        mTimer = 0;
        if (onMessage) onMessage("Die Gruppe wurde besiegt...");
    }
}

void BattleSystem::SyncBackToParty() {
    // HP/MP der Akteur-Battler zurueck in die Party schreiben, damit
    // Kampfschaeden/Heilung und MP-Kosten nach dem Kampf bestehen bleiben.
    auto& members = Game::Get().Party().Members();
    for (size_t i = 0; i < mActors.size(); ++i) {
        for (auto& m : members) {
            if (m.actorId == mActors[i].id) {
                m.hp = mActors[i].hp;
                m.mp = mActors[i].mp;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Event-Befehle im Kampf (331..340) - XP "Kampf"-Befehle
// ---------------------------------------------------------------------------
void BattleSystem::Abort() {
    if (IsInBattle()) {
        mState = BattleState::End;
        SyncBackToParty();
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
