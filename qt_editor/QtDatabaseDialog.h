#pragma once
// XP-artiger Datenbank-Dialog (Tabs: Actors..System).
// Links Liste + "Maximum ändern...", rechts Formularfelder,
// unten OK / Abbrechen / Anwenden. Arbeitet auf Kopien der
// Datenbank-Vektoren; OK/Anwenden schreibt zurück und speichert.

#include <QDialog>
#include <functional>
#include <vector>
#include <memory>

#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/EventSystem.h"

class QTabWidget;
class QListWidget;
class QDialogButtonBox;

namespace rpg { class Engine; }

namespace qt_editor {

class QtDatabaseDialog : public QDialog {
    Q_OBJECT
public:
    QtDatabaseDialog(rpg::Engine* engine, QWidget* parent = nullptr);

    /// Modal öffnen; true = Änderungen wurden übernommen
    static bool EditDatabase(QWidget* parent, rpg::Engine* engine);

signals:
    /// Trupps-Tab: „Kampftest" - Editor startet den Player mit --battletest
    void battleTestRequested(int troopId);

private slots:
    void onApply();
    void onOk();

private:
    // ------- generisches Listen-Tab (Liste + Maximum + Formular) ------------
    struct ListTab {
        QWidget* page = nullptr;
        QListWidget* list = nullptr;
        std::function<int()> count;
        std::function<QString(int)> nameAt;
        std::function<void(int)> setMax;         // Maximum ändern
        std::function<void(int)> loadForm;       // Daten -> Widgets
        std::function<void(int)> storeForm;      // Widgets -> Daten (vor Wechsel)
        int current = -1;
        int loading = 0;
    };
    ListTab& addListTab(const QString& title);
    int listTabCount() const { return (int)mListTabs.size(); }
    ListTab& listTab(int i) { return *mListTabs[(size_t)i]; }
    void rebuildList(ListTab& tab, int select = 0);
    void loadCurrent(ListTab& tab);
    void storeCurrent(ListTab& tab);
    void storeAll();

    // Tab-Bauer
    void buildActorsTab();
    void buildClassesTab();
    void buildSkillsTab();
    void buildItemsTab();
    void buildWeaponsTab();
    void buildArmorsTab();
    void buildEnemiesTab();
    void buildTroopsTab();
    void buildStatesTab();
    void buildAnimationsTab();
    void buildTilesetsTab();
    void buildCommonEventsTab();
    void buildSystemTab();

    rpg::Engine* mEngine = nullptr;

    // Arbeitskopien (werden bei OK/Anwenden nach Database::Get() geschrieben)
    std::vector<rpg::ActorData> mActors;
    std::vector<rpg::ClassData> mClasses;
    std::vector<rpg::ItemData> mItems;
    std::vector<rpg::WeaponData> mWeapons;
    std::vector<rpg::ArmorData> mArmors;
    std::vector<rpg::SkillData> mSkills;
    std::vector<rpg::EnemyData> mEnemies;
    std::vector<rpg::TroopData> mTroops;
    std::vector<rpg::StateData> mStates;
    std::vector<rpg::TilesetData> mTilesets;
    std::vector<rpg::AnimationData> mAnimations; // XP-Animationen (Paket 5)
    rpg::SystemData mSystem;
    std::vector<rpg::CommonEvent> mCEs;

    QTabWidget* mTabs = nullptr;
    QDialogButtonBox* mButtons = nullptr;
    std::vector<std::unique_ptr<ListTab>> mListTabs;
    std::vector<std::function<void()>> mExtraStore; // System-Tab-Writeback

    // System-Tab-Formular (eigenes Widget-Set, da stark abweichend)
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace qt_editor
