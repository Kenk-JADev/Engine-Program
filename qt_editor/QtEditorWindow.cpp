#include "QtEditorWindow.h"
#include "QtGameViewWidget.h"
#include "QtCodeWorkspace.h"
#include "QtMapTab.h"
#include "QtMapEditorDock.h"
#include "QtDatabaseEditorDock.h"
#include "QtDatabaseDialog.h"
#include "QtSoundTestDialog.h"
#include "QtMapPropertiesDialog.h"
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
#include "rpgmaker3d/Game.h"
#ifdef RPGMAKER3D_ENABLE_RMLUI
#include "rpgmaker3d/RmlUiSystem.h"
#endif

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
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

/// Standard-Symbol eines Qt-Styles (keine Asset-Dateien noetig)
QIcon stdIcon(QWidget* w, QStyle::StandardPixmap sp) {
    return w->style()->standardIcon(sp);
}

/// Automatische Symbol-Zuordnung fuer Ribbon-/Menue-Texte (easy-to-use:
/// erkennbare Icons statt nur Text). Leerstring sicher, kein Treffer = 0.
QStyle::StandardPixmap iconForText(const QString& text) {
    QString t = text;
    t.remove('\n');
    t.remove('&');
    if (t.contains(QStringLiteral("Neues Projekt")))     return QStyle::SP_FileIcon;
    if (t.contains(QStringLiteral("Projekt öffnen")))    return QStyle::SP_DirOpenIcon;
    if (t.contains(QStringLiteral("Speichern")))         return QStyle::SP_DialogSaveButton;
    if (t.contains(QStringLiteral("Rückgängig")))        return QStyle::SP_ArrowBack;
    if (t.contains(QStringLiteral("Wiederholen")))       return QStyle::SP_ArrowForward;
    if (t.contains(QStringLiteral("Löschen")))           return QStyle::SP_TrashIcon;
    if (t.contains(QStringLiteral("Beenden")))           return QStyle::SP_DialogCloseButton;
    if (t.contains(QStringLiteral("Playtest")))          return QStyle::SP_MediaPlay;
    if (t.contains(QStringLiteral("Sound-Test")))        return QStyle::SP_MediaVolume;
    if (t.contains(QStringLiteral("Datenbank")))         return QStyle::SP_FileDialogContentsView;
    if (t.contains(QStringLiteral("Karteneigenschaften"))) return QStyle::SP_FileDialogInfoView;
    if (t.contains(QStringLiteral("neu laden")))         return QStyle::SP_BrowserReload;
    if (t.contains(QStringLiteral("Neu laden")))         return QStyle::SP_BrowserReload;
    if (t.contains(QStringLiteral("Tastenkürzel")))      return QStyle::SP_FileDialogDetailedView;
    if (t.contains(QStringLiteral("ber RPG Maker")))     return QStyle::SP_MessageBoxInformation;
    if (t.contains(QStringLiteral("Willkommens")))       return QStyle::SP_TitleBarMenuButton;
    if (t.contains(QStringLiteral("exportieren")))       return QStyle::SP_DriveHDIcon;
    return QStyle::SP_CustomBase; // kein Symbol vorhanden
}

// ---------------------------------------------------------------------------
// Helfer fuer „Spiel exportieren" (QtEditorWindow::actionExportGame)
// ---------------------------------------------------------------------------

/// Elternordner von filePath sicher anlegen (QFile::copy erzeugt keine
/// Verzeichnisse).
bool ensureParentDir(const QString& filePath) {
    return QDir().mkpath(QFileInfo(filePath).absolutePath());
}

/// Rekursives Kopieren eines Verzeichnisses. Auf der Projektwurzel-Ebene
/// (topLevel=true) werden saves/ (Spielstaende des Entwicklers gehoeren
/// nicht in eine Auslieferung) und .git/ ausgelassen.
void copyProjectRecursive(const QString& srcPath, const QString& dstPath,
                          int& copied, int& failed, bool topLevel) {
    const QDir src(srcPath);
    const QFileInfoList entries = src.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& e : entries) {
        if (topLevel && e.isDir() &&
            (e.fileName().compare(QStringLiteral("saves"), Qt::CaseInsensitive) == 0 ||
             e.fileName() == QStringLiteral(".git")))
            continue;
        const QString to = dstPath + QStringLiteral("/") + e.fileName();
        if (e.isDir()) {
            copyProjectRecursive(e.absoluteFilePath(), to, copied, failed, false);
        } else {
            if (!ensureParentDir(to)) { ++failed; continue; }
            QFile::remove(to); // QFile::copy ueberschreibt nicht
            if (QFile::copy(e.absoluteFilePath(), to)) ++copied; else ++failed;
        }
    }
}

/// Ordnersicherer Spielname (Windows-Verbote: \/:*?"<>| ; keine Leerzeichen
/// oder Punkte am Ende).
QString exportGameFolderName(const QString& rawName) {
    QString name = rawName.trimmed();
    static const QRegularExpression kBad(QStringLiteral("[\\\\/:*?\"<>|]"));
    name.replace(kBad, QStringLiteral("_"));
    while (name.endsWith(QLatin1Char(' ')) || name.endsWith(QLatin1Char('.')))
        name.chop(1);
    return name.isEmpty() ? QStringLiteral("MeinSpiel") : name;
}

