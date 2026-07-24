#pragma once
// RPG Maker 3D - Einfaches rundenbasiertes Kampfsystem

#include <vector>
#include <functional>
#include <string>
#include <map>
#include "Types.h"
#include "Database.h"
#include "Game.h"

namespace rpg {

struct EventCommand; // EventSystem.h (kein include-Loop)

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
    // true = Ziel ist ein Akteur (Heil-Skills/Items auf Verbuendete),
    // false = Ziel ist ein Gegner (Angriff/Schadens-Skills)
    bool targetIsActor = false;
};

/// PAKET 9: Treffer-Art fuer das Kampf-Feedback (Popup-/Flash-Darstellung).
enum class BattleHitKind { Damage, Crit, Heal, Miss };

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
    bool isGuarding = false; // Verteidigen: halbiert Schaden bis zur naechsten eigenen Aktion
    // PAKET 18: Gegner-Flucht (XP basic 2) — zaehlt fuer den Sieg wie tot,
    // bringt aber kein EXP/Gold und feuert kein onEnemyDefeated.
    bool escaped = false;
    Vec3 position{0,0,0};
    std::string name;

    // ---- PAKET 17: XP-Zustaende (States) im Kampf ----------------------
    // Wird beim Setup aus der Party uebernommen und bei Kampfende via
    // SyncBackToParty zurueckgeschrieben (removeAtBattleEnd-Zustaende
    // loesen sich dabei XP-konform auf). Tod loescht alle Zustaende (XP).
    std::vector<int> states;        // aktive Zustands-IDs (Database::States)
    std::map<int,int> stateTurns;   // Runden seit Verhaengung (fuer holdTurn)

    bool HasState(int stateId) const;
    /// true = neu verhaengt (false = schon aktiv oder unbekannte ID)
    bool AddState(int stateId);
    /// true = war aktiv und wurde entfernt
    bool RemoveState(int stateId);
    /// Einschraenkung des hoechstpriorisierten Zustands (0 = keine)
    int  CurrentRestriction() const;
    /// „Kann sich nicht bewegen" (restriction 4)?
    bool CannotAct() const { return CurrentRestriction() == 4; }
    /// Summe aller hpDrainRate aktiver Zustaende (XP slip_damage), i. d. R. 0..1
    float TotalHpDrainRate() const;
    /// Name des hoechstpriorisierten Zustands (Meldungen/HUD), "" wenn keiner
    std::string MostSevereStateName() const;

    void ApplyDamage(int dmg);
    /// PAKET 9: Variante mit Treffer-Art (Crit-Anzeige im Popup)
    void ApplyDamage(int dmg, BattleHitKind kind);
    void Recover(int hp, int mp);
    /// PAKET 9: „Ausgewichen!" ohne HP-Aenderung melden
    void NotifyMiss();
};

class BattleSystem {
public:
    static BattleSystem& Get();

    void Setup(const std::vector<int>& enemyIds, bool canEscape = true, bool canLose = false,
               const std::vector<TroopPage>& pages = {});
    void Update(float dt);
    void Clear();

    BattleState GetState() const { return mState; }
    bool IsInBattle() const { return mState != BattleState::None && mState != BattleState::End; }

    void SetAction(const BattleAction& action) { mNextAction = action; }
    bool NeedsInput() const { return mState == BattleState::Input; }
    /// Index des Akteur-Battlers, der gerade eine Aktion waehlen darf
    /// (gueltig wenn NeedsInput() == true; entspricht dem Party-Index).
    int GetInputActorIndex() const { return mTurn; }
    /// false = "Kann nicht fliehen" (Battle Processing: Flucht verboten)
    bool CanEscape() const { return mCanEscape; }
    int GetTurn() const { return mTurn; }
    std::vector<Battler>& Actors() { return mActors; }
    std::vector<Battler>& Enemies() { return mEnemies; }
    int LastExp() const { return mLastExp; }
    int LastGold() const { return mLastGold; }

