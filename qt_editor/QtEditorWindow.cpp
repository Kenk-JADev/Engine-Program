#include "QtEditorWindow.h"
#include "QtGameViewWidget.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Scene.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

namespace qt_editor {

QtEditorWindow::QtEditorWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("RPG Maker 3D - Qt Editor");
    resize(1680, 980);

    mEngine = std::make_unique<rpg::Engine>();

    mView = new QtGameViewWidget(mEngine.get(), this);
    setCentralWidget(mView);

    buildDocks();
    buildMenus();
    buildToolbar();

    mStatusInfo = new QLabel("Starte...", this);
    statusBar()->addPermanentWidget(mStatusInfo);
    statusBar()->showMessage("Engine startet (GL-Kontext wird initialisiert)...");

    connect(mView, &QtGameViewWidget::engineReady, this, [this]() {
        log("Engine initialisiert (Embedded-Modus, Qt GL-Kontext).");
        statusBar()->showMessage("Bereit. QTimer treibt Update/Render, F9 toggelt RmlUi-HUD (Player-Kontext).");
    });
    connect(mView, &QtGameViewWidget::engineInitFailed, this, [this](QString msg) {
        log("FEHLER: " + msg);
        QMessageBox::critical(this, "Engine-Start fehlgeschlagen", msg);
    });
    connect(qApp, &QApplication::aboutToQuit, this, &QtEditorWindow::onAboutToQuit);

    // Game-Loop: ~60 Hz Logik-Update, dann Repaint des GL-Views
    mClock = new QElapsedTimer();
    mClock->start();
    mTimer = new QTimer(this);
    mTimer->setInterval(16);
    connect(mTimer, &QTimer::timeout, this, &QtEditorWindow::onTick);
    mTimer->start();
}

QtEditorWindow::~QtEditorWindow() = default;

void QtEditorWindow::buildMenus() {
    QMenu* mFile = menuBar()->addMenu("&Datei");
    mFile->addAction("Projekt &laden...", this, [this]() {
        log("Projekt laden: TODO (naechster Migrationsschritt)");
    });
    mFile->addAction("&Speichern", this, [this]() { log("Speichern: TODO"); });
    mFile->addSeparator();
    mFile->addAction("&Beenden", this, &QWidget::close);

    QMenu* mViewMenu = menuBar()->addMenu("&Ansicht");
    mViewMenu->addAction(mDockProject->toggleViewAction());
    mViewMenu->addAction(mDockProperties->toggleViewAction());
    mViewMenu->addAction(mDockConsole->toggleViewAction());

    QMenu* mPlay = menuBar()->addMenu("&Playtest");
    QAction* play = mPlay->addAction("Playtest starten/stoppen");
    play->setCheckable(true);
    connect(play, &QAction::toggled, this, &QtEditorWindow::onPlaytestToggled);

    QMenu* mHelp = menuBar()->addMenu("&Hilfe");
    mHelp->addAction("Ueber", this, [this]() {
        QMessageBox::about(this, "RPG Maker 3D Qt Editor",
            "Qt-basierter Editor (PoC):\n"
            "- QMainWindow mit echten, separaten Dock-Fenstern\n"
            "- Game-View als QOpenGLWidget (Engine im Embedded-Modus)\n"
            "- Ingame-UI laeuft weiter im GL-Kontext (RmlUi/ImGui unveraendert)");
    });
}

void QtEditorWindow::buildDocks() {
    mDockProject = new QDockWidget("Projekt", this);
    auto* projectList = new QListWidget(mDockProject);
    projectList->addItem("SampleProject");
    projectList->addItem("(Maps, Tilesets, DB - Binding folgt)");
    mDockProject->setWidget(projectList);
    addDockWidget(Qt::LeftDockWidgetArea, mDockProject);

    mDockProperties = new QDockWidget("Eigenschaften", this);
    mDockProperties->setWidget(new QLabel("Kein Objekt ausgewaehlt.\n\n"
        "Dieser Bereich wird Schritt fuer Schritt aus dem\n"
        "ImGui-Editor (Editor.cpp) portiert.", mDockProperties));
    addDockWidget(Qt::RightDockWidgetArea, mDockProperties);

    mDockConsole = new QDockWidget("Konsole", this);
    mConsole = new QPlainTextEdit(mDockConsole);
    mConsole->setReadOnly(true);
    mConsole->setMaximumBlockCount(2000);
    mDockConsole->setWidget(mConsole);
    addDockWidget(Qt::BottomDockWidgetArea, mDockConsole);
    log("Qt-Editor gestartet.");
}

void QtEditorWindow::buildToolbar() {
    QToolBar* tb = addToolBar("Editor");
    tb->setMovable(true);
    tb->addAction("Neu");
    tb->addAction("Oeffnen");
    tb->addAction("Speichern");
    tb->addSeparator();
    QAction* play = tb->addAction("Play");
    play->setCheckable(true);
    connect(play, &QAction::toggled, this, &QtEditorWindow::onPlaytestToggled);
}

void QtEditorWindow::log(const QString& msg) {
    if (mConsole) mConsole->appendPlainText(msg);
}

void QtEditorWindow::onTick() {
    if (!mView->IsEngineReady()) return;

    const qint64 ms = mClock->restart(); // restart() liefert Millisekunden
    float dt = static_cast<float>(ms) / 1000.0f;
    if (dt <= 0.0f) dt = 0.016f;
    if (dt > 0.1f) dt = 0.1f;

    mEngine->Update(dt);
    mView->update(); // -> paintGL -> Engine::Render

    // FPS in Statuszeile (0.5s Fenster)
    mFpsAccum += dt;
    mFpsFrames++;
    if (mFpsAccum >= 0.5f) {
        const int fps = static_cast<int>(mFpsFrames / mFpsAccum + 0.5f);
        mFpsAccum = 0.0f;
        mFpsFrames = 0;
        mStatusInfo->setText(QString("FPS: %1  |  Playtest: %2")
            .arg(fps).arg(mEngine->IsPlaying() ? "an" : "aus"));
    }
}

void QtEditorWindow::onPlaytestToggled(bool on) {
    if (!mView->IsEngineReady()) return;
    mEngine->SetPlaying(on);
    log(on ? "Playtest gestartet." : "Playtest gestoppt.");
}

void QtEditorWindow::closeEvent(QCloseEvent* event) {
    if (mTimer) mTimer->stop();
    event->accept();
}

void QtEditorWindow::onAboutToQuit() {
    if (mTimer) mTimer->stop();
    // GL-Ressourcen (RmlUi, Texturen, Framebuffers) brauchen current context
    if (mView) mView->makeCurrent();
    if (mEngine) mEngine->Shutdown();
    if (mView) mView->doneCurrent();
}

} // namespace qt_editor