/// Kurzanleitung (Start + vc_redist-Hinweis) neben die Game.exe legen (UTF-8).
void writeExportReadme(const QString& outDir) {
    QFile f(outDir + QStringLiteral("/LIESMICH.txt"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    const QString text = QStringLiteral(
        "Start: Game.exe doppelklicken - das Spiel liegt im Ordner \u201eGame\u201c daneben.\n"
        "\n"
        "Startet Game.exe nicht (Fehlermeldung \u00fcber eine fehlende DLL bzw.\n"
        "VCRUNTIME/MSVCP), fehlt auf diesem PC die Microsoft Visual C++\n"
        "Redistributable (x64, VS 2022) - einmalig installieren:\n"
        "https://aka.ms/vs/17/release/vc_redist.x64.exe\n"
        "\n"
        "Die neben Game.exe mitgelieferten DLLs kommen aus dem Engine-Build\n"
        "und m\u00fcssen im selben Ordner bleiben.\n");
    f.write(text.toUtf8());
}
} // namespace

// ---------------------------------------------------------------------------

QtEditorWindow::QtEditorWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("RPG Maker 3D - Editor"));
    resize(1680, 980);

    mEngine = std::make_unique<rpg::Engine>();

    buildCentral();
    buildDocks();
    buildMenus();
    buildRibbon(); // Schnellzugriff + Ribbon-Tabs (ersetzt die klassische Toolbar)

    // Statuszeile: links Hinweistexte, rechts permanent Feld / Karte / FPS-Info
    mStatusTile = new QLabel(QStringLiteral("Feld: -"), this);
    mStatusTile->setMinimumWidth(150);
    mStatusTile->setToolTip(QStringLiteral("Mausposition auf der Landkarte (2D-Tab)"));
    statusBar()->addPermanentWidget(mStatusTile);
    mStatusMap = new QLabel(QStringLiteral("Karte: -"), this);
    mStatusMap->setMinimumWidth(190);
    mStatusMap->setToolTip(QStringLiteral("Aktive Karte – Rechtsklick auf die Landkarte oder \n"
                                          "Doppelklick in der Kartenliste öffnet die Eigenschaften"));
    statusBar()->addPermanentWidget(mStatusMap);
    mStatusInfo = new QLabel(QStringLiteral("Starte …"), this);
    statusBar()->addPermanentWidget(mStatusInfo);
    statusBar()->showMessage(QStringLiteral("Engine startet (GL-Kontext wird initialisiert) …"));

    connect(mView, &QtGameViewWidget::engineReady, this, [this]() {
        log(QStringLiteral("Engine initialisiert (eingebetteter Qt-GL-Kontext)."));
        log(QStringLiteral("Tabs unten: Spielansicht (3D) | Landkarte (2D) | Spiel | Skript."));
        log(QStringLiteral("F5 = Playtest über die Player-exe, Umschalt+F5 = eingebettet."));
        statusBar()->showMessage(
            QStringLiteral("Bereit. F5 startet den Playtest im Player, F9 = HUD, F10 = Debug-Inspektor."));
        setSelectedEntity(-1);
        if (mCode) mCode->refresh();
        if (mMapTab) mMapTab->refresh();
        if (mMapDockWidget) {
            mMapDockWidget->refresh();
            mView->setPaintMode(mMapDockWidget->paintEnabled());
            mView->setPaintTile(mMapDockWidget->selectedTile());
            mView->setPaintLayer(mMapDockWidget->selectedLayer());
        }
        if (mDbDockWidget) mDbDockWidget->refresh();
        if (mEventDockWidget) mEventDockWidget->refresh();
        if (mAssetDockWidget) mAssetDockWidget->refresh();
        updateMapStatus();
        updatePlayTabInfo();
        rebuildRecentProjectsMenu();
        // Easy-to-use: Willkommens-Dialog, wenn (noch) kein Projekt geladen ist
        if (mEngine->GetProject().GetProjectPath().empty() &&
            QSettings().value(QStringLiteral("ui/showWelcome"), true).toBool()) {
            QTimer::singleShot(350, this, [this]() { showWelcomeDialog(); });
        }
    });
    connect(mView, &QtGameViewWidget::entityPicked, this, [this](int id) {
        setSelectedEntity(id);
    });
    connect(mView, &QtGameViewWidget::engineInitFailed, this, [this](QString msg) {
        log(QStringLiteral("FEHLER: ") + msg);
        QMessageBox::critical(this, QStringLiteral("Engine-Start fehlgeschlagen"), msg);
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
// Zentral: Tabs UNTEN im Browser-Stil (Spielansicht | Landkarte | Spiel | Skript)
// ---------------------------------------------------------------------------

void QtEditorWindow::buildCentral() {
    mCentralTabs = new QTabWidget(this);
    mCentralTabs->setObjectName(QStringLiteral("centralTabs"));
    mCentralTabs->setDocumentMode(true);
    mCentralTabs->setTabPosition(QTabWidget::South);   // Tabs unten wie im Browser
    mCentralTabs->setMovable(true);
    mCentralTabs->setUsesScrollButtons(true);

    mView = new QtGameViewWidget(mEngine.get(), mCentralTabs);
    mMapTab = new QtMapTab(mEngine.get(), mCentralTabs);
    mPlayTab = buildPlayTab();
    // Skript-Editor: eigenes Top-Level-Fenster wie in RPG Maker XP (oeffnen
    // ueber das Skript-Menue oder F11), KEIN Tab und keine Ausfuehr-Buttons.
    mCode = new QtCodeWorkspace(mEngine.get(), nullptr);
    mCode->setParent(this, Qt::Window);
    mCode->setWindowTitle(QStringLiteral("Skript-Editor"));
    mCode->resize(1100, 720);
    mCode->hide();

    // Zentrale Tabs mit erkennbaren Symbolen (Qt-Standardicons, keine Assets)
    mCentralTabs->addTab(mView, stdIcon(mCentralTabs, QStyle::SP_ComputerIcon),
                         QStringLiteral("Spielansicht"));
    mCentralTabs->setTabToolTip(0, QStringLiteral("3D-Szene bearbeiten, Objekte wählen, Karten malen"));
    mCentralTabs->addTab(mMapTab, stdIcon(mCentralTabs, QStyle::SP_DesktopIcon),
                         QStringLiteral("Landkarte"));
    mCentralTabs->setTabToolTip(1, QStringLiteral("Karte in der 2D-Draufsicht betrachten und malen"));
    mCentralTabs->addTab(mPlayTab, stdIcon(mCentralTabs, QStyle::SP_MediaPlay),
                         QStringLiteral("Spiel"));
    mCentralTabs->setTabToolTip(2, QStringLiteral("Playtest starten (Player-exe oder eingebettet)"));

    setCentralWidget(mCentralTabs);

    connect(mCentralTabs, &QTabWidget::currentChanged,
            this, &QtEditorWindow::onCentralTabChanged);
    connect(mCode, &QtCodeWorkspace::logMessage, this, [this](const QString& m) {
        log(m);
    });
    connect(mMapTab, &QtMapTab::tilesChanged, this, [this]() {
        // 3D-View neu aufbauen, sobald in der 2D-Karte gemalt wurde
        if (mView && mView->IsEngineReady() && mEngine) {
            // Die Map markiert sich selbst als "dirty"; nur triggern.
            mView->update();
        }
    });
}

QWidget* QtEditorWindow::buildPlayTab() {
    auto* page = new QWidget(mCentralTabs);
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(32, 32, 32, 32);
    lay->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("Playtest"), page);
    title->setObjectName(QStringLiteral("playTabTitle"));
    lay->addWidget(title);

    // --- Projekt-Übersicht (wird von updatePlayTabInfo() befüllt) ----------
    auto* infoBox = new QGroupBox(QStringLiteral("Projekt-Übersicht"), page);
    auto* infoLay = new QVBoxLayout(infoBox);
    mPlayTabInfo = new QLabel(QStringLiteral("(kein Projekt geladen)"), infoBox);
    mPlayTabInfo->setWordWrap(true);
    mPlayTabInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLay->addWidget(mPlayTabInfo);
    lay->addWidget(infoBox);

    // --- Start-Knopf + Alternativen ----------------------------------------
    auto* btnPlayer = new QPushButton(QStringLiteral("Playtest starten (Player-exe)   [F5]"), page);
    btnPlayer->setObjectName(QStringLiteral("playTabPrimary"));
    btnPlayer->setIcon(stdIcon(btnPlayer, QStyle::SP_MediaPlay));
    btnPlayer->setIconSize(QSize(22, 22));
    btnPlayer->setMinimumHeight(48);
    connect(btnPlayer, &QPushButton::clicked, this, [this]() { actionPlaytestPlayer(); });
    lay->addWidget(btnPlayer);

    mPlayTabEmbeddedBtn = new QPushButton(
        QStringLiteral("Eingebetteten Playtest umschalten   [Umschalt+F5]"), page);
    mPlayTabEmbeddedBtn->setCheckable(true);
    mPlayTabEmbeddedBtn->setMinimumHeight(40);
    connect(mPlayTabEmbeddedBtn, &QPushButton::toggled, this, [this](bool on) {
        if (mPlayAction && mPlayAction->isChecked() != on) mPlayAction->setChecked(on);
        else onPlaytestToggled(on);
    });
    lay->addWidget(mPlayTabEmbeddedBtn);

    // Speichern-Verhalten vor dem Playtest (merkt die Frage-Dialog-Option)
    mAutoSaveCheck = new QCheckBox(
        QStringLiteral("Vor dem Playtest immer speichern (Speicherfrage überspringen)"), page);
    mAutoSaveCheck->setChecked(
        QSettings().value(QStringLiteral("ui/autoSaveBeforePlaytest"), false).toBool());
    connect(mAutoSaveCheck, &QCheckBox::toggled, this, [](bool on) {
        QSettings().setValue(QStringLiteral("ui/autoSaveBeforePlaytest"), on);
    });
    lay->addWidget(mAutoSaveCheck);

    mPlayTabExeStatus = new QLabel(page);
    mPlayTabExeStatus->setWordWrap(true);
    lay->addWidget(mPlayTabExeStatus);

    // --- Ruby-Prüfung ohne Start --------------------------------------------
    auto* checkBox = new QGroupBox(QStringLiteral("Ruby-Skripte prüfen"), page);
    auto* checkLay = new QVBoxLayout(checkBox);
    auto* checkRow = new QHBoxLayout();
    auto* btnCheck = new QPushButton(QStringLiteral("Alle .rb jetzt prüfen"), checkBox);
    btnCheck->setIcon(stdIcon(btnCheck, QStyle::SP_DialogApplyButton));
    btnCheck->setToolTip(QStringLiteral(
        "Parst alle Ruby-Skripte mit dem echten Parser (ohne Ausführung) und\n"
        "zeigt Syntaxfehler mit Datei + Zeile – bevor der Playtest sie findet."));
    connect(btnCheck, &QPushButton::clicked, this, [this]() { runScriptCheck(); });
    checkRow->addWidget(btnCheck);
    checkRow->addStretch(1);
    checkLay->addLayout(checkRow);
    mScriptCheckOutput = new QPlainTextEdit(checkBox);
    mScriptCheckOutput->setReadOnly(true);
    mScriptCheckOutput->setMaximumHeight(110);
    mScriptCheckOutput->setPlaceholderText(QStringLiteral(
        "Noch nicht geprüft. Der Playtest prüft automatisch beim Start – hier kannst du es vorab tun."));
    checkLay->addWidget(mScriptCheckOutput);
    lay->addWidget(checkBox);

    mPlayTabStatus = new QLabel(QStringLiteral("Status: bereit"), page);
    lay->addWidget(mPlayTabStatus);
    lay->addStretch(1);

    auto* controls = new QLabel(
        QStringLiteral("Steuerung im Spiel:  WASD / Pfeile = Bewegen,  E / Eingabe = Aktion,  "
                       "Esc = Menü/Pause,  Umschalt = Rennen,  F9 = HUD ein/aus,  "
                       "F10 = Debug-Inspektor (Schalter/Variablen).\n"
                       "Umlaute in Dialogen und Namenseingabe (Alt+A/O/U im Namensfeld) "
                       "werden voll unterstützt (ä ö ü Ä Ö Ü ß)."),
        page);
    controls->setWordWrap(true);
    lay->addWidget(controls);
    return page;
}

void QtEditorWindow::showScriptEditor() {
    if (!mCode) return;
    mCode->refresh();
    if (!mCode->isVisible()) mCode->show();
    mCode->raise();
    mCode->activateWindow();
}

void QtEditorWindow::onCentralTabChanged(int index) {
    QWidget* w = mCentralTabs ? mCentralTabs->widget(index) : nullptr;
    if (w == mView) {
        mView->setFocus(Qt::OtherFocusReason);
        statusBar()->showMessage(QStringLiteral(
            "Spielansicht – WASD/Maus navigieren, Linksklick wählt aus, F5 = Playtest (Player)."));
    } else if (w == mMapTab) {
        mMapTab->refresh();
        statusBar()->showMessage(QStringLiteral(
            "Landkarte – 2D-Draufsicht. Klicken malt mit dem im Map-Dock gewählten Tile."));
    } else if (w == mPlayTab) {
        updatePlayTabInfo();
        statusBar()->showMessage(QStringLiteral("Spiel – Playtest starten."));
    }
}

// ---------------------------------------------------------------------------
// Menüs (mit echten Umlauten, /utf-8 für MSVC ist gesetzt)
// ---------------------------------------------------------------------------

void QtEditorWindow::buildMenus() {
    QMenu* mFile = menuBar()->addMenu(QStringLiteral("&Datei"));
    QAction* aNew = mFile->addAction(stdIcon(this, QStyle::SP_FileIcon),
                                     QStringLiteral("&Neues Projekt …"),
                                     this, [this]() { actionNewProject(); });
    aNew->setShortcut(QKeySequence(QStringLiteral("Ctrl+N")));
    QAction* aOpen = mFile->addAction(stdIcon(this, QStyle::SP_DirOpenIcon),
                                      QStringLiteral("Projekt ö&ffnen …"),
                                      this, [this]() { actionOpenProject(); });
    aOpen->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    mSaveAction = mFile->addAction(stdIcon(this, QStyle::SP_DialogSaveButton),
                                   QStringLiteral("&Speichern"),
                                   this, [this]() { actionSaveProject(); });
    mSaveAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    mFile->addSeparator();
    mFile->addAction(QStringLiteral("Szene laden …"),
                     this, [this]() { actionLoadSceneFrom(); });
    mFile->addAction(QStringLiteral("Szene speichern unter …"),
                     this, [this]() { actionSaveSceneAs(); });
    mFile->addSeparator();
    // Easy-to-use: Zuletzt geöffnete Projekte (QSettings-persistent)
    mRecentMenu = mFile->addMenu(stdIcon(this, QStyle::SP_FileDialogListView),
                                 QStringLiteral("Zu&letzt geöffnete Projekte"));
    connect(mRecentMenu, &QMenu::aboutToShow, this, [this]() {
        rebuildRecentProjectsMenu();
    });
    mFile->addSeparator();
    mFile->addAction(QStringLiteral("Skripte speichern"), this, [this]() {
        if (mCode) mCode->saveAll();
    });
    // Easy-to-use: fertige Auslieferung (Player-exe + Projekt) in einem Rutsch
    mFile->addAction(stdIcon(this, QStyle::SP_DriveHDIcon),
                     QStringLiteral("Spiel &exportieren …"),
                     this, [this]() { actionExportGame(); });
    mFile->addSeparator();
    mFile->addAction(stdIcon(this, QStyle::SP_DialogCloseButton),
                     QStringLiteral("&Beenden"), this, &QWidget::close);

    QMenu* mEdit = menuBar()->addMenu(QStringLiteral("&Bearbeiten"));
    mUndoAction = mEdit->addAction(stdIcon(this, QStyle::SP_ArrowBack),
                                   QStringLiteral("Rückgängig"), this, [this]() { actionUndo(); });
    mRedoAction = mEdit->addAction(stdIcon(this, QStyle::SP_ArrowForward),
                                   QStringLiteral("Wiederholen"), this, [this]() { actionRedo(); });
    mEdit->addSeparator();
    mDeleteAction = mEdit->addAction(stdIcon(this, QStyle::SP_TrashIcon),
                                     QStringLiteral("Ausgewähltes löschen"),
                                     this, [this]() { deleteSelected(); });
    mUndoAction->setEnabled(false);
    mRedoAction->setEnabled(false);
    mDeleteAction->setEnabled(false);

    QMenu* mCreate = menuBar()->addMenu(QStringLiteral("&Erstellen"));
    mCreate->addAction(QStringLiteral("Würfel"), this, [this]() { actionCreateCube(); });
    mCreate->addAction(QStringLiteral("Ebene"), this, [this]() { actionCreatePlane(); });
    mCreate->addAction(QStringLiteral("Licht"), this, [this]() { actionCreateLight(); });

    QMenu* mCodeMenu = menuBar()->addMenu(QStringLiteral("&Skript"));
    QAction* openScriptAction = mCodeMenu->addAction(
        QStringLiteral("Skript-Editor öffnen"), this, [this]() { showScriptEditor(); });
    openScriptAction->setShortcut(QKeySequence(QStringLiteral("F11")));
    mCodeMenu->addAction(QStringLiteral("Ruby-Skripte speichern"), this, [this]() {
        if (mCode) mCode->saveAll();
    });

    QMenu* mViewMenu = menuBar()->addMenu(QStringLiteral("&Ansicht"));
    mShowGameViewAction = mViewMenu->addAction(QStringLiteral("Spielansicht"), this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mView);
    });
    mViewMenu->addAction(QStringLiteral("Landkarte"), this, [this]() {
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mMapTab);
    });
    mShowCodeAction = mViewMenu->addAction(QStringLiteral("Skript-Editor"), this, [this]() {
        showScriptEditor();
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
    mViewMenu->addAction(QStringLiteral("RmlUi HUD umschalten (F9)"), this, [this]() {
        if (mEngine && mEngine->GetRmlUi()) mEngine->GetRmlUi()->ToggleVisible();
    });
#endif

    QMenu* mPlay = menuBar()->addMenu(QStringLiteral("&Playtest"));
    mPlayPlayerAction = mPlay->addAction(stdIcon(this, QStyle::SP_MediaPlay),
                                         QStringLiteral("Playtest starten (Player-exe)"),
                                         this, [this]() { actionPlaytestPlayer(); });
    mPlayPlayerAction->setShortcut(QKeySequence(Qt::Key_F5));
    mPlayPlayerAction->setShortcutContext(Qt::ApplicationShortcut);
    mPlayAction = mPlay->addAction(QStringLiteral("Playtest eingebettet starten/stoppen"));
    mPlayAction->setCheckable(true);
    mPlayAction->setShortcut(QKeySequence(QStringLiteral("Shift+F5")));
    mPlayAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(mPlayAction, &QAction::toggled, this, &QtEditorWindow::onPlaytestToggled);

    QMenu* mHelp = menuBar()->addMenu(QStringLiteral("&Hilfe"));
    mHelp->addAction(stdIcon(this, QStyle::SP_FileDialogDetailedView),
                     QStringLiteral("&Tastenkürzel anzeigen …"), this, [this]() {
        showShortcutsDialog();
    })->setShortcut(QKeySequence(QStringLiteral("Ctrl+?")));
    mHelp->addAction(stdIcon(this, QStyle::SP_TitleBarMenuButton),
                     QStringLiteral("&Willkommens-Dialog öffnen"), this, [this]() {
        showWelcomeDialog();
    });
    mHelp->addSeparator();
    mHelp->addAction(stdIcon(this, QStyle::SP_MessageBoxInformation),
                     QStringLiteral("Ü&ber RPG Maker 3D …"), this, [this]() {
        QMessageBox::about(this, QStringLiteral("RPG Maker 3D Editor"),
            QStringLiteral(
                "RPG Maker 3D – Editor (Qt)\n"
                "Version 2026.07  –  Qt %1\n\n"
                "Ein RPG-Maker-XP-artiges Tool für 3D-Rollenspiele:\n\n"
                "• Tabs unten: Spielansicht (3D), Landkarte (2D), Spiel (Playtest), Skript\n"
                "• Landkarte: Strg+Z/Strg+Y, Rechtsklick-Menü, Startposition per Klick\n"
                "• XP-Datenbank (13 Tabs), Sound-Test, Karteneigenschaften\n"
                "• XP-artiger Event-Editor mit vollem XP-Befehlssatz (docs/EVENTS-XP.md)\n"
                "• Playtest über die Player-exe (F5)\n"
                "• Umlaut-Unterstützung (ä ö ü Ä Ö Ü ß) in UI und Spiel")
                .arg(QString::fromLatin1(qVersion())));
    });

    auto* delView = new QShortcut(QKeySequence(Qt::Key_Delete), mView);
    delView->setContext(Qt::WidgetShortcut);
    connect(delView, &QShortcut::activated, this, [this]() { deleteSelected(); });
    auto* delTree = new QShortcut(QKeySequence(Qt::Key_Delete), mHierarchy);
    delTree->setContext(Qt::WidgetShortcut);
    connect(delTree, &QShortcut::activated, this, [this]() { deleteSelected(); });
    auto* undoView = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Z")), mView);
    undoView->setContext(Qt::WidgetShortcut);
    connect(undoView, &QShortcut::activated, this, [this]() { actionUndo(); });
    auto* undoTree = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Z")), mHierarchy);
    undoTree->setContext(Qt::WidgetShortcut);
    connect(undoTree, &QShortcut::activated, this, [this]() { actionUndo(); });
    auto* redoView = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Y")), mView);
    redoView->setContext(Qt::WidgetShortcut);
    connect(redoView, &QShortcut::activated, this, [this]() { actionRedo(); });
    auto* redoTree = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Y")), mHierarchy);
    redoTree->setContext(Qt::WidgetShortcut);
    connect(redoTree, &QShortcut::activated, this, [this]() { actionRedo(); });
}

// ---------------------------------------------------------------------------
// Docks
// ---------------------------------------------------------------------------

void QtEditorWindow::buildDocks() {
    setDockNestingEnabled(true);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    mDockHierarchy = new QDockWidget(QStringLiteral("Hierarchie"), this);
    mDockHierarchy->setObjectName(QStringLiteral("dockHierarchy"));
    mHierarchy = new QTreeWidget(mDockHierarchy);
    mHierarchy->setHeaderLabels(QStringList()
        << QStringLiteral("Objekt") << QStringLiteral("ID"));
    mHierarchy->setRootIsDecorated(false);
    mHierarchy->setAlternatingRowColors(true);
    mDockHierarchy->setWidget(mHierarchy);
    addDockWidget(Qt::LeftDockWidgetArea, mDockHierarchy);
    connect(mHierarchy, &QTreeWidget::itemSelectionChanged,
            this, [this]() { onHierarchySelectionChanged(); });

    mDockProperties = new QDockWidget(QStringLiteral("Eigenschaften"), this);
    mDockProperties->setObjectName(QStringLiteral("dockProperties"));
    mDockProperties->setWidget(new QLabel(QStringLiteral("Kein Objekt ausgewählt."),
                                          mDockProperties));
    addDockWidget(Qt::RightDockWidgetArea, mDockProperties);

    mDockConsole = new QDockWidget(QStringLiteral("Konsole"), this);
    mDockConsole->setObjectName(QStringLiteral("dockConsole"));
    mConsole = new QPlainTextEdit(mDockConsole);
    mConsole->setReadOnly(true);
    mConsole->setMaximumBlockCount(2000);
    mDockConsole->setWidget(mConsole);
    addDockWidget(Qt::BottomDockWidgetArea, mDockConsole);

    mDockMap = new QDockWidget(QStringLiteral("Map-Editor"), this);
    mDockMap->setObjectName(QStringLiteral("dockMap"));
    mMapDockWidget = new QtMapEditorDock(mEngine.get(), mDockMap);
    mDockMap->setWidget(mMapDockWidget);
    addDockWidget(Qt::RightDockWidgetArea, mDockMap);
    tabifyDockWidget(mDockProperties, mDockMap);

    mDockDatabase = new QDockWidget(QStringLiteral("Datenbank"), this);
    mDockDatabase->setObjectName(QStringLiteral("dockDatabase"));
    mDbDockWidget = new QtDatabaseEditorDock(mEngine.get(), mDockDatabase);
    mDockDatabase->setWidget(mDbDockWidget);
    addDockWidget(Qt::RightDockWidgetArea, mDockDatabase);
    tabifyDockWidget(mDockMap, mDockDatabase);

    mDockEvents = new QDockWidget(QStringLiteral("Events"), this);
    mDockEvents->setObjectName(QStringLiteral("dockEvents"));
    mEventDockWidget = new QtEventEditorDock(mEngine.get(), mDockEvents);
    mDockEvents->setWidget(mEventDockWidget);
    addDockWidget(Qt::RightDockWidgetArea, mDockEvents);
    tabifyDockWidget(mDockDatabase, mDockEvents);

    mDockAssets = new QDockWidget(QStringLiteral("Assets"), this);
    mDockAssets->setObjectName(QStringLiteral("dockAssets"));
    mAssetDockWidget = new QtAssetBrowserDock(mEngine.get(), mDockAssets);
    mDockAssets->setWidget(mAssetDockWidget);
    addDockWidget(Qt::LeftDockWidgetArea, mDockAssets);
    tabifyDockWidget(mDockHierarchy, mDockAssets);

    mDockProperties->raise();

    connect(mMapDockWidget, &QtMapEditorDock::logMessage, this, [this](const QString& m) { log(m); });
    connect(mDbDockWidget, &QtDatabaseEditorDock::logMessage, this, [this](const QString& m) { log(m); });
    connect(mEventDockWidget, &QtEventEditorDock::logMessage, this, [this](const QString& m) { log(m); });
    connect(mAssetDockWidget, &QtAssetBrowserDock::logMessage, this, [this](const QString& m) { log(m); });
    connect(mMapDockWidget, &QtMapEditorDock::paintStateChanged, this, [this]() {
        if (!mView || !mMapDockWidget) return;
        mView->setPaintMode(mMapDockWidget->paintEnabled());
        mView->setPaintTile(mMapDockWidget->selectedTile());
        mView->setPaintLayer(mMapDockWidget->selectedLayer());
        mView->setBrushMode(mMapDockWidget->brushMode());
        if (mMapTab) {
            mMapTab->setPaintTile(mMapDockWidget->selectedTile());
            mMapTab->setPaintLayer(mMapDockWidget->selectedLayer());
        }
    });
    connect(mMapDockWidget, &QtMapEditorDock::mapLoaded, this, [this]() {
        setSelectedEntity(-1);
        refreshHierarchy();
        if (mMapTab) mMapTab->refresh();
        updateMapStatus();
        updatePlayTabInfo(); // Ereignis-Zahl/Startkarte der Übersicht aktualisieren
    });
    // Landkarte: Karten-ID für Ereignis-Speicherung + Dock-Sync bei Event-Edits
    if (mMapTab) {
        mMapTab->setCurrentMapIdFn([this]() {
            const int idx = mMapDockWidget ? mMapDockWidget->selectedMapIndex() : -1;
            auto& infos = rpg::Database::Get().MapInfos();
            if (idx >= 0 && idx < (int)infos.size())
                return infos[(size_t)idx].id;
            return rpg::Database::Get().System().startMapId;
        });
        connect(mMapTab, &QtMapTab::eventsChanged, this, [this]() {
            if (mEventDockWidget) mEventDockWidget->refresh();
        });
        connect(mMapTab, &QtMapTab::logMessage, this, [this](const QString& m) { log(m); });
        // Tile-Wahl in der Landkarten-Palette -> Map-Dock/3D-Pinsel mitziehen
        connect(mMapTab, &QtMapTab::paintTilePicked, this, [this](int tid) {
            if (mMapDockWidget) mMapDockWidget->setSelectedTile(tid);
        });
        // Maus-Feld der Landkarte in die Statuszeile spiegeln
        connect(mMapTab, &QtMapTab::hoverInfo, this, [this](const QString& t) {
            if (mStatusTile) mStatusTile->setText(t);
        });
        // Rechtsklick auf der Landkarte -> Karteneigenschaften (wie Ribbon/Doppelklick)
        connect(mMapTab, &QtMapTab::mapPropertiesRequested, this, [this]() {
            if (!mView || !mView->IsEngineReady() || !mMapDockWidget) return;
            const int idx = mMapDockWidget->selectedMapIndex();
            if (QtMapPropertiesDialog::EditMapProperties(this, mEngine.get(), idx)) {
                mMapDockWidget->refresh();
                if (mMapTab) mMapTab->refresh();
                updateMapStatus();
                log(QStringLiteral("Karteneigenschaften übernommen und gespeichert."));
            }
        });
    }
    connect(mView, &QtGameViewWidget::tilePainted, this, [this](int x, int z, int tile) {
        static int n = 0;
        if ((++n % 8) == 0)
            log(QStringLiteral("Tile (%1,%2) = %3").arg(x).arg(z).arg(tile));
        if (mMapTab) mMapTab->refresh();
    });

    log(QStringLiteral("Qt-Editor gestartet."));
    log(QStringLiteral("  Tabs unten  = Spielansicht (3D) | Landkarte (2D) | Spiel | Skript"));
    log(QStringLiteral("  Ribbon oben = Datei | Werkzeuge | Ansicht | Fenster | Debug"));
    log(QStringLiteral("  F5 = Playtest über Player-exe, Umschalt+F5 = eingebettet"));
    log(QStringLiteral("Datei -> Projekt öffnen … um loszulegen."));
}

// ---------------------------------------------------------------------------
// Schnellzugriff + Ribbon (Datei / Werkzeuge / Ansicht / Fenster / Debug)
// ---------------------------------------------------------------------------

QWidget* QtEditorWindow::buildQuickAccessBar() {
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("quickAccessBar"));
    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(6, 2, 6, 0);
    lay->setSpacing(2);

    const auto mkBtn = [this, lay](const QString& text, const QString& tip,
                                   QStyle::StandardPixmap sp,
                                   std::function<void()> fn) {
        auto* b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        if (sp != QStyle::SP_CustomBase) {
            b->setIcon(stdIcon(b, sp));
            b->setIconSize(QSize(18, 18));
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        }
        connect(b, &QToolButton::clicked, this, [fn]() { fn(); });
        lay->addWidget(b);
        return b;
    };

    mkBtn(QStringLiteral("Speichern"), QStringLiteral("Projekt speichern [Strg+S]"),
          QStyle::SP_DialogSaveButton, [this]() { actionSaveProject(); });
    mkBtn(QString(), QStringLiteral("Rückgängig [Strg+Z]"), QStyle::SP_ArrowBack,
          [this]() { actionUndo(); });
    mkBtn(QString(), QStringLiteral("Wiederholen [Strg+Y]"), QStyle::SP_ArrowForward,
          [this]() { actionRedo(); });
    lay->addSpacing(10);
    mkBtn(QStringLiteral("Playtest (Player-exe)"),
          QStringLiteral("Projekt speichern und in der Player-exe testen [F5]"),
          QStyle::SP_MediaPlay, [this]() { actionPlaytestPlayer(); });
    lay->addStretch(1);
    return bar;
}

