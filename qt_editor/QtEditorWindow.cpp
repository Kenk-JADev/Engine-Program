#include "QtEditorWindow.h"
#include "QtGameViewWidget.h"
#include "QtCodeWorkspace.h"
#include "QtMapEditorDock.h"
#include "QtDatabaseEditorDock.h"
#include "QtEventEditorDock.h"
#include "QtAssetBrowserDock.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/ParticleSystem.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Command.h"
#include "rpgmaker3d/CommandHistory.h"
#include "rpgmaker3d/ScriptManager.h"
#include "rpgmaker3d/EventSystem.h"
#ifdef RPGMAKER3D_ENABLE_RMLUI
#include "rpgmaker3d/RmlUiSystem.h"
#endif

#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace qt_editor {

namespace {
constexpr int kIdRole = Qt::UserRole;

QDoubleSpinBox* makeSpin(double min, double max, double step, double value) {
    auto* s = new QDoubleSpinBox();
    s->setRange(min, max);
    s->setDecimals(3);
    s->setSingleStep(step);
    s->setValue(value);
    return s;
}
} // namespace

// ---------------------------------------------------------------------------

QtEditorWindow::QtEditorWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("RPG Maker 3D - Qt Editor");
    resize(1680, 980);

    mEngine = std::make_unique<rpg::Engine>();

    buildCentral();
    buildDocks();
    buildMenus();
    buildToolbar();

    mStatusInfo = new QLabel("Starte...", this);
    statusBar()->addPermanentWidget(mStatusInfo);
    statusBar()->showMessage("Engine startet (GL-Kontext wird initialisiert)...");

    connect(mView, &QtGameViewWidget::engineReady, this, [this]() {
        log("Engine initialisiert (Embedded-Modus, Qt GL-Kontext).");
        log("ImGui-Editor entfernt – Qt ist der einzige Editor-Host.");
        log("Tabs: Game View (3D) | Code (Ruby / C++ API).");
        statusBar()->showMessage("Bereit. Code-Tab fuer Ruby/C++, Game View fuer 3D. F9 = RmlUi-HUD.");
        setSelectedEntity(-1);
        if (mCode) mCode->refresh();
        if (mMapDockWidget) {
            mMapDockWidget->refresh();
            mView->setPaintMode(mMapDockWidget->paintEnabled());
            mView->setPaintTile(mMapDockWidget->selectedTile());
            mView->setPaintLayer(mMapDockWidget->selectedLayer());
        }
        if (mDbDockWidget) mDbDockWidget->refresh();
    });
    connect(mView, &QtGameViewWidget::entityPicked, this, [this](int id) {
        setSelectedEntity(id);
    });
    connect(mView, &QtGameViewWidget::engineInitFailed, this, [this](QString msg) {
        log("FEHLER: " + msg);
        QMessageBox::critical(this, "Engine-Start fehlgeschlagen", msg);
    });
    connect(qApp, &QApplication::aboutToQuit, this, &QtEditorWindow::onAboutToQuit);

    mClock = new QElapsedTimer();
    mClock->start();
    mTimer = new QTimer(this);
    mTimer->setInterval(16);
    connect(mTimer, &QTimer::timeout, this, &QtEditorWindow::onTick);
    mTimer->start();

    mUiTimer = new QTimer(this);
    mUiTimer->setInterval(500);
    connect(mUiTimer, &QTimer::timeout, this, &QtEditorWindow::onUiTick);
    mUiTimer->start();
}

QtEditorWindow::~QtEditorWindow() = default;

// ---------------------------------------------------------------------------
// Zentral: Game View + Code Workspace (ersetzt ImGui Game Scene)
// ---------------------------------------------------------------------------

void QtEditorWindow::buildCentral() {
    mCentralTabs = new QTabWidget(this);
    mCentralTabs->setDocumentMode(true);
    mCentralTabs->setTabPosition(QTabWidget::North);
    mCentralTabs->setMovable(false);

    mView = new QtGameViewWidget(mEngine.get(), mCentralTabs);
    mCode = new QtCodeWorkspace(mEngine.get(), mCentralTabs);

    mCentralTabs->addTab(mView, "Game View");
    mCentralTabs->addTab(mCode, "Code (Ruby / C++)");
    setCentralWidget(mCentralTabs);

    connect(mCentralTabs, &QTabWidget::currentChanged,
            this, &QtEditorWindow::onCentralTabChanged);
    connect(mCode, &QtCodeWorkspace::logMessage, this, [this](const QString& m) {
        log(m);
    });
}

void QtEditorWindow::onCentralTabChanged(int index) {
    if (index == 0 && mView) {
        mView->setFocus(Qt::OtherFocusReason);
        statusBar()->showMessage("Game View – WASD/Maus navigieren, Linksklick = Selektion, F5/Play = Playtest.");
    } else if (index == 1 && mCode) {
        mCode->setFocus(Qt::OtherFocusReason);
        mCode->refresh();
        statusBar()->showMessage("Code Workspace – Ruby-Spiellogik editieren, C++ Engine-API als Referenz.");
    }
}

// ---------------------------------------------------------------------------
// Menues / Toolbar
// ---------------------------------------------------------------------------