    // Event-Befehle (Kampf-Seite 3): 601 ff. Auswertung + Abbruch
    /// 0 = keiner, 1 = Sieg, 2 = Flucht, 3 = Niederlage (wird bei Setup zurueckgesetzt)
    int GetLastOutcome() const { return mLastOutcome; }
    /// Kampf abbrechen (Event-Befehl 340)
    void Abort();
    /// Event-Befehle 331..339 auf Kampfteilnehmer anwenden
    void ApplyEventCommand(const EventCommand& cmd);

    // Callbacks für UI/Audio
    std::function<void(const std::string&)> onMessage;
    std::function<void(int enemyId)> onEnemyDefeated;
    /// XP-Kampf-Feedback (PAKET 9): Treffer-Ereignis eines Battlers.
    /// kind Damage/Crit/Miss/Heal; amount = effektive HP-Aenderung
    /// (>0 Schaden, <0 Heilung, Miss = 0). Wird ZENTRAL aus
    /// Battler::ApplyDamage/Recover/NotifyMiss gemeldet und deckt so
    /// Angriffe, Fertigkeiten, Items UND Kampf-Ereignis-Befehle ab.
    std::function<void(const Battler& b, BattleHitKind kind, int amount)> onBattlerHit;
    /// PAKET 12: XP-Kampf-Animationen (Waffen-/Skill-/Item-Animation) — wird
    /// VOR dem Schaden einer Aktion am Ziel-Battler gefeuert. animId: Waffe
    /// des Angreifers (nur Akteure; XP: Gegner-Standardangriff ist ohne
    /// Animation), Skill.animationId (Fallback: Namens-Abgleich des alten
    /// String-Felds), Item.animationId. Die Engine projiziert die Position
    /// und startet die Sequenz; der Kampf wartet in BattleState::Action auf
    /// das Ende (IsAnimationPlaying), siehe ProcessTurn.
    std::function<void(const Battler& target, int animId)> onBattleAnimation;
    /// PAKET 15: XP-Sieg-ME (System.battleEndMe) — eigener zentraler
    /// Audio-Hook, WEIL die kampfstart-seitigen Setup-Pfade onVictory/
    /// onMessage regelmaessig neu setzen (Engine injiziert ihn einmal in
    /// Initialize; feuert in CheckVictory beim Uebergang nach Victory).
    std::function<void(const std::string& meName)> onVictoryMe;
    /// PAKET 15: XP-Ergebnisfluss — true, solange die Kampf-Nachricht
    /// (Sieg/EXP/Level-Ups) sichtbar ist; Victory wartet dann auf die
    /// Quittierung statt auf die starren 2 s (Engine injiziert:
    /// GameUI::Message().IsBusy()).
    std::function<bool()> isMessageBusy;
    std::function<void()> onVictory;
    std::function<void()> onDefeat;
    /// XP "Game Over": bei Niederlage UND !canLose (nach onDefeat).
    /// Die Engine zeigt die Anzeige und kehrt zum Titel zurueck.
    std::function<void()> onGameOver;
    /// XP-Kampfereignis-Seiten (Trupps-Tab): Wenn eine Seite feuert, startet
    /// die Engine das Gemeinsame Ereignis als blockierenden Interpreter mit
    /// der hier uebergebenen Laufzeit-ID; der Kampf pausiert, bis
    /// onIsTroopPageRunning(runtimeId) false liefert.
    std::function<void(int commonEventId, int runtimeEventId)> onRunTroopPage;
    std::function<bool(int runtimeEventId)> onIsTroopPageRunning;

private:
    BattleSystem() = default;
    void ProcessTurn();
    void CheckVictory();
    /// HP/MP der Akteur-Battler zurueck in die Party schreiben (Kampfende)
    void SyncBackToParty();
    /// XP-Kampfereignis-Seiten auswerten (max. eine Seite pro Aufruf, wie XP)
    void CheckTroopPages();
    bool TroopPageConditionMet(const TroopPage& p) const;
    // ---- PAKET 17: XP-Zustaende ----
    /// Schlupfschaden (XP slip_damage) am eigenen Zug des Kaempfers
    void ApplySlipDamage(Battler& b);
    /// Rundenzaehler hochzaehlen + Zustaende mit Timing „Nach Aktion" (1)
    /// nach Ablauf der Haltezeit aufloesen (mit Meldung)
    void TickSubjectStates(Battler& b);
    /// Zustaende mit Timing „Rundenende" (2) aller Kaempfer aufloesen
    void RoundEndStateRemovals();
    /// Fertigkeits-Zustaende am Ziel anwenden (plus/minus, XP-Trefferquote
    /// ueber die Resistenz-Raenge A..F des Ziels)
    void ApplySkillStates(Battler& target, const SkillData& sk);
    /// PAKET 20: generische Zustands-Anwendung (Skill UND Item aufrufbar)
    void ApplyStateSets(Battler& target, const std::vector<int>& plus,
                        const std::vector<int>& minus);
    // ---- PAKET 22: XP-Zielsystem (scope 0..7) fuer Skills UND Items ----
    /// Loesen einen XP-Scope in die konkrete Zielliste auf:
    /// 0 kein Ziel / 1 ein Gegner / 2 alle Gegner / 3 ein Verbuendeter /
    /// 4 alle Verbuendeten / 5 ein Verbuendeter (tot) / 6 alle (tot) /
    /// 7 Anwender. „Gegner"/„Verbuendeter" aus Sicht von subject;
    /// chosen = im Menue gewaehlter Index der betreffenden Seite (-1 =
    /// nicht gewaehlt). Ungueltige Wahl faellt auf ein zufaelliges
    /// gueltiges Ziel zurueck (XP-Verhalten bei KI/Schnellwahl).
    std::vector<Battler*> ResolveScopeTargets(Battler& subject, int scope,
                                              bool targetIsActor, int chosen);
    // ---- PAKET 18: XP-Gegner-Verhaltenstabelle (RPG::Enemy.actions) ----
    /// Waehlt die Aktion eines Gegner-Kaempfers (Bedinungen, Rating-Lostopf
    /// max-3, Skill-MP-Check; Flucht/Nichtstun wird intern abgewickelt und
    /// liefert dann BattleActionType::None). Gegner ohne Tabelleneintrag
    /// faellt auf den Standardangriff zurueck.
    BattleAction MakeEnemyAction(Battler& enemy);

    BattleState mState = BattleState::None;
    std::vector<Battler> mActors;
    std::vector<Battler> mEnemies;
    BattleAction mNextAction;
    float mTimer = 0.0f;
    int mTurn = 0;
    int mRound = 0; // Kampfrunde (0-basiert, fuer Seiten-Bedingung "Runde")
    // XP-Kampfereignis-Seiten
    struct PageState {
        bool doneOnce = false;   // Spanne "Kampf": schon gefeuert?
        int lastFiredTurn = -1;  // Spanne "Runde": letzte Runde, in der gefeuert wurde
        bool momentLatch = false; // Spanne "Moment": neurfeuern erst nach Nicht-Erfuellung
    };
    std::vector<TroopPage> mPages;
    std::vector<PageState> mPageStates;
    bool mPageWaiting = false;  // Kampf pausiert bis das Seiten-CE fertig ist
    int mPageRuntimeId = 0;     // positive Laufzeit-ID des laufenden Seiten-Interpreters
    bool mCanEscape = true;
    bool mCanLose = false;
    int mLastExp = 0;
    int mLastGold = 0;
    int mLastOutcome = 0; // 0=keiner, 1=Sieg, 2=Flucht, 3=Niederlage
};

} // namespace rpg