void QtEditorWindow::addRibbonPage(const QString& title) {
    // Seitengerüst wird in ribbonPage angelegt; diese Hilfe existiert der
    // Lesbarkeit halber (siehe buildRibbon).
    (void)title;
}

QWidget* QtEditorWindow::ribbonPage(const QString& title) {
    auto* page = new QWidget(mRibbonTabs);
    page->setObjectName(QStringLiteral("ribbonPage"));
    auto* lay = new QHBoxLayout(page);
    lay->setContentsMargins(8, 4, 8, 6);
    lay->setSpacing(4);
    lay->addStretch(1); // Inhalte links halten; Stretch am Ende
    mRibbonTabs->addTab(page, title);
    return page;
}

void QtEditorWindow::ribbonButton(QWidget* page, const QString& text, const QString& tip,
                                  std::function<void()> fn, bool checkable, bool checked) {
    auto* lay = qobject_cast<QHBoxLayout*>(page->layout());
    if (!lay) return;
    auto* b = new QToolButton(page);
    b->setObjectName(QStringLiteral("ribbonButton"));
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(checkable);
    b->setChecked(checked);
    // Easy-to-use: bekannte Aktionen bekommen ein erkennbares Symbol
    const QStyle::StandardPixmap sp = iconForText(text);
    if (sp != QStyle::SP_CustomBase) {
        b->setIcon(stdIcon(b, sp));
        b->setIconSize(QSize(26, 26));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon); // Ribbon-Optik
        b->setMinimumHeight(56);
    } else {
        b->setMinimumHeight(40);
    }
    if (checkable) {
        connect(b, &QToolButton::toggled, this, [fn](bool) { fn(); });
    } else {
        connect(b, &QToolButton::clicked, this, [fn]() { fn(); });
    }
    lay->insertWidget(lay->count() - 1, b);
}