void QtEditorWindow::buildMenus() {
    QMenu* mFile = menuBar()->addMenu("&Datei");
    QAction* aNew = mFile->addAction("&Neues Projekt...", this, [this]() { actionNewProject(); });
    aNew->setShortcut(QKeySequence("Ctrl+N"));
    QAction* aOpen = mFile->addAction("Projekt &oeffnen...", this, [this]() { actionOpenProject(); });
    aOpen->setShortcut(QKeySequence("Ctrl+O"));
    mSaveAction = mFile->addAction("&Speichern", this, [this]() { actionSaveProject(); });
    mSaveAction->setShortcut(QKeySequence("Ctrl+S"));
    mFile->addSeparator();
    mFile->addAction("Szene laden...", this, [this]() { actionLoadSceneFrom(); });
    mFile->addAction("Szene speichern unter...", this, [this]() { actionSaveSceneAs(); });
    mFile->addSeparator();
    mFile->addAction("Scripts speichern", this, [this]() {
        if (mCode) mCode->saveAll();
    });
    mFile->addSeparator();
    mFile->addAction("&Beenden", this, &QWidget::close);

    QMenu* mEdit = menuBar()->addMenu("&Bearbeiten");
    mUndoAction = mEdit->addAction("Rueckgaengig", this, [this]() { actionUndo(); });
    mRedoAction = mEdit->addAction("Wiederholen", this, [this]() { actionRedo(); });
    mEdit->addSeparator();
    mDeleteAction = mEdit->addAction("Ausgewaehltes loeschen", this, [this]() { deleteSelected(); });
    mUndoAction->setEnabled(false);
    mRedoAction->setEnabled(false);
    mDeleteAction->setEnabled(false);

    QMenu* mCreate = menuBar()->addMenu("&Erstellen");
    mCreate->addAction("Wuerfel", this, [this]() { actionCreateCube(); });
    mCreate->addAction("Ebene", this, [this]() { actionCreatePlane(); });
    mCreate->addAction("Licht", this, [this]() { actionCreateLight(); });

    QMenu* mCodeMenu = menuBar()->addMenu("&Code");
    mCodeMenu->addAction("Code-Workspace oeffnen", this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mCode);
    });
    mCodeMenu->addAction("Ruby-Scripts speichern", this, [this]() {
        if (mCode) mCode->saveAll();
    });
    mCodeMenu->addAction("Ruby ausfuehren (aktuell)", this, [this]() {
        if (mCode) mCode->runCurrent();
    });
    mCodeMenu->addAction("Alle Ruby-Scripts ausfuehren", this, [this]() {
        if (mCode) mCode->runAll();
    });
    mCodeMenu->addSeparator();
    mCodeMenu->addAction("Zu Ruby wechseln", this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mCode);
        if (mCode) mCode->setLanguage(0);
    });
    mCodeMenu->addAction("Zu C++ Referenz wechseln", this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mCode);
        if (mCode) mCode->setLanguage(1);
    });

    QMenu* mViewMenu = menuBar()->addMenu("&Ansicht");
    mShowGameViewAction = mViewMenu->addAction("Game View", this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mView);
    });
    mShowCodeAction = mViewMenu->addAction("Code Workspace", this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mCode);
    });
    mViewMenu->addSeparator();
    mViewMenu->addAction(mDockHierarchy->toggleViewAction());
    mViewMenu->addAction(mDockProperties->toggleViewAction());
    if (mDockMap) mViewMenu->addAction(mDockMap->toggleViewAction());
    if (mDockDatabase) mViewMenu->addAction(mDockDatabase->toggleViewAction());
    if (mDockEvents) mViewMenu->addAction(mDockEvents->toggleViewAction());
    if (mDockAssets) mViewMenu->addAction(mDockAssets->toggleViewAction());
    mViewMenu->addAction(mDockConsole->toggleViewAction());
#ifdef RPGMAKER3D_ENABLE_RMLUI
    mViewMenu->addSeparator();
    mViewMenu->addAction("RmlUi HUD umschalten (F9)", this, [this]() {
        if (mEngine && mEngine->GetRmlUi()) mEngine->GetRmlUi()->ToggleVisible();
    });
