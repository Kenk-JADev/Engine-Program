#pragma once
// RPG Maker 3D - Einfaches rundenbasiertes Kampfsystem

#include <vector>
#include <functional>
#include <string>
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
    Vec3 position{0,0,0};
    std::string name;

    void ApplyDamage(int dmg);
    void Recover(int hp, int mp);
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