void QtEditorWindow::buildRibbon() {
    // Schnellzugriffs-Leiste über den Ribbon-Tabs
    auto* host = new QWidget(this);
    host->setObjectName(QStringLiteral("ribbonHost"));
    auto* hostLay = new QVBoxLayout(host);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->setSpacing(0);
    hostLay->addWidget(buildQuickAccessBar());

    mRibbonTabs = new QTabWidget(host);
    mRibbonTabs->setObjectName(QStringLiteral("ribbonTabs"));
    mRibbonTabs->setDocumentMode(true);
    mRibbonTabs->setUsesScrollButtons(true);
    hostLay->addWidget(mRibbonTabs);
    addToolBarBreak();                     // eigenes Band unter der Menüleiste
    auto* tbArea = addToolBar(QStringLiteral("Ribbon"));
    tbArea->setObjectName(QStringLiteral("ribbonToolbar"));
    tbArea->setMovable(false);
    tbArea->setFloatable(false);
    tbArea->setAllowedAreas(Qt::TopToolBarArea);
    tbArea->addWidget(host);

    // ---- Tab: Datei ---------------------------------------------------------
    if (QWidget* p = ribbonPage(QStringLiteral("Datei"))) {
        ribbonButton(p, QStringLiteral("Neues\nProjekt"), QStringLiteral("Neues Projekt anlegen [Strg+N]"),
                     [this]() { actionNewProject(); });
        ribbonButton(p, QStringLiteral("Projekt\nöffnen"), QStringLiteral("Projekt öffnen [Strg+O]"),
                     [this]() { actionOpenProject(); });
        ribbonButton(p, QStringLiteral("Speichern"), QStringLiteral("Alles speichern [Strg+S]"),
                     [this]() { actionSaveProject(); });
        ribbonButton(p, QStringLiteral("Skripte\nspeichern"), QStringLiteral("Nur Skripte speichern"),
                     [this]() { if (mCode) mCode->saveAll(); });
        ribbonButton(p, QStringLiteral("Spiel\nexportieren"),
                     QStringLiteral("Fertiges Spiel exportieren (Game.exe + Projektordner „Game“)"),
                     [this]() { actionExportGame(); });
        ribbonButton(p, QStringLiteral("Beenden"), QStringLiteral("Editor schließen"),
                     [this]() { close(); });
    }
    // ---- Tab: Werkzeuge ------------------------------------------------------
    if (QWidget* p = ribbonPage(QStringLiteral("Werkzeuge"))) {
        ribbonButton(p, QStringLiteral("Datenbank"), QStringLiteral("XP-Datenbank-Editor öffnen"),
                     [this]() {
                         if (!mView || !mView->IsEngineReady()) return;
                         if (QtDatabaseDialog::EditDatabase(this, mEngine.get())) {
                             if (mDbDockWidget) mDbDockWidget->refresh();
                             log(QStringLiteral("Datenbank (XP-Dialog) gespeichert."));
                         }
                     });
        ribbonButton(p, QStringLiteral("Sound-Test"), QStringLiteral("Sound-Test-Fenster öffnen (BGM/BGS/ME/SE durchhören)"),
                     [this]() {
                         if (!mView || !mView->IsEngineReady()) return;
                         QtSoundTestDialog::ShowSoundTest(this, mEngine.get());
                     });
        ribbonButton(p, QStringLiteral("Karten-\neigenschaften"), QStringLiteral("Karteneigenschaften öffnen (Name, Tileset, Größe, BGM ...)"),
                     [this]() {
                         if (!mView || !mView->IsEngineReady() || !mMapDockWidget) return;
                         const int idx = mMapDockWidget->selectedMapIndex();
                         if (QtMapPropertiesDialog::EditMapProperties(this, mEngine.get(), idx)) {
                             mMapDockWidget->refresh();
                             if (mMapTab) mMapTab->refresh();
                             log(QStringLiteral("Karteneigenschaften übernommen und gespeichert."));
                         }
                     });
        ribbonButton(p, QStringLiteral("Würfel"), QStringLiteral("Würfel erstellen"),
                     [this]() { actionCreateCube(); });
        ribbonButton(p, QStringLiteral("Ebene"), QStringLiteral("Ebene erstellen"),
                     [this]() { actionCreatePlane(); });
        ribbonButton(p, QStringLiteral("Licht"), QStringLiteral("Lichtquelle erstellen"),
                     [this]() { actionCreateLight(); });
        ribbonButton(p, QStringLiteral("Gizmo"), QStringLiteral("Translate-Gizmo ein/aus"),
                     [this]() {
                         const int mode = mView->gizmoMode() == 0 ? 1 : 0;
                         mView->setGizmoMode(mode);
                         log(mode ? QStringLiteral("Gizmo: an") : QStringLiteral("Gizmo: aus"));
                     }, true, true);
        ribbonButton(p, QStringLiteral("Karten\nmalen"), QStringLiteral("Tile-Malen im 3D-View ein/aus"),
                     [this]() {
                         if (mMapDockWidget) mMapDockWidget->setPaintEnabled(
                             !mMapDockWidget->paintEnabled());
                     }, true, false);
        ribbonButton(p, QStringLiteral("Playtest\neingebettet"),
                     QStringLiteral("Schnelltest direkt im Editor [Umschalt+F5]"),
                     [this]() {
                         if (mPlayAction) mPlayAction->setChecked(!mPlayAction->isChecked());
                     }, true, false);
    }
    // ---- Tab: Ansicht ---------------------------------------------------------
    if (QWidget* p = ribbonPage(QStringLiteral("Ansicht"))) {
        ribbonButton(p, QStringLiteral("Spiel-\nansicht"), QStringLiteral("Zum 3D-Tab wechseln"),
                     [this]() { mCentralTabs->setCurrentWidget(mView); });
        ribbonButton(p, QStringLiteral("Landkarte"), QStringLiteral("Zur 2D-Karte wechseln"),
                     [this]() { mCentralTabs->setCurrentWidget(mMapTab); });
        ribbonButton(p, QStringLiteral("Spiel"), QStringLiteral("Zum Playtest-Tab wechseln"),
                     [this]() { mCentralTabs->setCurrentWidget(mPlayTab); });
        ribbonButton(p, QStringLiteral("Skript"), QStringLiteral("Skript-Editor öffnen [F11]"),
                     [this]() { showScriptEditor(); });
#ifdef RPGMAKER3D_ENABLE_RMLUI
        ribbonButton(p, QStringLiteral("HUD umschalten"), QStringLiteral("RmlUi-HUD ein/aus [F9]"),
                     [this]() {
                         if (mEngine && mEngine->GetRmlUi()) mEngine->GetRmlUi()->ToggleVisible();
                     });
#endif
    }
    // ---- Tab: Fenster -----------------------------------------------------------
    if (QWidget* p = ribbonPage(QStringLiteral("Fenster"))) {
        const QDockWidget* docks[] = {mDockHierarchy, mDockProperties, mDockMap,
                                      mDockDatabase, mDockEvents, mDockAssets, mDockConsole};
        for (const QDockWidget* d : docks) {
            if (!d) continue;
            QAction* toggle = d->toggleViewAction();
            ribbonButton(p, d->windowTitle(), QStringLiteral("Fenster ein-/ausblenden"),
                         [toggle]() { toggle->trigger(); });
        }
    }
    // ---- Tab: Debug -------------------------------------------------------------
    if (QWidget* p = ribbonPage(QStringLiteral("Debug"))) {
        ribbonButton(p, QStringLiteral("Konsole\nleeren"), QStringLiteral("Konsolenausgabe löschen"),
                     [this]() { if (mConsole) mConsole->clear(); });
        ribbonButton(p, QStringLiteral("Events neu\nladen"), QStringLiteral("Events der Karte neu einlesen"),
                     [this]() {
                         const int mapId = rpg::Database::Get().System().startMapId;
                         rpg::EventSystem::Get().LoadMapEvents(
                             mapId > 0 ? mapId : 1,
                             mEngine->GetProject().GetProjectPath());
                         if (mEventDockWidget) mEventDockWidget->refresh();
                         log(QStringLiteral("Events neu geladen (Debug)."));
                     });
        ribbonButton(p, QStringLiteral("Spielzustand\nzurücksetzen"),
                     QStringLiteral("Schalter/Variablen/Gruppe zurücksetzen (NewGame)"),
                     [this]() {
                         if (!mView || !mView->IsEngineReady()) return;
                         rpg::Game::Get().NewGame();
                         rpg::EventSystem::Get().RefreshAllPages();
                         log(QStringLiteral("Spielzustand zurückgesetzt."));
                     });
        ribbonButton(p, QStringLiteral("Statistik"), QStringLiteral("Szene/Event-Statistik in die Konsole"),
                     [this]() {
                         if (!mView || !mView->IsEngineReady()) return;
                         auto& scene = mEngine->GetScene();
                         const auto& events = rpg::EventSystem::Get().GetEvents();
                         log(QStringLiteral("--- Statistik ---"));
                         log(QStringLiteral("Entities: %1").arg((int)scene.GetEntities().size()));
                         log(QStringLiteral("Events:   %1").arg((int)events.size()));
                         log(QStringLiteral("Map:      %1 x %2")
                                 .arg(mEngine->GetMap().GetWidth())
                                 .arg(mEngine->GetMap().GetHeight()));
                     });
    }
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
    // Nur repainten wenn die Spielansicht sichtbar ist (spart GPU in anderen Tabs)
    if (mCentralTabs && mCentralTabs->currentWidget() == mView) {
        mView->update();
    }

    mFpsAccum += dt;
    mFpsFrames++;
    if (mFpsAccum >= 0.5f) {
        const int fps = static_cast<int>(mFpsFrames / mFpsAccum + 0.5f);
        mFpsAccum = 0.0f;
        mFpsFrames = 0;
        QString tab = QStringLiteral("?");
        if (mCentralTabs) {
            QWidget* w = mCentralTabs->currentWidget();
            if (w == mView) tab = QStringLiteral("Spielansicht");
            else if (w == mMapTab) tab = QStringLiteral("Landkarte");
            else if (w == mCode) tab = QStringLiteral("Skript");
            else if (w == mPlayTab) tab = QStringLiteral("Spiel");
        }
        mStatusInfo->setText(QStringLiteral("FPS: %1  |  Tab: %2  |  Playtest: %3  |  Projekt: %4")
            .arg(fps)
            .arg(tab)
            .arg(mEngine->IsPlaying() ? QStringLiteral("an") : QStringLiteral("aus"))
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
        ? QStringLiteral("Rückgängig (%1)  [Strg+Z]")
              .arg(QString::fromStdString(history.GetUndoName()))
        : QStringLiteral("Rückgängig  [Strg+Z]"));
    mRedoAction->setText(canRedo
        ? QStringLiteral("Wiederholen (%1)  [Strg+Y]")
              .arg(QString::fromStdString(history.GetRedoName()))
        : QStringLiteral("Wiederholen  [Strg+Y]"));
    mDeleteAction->setEnabled(mSelectedEntity >= 0 && selectedEntityExists());

    if (mSelectedEntity >= 0 && !selectedEntityExists())
        setSelectedEntity(-1);

    refreshHierarchy();
    syncPropertyValues();

    if (mPlayTabStatus) {
        mPlayTabStatus->setText(mEngine->IsPlaying()
            ? QStringLiteral("Status: eingebetteter Playtest läuft")
            : QStringLiteral("Status: bereit"));
    }
}

// ---------------------------------------------------------------------------
// Playtest
// ---------------------------------------------------------------------------

void QtEditorWindow::onPlaytestToggled(bool on) {
    if (!mView->IsEngineReady()) return;
    if (on) {
        // Vor dem Start fragen (oder automatisch), ob gespeichert werden soll:
        // Skripte/Events/Karte liest die Runtime von der Festplatte.
        if (!confirmPlaytestSave()) {
            if (mPlayAction) {
                mPlayAction->blockSignals(true);
                mPlayAction->setChecked(false);
                mPlayAction->blockSignals(false);
            }
            if (mPlayTabEmbeddedBtn) {
                mPlayTabEmbeddedBtn->blockSignals(true);
                mPlayTabEmbeddedBtn->setChecked(false);
                mPlayTabEmbeddedBtn->blockSignals(false);
            }
            log(QStringLiteral("Eingebetteter Playtest abgebrochen (Speicherfrage)."));
            return;
        }
    }
    mEngine->SetPlaying(on);
    log(on ? QStringLiteral("Eingebetteter Playtest gestartet.")
           : QStringLiteral("Eingebetteter Playtest gestoppt."));
    if (on && mCentralTabs) {
        mCentralTabs->setCurrentWidget(mView);
    }
    if (mCode) mCode->refresh(); // Skripte während Playtest schreibgeschützt
    if (mPlayTabStatus)
        mPlayTabStatus->setText(on ? QStringLiteral("Status: Playtest läuft (eingebettet) …")
                                   : QStringLiteral("Status: bereit"));
}

QString QtEditorWindow::findPlayerExecutable() const {
    const QString dir = QCoreApplication::applicationDirPath();
    const QStringList names = {
        QStringLiteral("RPGMaker3D_Player.exe"),
        QStringLiteral("RPGMaker3D_Player")
    };
    // MSVC legt in Konfigurations-Unterordner ab; auch Elternordner prüfen
    // (z.B. Editor in build/Release, Player in build/Debug – ungewöhnlich,
    // aber abgedeckt).
    const QStringList subdirs = {
        QString(), QStringLiteral("Debug"), QStringLiteral("Release"),
        QStringLiteral("RelWithDebInfo"), QStringLiteral("MinSizeRel")
    };
    QStringList roots = {dir, dir + QStringLiteral("/..")};
    for (const QString& root : roots) {
        for (const QString& name : names) {
            for (const QString& sub : subdirs) {
                QString p = root + (sub.isEmpty() ? QString() : QStringLiteral("/") + sub)
                          + QStringLiteral("/") + name;
                QFileInfo fi(p);
                if (fi.exists() && fi.isFile())
                    return fi.canonicalFilePath();
            }
        }
    }
    return QString();
}

void QtEditorWindow::actionPlaytestPlayer() {
    if (!mView->IsEngineReady()) return;
    const std::string projectPath = mEngine->GetProject().GetProjectPath();
    if (projectPath.empty()) {
        QMessageBox::information(this, QStringLiteral("Playtest"),
            QStringLiteral("Kein Projekt geladen.\n"
                           "Bitte zuerst ein Projekt öffnen oder anlegen."));
        return;
    }

    // Vor dem Start fragen (bzw. automatisch speichern, wenn so eingestellt).
    // Die Player-exe liest ALLES von der Festplatte – ohne Speichern würde
    // sie einen alten Stand testen.
    if (!confirmPlaytestSave()) {
        log(QStringLiteral("Playtest abgebrochen (Speicherfrage)."));
        return;
    }

    const QString exe = findPlayerExecutable();
    if (exe.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Playtest (Player-exe)"),
            QStringLiteral("Die Player-exe wurde nicht gefunden.\n\n"
                           "Gesucht: RPGMaker3D_Player(.exe) neben der Editor-exe:\n%1\n\n"
                           "Baue das CMake-Target 'RPGMaker3D_Player' "
                           "(Option RPGMAKER3D_BUILD_PLAYER=ON), dann liegt sie ")
                .arg(QCoreApplication::applicationDirPath()));
        log(QStringLiteral("Playtest: Player-exe nicht gefunden (Build-Target RPGMaker3D_Player)."));
        return;
    }

    const QString proj = QString::fromStdString(projectPath);
    QStringList args;
    args << QStringLiteral("--project") << proj;

    if (QProcess::startDetached(exe, args)) {
        log(QStringLiteral("Playtest gestartet: %1 --project \"%2\"").arg(exe, proj));
        statusBar()->showMessage(QStringLiteral("Player läuft im eigenen Fenster …"), 5000);
    } else {
        QMessageBox::warning(this, QStringLiteral("Playtest (Player-exe)"),
            QStringLiteral("Der Player konnte nicht gestartet werden:\n%1").arg(exe));
        log(QStringLiteral("Playtest FEHLER: Player-exe startete nicht: %1").arg(exe));
    }
}

