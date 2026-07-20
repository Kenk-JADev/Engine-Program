#pragma once
// Qt-Editor-Hauptfenster: QMainWindow mit nativen Dock-Fenstern.
//
// Oberfläche (Engine-Design, dunkel):
//  - Menüleiste + Schnellzugriffs-Toolbar + Ribbon mit Tabs
//    (Datei / Werkzeuge / Ansicht / Fenster / Debug)
//  - Zentral: QTabWidget mit Tabs UNTEN (Browser-Stil):
//    "Spielansicht" (3D), "Landkarte" (2D), "Spiel" (Playtest), "Skript" (Code)
//  - Docks: Hierarchie, Eigenschaften, Konsole, Map, Database, Events, Assets
//  - Playtest startet die Player-exe mit dem Projektordner als Argument.

#include <QMainWindow>
#include <memory>
#include <vector>

class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QTimer;
class QElapsedTimer;
class QTreeWidget;
class QLineEdit;
class QDoubleSpinBox;
class QAction;
class QWidget;
class QTabWidget;
class QCheckBox;
class QPushButton;

namespace rpg { class Engine; }

namespace qt_editor {

class QtGameViewWidget;
class QtCodeWorkspace;
class QtMapTab;
class QtMapEditorDock;
class QtDatabaseEditorDock;
class QtEventEditorDock;
class QtAssetBrowserDock;

class QtEditorWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit QtEditorWindow(QWidget* parent = nullptr);
    ~QtEditorWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTick();               // Game-Loop (QTimer, ~60 Hz)
    void onUiTick();             // UI-Sync (500 ms): Hierarchie/Properties/Menüs
    void onPlaytestToggled(bool on);
    void onAboutToQuit();        // sauberes Engine-Shutdown mit GL-Kontext
    void onCentralTabChanged(int index);

    // Datei
    void actionNewProject();
    void actionOpenProject();
    void actionSaveProject();
    void actionSaveSceneAs();
    void actionLoadSceneFrom();
    /// Projekt aus einem konkreten Pfad öffnen (Dialog, Zuletzt-Liste, Willkommen)
    void openProjectPath(const QString& path);

    // Playtest
    void actionPlaytestPlayer();     // externe Player-exe (F5)

    // Erstellen / Bearbeiten
    void actionCreateCube();
    void actionCreatePlane();
    void actionCreateLight();
    void actionUndo();
    void actionRedo();
    void deleteSelected();

private:
    void buildMenus();
    void buildDocks();
    void buildRibbon();
    void buildCentral();
    QWidget* buildQuickAccessBar();
    QWidget* buildPlayTab();
    void addRibbonPage(const QString& title);
    QWidget* ribbonPage(const QString& title);
    void ribbonButton(QWidget* page, const QString& text, const QString& tip,
                      std::function<void()> fn, bool checkable = false, bool checked = false);
    QString findPlayerExecutable() const;
    QWidget* buildPropertiesWidget();
    void log(const QString& msg);

    // Easy-to-use: Zuletzt geöffnete Projekte, Willkommens-Dialog, Hilfe
    void addRecentProject(const QString& path);
    void rebuildRecentProjectsMenu();
    void showWelcomeDialog();
    void showShortcutsDialog();
    /// Statuszeile: aktive Karte (Name, ID, Größe) anzeigen
    void updateMapStatus();
    /// Spiel-Tab: Projekt-Übersicht/Statusanzeige aktualisieren
    void updatePlayTabInfo();
    /// Fragt vor dem Playtest, ob gespeichert werden soll (mit Merk-Option).
    /// true = fortfahren, false = abgebrochen
    bool confirmPlaytestSave();
    /// Skripte + Szene speichern (für den Playtest, Player liest von Disk)
    void saveAllForPlaytest();
    /// Spiel-Tab: alle Ruby-Skripte ohne Start prüfen, Ergebnis anzeigen
    void runScriptCheck();

    // Engine-Aktionen
    void loadScenePackage();
    void saveScenePackage();
    void createSimpleEntity(int kind); // 0=Cube 1=Plane 2=Light

    // Selektion & UI-Sync
    void afterProjectChanged();
    void refreshHierarchy();
    void onHierarchySelectionChanged();
    void rebuildProperties();
    void syncPropertyValues();
    void setSelectedEntity(int id);
    bool selectedEntityExists() const;

    std::unique_ptr<rpg::Engine> mEngine;
    QTabWidget* mCentralTabs = nullptr;
    QtGameViewWidget* mView = nullptr;
    QtMapTab* mMapTab = nullptr;
    QWidget* mPlayTab = nullptr;
    QtCodeWorkspace* mCode = nullptr;
    QTabWidget* mRibbonTabs = nullptr;

    QtMapEditorDock* mMapDockWidget = nullptr;
    QtDatabaseEditorDock* mDbDockWidget = nullptr;
    QtEventEditorDock* mEventDockWidget = nullptr;
    QtAssetBrowserDock* mAssetDockWidget = nullptr;

    // Docks
    QDockWidget* mDockHierarchy = nullptr;
    QDockWidget* mDockProperties = nullptr;
    QDockWidget* mDockConsole = nullptr;
    QDockWidget* mDockMap = nullptr;
    QDockWidget* mDockDatabase = nullptr;
    QDockWidget* mDockEvents = nullptr;
    QDockWidget* mDockAssets = nullptr;
    QTreeWidget* mHierarchy = nullptr;
    QWidget* mPropsWidget = nullptr;
    QPlainTextEdit* mConsole = nullptr;

    // Property-Editoren
    QLineEdit* mNameEdit = nullptr;
    QDoubleSpinBox* mPos[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* mRot[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* mScale[3] = {nullptr, nullptr, nullptr};

    QLabel* mStatusInfo = nullptr;
    QLabel* mStatusMap = nullptr;   // permanente Statuszeile: aktive Karte
    QLabel* mStatusTile = nullptr;  // permanente Statuszeile: Maus-Feld (Landkarte)
    QLabel* mPlayTabStatus = nullptr;
    class QMenu* mRecentMenu = nullptr;
    // Spiel-Tab (Playtest-Übersicht)
    QLabel* mPlayTabInfo = nullptr;
    QLabel* mPlayTabExeStatus = nullptr;
    QPushButton* mPlayTabEmbeddedBtn = nullptr;
    QCheckBox* mAutoSaveCheck = nullptr;
    QPlainTextEdit* mScriptCheckOutput = nullptr;

    // Menü-Aktionen
    QAction* mUndoAction = nullptr;
    QAction* mRedoAction = nullptr;
    QAction* mDeleteAction = nullptr;
    QAction* mSaveAction = nullptr;
    QAction* mPlayAction = nullptr;       // eingebetteter Playtest (Shift+F5)
    QAction* mPlayPlayerAction = nullptr; // externe Player-exe (F5)
    QAction* mShowGameViewAction = nullptr;
    QAction* mShowCodeAction = nullptr;

    QTimer* mTimer = nullptr;
    QTimer* mUiTimer = nullptr;
    QElapsedTimer* mClock = nullptr;
    float mFpsAccum = 0.0f;
    int mFpsFrames = 0;

    int mSelectedEntity = -1;
    std::vector<int> mLastHierarchyIds;
    bool mSyncingProps = false;
};

} // namespace qt_editor