#endif

    QMenu* mPlay = menuBar()->addMenu("&Playtest");
    mPlayAction = mPlay->addAction("Playtest starten/stoppen");
    mPlayAction->setCheckable(true);
    mPlayAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(mPlayAction, &QAction::toggled, this, &QtEditorWindow::onPlaytestToggled);

    QMenu* mHelp = menuBar()->addMenu("&Hilfe");
    mHelp->addAction("Ueber", this, [this]() {
        QMessageBox::about(this, "RPG Maker 3D Qt Editor",
            "Qt-basierter Editor (ImGui entfernt):\n"
            "- Docks: Hierarchie, Map, Database, Events, Code, Konsole\n"
            "- Game View (QOpenGLWidget) + Tile-Malen\n"
            "- Code Workspace: Ruby/C++ mit Syntax-Highlighting\n"
            "- RmlUi-Input-Bruecke im Game View (F9)\n"
            "- Undo/Redo ueber CommandHistory");
    });

    auto* delView = new QShortcut(QKeySequence(Qt::Key_Delete), mView);
    delView->setContext(Qt::WidgetShortcut);
    connect(delView, &QShortcut::activated, this, [this]() { deleteSelected(); });
    auto* delTree = new QShortcut(QKeySequence(Qt::Key_Delete), mHierarchy);
    delTree->setContext(Qt::WidgetShortcut);
    connect(delTree, &QShortcut::activated, this, [this]() { deleteSelected(); });
    auto* undoView = new QShortcut(QKeySequence("Ctrl+Z"), mView);
    undoView->setContext(Qt::WidgetShortcut);
    connect(undoView, &QShortcut::activated, this, [this]() { actionUndo(); });
    auto* undoTree = new QShortcut(QKeySequence("Ctrl+Z"), mHierarchy);
    undoTree->setContext(Qt::WidgetShortcut);
    connect(undoTree, &QShortcut::activated, this, [this]() { actionUndo(); });
    auto* redoView = new QShortcut(QKeySequence("Ctrl+Y"), mView);
    redoView->setContext(Qt::WidgetShortcut);
    connect(redoView, &QShortcut::activated, this, [this]() { actionRedo(); });
    auto* redoTree = new QShortcut(QKeySequence("Ctrl+Y"), mHierarchy);
    redoTree->setContext(Qt::WidgetShortcut);
    connect(redoTree, &QShortcut::activated, this, [this]() { actionRedo(); });
}

void QtEditorWindow::buildDocks() {
    mDockHierarchy = new QDockWidget("Hierarchie", this);
    mHierarchy = new QTreeWidget(mDockHierarchy);
    mHierarchy->setHeaderLabels(QStringList() << "Objekt" << "ID");
    mHierarchy->setRootIsDecorated(false);
    mHierarchy->setAlternatingRowColors(true);
    mDockHierarchy->setWidget(mHierarchy);
    addDockWidget(Qt::LeftDockWidgetArea, mDockHierarchy);
    connect(mHierarchy, &QTreeWidget::itemSelectionChanged,
            this, [this]() { onHierarchySelectionChanged(); });

    mDockProperties = new QDockWidget("Eigenschaften", this);
    mDockProperties->setWidget(new QLabel("Kein Objekt ausgewaehlt.", mDockProperties));
    addDockWidget(Qt::RightDockWidgetArea, mDockProperties);

    mDockConsole = new QDockWidget("Konsole", this);
    mConsole = new QPlainTextEdit(mDockConsole);
    mConsole->setReadOnly(true);
    mConsole->setMaximumBlockCount(2000);
    mDockConsole->setWidget(mConsole);
    addDockWidget(Qt::BottomDockWidgetArea, mDockConsole);

    // Map-Editor Dock
    mDockMap = new QDockWidget("Map-Editor", this);
    mMapDockWidget = new QtMapEditorDock(mEngine.get(), mDockMap);
    mDockMap->setWidget(mMapDockWidget);
    addDockWidget(Qt::RightDockWidgetArea, mDockMap);
    tabifyDockWidget(mDockProperties, mDockMap);

    // Database Dock
    mDockDatabase = new QDockWidget("Database", this);
    mDbDockWidget = new QtDatabaseEditorDock(mEngine.get(), mDockDatabase);
    mDockDatabase->setWidget(mDbDockWidget);
    addDockWidget(Qt::RightDockWidgetArea, mDockDatabase);
    tabifyDockWidget(mDockMap, mDockDatabase);

    mDockEvents = new QDockWidget("Events", this);
    mEventDockWidget = new QtEventEditorDock(mEngine.get(), mDockEvents);
    mDockEvents->setWidget(mEventDockWidget);
    addDockWidget(Qt::RightDockWidgetArea, mDockEvents);
    tabifyDockWidget(mDockDatabase, mDockEvents);

    mDockAssets = new QDockWidget("Assets", this);
    mAssetDockWidget = new QtAssetBrowserDock(mEngine.get(), mDockAssets);
    mDockAssets->setWidget(mAssetDockWidget);
    addDockWidget(Qt::LeftDockWidgetArea, mDockAssets);
    tabifyDockWidget(mDockHierarchy, mDockAssets);

    mDockProperties->raise();

    connect(mMapDockWidget, &QtMapEditorDock::logMessage, this, [this](const QString& m) { log(m); });
    connect(mDbDockWidget, &QtDatabaseEditorDock::logMessage, this, [this](const QString& m) { log(m); });
    connect(mEventDockWidget, &QtEventEditorDock::logMessage, this, [this](const QString& m) { log(m); });
    connect(mAssetDockWidget, &QtAssetBrowserDock::logMessage, this, [this](const QString& m) { log(m); });
    connect(mEventDockWidget, &QtEventEditorDock::eventsChanged, this, [this]() {
        // nothing heavy – hierarchy is entities, not events
    });
    connect(mMapDockWidget, &QtMapEditorDock::paintStateChanged, this, [this]() {
        if (!mView || !mMapDockWidget) return;
        mView->setPaintMode(mMapDockWidget->paintEnabled());
        mView->setPaintTile(mMapDockWidget->selectedTile());
        mView->setPaintLayer(mMapDockWidget->selectedLayer());
        mView->setBrushMode(mMapDockWidget->brushMode());
    });
    connect(mMapDockWidget, &QtMapEditorDock::mapLoaded, this, [this]() {
        setSelectedEntity(-1);
        refreshHierarchy();
    });
    connect(mView, &QtGameViewWidget::tilePainted, this, [this](int x, int z, int tile) {
        // sparsam loggen: nur gelegentlich
        static int n = 0;
        if ((++n % 8) == 0)
            log(QString("Tile (%1,%2) = %3").arg(x).arg(z).arg(tile));
    });

    log("Qt-Editor gestartet (ohne ImGui).");
    log("  Tab 'Game View'  = 3D-Szene / Playtest / Tile-Malen");
    log("  Tab 'Code'       = Ruby-Scripts + C++ Engine-API (Syntax-HL)");
    log("  Docks: Hierarchie | Eigenschaften | Map | Database | Events | Konsole");
    log("  F9 = RmlUi HUD umschalten (Input im Game View aktiv)");
    log("Datei -> Projekt oeffnen... um loszulegen.");
}