// ---------------------------------------------------------------------------
// „Spiel exportieren" (PAKET 8): fertige Auslieferung bauen
//   <Ziel>/<Spielname>/Game.exe   = Player-exe (umbenannt, XP-Anmutung)
//   <Ziel>/<Spielname>/Game/      = Projektordner (ohne saves/ und .git/)
//   <Ziel>/<Spielname>/*.dll      = neben der Player-exe liegende DLLs
//                                   (Qt6-DLLs ausgenommen – die braucht nur
//                                   der Editor)
//   <Ziel>/<Spielname>/LIESMICH.txt = Start- + vc_redist-Hinweis
// Die Game.exe findet „./Game/project.json" automatisch (player_main.cpp
// ParseProjectPath – kein Kommandozeilen-Argument noetig, XP-Gefuehl).
// ---------------------------------------------------------------------------
void QtEditorWindow::actionExportGame() {
    if (!mView || !mView->IsEngineReady()) return;
    const std::string projectPath = mEngine->GetProject().GetProjectPath();
    if (projectPath.empty()) {
        QMessageBox::information(this, QStringLiteral("Spiel exportieren"),
            QStringLiteral("Kein Projekt geladen.\n"
                           "Bitte zuerst ein Projekt öffnen oder anlegen."));
        return;
    }

    // Der Export kopiert die Dateien von der Festplatte -> Speicherfrage
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Spiel exportieren"));
    box.setIcon(QMessageBox::Question);
    box.setText(QStringLiteral("Das Projekt vor dem Export speichern?"));
    box.setInformativeText(QStringLiteral(
        "Der Export enthält den Stand auf der Festplatte –\n"
        "ohne Speichern fehlen die letzten Änderungen."));
    auto* saveBtn = box.addButton(QStringLiteral("Speichern && Exportieren"),
                                  QMessageBox::AcceptRole);
    auto* skipBtn = box.addButton(QStringLiteral("Ohne Speichern exportieren"),
                                  QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("Abbrechen"), QMessageBox::RejectRole);
    box.setDefaultButton(saveBtn);
    box.exec();
    if (box.clickedButton() == saveBtn) {
        saveAllForPlaytest();
    } else if (box.clickedButton() != skipBtn) {
        log(QStringLiteral("Export abgebrochen (Speicherfrage)."));
        return;
    }

    // Ohne Player-exe waere das exportierte Spiel nicht startbar -> Abbruch.
    const QString exe = findPlayerExecutable();
    if (exe.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Spiel exportieren"),
            QStringLiteral("Die Player-exe wurde nicht gefunden – ohne sie wäre der\n"
                           "Export nicht spielbar.\n\n"
                           "Baue das CMake-Target 'RPGMaker3D_Player' und versuche es erneut."));
        log(QStringLiteral("Export abgebrochen: Player-exe fehlt (Build-Target RPGMaker3D_Player)."));
        return;
    }

    const QString targetRoot = QFileDialog::getExistingDirectory(this,
        QStringLiteral("Zielordner für den Export wählen"));
    if (targetRoot.isEmpty()) return;

    // Ordnername aus dem Spielnamen (project.json -> info.name)
    const QString folderName = exportGameFolderName(
        QString::fromStdString(mEngine->GetProject().GetInfo().name));
    const QString outDir = targetRoot + QStringLiteral("/") + folderName;
    if (QFileInfo::exists(outDir)) {
        const auto ret = QMessageBox::question(this, QStringLiteral("Spiel exportieren"),
            QStringLiteral("Der Ordner existiert bereits:\n%1\n\n"
                           "Dateien werden aktualisiert bzw. überschrieben. Fortfahren?")
                .arg(outDir));
        if (ret != QMessageBox::Yes) return;
    }
    log(QStringLiteral("Exportiere Spiel nach %1 …").arg(outDir));

    int copied = 0, failed = 0;

    // 1) Projektordner -> <ziel>/<name>/Game/ (Top-Level ohne saves/ + .git/)
    copyProjectRecursive(QString::fromStdString(projectPath),
                         outDir + QStringLiteral("/Game"), copied, failed, true);

    // 2) Player-exe -> <ziel>/<name>/Game.exe
    const QFileInfo exeInfo(exe);
    const bool isExe = exeInfo.suffix().compare(QStringLiteral("exe"),
                                                Qt::CaseInsensitive) == 0;
    const QString gameExe = outDir + (isExe ? QStringLiteral("/Game.exe")
                                            : QStringLiteral("/Game"));
    if (!ensureParentDir(gameExe)) {
        ++failed;
    } else {
        QFile::remove(gameExe); // QFile::copy ueberschreibt nicht
        if (QFile::copy(exe, gameExe)) ++copied; else ++failed;
    }

    // 3) DLLs neben der Player-exe mitnehmen (SDL2.dll & Co). Qt-DLLs
    //    ausgenommen – die gehoeren zum Editor, nicht zum Spiel.
    const QDir exeDir = exeInfo.absoluteDir();
    const QStringList dlls = exeDir.entryList({QStringLiteral("*.dll")}, QDir::Files);
    for (const QString& dll : dlls) {
        if (dll.startsWith(QStringLiteral("Qt"), Qt::CaseInsensitive)) continue;
        const QString to = outDir + QStringLiteral("/") + dll;
        QFile::remove(to);
        if (QFile::copy(exeDir.absoluteFilePath(dll), to)) ++copied; else ++failed;
    }

    // 4) LIESMICH (Start + vc_redist-Hinweis)
    writeExportReadme(outDir);

    if (failed == 0) {
        log(QStringLiteral("Spiel exportiert: %1 (%2 Dateien kopiert).")
                .arg(outDir).arg(copied));
        statusBar()->showMessage(QStringLiteral("Export fertig: %1").arg(outDir), 8000);
        QMessageBox::information(this, QStringLiteral("Spiel exportieren"),
            QStringLiteral("Das Spiel wurde exportiert nach:\n%1\n\n"
                           "Start: Game.exe – das Projekt liegt im Ordner „Game“ daneben.\n"
                           "Auf anderen PCs ist ggf. die VC++-Laufzeit (vc_redist) nötig,\n"
                           "siehe LIESMICH.txt im Exportordner.").arg(outDir));
    } else {
        log(QStringLiteral("Export mit FEHLERN abgeschlossen: %1 kopiert, %2 fehlgeschlagen (%3).")
                .arg(copied).arg(failed).arg(outDir));
        QMessageBox::warning(this, QStringLiteral("Spiel exportieren"),
            QStringLiteral("Der Export lief, aber %1 Datei(en) konnten nicht kopiert werden\n"
                           "(Details in der Konsole). Ziel:\n%2").arg(failed).arg(outDir));
    }
}

