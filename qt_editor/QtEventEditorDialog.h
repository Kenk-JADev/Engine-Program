#pragma once
// XP-artiger Event-Dialog ("Event - ID:001"):
// Seiten mit Bedingungen, Grafik, Autonomer Bewegung, Optionen, Trigger
// und der Befehlsliste (@>-Stil, Einrückung nach Verschachtelung).

#include <QDialog>
#include <vector>

#include "rpgmaker3d/EventSystem.h"

class QTabWidget;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

namespace qt_editor {

/// Kleiner Editor für eine Bewegungsroute (UDLR-Text + Wiederholen/Überspringen).
/// Liefert false bei Abbruch.
bool EditMoveRoute(QWidget* parent, std::string& routeText, bool& repeat, bool& skippable);

class QtEventEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit QtEventEditorDialog(const rpg::MapEvent& ev, QWidget* parent = nullptr);

    /// Ergebnis-Event (inkl. aktuell angezeigter Seite)
    rpg::MapEvent mapEvent() const;

    /// Modal bearbeiten; true = übernommen (ev wurde ersetzt)
    static bool EditEvent(QWidget* parent, rpg::MapEvent& ev);

private slots:
    void onPageTabChanged(int index);
    void onNewPage();
    void onCopyPage();
    void onPastePage();
    void onDeletePage();
    void onClearPage();

    void onAddCommand();
    void onEditCommandItem(QTreeWidgetItem* item, int column);
    void onEditCommand();
    void onRemoveCommand();
    void onCommandUp();
    void onCommandDown();
    void onEditMoveRoute();

    void onApply();
    void onOk();

private:
    void buildUi();
    void savePageWidgets();       // Widgets -> aktuelle Seite (mEvent.pages[mPage])
    void loadPageWidgets();       // aktuelle Seite -> Widgets
    void rebuildPageTabs();
    void rebuildCommandList();
    int currentCommandIndex() const;
    int insertIndent() const;     // sinnvoller Einzug für neue Befehle
    void replaceCommandBlock(int index, const std::vector<rpg::EventCommand>& block,
                             int removeCount);
    void reconcileChoices(int headerIndex, const rpg::EventCommand& edited);
    void reconcileBranch(int headerIndex, const rpg::EventCommand& edited);
    rpg::EventPage& page(int i) { return mEvent.pages[(size_t)i]; }

    rpg::MapEvent mEvent;
    int mPage = 0;
    bool mHasClipboard = false;
    rpg::EventPage mClipboard;
    bool mSyncing = false;

    // Kopfzeile
    QLineEdit* mNameEdit = nullptr;
    QTabWidget* mPageTabs = nullptr;

    // Bedingungen
    QCheckBox* mSwitch1Check = nullptr;
    QComboBox* mSwitch1Combo = nullptr;
    QCheckBox* mSwitch2Check = nullptr;
    QComboBox* mSwitch2Combo = nullptr;
    QCheckBox* mVarCheck = nullptr;
    QComboBox* mVarCombo = nullptr;
    QSpinBox* mVarValue = nullptr;
    QCheckBox* mSelfCheck = nullptr;
    QComboBox* mSelfCombo = nullptr;

    // Grafik
    QLineEdit* mGraphicEdit = nullptr;
    QSpinBox* mGraphicIndex = nullptr;

    // Autonome Bewegung
    QComboBox* mMoveType = nullptr;
    QComboBox* mMoveSpeed = nullptr;
    QComboBox* mMoveFreq = nullptr;

    // Optionen
    QCheckBox* mWalkAnime = nullptr;
    QCheckBox* mStepAnime = nullptr;
    QCheckBox* mDirFix = nullptr;
    QCheckBox* mThrough = nullptr;
    QCheckBox* mAlwaysTop = nullptr;

    // Trigger
    QComboBox* mTrigger = nullptr;

    // Befehlsliste
    QTreeWidget* mCommandTree = nullptr;
    QLabel* mHintLabel = nullptr;
};

} // namespace qt_editor