void QtEditorWindow::buildToolbar() {
    QToolBar* tb = addToolBar("Editor");
    tb->setMovable(true);
    const auto addBtn = [this, tb](const char* text, void (QtEditorWindow::*slot)()) {
        QAction* a = tb->addAction(text);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    addBtn("Wuerfel", &QtEditorWindow::actionCreateCube);
    addBtn("Ebene", &QtEditorWindow::actionCreatePlane);
    addBtn("Licht", &QtEditorWindow::actionCreateLight);
    tb->addSeparator();
    addBtn("Speichern", &QtEditorWindow::actionSaveProject);
    tb->addSeparator();

    QAction* codeTab = tb->addAction("Code");
    connect(codeTab, &QAction::triggered, this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mCode);
    });
    QAction* gameTab = tb->addAction("Game View");
    connect(gameTab, &QAction::triggered, this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mView);
    });
    QAction* gizmo = tb->addAction("Gizmo");
    gizmo->setCheckable(true);
    gizmo->setChecked(true);
    gizmo->setToolTip("Translate-Gizmo an selektiertem Objekt (X/Y/Z Achsen ziehen)");
    connect(gizmo, &QAction::toggled, this, [this](bool on) {
        if (mView) mView->setGizmoMode(on ? 1 : 0);
        log(on ? "Gizmo: Translate an" : "Gizmo aus");
    });
    tb->addSeparator();

    QAction* play = tb->addAction("Play");
    play->setCheckable(true);
    connect(play, &QAction::toggled, this, [this](bool on) {
        if (mPlayAction) mPlayAction->setChecked(on);
        onPlaytestToggled(on);
    });
    connect(mPlayAction, &QAction::toggled, this, [play](bool on) {
        play->setChecked(on);
    });
}

void QtEditorWindow::log(const QString& msg) {
    if (mConsole) mConsole->appendPlainText(msg);
}

// ---------------------------------------------------------------------------
// Game-Loop / UI-Sync
// ---------------------------------------------------------------------------

void QtEditorWindow::onTick() {
    if (!mView->IsEngineReady()) return;

    const qint64 ms = mClock->restart();
    float dt = static_cast<float>(ms) / 1000.0f;
    if (dt <= 0.0f) dt = 0.016f;
    if (dt > 0.1f) dt = 0.1f;

    mEngine->Update(dt);
    // Nur repainten wenn Game View sichtbar (spart GPU im Code-Tab)
    if (mCentralTabs && mCentralTabs->currentWidget() == mView) {
        mView->update();
    }

    mFpsAccum += dt;
    mFpsFrames++;
    if (mFpsAccum >= 0.5f) {
        const int fps = static_cast<int>(mFpsFrames / mFpsAccum + 0.5f);
        mFpsAccum = 0.0f;
        mFpsFrames = 0;
        const char* tab = (mCentralTabs && mCentralTabs->currentWidget() == mCode)
            ? "Code" : "Game";
        mStatusInfo->setText(QString("FPS: %1  |  Tab: %2  |  Playtest: %3  |  Projekt: %4")
            .arg(fps)
            .arg(tab)
            .arg(mEngine->IsPlaying() ? "an" : "aus")
            .arg(QString::fromStdString(mEngine->GetProject().GetInfo().name)));
    }
}

void QtEditorWindow::onUiTick() {
    if (!mView->IsEngineReady()) return;

    auto& history = mEngine->GetCommandHistory();
    const bool canUndo = history.CanUndo();
    const bool canRedo = history.CanRedo();
    mUndoAction->setEnabled(canUndo);
    mRedoAction->setEnabled(canRedo);
    mUndoAction->setText(canUndo
        ? QString("Rueckgaengig (%1)  [Strg+Z]").arg(QString::fromStdString(history.GetUndoName()))
        : QString("Rueckgaengig  [Strg+Z]"));
    mRedoAction->setText(canRedo
        ? QString("Wiederholen (%1)  [Strg+Y]").arg(QString::fromStdString(history.GetRedoName()))
        : QString("Wiederholen  [Strg+Y]"));
    mDeleteAction->setEnabled(mSelectedEntity >= 0 && selectedEntityExists());

    if (mSelectedEntity >= 0 && !selectedEntityExists())
        setSelectedEntity(-1);

    refreshHierarchy();
    syncPropertyValues();
}