void QtEditorWindow::StartBattleTest(int troopId) {
    if (!mView->IsEngineReady()) return;
    const std::string projectPath = mEngine->GetProject().GetProjectPath();
    if (projectPath.empty()) {
        QMessageBox::information(this, QStringLiteral("Kampftest"),
            QStringLiteral("Kein Projekt geladen.\n"
                           "Bitte zuerst ein Projekt öffnen oder anlegen."));
        return;
    }
    // Wie beim normalen Playtest: Die Player-exe liest alles von der
    // Festplatte, also vorher speichern (bzw. fragen).
    if (!confirmPlaytestSave()) {
        log(QStringLiteral("Kampftest abgebrochen (Speicherfrage)."));
        return;
    }
    const QString exe = findPlayerExecutable();
    if (exe.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Kampftest"),
            QStringLiteral("Die Player-exe wurde nicht gefunden.\n"
                           "Baue das CMake-Target 'RPGMaker3D_Player'."));
        log(QStringLiteral("Kampftest: Player-exe nicht gefunden."));
        return;
    }
    const QString proj = QString::fromStdString(projectPath);
    QStringList args;
    args << QStringLiteral("--project") << proj
         << (QStringLiteral("--battletest=") + QString::number(troopId > 0 ? troopId : 1));
    if (QProcess::startDetached(exe, args)) {
        log(QStringLiteral("Kampftest gestartet: Trupp %1 (%2)").arg(troopId).arg(exe));
        statusBar()->showMessage(QStringLiteral("Kampftest läuft im eigenen Fenster …"), 5000);
    } else {
        QMessageBox::warning(this, QStringLiteral("Kampftest"),
            QStringLiteral("Der Player konnte nicht gestartet werden:\n%1").arg(exe));
        log(QStringLiteral("Kampftest FEHLER: Player-exe startete nicht."));
    }
}

// ---------------------------------------------------------------------------
// Datei-Aktionen
// ---------------------------------------------------------------------------

