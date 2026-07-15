#pragma once
// Qt-Editor-Hauptfenster: QMainWindow mit eigenen, nativen Dock-Fenstern
// (das, was mit ImGui nicht moeglich ist), eingebetteter Game-View (Qt GL),
// Menues/Toolbars/Statuszeile und dem Qt-getriebenen Game-Loop (QTimer).
//
// Erster Migrationsslice aus dem ImGui-Editor (Editor.cpp):
//  - Datei-Menue komplett (Projekt neu/oeffnen/speichern, Szene laden/speichern)
//  - Erstellen-Menue (Wuerfel/Ebene/Licht) via CommandHistory (undobar)
//  - Bearbeiten-Menue (Rueckgaengig/Wiederholen/Loeschen) via CommandHistory
//  - Hierarchie-Dock: live Liste der Scene-Entities, Klick = Selektion
//  - Eigenschaften-Dock: Name + Transform des selektierten Objekts editieren

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

namespace rpg { class Engine; }

namespace qt_editor {

class QtGameViewWidget;
class QtTilesetPanel;

class QtEditorWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit QtEditorWindow(QWidget* parent = nullptr);
    ~QtEditorWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTick();               // Game-Loop (QTimer, ~60 Hz)
    void onUiTick();             // UI-Sync (500 ms): Hierarchie/Properties/Menues
    void onPlaytestToggled(bool on);
    void onAboutToQuit();        // sauberes Engine-Shutdown mit GL-Kontext

    // Datei
    void actionNewProject();
    void actionOpenProject();
    void actionSaveProject();
    void actionSaveSceneAs();
    void actionLoadSceneFrom();

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
    void buildToolbar();
    QWidget* buildPropertiesWidget();
    void log(const QString& msg);

    // Engine-Aktionen (Spiegel der ImGui-Editor-Logik)
    void loadScenePackage();     // Editor::LoadMap-Aequivalent
    void saveScenePackage();     // Editor::SaveMap-Aequivalent
    void loadTilesetForCurrentProject(); // Editor::LoadTilesetForMap-Aequivalent
    void createSimpleEntity(int kind); // 0=Cube 1=Plane 2=Light
    void onGroundClicked(float wx, float wz); // Tile-Malen/Radieren (SetTileCommand)
    void onGroundContextMenu(int gx, int gy, float wx, float wz); // Rechtsklick-Menue
    bool worldToTile(float wx, float wz, int& outX, int& outZ) const; // gemeinsame Umrechnung
    void createEventAt(int x, int z, bool asNPC); // Port von Editor::CreateEventAt

    // Selektion & UI-Sync
    void afterProjectChanged();
    void refreshHierarchy();
    void onHierarchySelectionChanged();
    void rebuildProperties();
    void syncPropertyValues();
    void setSelectedEntity(int id);
    bool selectedEntityExists() const;

    std::unique_ptr<rpg::Engine> mEngine;
    QtGameViewWidget* mView = nullptr;

    // Docks
    QDockWidget* mDockHierarchy = nullptr;
    QDockWidget* mDockTileset = nullptr;
    QDockWidget* mDockProperties = nullptr;
    QDockWidget* mDockConsole = nullptr;
    QTreeWidget* mHierarchy = nullptr;
    QtTilesetPanel* mTilesetPanel = nullptr;
    QWidget* mPropsWidget = nullptr;
    QPlainTextEdit* mConsole = nullptr;

    // Property-Editoren (werden bei rebuildProperties neu gebaut)
    QLineEdit* mNameEdit = nullptr;
    QDoubleSpinBox* mPos[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* mRot[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* mScale[3] = {nullptr, nullptr, nullptr};

    QLabel* mStatusInfo = nullptr;

    // Menue-Aktionen (Text/Enable-Sync via Timer)
    QAction* mUndoAction = nullptr;
    QAction* mRedoAction = nullptr;
    QAction* mDeleteAction = nullptr;
    QAction* mSaveAction = nullptr;
    QAction* mPlayAction = nullptr;

    QTimer* mTimer = nullptr;       // Game-Loop
    QTimer* mUiTimer = nullptr;     // UI-Sync
    QElapsedTimer* mClock = nullptr;
    float mFpsAccum = 0.0f;
    int mFpsFrames = 0;

    int mSelectedEntity = -1;
    std::vector<int> mLastHierarchyIds; // Aenderungserkennung fuer refresh
    bool mSyncingProps = false;         // Reentry-Schutz beim Werte-Sync
};

} // namespace qt_editor