void QtEditorWindow::onPlaytestToggled(bool on) {
    if (!mView->IsEngineReady()) return;
    if (on && mCode) {
        // Scripts vor Playtest speichern (damit Runtime den aktuellen Stand hat)
        mCode->saveAll();
    }
    mEngine->SetPlaying(on);
    log(on ? "Playtest gestartet." : "Playtest gestoppt.");
    if (on && mCentralTabs) {
        mCentralTabs->setCurrentWidget(mView);
    }
    if (mCode) mCode->refresh(); // read-only waehrend Playtest
}

void QtEditorWindow::closeEvent(QCloseEvent* event) {
    if (mCode && mCode->hasUnsavedChanges()) {
        const auto r = QMessageBox::question(this, "Beenden",
            "Ungespeicherte Script-Aenderungen. Trotzdem beenden?",
            QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    if (mTimer) mTimer->stop();
    if (mUiTimer) mUiTimer->stop();
    event->accept();
}

void QtEditorWindow::onAboutToQuit() {
    if (mTimer) mTimer->stop();
    if (mUiTimer) mUiTimer->stop();
    if (mView) mView->makeCurrent();
    if (mEngine) mEngine->Shutdown();
    if (mView) mView->doneCurrent();
}

// ---------------------------------------------------------------------------
// Datei-Aktionen
// ---------------------------------------------------------------------------

void QtEditorWindow::actionNewProject() {
    if (!mView->IsEngineReady()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Neues Projekt", "Projektname:",
                                               QLineEdit::Normal, "NeuesRPG", &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const QString parent = QFileDialog::getExistingDirectory(
        this, "Basisordner fuer das Projekt waehlen");
    if (parent.isEmpty()) return;

    const QString path = parent + "/" + name.trimmed();
    if (!mEngine->GetProject().New(path.toStdString(), name.trimmed().toStdString())) {
        QMessageBox::warning(this, "Neues Projekt", "Projekt konnte nicht angelegt werden.");
        return;
    }
    mEngine->GetScriptManager().CreateDefaultScripts(path.toStdString());
    mEngine->GetScriptManager().LoadProjectScripts(path.toStdString());
    log("Neues Projekt angelegt: " + path);
    afterProjectChanged();
}

void QtEditorWindow::actionOpenProject() {
    if (!mView->IsEngineReady()) return;
    const QString path = QFileDialog::getExistingDirectory(
        this, "Projekt oeffnen (Projektordner mit project.json)");
    if (path.isEmpty()) return;

    if (!mEngine->GetProject().Load(path.toStdString())) {
        QMessageBox::warning(this, "Projekt oeffnen",
            "Kein gueltiges Projekt (project.json nicht gefunden):\n" + path);
        return;
    }
    rpg::Database::Get().Load(path.toStdString());
    mEngine->GetScriptManager().LoadProjectScripts(path.toStdString());
    if (mEngine->GetScriptManager().GetScripts().empty()) {
        mEngine->GetScriptManager().CreateDefaultScripts(path.toStdString());
        mEngine->GetScriptManager().LoadProjectScripts(path.toStdString());
    }
    log("Projekt geladen: " + path);
    loadScenePackage();
    afterProjectChanged();
}

void QtEditorWindow::actionSaveProject() {
    if (!mView->IsEngineReady()) return;
    if (mEngine->GetProject().GetProjectPath().empty()) {
        QMessageBox::information(this, "Speichern",
            "Kein Projekt geladen - bitte zuerst ein Projekt anlegen oder oeffnen.");
        return;
    }
    if (mCode) mCode->saveAll();
    saveScenePackage();
    log("Projekt gespeichert: " + QString::fromStdString(mEngine->GetProject().GetProjectPath()));
}

void QtEditorWindow::actionSaveSceneAs() {
    if (!mView->IsEngineReady()) return;
    const QString path = QFileDialog::getSaveFileName(
        this, "Szene speichern unter", QString(), "JSON-Dateien (*.json)");
    if (path.isEmpty()) return;
    mEngine->SaveScene(path.toStdString());
    log("Szene gespeichert: " + path);
}

void QtEditorWindow::actionLoadSceneFrom() {
    if (!mView->IsEngineReady()) return;
    const QString path = QFileDialog::getOpenFileName(
        this, "Szene laden", QString(), "JSON-Dateien (*.json)");
    if (path.isEmpty()) return;
    if (mEngine->LoadScene(path.toStdString())) {
        setSelectedEntity(-1);
        log("Szene geladen: " + path);
    } else {
        QMessageBox::warning(this, "Szene laden", "Laden fehlgeschlagen:\n" + path);
    }
}

void QtEditorWindow::loadScenePackage() {
    auto& proj = mEngine->GetProject();
    const std::string scenePath = proj.GetProjectPath() + "/scene.json";
    if (!mEngine->LoadScene(scenePath)) {
        const std::string mapPath = proj.GetMapPath(1);
        if (mEngine->GetMap().Load(mapPath)) {
            log("Map geladen (binaer): " + QString::fromStdString(mapPath));
        } else {
            log("Hinweis: keine scene.json/Map gefunden - leere Szene.");
        }
    } else {
        log("Szene geladen: " + QString::fromStdString(scenePath));
    }
}

void QtEditorWindow::saveScenePackage() {
    auto& proj = mEngine->GetProject();
    const std::string pp = proj.GetProjectPath();
    mEngine->SaveScene(pp + "/scene.json");
    mEngine->GetMap().Save(proj.GetMapPath(1));
    rpg::Database::Get().Save(pp);
    const int mapId = rpg::Database::Get().System().startMapId;
    rpg::EventSystem::Get().SaveMapEvents(mapId > 0 ? mapId : 1, pp);
    proj.Save();
}

void QtEditorWindow::afterProjectChanged() {
    setSelectedEntity(-1);
    setWindowTitle(QString("RPG Maker 3D - Qt Editor  [%1]")
        .arg(QString::fromStdString(mEngine->GetProject().GetInfo().name)));
    if (mCode) mCode->refresh();
    if (mMapDockWidget) mMapDockWidget->refresh();
    if (mDbDockWidget) mDbDockWidget->refresh();
    if (mEventDockWidget) mEventDockWidget->refresh();
    if (mAssetDockWidget) mAssetDockWidget->refresh();
}

// ---------------------------------------------------------------------------
// Erstellen / Bearbeiten
// ---------------------------------------------------------------------------

void QtEditorWindow::createSimpleEntity(int kind) {
    if (!mView->IsEngineReady()) return;
    const char* names[] = {"Cube", "Plane", "Light"};

    auto cmd = std::make_shared<rpg::CreateEntityCommand>(names[kind]);
    mEngine->GetCommandHistory().Execute(*mEngine, cmd);
    const rpg::EntityID id = cmd->GetEntityID();
    if (id == rpg::INVALID_ENTITY) return;

    auto& scene = mEngine->GetScene();
    auto* transform = scene.AddComponent<rpg::TransformComponent>(id);
    if (kind == 0) {
        transform->transform.position = rpg::Vec3(0, 0.5f, 0);
        auto* model = scene.AddComponent<rpg::ModelRendererComponent>(id);
        model->model = std::make_shared<rpg::Model>();
        model->model->AddMesh(rpg::MeshFactory::CreateCube(1.0f));
    } else if (kind == 1) {
        transform->transform.position = rpg::Vec3(0, 0.1f, 0);
        auto* model = scene.AddComponent<rpg::ModelRendererComponent>(id);
        model->model = std::make_shared<rpg::Model>();
        model->model->AddMesh(rpg::MeshFactory::CreatePlane(2.0f));
    } else {
        transform->transform.position = rpg::Vec3(0, 3.0f, 0);
        auto* light = scene.AddComponent<rpg::LightComponent>(id);
        light->color = rpg::Color(1.0f, 1.0f, 0.0f, 1.0f);
        light->intensity = 1.0f;
    }

    log(QString("Erstellt: %1 (ID %2)").arg(names[kind]).arg(static_cast<int>(id)));
    setSelectedEntity(static_cast<int>(id));
    if (mCentralTabs) mCentralTabs->setCurrentWidget(mView);
}

void QtEditorWindow::actionCreateCube()  { createSimpleEntity(0); }
void QtEditorWindow::actionCreatePlane() { createSimpleEntity(1); }
void QtEditorWindow::actionCreateLight() { createSimpleEntity(2); }

void QtEditorWindow::actionUndo() {
    if (!mView->IsEngineReady()) return;
    auto& history = mEngine->GetCommandHistory();
    if (!history.CanUndo()) return;
    history.Undo(*mEngine);
    log(QString("Rueckgaengig: %1").arg(QString::fromStdString(history.GetRedoName())));
    refreshHierarchy();
    rebuildProperties();
}

void QtEditorWindow::actionRedo() {
    if (!mView->IsEngineReady()) return;
    auto& history = mEngine->GetCommandHistory();
    if (!history.CanRedo()) return;
    history.Redo(*mEngine);
    log(QString("Wiederholen: %1").arg(QString::fromStdString(history.GetUndoName())));
    refreshHierarchy();
    rebuildProperties();
}

void QtEditorWindow::deleteSelected() {
    if (!mView->IsEngineReady() || mSelectedEntity < 0) return;
    const rpg::EntityID id = static_cast<rpg::EntityID>(mSelectedEntity);
    auto& scene = mEngine->GetScene();
    auto* transform = scene.GetComponent<rpg::TransformComponent>(id);
    const rpg::Transform t = transform ? transform->transform : rpg::Transform();
    auto cmd = std::make_shared<rpg::DeleteEntityCommand>(id, scene.GetEntityName(id), t);
    mEngine->GetCommandHistory().Execute(*mEngine, cmd);
    log(QString("Geloescht: ID %1").arg(mSelectedEntity));
    setSelectedEntity(-1);
}

// ---------------------------------------------------------------------------
// Hierarchie / Selektion / Eigenschaften
// ---------------------------------------------------------------------------

void QtEditorWindow::refreshHierarchy() {
    if (!mView->IsEngineReady()) return;
    auto& scene = mEngine->GetScene();

    std::vector<int> ids;
    ids.reserve(scene.GetEntities().size());
    for (rpg::EntityID id : scene.GetEntities()) ids.push_back(static_cast<int>(id));

    if (ids == mLastHierarchyIds) return;
    mLastHierarchyIds = ids;

    mHierarchy->blockSignals(true);
    mHierarchy->clear();
    for (int id : ids) {
        auto* item = new QTreeWidgetItem(mHierarchy);
        item->setText(0, QString::fromStdString(scene.GetEntityName(static_cast<rpg::EntityID>(id))));
        item->setText(1, QString::number(id));
        item->setData(0, kIdRole, id);
    }
    if (mSelectedEntity >= 0) {
        for (int i = 0; i < mHierarchy->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = mHierarchy->topLevelItem(i);
            if (item->data(0, kIdRole).toInt() == mSelectedEntity) {
                item->setSelected(true);
                break;
            }
        }
    }
    mHierarchy->blockSignals(false);
}

void QtEditorWindow::onHierarchySelectionChanged() {
    QTreeWidgetItem* item = mHierarchy->currentItem();
    const int id = item ? item->data(0, kIdRole).toInt() : -1;
    if (id != mSelectedEntity)
        setSelectedEntity(id);
}

void QtEditorWindow::setSelectedEntity(int id) {
    mSelectedEntity = id;
    if (mView && mView->IsEngineReady())
        mEngine->SetSelectedEntity(id);
    refreshHierarchy();
    mHierarchy->blockSignals(true);
    for (int i = 0; i < mHierarchy->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = mHierarchy->topLevelItem(i);
        item->setSelected(item->data(0, kIdRole).toInt() == id);
    }
    mHierarchy->blockSignals(false);
    rebuildProperties();
}

bool QtEditorWindow::selectedEntityExists() const {
    if (mSelectedEntity < 0) return false;
    for (int id : mLastHierarchyIds)
        if (id == mSelectedEntity) return true;
    auto& scene = mEngine->GetScene();
    for (rpg::EntityID id : scene.GetEntities())
        if (static_cast<int>(id) == mSelectedEntity) return true;
    return false;
}

QWidget* QtEditorWindow::buildPropertiesWidget() {
    mNameEdit = nullptr;
    for (int i = 0; i < 3; ++i) { mPos[i] = nullptr; mRot[i] = nullptr; mScale[i] = nullptr; }

    auto& scene = mEngine->GetScene();
    const rpg::EntityID id = static_cast<rpg::EntityID>(mSelectedEntity);
    const bool valid = mSelectedEntity >= 0 && selectedEntityExists();

    auto* root = new QWidget();
    auto* layout = new QVBoxLayout(root);

    if (!valid) {
        layout->addWidget(new QLabel(
            "Kein Objekt ausgewaehlt.\n\n"
            "Objekt in der Hierarchie anklicken, oder\n"
            "ueber 'Erstellen' ein neues Objekt anlegen.\n\n"
            "Fuer Ruby/C++-Code: Tab 'Code' oeffnen.", root));
        layout->addStretch(1);
        return root;
    }

    auto* baseBox = new QGroupBox("Basis", root);
    auto* baseForm = new QFormLayout(baseBox);
    mNameEdit = new QLineEdit(QString::fromStdString(scene.GetEntityName(id)), baseBox);
    baseForm->addRow("Name", mNameEdit);
    baseForm->addRow("ID", new QLabel(QString::number(static_cast<int>(id)), baseBox));
    layout->addWidget(baseBox);
    connect(mNameEdit, &QLineEdit::editingFinished, this, [this, id]() {
        if (!selectedEntityExists()) return;
        mEngine->GetScene().SetEntityName(id, mNameEdit->text().toStdString());
        mLastHierarchyIds.clear();
        refreshHierarchy();
        log(QString("Umbenannt: ID %1 -> '%2'").arg(static_cast<int>(id)).arg(mNameEdit->text()));
    });

    auto* tComp = scene.GetComponent<rpg::TransformComponent>(id);
    if (tComp) {
        auto* tBox = new QGroupBox("Transform", root);
        auto* tForm = new QFormLayout(tBox);
        const auto mkRow = [this, tForm, tBox](const char* label, QDoubleSpinBox** dst,
                                               float x, float y, float z,
                                               double min, double max, double step) {
            auto* row = new QWidget(tBox);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            const float v[3] = {x, y, z};
            const char* axisNames[3] = {"x", "y", "z"};
            for (int i = 0; i < 3; ++i) {
                hl->addWidget(new QLabel(axisNames[i], row));
                dst[i] = makeSpin(min, max, step, v[i]);
                hl->addWidget(dst[i]);
            }
            tForm->addRow(label, row);
        };
        mkRow("Position", mPos, tComp->transform.position.x, tComp->transform.position.y,
              tComp->transform.position.z, -100000.0, 100000.0, 0.1);
        mkRow("Rot (rad)", mRot, tComp->transform.rotation.x, tComp->transform.rotation.y,
              tComp->transform.rotation.z, -360.0 * 100.0, 360.0 * 100.0, 0.1);
        mkRow("Skalierung", mScale, tComp->transform.scale.x, tComp->transform.scale.y,
              tComp->transform.scale.z, 0.001, 100000.0, 0.1);
        layout->addWidget(tBox);

        const auto writeBack = [this, id]() {
            if (mSyncingProps || !selectedEntityExists()) return;
            auto* tc = mEngine->GetScene().GetComponent<rpg::TransformComponent>(id);
            if (!tc) return;
            if (mPos[0])   tc->transform.position = rpg::Vec3(
                static_cast<float>(mPos[0]->value()), static_cast<float>(mPos[1]->value()),
                static_cast<float>(mPos[2]->value()));
            if (mRot[0])   tc->transform.rotation = rpg::Vec3(
                static_cast<float>(mRot[0]->value()), static_cast<float>(mRot[1]->value()),
                static_cast<float>(mRot[2]->value()));
            if (mScale[0]) tc->transform.scale = rpg::Vec3(
                static_cast<float>(mScale[0]->value()), static_cast<float>(mScale[1]->value()),
                static_cast<float>(mScale[2]->value()));
        };
        for (int i = 0; i < 3; ++i) {
            for (QDoubleSpinBox* s : {mPos[i], mRot[i], mScale[i]}) {
                if (s) connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                               this, [writeBack](double) { writeBack(); });
            }
        }
    }

    auto* compBox = new QGroupBox("Komponenten", root);
    auto* compLay = new QVBoxLayout(compBox);
    const struct { rpg::ComponentType type; const char* name; } compTypes[] = {
        {rpg::ComponentType::Transform, "Transform"},
        {rpg::ComponentType::ModelRenderer, "Model Renderer"},
        {rpg::ComponentType::Sprite, "Sprite"},
        {rpg::ComponentType::Material, "Material"},
        {rpg::ComponentType::Light, "Licht"},
        {rpg::ComponentType::Camera, "Kamera"},
        {rpg::ComponentType::Script, "Script"},
        {rpg::ComponentType::AudioSource, "Audio Source"},
        {rpg::ComponentType::ParticleEmitter, "Particle Emitter"},
    };
    bool any = false;
    for (const auto& ct : compTypes) {
        bool present = false;
        switch (ct.type) {
            case rpg::ComponentType::Transform:
                present = scene.GetComponent<rpg::TransformComponent>(id) != nullptr; break;
            case rpg::ComponentType::ModelRenderer:
                present = scene.GetComponent<rpg::ModelRendererComponent>(id) != nullptr; break;
            case rpg::ComponentType::Sprite:
                present = scene.GetComponent<rpg::SpriteComponent>(id) != nullptr; break;
            case rpg::ComponentType::Material:
                present = scene.GetComponent<rpg::MaterialComponent>(id) != nullptr; break;
            case rpg::ComponentType::Light:
                present = scene.GetComponent<rpg::LightComponent>(id) != nullptr; break;
            case rpg::ComponentType::Camera:
                present = scene.GetComponent<rpg::CameraComponent>(id) != nullptr; break;
            case rpg::ComponentType::Script:
                present = scene.GetComponent<rpg::ScriptComponent>(id) != nullptr; break;
            case rpg::ComponentType::ParticleEmitter:
                present = scene.GetComponent<rpg::ParticleEmitterComponent>(id) != nullptr; break;
            default: break;
        }
        if (present) { compLay->addWidget(new QLabel(ct.name, compBox)); any = true; }
    }
    if (!any) compLay->addWidget(new QLabel("(keine)", compBox));
    layout->addWidget(compBox);
    layout->addStretch(1);
    return root;
}

void QtEditorWindow::rebuildProperties() {
    if (!mView->IsEngineReady()) return;
    if (mPropsWidget) mPropsWidget->deleteLater();
    mPropsWidget = buildPropertiesWidget();
    mDockProperties->setWidget(mPropsWidget);
}

void QtEditorWindow::syncPropertyValues() {
    if (!mView->IsEngineReady() || mSelectedEntity < 0 || !selectedEntityExists()) return;
    auto* tc = mEngine->GetScene().GetComponent<rpg::TransformComponent>(
        static_cast<rpg::EntityID>(mSelectedEntity));
    if (!tc) return;

    mSyncingProps = true;
    const auto syncRow = [](QDoubleSpinBox** dst, const rpg::Vec3& v) {
        const double vals[3] = {v.x, v.y, v.z};
        for (int i = 0; i < 3; ++i) {
            if (dst[i] && !dst[i]->hasFocus()) dst[i]->setValue(vals[i]);
        }
    };
    if (mPos[0])   syncRow(mPos, tc->transform.position);
    if (mRot[0])   syncRow(mRot, tc->transform.rotation);
    if (mScale[0]) syncRow(mScale, tc->transform.scale);
    mSyncingProps = false;
}

} // namespace qt_editor