void QtEditorWindow::closeEvent(QCloseEvent* event) {
    if (mCode && mCode->hasUnsavedChanges()) {
        const auto r = QMessageBox::question(this, QStringLiteral("Beenden"),
            QStringLiteral("Ungespeicherte Skript-Änderungen. Trotzdem beenden?"),
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

// "Alles custom": Game.ini-Vorlage mit erklaerten Schaltern anlegen
// (nie ueberschreiben - nur wenn keine existiert).
static void ensureGameIniTemplate(const QString& projectPath) {
    const QString iniPath = projectPath + QStringLiteral("/Game.ini");
    if (QFile::exists(iniPath)) return;
    QFile ini(iniPath);
    if (ini.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ini.write(QStringLiteral(
            "; RPG Maker 3D ---- Alles custom ----------------------------------\n"
            "; Eingebaute Oberflaechen abschalten (0) und durch eigene\n"
            "; Ruby-Szenen / RmlUi-Skins ersetzen. Standard ist 1 (an).\n"
            "[RPG Maker 3D]\n"
            "NativeTitle=1        ; 0 = kein eingebauter Titel -> Ruby-Hook Game.custom_title\n"
            "NativeHud=1          ; 0 = HUD beim Start aus (UI.hud_visible= steuert)\n"
            "NativeGameMenu=1     ; 0 = Esc oeffnet NICHT das eingebaute Spielmenue\n"
            "NativeBattleMenu=1   ; 0 = kein eingebautes Kampfmenue (Battle-API nutzen)\n"
            "NativeBattleStatus=1 ; 0 = keine eingebaute Gegner-/Gruppenzeile im Kampf\n"
            "\n"
            "; UI-Skins: optional eigene Oberflaechen-Dateien in <Projekt>/UI/\n"
            ";   Skin.rcss = komplettes Stylesheet, Game.rml = HUD-Layout\n"
            ";   siehe README Abschnitt 'Alles custom'.\n").toUtf8());
    }
}

void QtEditorWindow::actionNewProject() {
    if (!mView->IsEngineReady()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Neues Projekt"),
                                               QStringLiteral("Projektname:"),
                                               QLineEdit::Normal,
                                               QStringLiteral("NeuesRPG"), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const QString parent = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Basisordner für das Projekt wählen"));
    if (parent.isEmpty()) return;

    const QString path = parent + QStringLiteral("/") + name.trimmed();
    if (!mEngine->GetProject().New(path.toStdString(), name.trimmed().toStdString())) {
        QMessageBox::warning(this, QStringLiteral("Neues Projekt"),
                             QStringLiteral("Projekt konnte nicht angelegt werden."));
        return;
    }
    mEngine->GetScriptManager().CreateDefaultScripts(path.toStdString());
    mEngine->GetScriptManager().LoadProjectScripts(path.toStdString());
    ensureGameIniTemplate(path); // "Alles custom": Schalter direkt sichtbar
    log(QStringLiteral("Neues Projekt angelegt: ") + path);
    addRecentProject(path);
    afterProjectChanged();
}

void QtEditorWindow::actionOpenProject() {
    if (!mView->IsEngineReady()) return;
    // Startordner: zuletzt benutzter Projektordner (merkt sich QSettings)
    QSettings s;
    const QString startDir = s.value(QStringLiteral("ui/lastProjectDir")).toString();
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Projekt öffnen (Projektordner mit project.json)"), startDir);
    if (path.isEmpty()) return;
    openProjectPath(path);
}

void QtEditorWindow::openProjectPath(const QString& path) {
    if (!mView->IsEngineReady()) return;
    if (!QFile::exists(path + QStringLiteral("/project.json"))) {
        QMessageBox::warning(this, QStringLiteral("Projekt öffnen"),
            QStringLiteral("Kein gültiges Projekt (project.json nicht gefunden):\n") + path);
        // Verwaisten Eintrag aus der Zuletzt-Liste entfernen
        QSettings s;
        QStringList recent = s.value(QStringLiteral("recentProjects")).toStringList();
        if (recent.removeAll(path) > 0) {
            s.setValue(QStringLiteral("recentProjects"), recent);
            rebuildRecentProjectsMenu();
        }
        return;
    }

    if (!mEngine->GetProject().Load(path.toStdString())) {
        QMessageBox::warning(this, QStringLiteral("Projekt öffnen"),
            QStringLiteral("Projekt konnte nicht geladen werden:\n") + path);
        return;
    }
    rpg::Database::Get().Load(path.toStdString());
    mEngine->GetScriptManager().LoadProjectScripts(path.toStdString());
    if (mEngine->GetScriptManager().GetScripts().empty()) {
        mEngine->GetScriptManager().CreateDefaultScripts(path.toStdString());
        mEngine->GetScriptManager().LoadProjectScripts(path.toStdString());
    }
    ensureGameIniTemplate(path); // "Alles custom": Schalter nachruesten
    QSettings().setValue(QStringLiteral("ui/lastProjectDir"), path);
    log(QStringLiteral("Projekt geladen: ") + path);
    addRecentProject(path);
    loadScenePackage();
    afterProjectChanged();
}

void QtEditorWindow::actionSaveProject() {
    if (!mView->IsEngineReady()) return;
    if (mEngine->GetProject().GetProjectPath().empty()) {
        QMessageBox::information(this, QStringLiteral("Speichern"),
            QStringLiteral("Kein Projekt geladen – bitte zuerst ein Projekt anlegen oder öffnen."));
        return;
    }
    if (mCode) mCode->saveAll();
    saveScenePackage();
    log(QStringLiteral("Projekt gespeichert: ")
        + QString::fromStdString(mEngine->GetProject().GetProjectPath()));
}

void QtEditorWindow::actionSaveSceneAs() {
    if (!mView->IsEngineReady()) return;
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Szene speichern unter"), QString(),
        QStringLiteral("JSON-Dateien (*.json)"));
    if (path.isEmpty()) return;
    mEngine->SaveScene(path.toStdString());
    log(QStringLiteral("Szene gespeichert: ") + path);
}

void QtEditorWindow::actionLoadSceneFrom() {
    if (!mView->IsEngineReady()) return;
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Szene laden"), QString(),
        QStringLiteral("JSON-Dateien (*.json)"));
    if (path.isEmpty()) return;
    if (mEngine->LoadScene(path.toStdString())) {
        setSelectedEntity(-1);
        log(QStringLiteral("Szene geladen: ") + path);
    } else {
        QMessageBox::warning(this, QStringLiteral("Szene laden"),
                             QStringLiteral("Laden fehlgeschlagen:\n") + path);
    }
}

void QtEditorWindow::loadScenePackage() {
    auto& proj = mEngine->GetProject();
    const std::string scenePath = proj.GetProjectPath() + "/scene.json";
    if (!mEngine->LoadScene(scenePath)) {
        // Startkarte aus der Datenbank laden (nicht fest Karte 1 – der Player
        // laedt ebenfalls die Startkarte, so verhalten sich beide gleich)
        int startId = rpg::Database::Get().System().startMapId;
        if (startId <= 0) startId = 1;
        const std::string mapPath = proj.GetMapPath(startId);
        if (mEngine->GetMap().Load(mapPath)) {
            log(QStringLiteral("Karte geladen (binär): ") + QString::fromStdString(mapPath));
        } else {
            log(QStringLiteral("Hinweis: keine scene.json/Karte gefunden – leere Szene."));
        }
    } else {
        log(QStringLiteral("Szene geladen: ") + QString::fromStdString(scenePath));
    }
    // Events der Startkarte laden (XP: Map-Events gehören zur Karte)
    const int mapId = rpg::Database::Get().System().startMapId;
    rpg::EventSystem::Get().LoadMapEvents(mapId > 0 ? mapId : 1, proj.GetProjectPath());
}

void QtEditorWindow::saveScenePackage() {
    auto& proj = mEngine->GetProject();
    const std::string pp = proj.GetProjectPath();
    mEngine->SaveScene(pp + "/scene.json");
    // Aktive Karte unter IHRER ID speichern (nicht fest map1 – sonst wuerde
    // bei Startkarte != 1 die falsche Datei beschrieben und der Player laedt
    // eine andere Karte als im Editor bearbeitet wurde).
    int activeId = rpg::Database::Get().System().startMapId;
    if (mMapDockWidget) {
        const int idx = mMapDockWidget->selectedMapIndex();
        auto& infos = rpg::Database::Get().MapInfos();
        if (idx >= 0 && idx < static_cast<int>(infos.size()))
            activeId = infos[static_cast<size_t>(idx)].id;
    }
    if (activeId <= 0) activeId = 1;
    mEngine->GetMap().Save(proj.GetMapPath(activeId));
    rpg::Database::Get().Save(pp);
    rpg::EventSystem::Get().SaveMapEvents(activeId, pp);
    proj.Save();
}

void QtEditorWindow::afterProjectChanged() {
    setSelectedEntity(-1);
    setWindowTitle(QStringLiteral("RPG Maker 3D - Editor  [%1]")
        .arg(QString::fromStdString(mEngine->GetProject().GetInfo().name)));
    if (mCode) mCode->refresh();
    if (mMapTab) mMapTab->refresh();
    if (mMapDockWidget) mMapDockWidget->refresh();
    if (mDbDockWidget) mDbDockWidget->refresh();
    if (mEventDockWidget) mEventDockWidget->refresh();
    if (mAssetDockWidget) mAssetDockWidget->refresh();
    updateMapStatus();
    updatePlayTabInfo();
}

// ---------------------------------------------------------------------------
// Easy-to-use: Zuletzt geöffnete Projekte / Willkommen / Tastenkürzel
// ---------------------------------------------------------------------------

void QtEditorWindow::addRecentProject(const QString& path) {
    if (path.isEmpty()) return;
    QSettings s;
    QStringList recent = s.value(QStringLiteral("recentProjects")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 8) recent.removeLast();
    s.setValue(QStringLiteral("recentProjects"), recent);
    rebuildRecentProjectsMenu();
}

void QtEditorWindow::rebuildRecentProjectsMenu() {
    if (!mRecentMenu) return;
    mRecentMenu->clear();
    const QStringList recent =
        QSettings().value(QStringLiteral("recentProjects")).toStringList();
    if (recent.isEmpty()) {
        auto* a = mRecentMenu->addAction(QStringLiteral("(keine zuletzt geöffneten Projekte)"));
        a->setEnabled(false);
        return;
    }
    for (const QString& p : recent) {
        const bool valid = QFile::exists(p + QStringLiteral("/project.json"));
        auto* a = mRecentMenu->addAction(QStringLiteral("%1%2")
            .arg(valid ? QString() : QStringLiteral("⚠ "))
            .arg(p), this, [this, p]() { openProjectPath(p); });
        a->setToolTip(valid ? QStringLiteral("Projekt öffnen")
                            : QStringLiteral("project.json nicht gefunden – Eintrag wird beim Öffnen entfernt"));
    }
    mRecentMenu->addSeparator();
    mRecentMenu->addAction(QStringLiteral("Liste leeren"), this, [this]() {
        QSettings().remove(QStringLiteral("recentProjects"));
        rebuildRecentProjectsMenu();
    });
}

void QtEditorWindow::showWelcomeDialog() {
    if (!mView || !mView->IsEngineReady()) return;
    QSettings s;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Willkommen bei RPG Maker 3D"));
    dlg.setMinimumWidth(520);
    auto* lay = new QVBoxLayout(&dlg);

    auto* title = new QLabel(QStringLiteral("Willkommen bei RPG Maker 3D!"), &dlg);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    lay->addWidget(title);
    auto* intro = new QLabel(QStringLiteral(
        "Womit möchtest du beginnen? Neue und vorhandene Projekte erreichst du "
        "später jederzeit über das Menü „Datei“."), &dlg);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    auto* btnRow = new QHBoxLayout();
    auto* bNew = new QPushButton(QStringLiteral("Neues Projekt …"), &dlg);
    bNew->setToolTip(QStringLiteral("Legt einen neuen Projektordner mit Standard-Karte, "
                                    "Datenbank und Skripten an. [Strg+N]"));
    auto* bOpen = new QPushButton(QStringLiteral("Projekt öffnen …"), &dlg);
    bOpen->setToolTip(QStringLiteral("Öffnet einen vorhandenen Projektordner. [Strg+O]"));
    btnRow->addWidget(bNew, 1);
    btnRow->addWidget(bOpen, 1);
    lay->addLayout(btnRow);

    const QStringList recent =
        QSettings().value(QStringLiteral("recentProjects")).toStringList();
    QListWidget* recentList = nullptr;
    if (!recent.isEmpty()) {
        lay->addSpacing(6);
        lay->addWidget(new QLabel(QStringLiteral(
            "Zuletzt geöffnete Projekte (Doppelklick öffnet):"), &dlg));
        recentList = new QListWidget(&dlg);
        for (const QString& p : recent) {
            const bool valid = QFile::exists(p + QStringLiteral("/project.json"));
            auto* it = new QListWidgetItem(QStringLiteral("%1%2")
                .arg(valid ? QString() : QStringLiteral("⚠ ")).arg(p), recentList);
            it->setToolTip(p);
        }
        recentList->setMaximumHeight(150);
        lay->addWidget(recentList);
    }

    auto* chk = new QCheckBox(QStringLiteral("Beim Start anzeigen"), &dlg);
    chk->setChecked(s.value(QStringLiteral("ui/showWelcome"), true).toBool());
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    auto* row = new QHBoxLayout();
    row->addWidget(chk, 1);
    row->addWidget(btnBox, 0);
    lay->addSpacing(8);
    lay->addLayout(row);

    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(bNew, &QPushButton::clicked, &dlg, [this, &dlg]() {
        dlg.accept();
        actionNewProject();
    });
    QObject::connect(bOpen, &QPushButton::clicked, &dlg, [this, &dlg]() {
        dlg.accept();
        actionOpenProject();
    });
    if (recentList) {
        QObject::connect(recentList, &QListWidget::itemActivated, &dlg,
                         [this, &dlg](QListWidgetItem* it) {
            QString p = it->text();
            if (p.startsWith(QStringLiteral("⚠ "))) p = p.mid(2);
            dlg.accept();
            openProjectPath(p);
        });
    }
    dlg.exec();
    s.setValue(QStringLiteral("ui/showWelcome"), chk->isChecked());
}

void QtEditorWindow::showShortcutsDialog() {
    QMessageBox box(QMessageBox::Information,
        QStringLiteral("Tastenkürzel"),
        QStringLiteral(
            "<b>Allgemein</b><br>"
            "Strg+N – Neues Projekt<br>"
            "Strg+O – Projekt öffnen<br>"
            "Strg+S – Projekt speichern<br>"
            "F5 – Playtest (Player-exe)<br>"
            "Umschalt+F5 – Playtest eingebettet<br>"
            "F9 – HUD ein/aus<br>"
            "F10 – Debug-Inspektor: Schalter/Variablen live (wie XP-F9)<br><br>"
            "<b>Landkarte (2D)</b><br>"
            "Strg+Z / Strg+Y – Rückgängig / Wiederholen<br>"
            "Links ziehen – Tile malen<br>"
            "Rechtsklick – Menü (Startposition, Ereignis, Eigenschaften)<br>"
            "EV-Modus: Doppelklick – Ereignis anlegen/bearbeiten<br>"
            "EV-Modus: Entf – Ereignis löschen<br><br>"
            "<b>3D-Spielansicht</b><br>"
            "Strg+Z / Strg+Y – Objekt-Verlauf<br>"
            "Entf – Ausgewähltes Objekt löschen<br>"
            "WASD + Maus – Kamera<br><br>"
            "<b>Skript</b><br>"
            "F2 – Skript umbenennen"),
        QMessageBox::Ok, this);
    box.setTextFormat(Qt::RichText);
    box.exec();
}

void QtEditorWindow::updateMapStatus() {
    if (!mStatusMap) return;
    auto& infos = rpg::Database::Get().MapInfos();
    const int idx = mMapDockWidget ? mMapDockWidget->selectedMapIndex() : -1;
    if (idx < 0 || idx >= static_cast<int>(infos.size())) {
        mStatusMap->setText(QStringLiteral("Karte: -"));
        return;
    }
    const auto& info = infos[static_cast<size_t>(idx)];
    mStatusMap->setText(QStringLiteral("Karte: %1 (ID %2, %3×%4)")
        .arg(QString::fromStdString(info.name))
        .arg(info.id).arg(info.width).arg(info.height));
}

// ---------------------------------------------------------------------------
// Spiel-Tab: Übersicht + Speicherfrage + Ruby-Prüfung
// ---------------------------------------------------------------------------

void QtEditorWindow::updatePlayTabInfo() {
    if (!mPlayTabInfo) return;
    const auto& proj = mEngine->GetProject();
    const std::string pp = proj.GetProjectPath();
    if (pp.empty()) {
        mPlayTabInfo->setText(QStringLiteral(
            "(Kein Projekt geladen – Datei > Neues Projekt anlegen oder öffnen.)"));
    } else {
        const auto& sys = rpg::Database::Get().System();
        const auto& infos = rpg::Database::Get().MapInfos();
        QString startName = QStringLiteral("(keine)");
        for (const auto& mi : infos)
            if (mi.id == sys.startMapId) {
                startName = QStringLiteral("%1 (ID %2)")
                    .arg(QString::fromStdString(mi.name)).arg(mi.id);
                break;
            }
        const int scripts = static_cast<int>(
            mEngine->GetScriptManager().GetScripts().size());
        const int events = static_cast<int>(
            rpg::EventSystem::Get().GetEvents().size());
        QString gameTitle = QString::fromStdString(sys.gameTitle);
        if (gameTitle.trimmed().isEmpty()) gameTitle = QStringLiteral("(nicht gesetzt)");
        mPlayTabInfo->setText(QStringLiteral(
            "Name: %1\nOrdner: %2\nSpieltitel: %3\nStartkarte: %4\n"
            "Karten: %5   •   Ereignisse (aktive Karte): %6   •   Ruby-Skripte: %7")
            .arg(QString::fromStdString(proj.GetInfo().name))
            .arg(QString::fromStdString(pp))
            .arg(gameTitle)
            .arg(startName)
            .arg(static_cast<int>(infos.size()))
            .arg(events)
            .arg(scripts));
    }
    if (mPlayTabExeStatus) {
        const QString exe = findPlayerExecutable();
        if (exe.isEmpty()) {
            mPlayTabExeStatus->setText(QStringLiteral(
                "⚠ Player-exe nicht gefunden: Bitte das CMake-Target „RPGMaker3D_Player“ "
                "bauen (CI baut beides). Der eingebettete Test funktioniert trotzdem."));
        } else {
            mPlayTabExeStatus->setText(QStringLiteral("✓ Player-exe gefunden: %1").arg(exe));
        }
    }
}

bool QtEditorWindow::confirmPlaytestSave() {
    if (QSettings().value(QStringLiteral("ui/autoSaveBeforePlaytest"), false).toBool()) {
        saveAllForPlaytest();
        return true;
    }
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Playtest"));
    box.setIcon(QMessageBox::Question);
    box.setText(QStringLiteral("Das Projekt vor dem Playtest speichern?"));
    box.setInformativeText(QStringLiteral(
        "Skripte, Karte, Events und Datenbank liest das Spiel von der Festplatte –\n"
        "ohne Speichern testest du einen älteren Stand."));
    auto* saveBtn = box.addButton(QStringLiteral("Speichern && Starten"),
                                  QMessageBox::AcceptRole);
    auto* skipBtn = box.addButton(QStringLiteral("Ohne Speichern starten"),
                                  QMessageBox::DestructiveRole);
    auto* cancelBtn = box.addButton(QStringLiteral("Abbrechen"),
                                    QMessageBox::RejectRole);
    (void)cancelBtn;
    box.setDefaultButton(saveBtn);
    QCheckBox chk(QStringLiteral("Immer speichern – nicht mehr fragen"));
    box.setCheckBox(&chk);
    box.exec();
    if (box.clickedButton() == saveBtn) {
        if (chk.isChecked()) {
            QSettings().setValue(QStringLiteral("ui/autoSaveBeforePlaytest"), true);
            if (mAutoSaveCheck) {
                mAutoSaveCheck->blockSignals(true);
                mAutoSaveCheck->setChecked(true);
                mAutoSaveCheck->blockSignals(false);
            }
        }
        saveAllForPlaytest();
        return true;
    }
    if (box.clickedButton() == skipBtn) {
        log(QStringLiteral("Playtest OHNE Speichern – das Spiel zeigt den letzten gespeicherten Stand."));
        return true;
    }
    return false; // Abbrechen / Fenster geschlossen
}

void QtEditorWindow::saveAllForPlaytest() {
    if (mCode) mCode->saveAll();
    actionSaveProject(); // prüft selbst auf fehlendes Projekt (mit Hinweis)
}

void QtEditorWindow::runScriptCheck() {
    if (!mView || !mView->IsEngineReady() || !mScriptCheckOutput) return;
    if (mCode) mCode->saveAll(); // sonst würden alte Datei-Stände geprüft
    std::vector<std::string> errors;
    const auto& scripts = mEngine->GetScriptManager().GetScripts();
    mEngine->GetScriptManager().ValidateAllScripts(errors);
    if (errors.empty()) {
        mScriptCheckOutput->setPlainText(QStringLiteral(
            "✓ Alle %1 Ruby-Skripte sind syntaktisch korrekt.")
            .arg(static_cast<int>(scripts.size())));
        log(QStringLiteral("Ruby-Prüfung: alle Skripte OK."));
    } else {
        QString txt = QStringLiteral("✗ %1 Syntaxfehler gefunden:\n\n").arg(errors.size());
        for (const auto& e : errors)
            txt += QStringLiteral("• ") + QString::fromStdString(e) + QStringLiteral("\n");
        mScriptCheckOutput->setPlainText(txt);
        log(QStringLiteral("Ruby-Prüfung: %1 Fehler gefunden (Details im Spiel-Tab).")
            .arg(errors.size()));
        if (mCentralTabs) mCentralTabs->setCurrentWidget(mPlayTab);
    }
    updatePlayTabInfo();
}

// ---------------------------------------------------------------------------
// Erstellen / Bearbeiten
// ---------------------------------------------------------------------------

void QtEditorWindow::createSimpleEntity(int kind) {
    if (!mView->IsEngineReady()) return;
    const char* names[] = {"Würfel", "Ebene", "Licht"};

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

    log(QStringLiteral("Erstellt: %1 (ID %2)").arg(QString::fromUtf8(names[kind]))
        .arg(static_cast<int>(id)));
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
    log(QStringLiteral("Rückgängig: %1").arg(QString::fromStdString(history.GetRedoName())));
    refreshHierarchy();
    rebuildProperties();
}

void QtEditorWindow::actionRedo() {
    if (!mView->IsEngineReady()) return;
    auto& history = mEngine->GetCommandHistory();
    if (!history.CanRedo()) return;
    history.Redo(*mEngine);
    log(QStringLiteral("Wiederholen: %1").arg(QString::fromStdString(history.GetUndoName())));
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
    log(QStringLiteral("Gelöscht: ID %1").arg(mSelectedEntity));
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
        layout->addWidget(new QLabel(QStringLiteral(
            "Kein Objekt ausgewählt.\n\n"
            "Objekt in der Hierarchie anklicken, oder\n"
            "über 'Erstellen' ein neues Objekt anlegen."), root));
        layout->addStretch(1);
        return root;
    }

    auto* baseBox = new QGroupBox(QStringLiteral("Basis"), root);
    auto* baseForm = new QFormLayout(baseBox);
    mNameEdit = new QLineEdit(QString::fromStdString(scene.GetEntityName(id)), baseBox);
    baseForm->addRow(QStringLiteral("Name"), mNameEdit);
    baseForm->addRow(QStringLiteral("ID"), new QLabel(QString::number(static_cast<int>(id)), baseBox));
    layout->addWidget(baseBox);
    connect(mNameEdit, &QLineEdit::editingFinished, this, [this, id]() {
        if (!selectedEntityExists()) return;
        mEngine->GetScene().SetEntityName(id, mNameEdit->text().toStdString());
        mLastHierarchyIds.clear();
        refreshHierarchy();
        log(QStringLiteral("Umbenannt: ID %1 -> '%2'").arg(static_cast<int>(id)).arg(mNameEdit->text()));
    });

    auto* tComp = scene.GetComponent<rpg::TransformComponent>(id);
    if (tComp) {
        auto* tBox = new QGroupBox(QStringLiteral("Transform"), root);
        auto* tForm = new QFormLayout(tBox);
        const auto mkRow = [this, tForm, tBox](const QString& label, QDoubleSpinBox** dst,
                                               float x, float y, float z,
                                               double min, double max, double step) {
            auto* row = new QWidget(tBox);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            const float v[3] = {x, y, z};
            const char* axisNames[3] = {"x", "y", "z"};
            for (int i = 0; i < 3; ++i) {
                hl->addWidget(new QLabel(QLatin1String(axisNames[i]), row));
                dst[i] = makeSpin(min, max, step, v[i]);
                hl->addWidget(dst[i]);
            }
            tForm->addRow(label, row);
        };
        mkRow(QStringLiteral("Position"), mPos, tComp->transform.position.x,
              tComp->transform.position.y, tComp->transform.position.z,
              -100000.0, 100000.0, 0.1);
        mkRow(QStringLiteral("Rotation (rad)"), mRot, tComp->transform.rotation.x,
              tComp->transform.rotation.y, tComp->transform.rotation.z,
              -360.0 * 100.0, 360.0 * 100.0, 0.1);
        mkRow(QStringLiteral("Skalierung"), mScale, tComp->transform.scale.x,
              tComp->transform.scale.y, tComp->transform.scale.z,
              0.001, 100000.0, 0.1);
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

    auto* compBox = new QGroupBox(QStringLiteral("Komponenten"), root);
    auto* compLay = new QVBoxLayout(compBox);
    const struct { rpg::ComponentType type; const char* name; } compTypes[] = {
        {rpg::ComponentType::Transform, "Transform"},
        {rpg::ComponentType::ModelRenderer, "Model Renderer"},
        {rpg::ComponentType::Sprite, "Sprite"},
        {rpg::ComponentType::Material, "Material"},
        {rpg::ComponentType::Light, "Licht"},
        {rpg::ComponentType::Camera, "Kamera"},
        {rpg::ComponentType::Script, "Skript"},
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
        if (present) { compLay->addWidget(new QLabel(QLatin1String(ct.name), compBox)); any = true; }
    }
    if (!any) compLay->addWidget(new QLabel(QStringLiteral("(keine)"), compBox));
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
