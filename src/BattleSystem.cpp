#include "rpgmaker3d/BattleSystem.h"
#include "rpgmaker3d/EventSystem.h" // EventCommand fuer ApplyEventCommand
#include "rpgmaker3d/Logger.h"
#include <algorithm>
#include <random>
#include <cctype>

namespace rpg {

namespace {
/// Geteiltes Zufallsrad der Kampfregeln (Crit/Miss-Wuerfe, PAKET 9)
std::mt19937& BattleRng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

// --- PAKET 12: Animationsauflösung (XP animation_id / Legacy-Name) --------
/// Legacy-Fallback: der alte Freitext (Skill.animation) wird per
/// Namensabgleich (gross/klein egal) gegen die Datenbank-Animationen geloest.
/// Effektive ID wie im Editor: id > 0, sonst Listenindex + 1.
int FindAnimationIdByName(const std::string& name) {
    if (name.empty()) return 0;
    const auto& set = Database::Get().AnimationSet();
    for (size_t i = 0; i < set.size(); ++i) {
        const std::string& t = set[i].name;
        if (t.size() != name.size()) continue;
        bool eq = true;
        for (size_t k = 0; k < t.size(); ++k) {
            if (std::tolower((unsigned char)t[k]) != std::tolower((unsigned char)name[k])) {
                eq = false; break;
            }
        }
        if (eq) return set[i].id > 0 ? set[i].id : (int)i + 1;
    }
    return 0;
}

/// XP Game_Actor#animation1_id: Animation der ausgeruesteten Waffe.
int ActorWeaponAnimationId(int actorId) {
    if (auto* ga = Game::Get().Party().GetActor(actorId)) {
        for (const auto& w : Database::Get().Weapons())
            if (w.id == ga->weaponId) return w.animationId;
    }
    return 0;
}
} // namespace

BattleSystem& BattleSystem::Get() {
    static BattleSystem instance;
    return instance;
}

void Battler::ApplyDamage(int dmg) {
    ApplyDamage(dmg, BattleHitKind::Damage);
}
void Battler::ApplyDamage(int dmg, BattleHitKind kind) {
    const int before = hp;
    hp -= dmg;
    if (hp <= 0) { hp = 0; isDead = true; }
    // PAKET 9: XP-Kampf-Feedback — effektive HP-Aenderung melden (>0 Schaden)
    if (auto& hook = BattleSystem::Get().onBattlerHit) {
        const int eff = before - hp;
        if (eff != 0) hook(*this, kind, eff);
    }
}
void Battler::Recover(int h, int m) {
    const int before = hp;
    hp += h; if (hp > maxHp) hp = maxHp;
    mp += m; if (mp > maxMp) mp = maxMp;
    if (hp > 0) isDead = false;
    // PAKET 9: Heilung als negative HP-Aenderung melden (nur wenn HP wirklich
    // stiegen — reine MP-Heilung loest kein Popup aus)
    if (auto& hook = BattleSystem::Get().onBattlerHit) {
        const int eff = hp - before;
        if (eff != 0) hook(*this, BattleHitKind::Heal, -eff);
    }
}
void Battler::NotifyMiss() {
    // PAKET 9: „Ausgewichen!" (0 Aenderung — Popup-/Flash-Text entscheidet)
    if (auto& hook = BattleSystem::Get().onBattlerHit) hook(*this, BattleHitKind::Miss, 0);
}

void BattleSystem::Setup(const std::vector<int>& enemyIds, bool canEscape, bool canLose,
                         const std::vector<TroopPage>& pages) {
    Clear();
    mCanEscape = canEscape;
    mCanLose = canLose;
    mLastOutcome = 0; // Ergebnis fuer IfWin/IfEscape/IfLose zuruecksetzen
    // XP-Kampfereignis-Seiten des Trupps uebernehmen (Copy, da Runtime-Flags)
    mPages = pages;
    mPageStates.assign(mPages.size(), {});

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
    mRound = 0;
    mTimer = 0.0f;
    mLastExp = 0;
    mLastGold = 0;
    mPages.clear();
    mPageStates.clear();
    mPageWaiting = false;
}

void BattleSystem::Update(float dt) {
    if (mState==BattleState::None || mState==BattleState::End) return;

    // XP-Kampfereignis: Solange eine Seiten-Befehlsliste laeuft, pausiert
    // der komplette Kampffluss (Timer, Zuege, Sieg/Niederlage).
    if (mPageWaiting) {
        if (onIsTroopPageRunning && onIsTroopPageRunning(mPageRuntimeId)) return;
        mPageWaiting = false;
    }
    // Seiten auswerten (Kampf-/Runden-/Moment-Spannen). Feuert eine Seite,
    // wartet der Kampf bis zum naechsten Frame auf die Befehlsliste.
    if (!mPages.empty()) {
        CheckTroopPages();
        if (mPageWaiting) return;
    }

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
            // PAKET 12: XP-Warteverhalten — die Aktions-Animation (Waffe/
            // Skill/Item) laeuft sichtbar zu Ende, bevor der naechste
            // Kaempfer an der Reihe ist. Mindestpause 0.8 s wie bisher.
            if (mTimer > 0.8f && !Game::Get().IsAnimationPlaying()) {
                mState = BattleState::Turn;
                mTimer = 0;
                mTurn++;
                if (mTurn >= (int)(mActors.size()+mEnemies.size())) {
                    mTurn = 0;
                    ++mRound; // neue Kampfrunde (Seiten-Bedingung "Runde")
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

// ---------------------------------------------------------------------------
// XP-Kampfereignis-Seiten (Trupps-Tab)
// ---------------------------------------------------------------------------
bool BattleSystem::TroopPageConditionMet(const TroopPage& p) const {
    if (p.switchValid && !Game::Get().Switches().Get(p.switchId)) return false;
    if (p.turnValid) {
        // Runde turnA + turnB*x trifft zu (x >= 0)
        if (mRound < p.turnA) return false;
        const int d = mRound - p.turnA;
        if (p.turnB > 0) { if (d % p.turnB != 0) return false; }
        else if (d != 0) return false;
    }
    if (p.actorValid) {
        const int i = p.actorIndex - 1; // 1-basiert (Party-Platz)
        if (i < 0 || i >= (int)mActors.size()) return false;
        const Battler& a = mActors[(size_t)i];
        const int pct = (a.maxHp > 0) ? (100 * a.hp / a.maxHp) : 0;
        if (a.isDead || pct > p.actorHpBelow) return false;
    }
    if (p.enemyValid) {
        const int i = p.enemyIndex - 1; // 1-basiert (Trupp-Platz)
        if (i < 0 || i >= (int)mEnemies.size()) return false;
        const Battler& e = mEnemies[(size_t)i];
        const int pct = (e.maxHp > 0) ? (100 * e.hp / e.maxHp) : 0;
        if (e.isDead || pct > p.enemyHpBelow) return false;
    }
    return true;
}

void BattleSystem::CheckTroopPages() {
    for (size_t i = 0; i < mPages.size(); ++i) {
        const TroopPage& p = mPages[i];
        PageState& st = (i < mPageStates.size()) ? mPageStates[i] : mPageStates.emplace_back();
        const bool met = TroopPageConditionMet(p);
        bool fire = false;
        if (p.span == 0) {          // Kampf: einmal je Kampf
            if (met && !st.doneOnce) fire = true;
        } else if (p.span == 1) {   // Runde: einmal je Runde
            if (met && st.lastFiredTurn != mRound) fire = true;
        } else {                    // Moment: sofort; neu erst nach Nicht-Erfuellung
            if (met && !st.momentLatch) fire = true;
            if (!met) st.momentLatch = false;
        }
        if (!fire) continue;
        // Flags setzen (auch ohne Callback/CE, damit kein Dauerfeuer)
        if (p.span == 0) st.doneOnce = true;
        else if (p.span == 1) st.lastFiredTurn = mRound;
        else st.momentLatch = true;
        if (p.commonEventId > 0 && onRunTroopPage) {
            // Laufzeit-ID pro Seite stabil (blockierender Interpreter pausiert
            // den Kampf ueber mPageWaiting bis er fertig ist)
            mPageRuntimeId = 900000 + (int)i + 1;
            mPageWaiting = true;
            RPG_LOG_INFO("Kampfereignis-Seite " + std::to_string(i + 1) +
                         " -> Gem. Event " + std::to_string(p.commonEventId));
            onRunTroopPage(p.commonEventId, mPageRuntimeId);
        }
        return; // XP: max. eine Seite pro Ausloese-Gelegenheit
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
            // PAKET 12: Waffen-Animation am Ziel (nur Akteure — XP: der
            // Gegner-Standardangriff hat keine Grafiksequenz, das Ziel
            // blinkt/blitzt ueber onBattlerHit).
            if (subject->isActor && onBattleAnimation) {
                const int animId = ActorWeaponAnimationId(subject->id);
                if (animId > 0) onBattleAnimation(*target, animId);
            }
            // PAKET 9: XP-Kampfregel — Ausweichen (5%) vor kritischem
            // Treffer (1/16, dreifacher Schaden), beides im Popup sichtbar
            std::uniform_real_distribution<float> uni(0.0f, 1.0f);
            if (uni(BattleRng()) < 0.05f) {
                target->NotifyMiss();
                if (onMessage) onMessage(subject->name + " greift " + target->name +
                                         " an... Ausgewichen!");
            } else {
                int dmg = std::max(1, subject->atk - target->def/2);
                if (target->isGuarding) dmg = std::max(1, dmg/2);
                const bool crit = uni(BattleRng()) < 0.0625f;
                if (crit) dmg *= 3;
                target->ApplyDamage(dmg, crit ? BattleHitKind::Crit : BattleHitKind::Damage);
                std::string msg = subject->name + " greift " + target->name + " an: " +
                                  std::to_string(dmg) + " Schaden!";
                if (crit) msg += " Kritischer Treffer!";
                if (target->isDead) msg += " " + target->name + " wurde besiegt!";
                if (onMessage) onMessage(msg);
                if (target->isDead && onEnemyDefeated && !target->isActor) onEnemyDefeated(target->id);
            }
        } else if (onMessage) onMessage(subject->name + " greift an... aber da ist niemand!");
    } else if (action.type==BattleActionType::Skill) {
        // XP-Semantik: scope>=3 = eigene Seite (Heilung um |power|),
        // sonst Schaden am Gegner (power + atk/2 - def/2).
        const SkillData* sk = Database::Get().GetSkill(action.skillId);
        const int power = sk ? (sk->power < 0 ? -sk->power : sk->power) : 40;
        const bool allyScope = sk && sk->scope >= 3;
        const std::string sname = sk ? sk->name : "Fertigkeit";
        if (sk) subject->mp = std::max(0, subject->mp - sk->mpCost);
        // PAKET 12: Skill-Animation (XP animation_id; Fallback: Legacy-
        // Namens-String per Datenbank-Abgleich).
        const int skillAnimId = sk ? (sk->animationId > 0
                                      ? sk->animationId
                                      : FindAnimationIdByName(sk->animation)) : 0;
        if (!allyScope) {
            // Schadens-Skill - Zielseite steht in action.targetIsActor
            Battler* target = targetOf(action);
            if (target && !target->isDead) {
                if (skillAnimId > 0 && onBattleAnimation)
                    onBattleAnimation(*target, skillAnimId);
                // PAKET 9: gleiche XP-Regel wie beim Angriff (Miss 5%, Crit 1/16 x3)
                std::uniform_real_distribution<float> uni(0.0f, 1.0f);
                if (uni(BattleRng()) < 0.05f) {
                    target->NotifyMiss();
                    if (onMessage) onMessage(subject->name + " setzt " + sname + " ein... Ausgewichen!");
                } else {
                    int dmg = std::max(1, power + subject->atk/2 - target->def/2);
                    if (target->isGuarding) dmg = std::max(1, dmg/2);
                    const bool crit = uni(BattleRng()) < 0.0625f;
                    if (crit) dmg *= 3;
                    target->ApplyDamage(dmg, crit ? BattleHitKind::Crit : BattleHitKind::Damage);
                    std::string msg = subject->name + " setzt " + sname + " ein: " +
                                      std::to_string(dmg) + " Schaden!";
                    if (crit) msg += " Kritischer Treffer!";
                    if (target->isDead) msg += " " + target->name + " wurde besiegt!";
                    if (onMessage) onMessage(msg);
                    if (target->isDead && onEnemyDefeated && !target->isActor) onEnemyDefeated(target->id);
                }
            }
        } else {
            // Heil-Skill: Ziel = Verbuendeter (Standard: Anwender selbst)
            Battler* target = subject;
            if (action.targetIsActor) {
                if (Battler* t = targetOf(action)) target = t;
            }
            // PAKET 12
            if (skillAnimId > 0 && onBattleAnimation)
                onBattleAnimation(*target, skillAnimId);
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
                    // PAKET 12
                    if (it->animationId > 0 && onBattleAnimation)
                        onBattleAnimation(*target, it->animationId);
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
                // PAKET 12
                if (it->animationId > 0 && onBattleAnimation)
                    onBattleAnimation(*target, it->animationId);
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
