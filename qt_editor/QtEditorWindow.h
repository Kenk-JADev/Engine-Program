#pragma once
// Qt-Editor-Hauptfenster: QMainWindow mit nativen Dock-Fenstern.
// Der ImGui-Editor ist entfernt – Qt ist der einzige Editor-Host.
//
// Layout:
//  - Zentral: QTabWidget mit "Game View" (3D) und "Code" (Ruby/C++)
//  - Docks: Hierarchie, Eigenschaften, Konsole
//  - Menues/Toolbars + QTimer-Game-Loop

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

namespace rpg { class Engine; }

namespace qt_editor {

class QtGameViewWidget;
class QtCodeWorkspace;
class QtMapEditorDock;
class QtDatabaseEditorDock;

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
    void onCentralTabChanged(int index);

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
    void buildCentral();
    QWidget* buildPropertiesWidget();
    void log(const QString& msg);

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
    QtCodeWorkspace* mCode = nullptr;
    QtMapEditorDock* mMapDockWidget = nullptr;
    QtDatabaseEditorDock* mDbDockWidget = nullptr;

    // Docks
    QDockWidget* mDockHierarchy = nullptr;
    QDockWidget* mDockProperties = nullptr;
    QDockWidget* mDockConsole = nullptr;
    QDockWidget* mDockMap = nullptr;
    QDockWidget* mDockDatabase = nullptr;
    QTreeWidget* mHierarchy = nullptr;
    QWidget* mPropsWidget = nullptr;
    QPlainTextEdit* mConsole = nullptr;

    // Property-Editoren
    QLineEdit* mNameEdit = nullptr;
    QDoubleSpinBox* mPos[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* mRot[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* mScale[3] = {nullptr, nullptr, nullptr};

    QLabel* mStatusInfo = nullptr;

    // Menue-Aktionen
    QAction* mUndoAction = nullptr;
    QAction* mRedoAction = nullptr;
    QAction* mDeleteAction = nullptr;
    QAction* mSaveAction = nullptr;
    QAction* mPlayAction = nullptr;
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
