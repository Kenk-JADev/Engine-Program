#include "QtCodeWorkspace.h"
#include "QtSyntaxHighlighter.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/ScriptManager.h"
#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/Project.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextCursor>

namespace qt_editor {

namespace {

const char* kRubySnippets[][2] = {
    {"Scene_Base Stub",
     "class Scene_MyScene < Scene_Base\n"
     "  def start\n"
     "    super\n"
     "    # setup\n"
     "  end\n"
     "  def update\n"
     "    # per-frame\n"
     "  end\n"
     "end\n"},
    {"Screen Text HUD",
     "UI.show_screen_text(\"Hello\", 0.5, 0.1, 1.0, 1.0, 1.0, 0.0)\n"},
    {"Input Check",
     "if Input.key_down?(:return)\n"
     "  # Enter gedrueckt\n"
     "end\n"},
    {"Scene Switch",
     "SceneManager.goto(Scene_Map)\n"},
    {"Audio BGM",
     "Audio.bgm_play(\"town.ogg\")\n"},
    // --- XP-Spielobjekte ($game_*): Das Spiel direkt aus Skripten steuern ---
    {"Schalter setzen (XP)",
     "$game_switches[1] = true  # Event-Seiten reagieren sofort\n"},
    {"Schalter abfragen (XP)",
     "if $game_switches[3]\n"
     "  UI.show_message(\"Schalter 3 ist AN\")\n"
     "end\n"},
    {"Variable erhoehen (XP)",
     "$game_variables[2] += 1  # Zaehler hochsetzen, Seiten pruefen neu\n"},
    {"Self-Switch setzen (XP)",
     "key = [$game_map.id, 1, \"A\"]  # [Karten-ID, Event-ID, Buchstabe]\n"
     "$game_self_switches[key] = true\n"},
    {"Gold geben / nehmen (XP)",
     "$game_party.gain_gold(100)\n"
     "$game_party.lose_gold(50)\n"},
    {"Gegenstand geben (XP)",
     "$game_party.gain_item(1, 3)  # Gegenstand 1, 3 Stueck\n"},
    {"Akteur in die Gruppe (XP)",
     "$game_party.add_actor(2)\n"
     "if $game_party.has_actor(2)\n"
     "  UI.show_message(\"Akteur 2 ist dabei!\")\n"
     "end\n"},
    {"Gruppen-Mitglieder auflisten (XP)",
     "for m in $game_party.members\n"
     "  UI.show_screen_text(m[:name], 0.05, 0.15, 1.0, 1.0, 1.0, 0.0)\n"
     "end\n"},
    {"Spieler teleportieren",
     "$game_player.move_to(10.0, 0.0, 10.0)  # x, y, z\n"},
    {"HUD ein-/ausblenden",
     "UI.hud_visible = false  # true blendet es wieder ein\n"},
    {"Speichern / Laden",
     "Game.save(1)  # Slot 1 (Datei <Projekt>/saves/save1.json)\n"
     "Game.load(1)\n"},
    {"Speicherbildschirm (XP)",
     "UI.open_save_screen(true)  # 4 Slots mit Info, wie in XP\n"},
    {"Spielmenue oeffnen (XP)",
     "UI.open_menu()  # Gegenstaende / Speichern / Beenden\n"},
    // --- "Alles custom": eigene Oberflaechen / Szenen statt der eingebauten ---
    {"Eigenes Menue (custom)",
     "# Beliebiges Listenmenue mit Block (Index oder -1 bei Esc)\n"
     "UI.open_list_menu(\"Lager\", [\"Trank\", \"Elixier\", [\"Schluessel\", false], \"Zurueck\"]) do |i|\n"
     "  if i == 0\n"
     "    UI.show_message(\"Trank benutzt!\")\n"
     "  elsif i == 3 || i == -1\n"
     "    # zurueck / abgebrochen\n"
     "  end\n"
     "end\n"},
    {"Eigene Kampfszene (custom)",
     "# In Game.ini: NativeBattleMenu=0 (eingebautes Menue aus), dann z. B. in\n"
     "# $game.update(dt) oder einer eigenen Scene die Eingabe selbst machen:\n"
     "if Battle.needs_input? && !@battle_menu_open\n"
     "  @battle_menu_open = true\n"
     "  UI.open_list_menu(\"Was tun?\", [\"Angriff\", \"Verteidigen\", \"Flucht\"]) do |i|\n"
     "    @battle_menu_open = false\n"
     "    Battle.set_action(Battle::ATTACK, 0, 0, 0, false) if i == 0\n"
     "    Battle.set_action(Battle::GUARD, 0, 0, 0, false)  if i == 1\n"
     "    if i == 2 && Battle.can_escape?\n"
     "      Battle.set_action(Battle::ESCAPE, 0, 0, 0, false)\n"
     "    end\n"
     "  end\n"
     "end\n"},
    {"Kampf mit eigener Gegnerliste (Battle-API)",
     "# Startet einen Kampf OHNE Trupp (freie Gegnerliste aus dem Skript):\n"
     "Battle.setup([1, 1, 2], true, false)  # 2x Gegner 1, 1x Gegner 2\n"
     "# Status auslesen: Battle.enemies -> [{\"hp\"=>.., \"dead\"=>..}, ...]\n"},
    {"Eigener Titel (custom)",
     "# In Game.ini: NativeTitle=0 -> die Engine ruft diesen Hook statt dem\n"
     "# eingebauten Titelbildschirm auf:\n"
     "class Game\n"
     "  def self.custom_title\n"
     "    UI.hud_visible = false\n"
     "    UI.open_list_menu(\"MEIN SPIEL\", [\"Start\", \"Beenden\"]) do |i|\n"
     "      Game.start_game if i == 0   # NewGame + Spielmodus an\n"
     "    end\n"
     "  end\n"
     "end\n"},
};

const char* kCppSnippets[][2] = {
    {"Create Entity",
     "// C++ Engine API\n"
     "auto& scene = engine.GetScene();\n"
     "rpg::EntityID id = scene.CreateEntity(\"MyObject\");\n"
     "auto* t = scene.AddComponent<rpg::TransformComponent>(id);\n"
     "t->transform.position = rpg::Vec3(0, 1, 0);\n"},
    {"Load Project",
     "engine.GetProject().Load(\"./MyGame\");\n"
     "engine.LoadScene(engine.GetProject().GetProjectPath() + \"/scene.json\");\n"},
    {"Playtest",
     "engine.SetPlaying(true);  // startet Game + Ruby-Scripts\n"
     "// ...\n"
     "engine.SetPlaying(false); // restore editor scene\n"},
    {"Script Execute",
     "engine.GetScriptManager().ExecuteAllScripts();\n"
     "engine.GetRubyVM().ExecuteString(\"puts 'hello from C++'\");\n"},
};

} // namespace

QtCodeWorkspace::QtCodeWorkspace(rpg::Engine* engine, QWidget* parent)
    : QWidget(parent), mEngine(engine) {
    buildUi();
    ensureCppDocs();
    refresh();
}

void QtCodeWorkspace::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* tb = new QToolBar(this);
    tb->setMovable(false);

    mLangCombo = new QComboBox(tb);
    mLangCombo->addItem("Ruby (Spiellogik)");
    mLangCombo->addItem("C++ (Engine API)");
    connect(mLangCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QtCodeWorkspace::setLanguage);
    tb->addWidget(new QLabel("  Sprache: ", tb));
    tb->addWidget(mLangCombo);
    tb->addSeparator();

    mNewAction = tb->addAction("Neu", this, &QtCodeWorkspace::onNewRubyScript);
    mSaveAction = tb->addAction("Speichern", this, [this]() { saveCurrent(); });
    tb->addAction("Alle speichern", this, [this]() { saveAll(); });
    mDeleteAction = tb->addAction("Löschen", this, &QtCodeWorkspace::onDeleteRubyScript);
    auto* renameAction = tb->addAction("Umbenennen", this, &QtCodeWorkspace::onRenameRubyScript);
    renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    renameAction->setToolTip(QStringLiteral("Aktuelles Script umbenennen [F2]"));
    tb->addAction("Neu laden", this, &QtCodeWorkspace::onReloadFromDisk);
    tb->addSeparator();
    mRunAction = tb->addAction("Ausführen", this, &QtCodeWorkspace::runCurrent);
    tb->addAction("Alle ausführen", this, &QtCodeWorkspace::runAll);
    tb->addAction("Hot-Reload", this, &QtCodeWorkspace::onHotReload);
    tb->addSeparator();
    tb->addAction("Extern öffnen", this, &QtCodeWorkspace::onOpenExternal);
    tb->addSeparator();
    tb->addWidget(new QLabel(" Suchen: ", tb));
    mFindEdit = new QLineEdit(tb);
    mFindEdit->setPlaceholderText("Ctrl+F");
    mFindEdit->setMaximumWidth(180);
    tb->addWidget(mFindEdit);
    tb->addAction("Suchen", this, &QtCodeWorkspace::onFind);
    tb->addAction("Weiter", this, &QtCodeWorkspace::onFindNext);
    connect(mFindEdit, &QLineEdit::returnPressed, this, &QtCodeWorkspace::onFind);

    tb->addSeparator();
    tb->addWidget(new QLabel(" Snippet: ", tb));
    mSnippetCombo = new QComboBox(tb);
    mSnippetCombo->setMinimumWidth(160);
    connect(mSnippetCombo, QOverload<int>::of(&QComboBox::activated),
            this, &QtCodeWorkspace::onInsertSnippet);
    tb->addWidget(mSnippetCombo);

    root->addWidget(tb);

    auto* split = new QSplitter(Qt::Horizontal, this);

    mFileList = new QListWidget(split);
    mFileList->setMinimumWidth(200);
    mFileList->setMaximumWidth(360);
    connect(mFileList, &QListWidget::currentRowChanged, this, [this](int) {
        onFileSelected();
    });
    // XP: Doppelklick/Enter auf den Listeneintrag benennt das Script um
    connect(mFileList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        if (mLanguage == CodeLanguage::Ruby) onRenameRubyScript();
    });

    auto* right = new QWidget(split);
    auto* rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(4, 4, 4, 4);

    auto* meta = new QHBoxLayout();
    mPathLabel = new QLabel("(keine Datei)", right);
    mPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mDirtyLabel = new QLabel(right);
    mDirtyLabel->setStyleSheet("color: #e0a000; font-weight: bold;");
    meta->addWidget(mPathLabel, 1);
    meta->addWidget(mDirtyLabel, 0);
    rightLay->addLayout(meta);

    mEditor = new QPlainTextEdit(right);
    mEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    mEditor->setTabStopDistance(4 * mEditor->fontMetrics().horizontalAdvance(' '));
    applyEditorFont();
    mHighlighter = new QtSyntaxHighlighter(mEditor->document());
    mHighlighter->setLanguage(HighlightLanguage::Ruby);
    // Dunkler Editor-Hintergrund (Syntax-Farben sind darauf abgestimmt)
    mEditor->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  selection-background-color: #264f78;"
        "  border: 1px solid #3c3c3c;"
        "}");
    connect(mEditor, &QPlainTextEdit::textChanged, this, &QtCodeWorkspace::onTextChanged);
    rightLay->addWidget(mEditor, 1);

    split->addWidget(mFileList);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({240, 800});

    root->addWidget(split, 1);

    // Snippets initial (Ruby)
    mSnippetCombo->clear();
    mSnippetCombo->addItem("(Snippet einfuegen...)");
    for (auto& s : kRubySnippets) mSnippetCombo->addItem(s[0]);
}

void QtCodeWorkspace::applyEditorFont() {
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(11);
    mono.setStyleHint(QFont::Monospace);
    mEditor->setFont(mono);
    mFileList->setFont(mono);
}

void QtCodeWorkspace::ensureCppDocs() {
    if (!mCppDocs.empty()) return;

    mCppDocs.push_back({
        "Engine.h – Haupt-API",
        "include/rpgmaker3d/Engine.h",
        R"CPP(// RPG Maker 3D – Engine C++ API (Referenz)
// Diese Dateien liegen im Repo unter include/rpgmaker3d/
// Der Qt-Editor oeffnet sie read-only als Schnellreferenz.
// Zum echten C++-Entwickeln: IDE (VS/CLion) + CMake.

#pragma once
// Kernklasse: rpg::Engine

namespace rpg {

class Engine {
public:
    // Lebenszyklus
    bool Initialize(const std::string& title, int w, int h, bool editorMode = true);
    bool InitializeEmbedded(int w, int h, bool editorMode = true); // Qt/QOpenGLWidget
    void Shutdown();
    void Run();                 // SDL-Hauptschleife (Player)
    void Update(float dt);      // Host-getriebener Tick (Qt)
    void Render();              // Host-getriebener Frame (Qt)

    // Subsysteme
    Window&          GetWindow();
    Renderer&        GetRenderer();
    Input&           GetInput();
    AudioManager&    GetAudio();
    Scene&           GetScene();
    Project&         GetProject();
    Map&             GetMap();
    ResourceManager& GetResources();
    CommandHistory&  GetCommandHistory();
    RubyVM&          GetRubyVM();
    ScriptManager&   GetScriptManager();

    // Playtest
    void SetPlaying(bool playing);
    bool IsPlaying() const;

    // Szene I/O
    void SaveScene(const std::string& path) const;
    bool LoadScene(const std::string& path);

    // Editor-Selektion (Qt-Host)
    void SetSelectedEntity(int id);
    int  GetSelectedEntity() const;
};

} // namespace rpg
)CPP"
    });

    mCppDocs.push_back({
        "Scene.h – Entities / Komponenten",
        "include/rpgmaker3d/Scene.h",
        R"CPP(// rpg::Scene – Entity-Component Container

namespace rpg {

// Wichtige Komponenten (Types.h / Scene.h):
//   TransformComponent       position/rotation/scale
//   ModelRendererComponent   shared_ptr<Model>
//   MaterialComponent        Material (diffuse, metallic, ...)
//   LightComponent           color, intensity, range
//   CameraComponent          fov, near, far, isMain
//   ScriptComponent          Ruby/C++ script binding
//   SpriteComponent          Billboard / 2D
//   ParticleEmitterComponent Partikel

class Scene {
public:
    EntityID CreateEntity(const std::string& name = "Entity");
    void     DestroyEntity(EntityID id);
    void     Clear();

    const std::vector<EntityID>& GetEntities() const;
    std::string GetEntityName(EntityID id) const;
    void        SetEntityName(EntityID id, const std::string& name);

    template<typename T> T* AddComponent(EntityID id);
    template<typename T> T* GetComponent(EntityID id);
};

// Beispiel: Wuerfel erzeugen
//   EntityID id = scene.CreateEntity("Cube");
//   auto* t = scene.AddComponent<TransformComponent>(id);
//   t->transform.position = Vec3(0, 0.5f, 0);
//   auto* m = scene.AddComponent<ModelRendererComponent>(id);
//   m->model = std::make_shared<Model>();
//   m->model->AddMesh(MeshFactory::CreateCube(1.0f));

} // namespace rpg
)CPP"
    });

    mCppDocs.push_back({
        "ScriptManager + RubyVM",
        "include/rpgmaker3d/ScriptManager.h",
        R"CPP(// Ruby-Scripting aus C++ steuern

namespace rpg {

class ScriptManager {
public:
    struct Script {
        std::string name;     // z.B. "06_Scene_Map.rb"
        std::string path;     // voller Pfad
        std::string content;  // Quelltext
        bool modified = false;
        bool isCore = false;
    };

    void LoadProjectScripts(const std::string& projectPath);
    void CreateDefaultScripts(const std::string& projectPath);
    const std::vector<std::shared_ptr<Script>>& GetScripts() const;

    std::shared_ptr<Script> CreateScript(const std::string& name);
    void DeleteScript(const std::string& name);
    bool SaveScript(std::shared_ptr<Script> script);
    void SaveAllScripts();
    void ReloadFromDisk();
    void ExecuteAllScripts(); // in Load-Order (Dateiname sortiert)
};

class RubyVM {
public:
    bool Initialize(Engine* engine);
    bool ExecuteString(const std::string& code);
    bool ExecuteFile(const std::string& path);
    bool Update(float deltaTime);
};

// Typischer Playtest-Ablauf (Engine::SetPlaying):
//   1. Scene-Backup speichern
//   2. Game::NewGameAt(spawn)
//   3. EventSystem laden
//   4. ScriptManager::ExecuteAllScripts()
//   5. RubyVM::Update(dt) pro Frame

} // namespace rpg
)CPP"
    });

    mCppDocs.push_back({
        "Qt-Editor Einbindung",
        "qt_editor/QtEditorWindow.cpp",
        R"CPP(// So ist der Qt-Editor an die Engine angebunden:

// 1) QApplication + GL 3.3 Core Surface (QtMain.cpp)
// 2) QtEditorWindow besitzt rpg::Engine
// 3) QtGameViewWidget (QOpenGLWidget):
//      initializeGL() -> gladLoadGL + Engine::InitializeEmbedded()
//      paintGL()      -> Engine::Render()
// 4) QTimer ~60 Hz:
//      Engine::Update(dt); view->update();
// 5) QtCodeWorkspace (dieser Panel):
//      Ruby-Scripts via ScriptManager editieren
//      C++ API-Referenz read-only
// 6) Hierarchie/Eigenschaften-Docks fuer Scene-Entities

// Build:
//   cmake -B build -S . -DRPGMAKER3D_EDITOR_QT=ON -DRPGMAKER3D_BUILD_EDITOR=ON
//   cmake --build build --config Release

// Hinweis: ImGui-Editor ist entfernt. Einziger Editor-Host ist Qt.
)CPP"
    });

    mCppDocs.push_back({
        "Input / Camera (Runtime)",
        "include/rpgmaker3d/Input.h",
        R"CPP(// Input & Kamera aus C++

// Input (rpg::Input):
//   bool IsKeyDown(Key k) / IsKeyPressed(Key k)
//   bool IsMouseDown(MouseButton b)
//   Vec2 GetMousePosition() / GetMouseDelta()
//   float GetMouseWheel()
//
// Im Qt-Editor: QtGameViewWidget mappt Qt-Events -> Input::OnKeyChanged / OnMouse*

// Kamera (Renderer::GetCamera()):
//   SetPosition / GetPosition
//   SetRotation / GetRotation   (Euler, Grad)
//   SetPerspective(fov, aspect, near, far)
//   GetForward / GetRight / GetUp
//
// Playtest-Follow:
//   engine.SetPlayModeFollowPlayer(true);
//   -> Kamera folgt Game::Get().Player()

// Raycast-Picking (Qt Linksklick):
//   Ray ray = Raycast::ScreenPointToRay(cam, mouse, viewSize);
//   RaycastHit hit = Raycast::PickEntity(ray, scene, 2000.f);
)CPP"
    });
}

void QtCodeWorkspace::setLanguage(int index) {
    if (mDirty && mLanguage == CodeLanguage::Ruby) {
        flushCurrentToManager();
    }
    mLanguage = (index == 1) ? CodeLanguage::Cpp : CodeLanguage::Ruby;
    mCurrentIndex = -1;
    mDirty = false;
    mEditor->setReadOnly(mLanguage == CodeLanguage::Cpp);

    mNewAction->setEnabled(mLanguage == CodeLanguage::Ruby);
    mDeleteAction->setEnabled(mLanguage == CodeLanguage::Ruby);
    mSaveAction->setEnabled(mLanguage == CodeLanguage::Ruby);
    mRunAction->setEnabled(mLanguage == CodeLanguage::Ruby);

    mSnippetCombo->blockSignals(true);
    mSnippetCombo->clear();
    mSnippetCombo->addItem("(Snippet einfuegen...)");
    if (mLanguage == CodeLanguage::Ruby) {
        for (auto& s : kRubySnippets) mSnippetCombo->addItem(s[0]);
    } else {
        for (auto& s : kCppSnippets) mSnippetCombo->addItem(s[0]);
    }
    mSnippetCombo->blockSignals(false);

    updateHighlighter();
    refresh();
}

void QtCodeWorkspace::updateHighlighter() {
    if (!mHighlighter) return;
    mHighlighter->setLanguage(mLanguage == CodeLanguage::Cpp
        ? HighlightLanguage::Cpp
        : HighlightLanguage::Ruby);
}

void QtCodeWorkspace::refresh() {
    // Engine ist im Qt-Host erst nach initializeGL (GL-Kontext) initialisiert.
    // Der Konstruktor ruft refresh() auf, bevor das der Fall ist -> Guard,
    // sonst wuerde GetScriptManager() auf einen nullptr dereferenzieren.
    if (!mEngine || !mEngine->IsInitialized()) return;
    if (mLanguage == CodeLanguage::Ruby) populateRubyList();
    else populateCppList();
    updateDirtyLabel();
    updateHighlighter();
}

void QtCodeWorkspace::populateRubyList() {
    mFileList->blockSignals(true);
    mFileList->clear();
    if (!mEngine) {
        mFileList->blockSignals(false);
        return;
    }
    auto& scripts = mEngine->GetScriptManager().GetScripts();
    int select = mCurrentIndex;
    for (size_t i = 0; i < scripts.size(); ++i) {
        const auto& s = scripts[i];
        QString label = QString::fromStdString(s->name);
        if (s->modified) label += " *";
        if (s->isCore) label += "  [core]";
        mFileList->addItem(label);
    }
    if (scripts.empty()) {
        mFileList->addItem("(keine Skripte – Projekt öffnen oder Neu)");
    }
    mFileList->blockSignals(false);

    if (select < 0 || select >= mFileList->count()) select = scripts.empty() ? -1 : 0;
    if (select >= 0) {
        mFileList->setCurrentRow(select);
        loadRubyFile(select);
    } else {
        mLoading = true;
        mEditor->setPlainText(
            "# Ruby Code Workspace\n"
            "#\n"
            "# Öffne ein Projekt (Datei -> Projekt öffnen)\n"
            "# oder lege ein neues Script an (Neu).\n"
            "# Scripts liegen unter <Projekt>/scripts/*.rb\n"
            "# und werden beim Playtest in Dateiname-Reihenfolge geladen.\n");
        mLoading = false;
        mPathLabel->setText("(kein Script)");
        mCurrentIndex = -1;
        mDirty = false;
        updateDirtyLabel();
    }
}

void QtCodeWorkspace::populateCppList() {
    ensureCppDocs();
    mFileList->blockSignals(true);
    mFileList->clear();
    for (const auto& d : mCppDocs) {
        mFileList->addItem(d.name);
    }
    mFileList->blockSignals(false);
    int select = mCurrentIndex >= 0 ? mCurrentIndex : 0;
    if (!mCppDocs.empty()) {
        mFileList->setCurrentRow(select);
        loadCppReference(select);
    }
}

void QtCodeWorkspace::onFileSelected() {
    const int row = mFileList->currentRow();
    if (row < 0) return;
    if (mLanguage == CodeLanguage::Ruby) {
        if (mDirty) flushCurrentToManager();
        loadRubyFile(row);
    } else {
        loadCppReference(row);
    }
}

void QtCodeWorkspace::loadRubyFile(int index) {
    if (!mEngine) return;
    auto& scripts = mEngine->GetScriptManager().GetScripts();
    if (index < 0 || index >= static_cast<int>(scripts.size())) return;

    mLoading = true;
    mCurrentIndex = index;
    auto& s = scripts[static_cast<size_t>(index)];
    mCurrentName = QString::fromStdString(s->name);
    mCurrentPath = QString::fromStdString(s->path);
    mCurrentBuffer = QString::fromStdString(s->content);
    mEditor->setPlainText(mCurrentBuffer);
    mEditor->setReadOnly(mEngine->IsPlaying()); // waehrend Playtest schreibgeschuetzt
    mPathLabel->setStyleSheet("");
    mPathLabel->setText(mCurrentPath);
    mDirty = s->modified;
    mLoading = false;
    updateDirtyLabel();
}

void QtCodeWorkspace::loadCppReference(int index) {
    ensureCppDocs();
    if (index < 0 || index >= static_cast<int>(mCppDocs.size())) return;
    mLoading = true;
    mCurrentIndex = index;
    const auto& d = mCppDocs[static_cast<size_t>(index)];
    mCurrentName = d.name;
    mCurrentPath = d.pathHint;
    mCurrentBuffer = d.content;
    mEditor->setPlainText(d.content);
    mEditor->setReadOnly(true);
    mPathLabel->setText(d.pathHint + "  (Referenz, read-only)");
    mDirty = false;
    mLoading = false;
    updateDirtyLabel();
}

void QtCodeWorkspace::onTextChanged() {
    if (mLoading) return;
    if (mLanguage != CodeLanguage::Ruby) return;
    if (mEngine && mEngine->IsPlaying()) return;
    mCurrentBuffer = mEditor->toPlainText();
    mDirty = true;
    // live in ScriptManager spiegeln
    flushCurrentToManager();
    updateDirtyLabel();
    // Liste-Sternchen
    if (mCurrentIndex >= 0 && mCurrentIndex < mFileList->count()) {
        auto* item = mFileList->item(mCurrentIndex);
        QString label = mCurrentName + " *";
        if (mEngine) {
            auto& scripts = mEngine->GetScriptManager().GetScripts();
            if (mCurrentIndex < static_cast<int>(scripts.size()) && scripts[mCurrentIndex]->isCore)
                label += "  [core]";
        }
        item->setText(label);
    }
}

void QtCodeWorkspace::flushCurrentToManager() {
    if (!mEngine || mLanguage != CodeLanguage::Ruby || mCurrentIndex < 0) return;
    auto& scripts = mEngine->GetScriptManager().GetScripts();
    if (mCurrentIndex >= static_cast<int>(scripts.size())) return;
    auto& s = scripts[static_cast<size_t>(mCurrentIndex)];
    s->content = mCurrentBuffer.toStdString();
    s->modified = true;
}

void QtCodeWorkspace::updateDirtyLabel() {
    if (mLanguage == CodeLanguage::Cpp) {
        mDirtyLabel->setText("C++ Referenz");
        return;
    }
    if (mEngine && mEngine->IsPlaying()) {
        mDirtyLabel->setText("PLAYTEST (schreibgeschützt)");
        return;
    }
    mDirtyLabel->setText(mDirty ? "geändert *" : "");
}

bool QtCodeWorkspace::hasUnsavedChanges() const {
    if (mLanguage != CodeLanguage::Ruby || !mEngine) return false;
    for (const auto& s : mEngine->GetScriptManager().GetScripts()) {
        if (s->modified) return true;
    }
    return mDirty;
}

bool QtCodeWorkspace::saveCurrent() {
    if (!mEngine || mLanguage != CodeLanguage::Ruby || mCurrentIndex < 0) return false;
    flushCurrentToManager();
    auto& sm = mEngine->GetScriptManager();
    auto& scripts = sm.GetScripts();
    if (mCurrentIndex >= static_cast<int>(scripts.size())) return false;
    if (scripts[mCurrentIndex]->isCore) {
        // core darf trotzdem auf Disk – CreateDefaultScripts schreibt sie auch
    }
    const bool ok = sm.SaveScript(scripts[mCurrentIndex]);
    if (ok) {
        mDirty = false;
        updateDirtyLabel();
        emit logMessage(QString("Skript gespeichert: %1").arg(mCurrentName));
        // Liste ohne Stern
        if (mCurrentIndex < mFileList->count()) {
            QString label = mCurrentName;
            if (scripts[mCurrentIndex]->isCore) label += "  [core]";
            mFileList->item(mCurrentIndex)->setText(label);
        }
        emit scriptsChanged();
    } else {
        emit logMessage(QString("FEHLER: Speichern fehlgeschlagen: %1").arg(mCurrentName));
    }
    return ok;
}

bool QtCodeWorkspace::saveAll() {
    if (!mEngine || mLanguage != CodeLanguage::Ruby) return false;
    flushCurrentToManager();
    mEngine->GetScriptManager().SaveAllScripts();
    mDirty = false;
    updateDirtyLabel();
    emit logMessage("Alle Scripts gespeichert.");
    refresh();
    emit scriptsChanged();
    return true;
}

void QtCodeWorkspace::runCurrent() {
    if (!mEngine || mLanguage != CodeLanguage::Ruby || mCurrentIndex < 0) return;
    flushCurrentToManager();
    auto& scripts = mEngine->GetScriptManager().GetScripts();
    if (mCurrentIndex >= static_cast<int>(scripts.size())) return;
    auto& script = scripts[static_cast<size_t>(mCurrentIndex)];
    const bool ok = mEngine->GetRubyVM().ExecuteString(script->content, script->name);
    if (ok) {
        emit logMessage(QString("Ruby OK: %1").arg(mCurrentName));
        showRubyError(QString());
    } else {
        const QString err = QString::fromStdString(mEngine->GetRubyVM().GetLastError());
        emit logMessage(QString("Ruby-Fehler in %1: %2").arg(mCurrentName, err));
        showRubyError(err);
    }
}

void QtCodeWorkspace::runAll() {
    if (!mEngine) return;
    if (mLanguage == CodeLanguage::Ruby) flushCurrentToManager();
    mEngine->GetScriptManager().ExecuteAllScripts();
    if (mEngine->GetRubyVM().HasError()) {
        const QString err = QString::fromStdString(mEngine->GetRubyVM().GetLastError());
        emit logMessage("Ruby-Fehler beim Ausführen aller Skripte: " + err);
        showRubyError(err);
    } else {
        emit logMessage("Alle Ruby-Scripts ausgefuehrt (Load-Order).");
        showRubyError(QString());
    }
}

void QtCodeWorkspace::hotReloadAll() {
    if (!mEngine) return;
    if (mLanguage == CodeLanguage::Ruby) {
        flushCurrentToManager();
        saveAll();
    }
    mEngine->GetScriptManager().ExecuteAllScripts();
    if (mEngine->GetRubyVM().HasError()) {
        showRubyError(QString::fromStdString(mEngine->GetRubyVM().GetLastError()));
        emit logMessage("Hot-Reload mit Fehlern – siehe Konsole.");
    } else {
        showRubyError(QString());
        emit logMessage("Hot-Reload OK (alle Scripts neu ausgefuehrt).");
    }
}

void QtCodeWorkspace::onHotReload() { hotReloadAll(); }

void QtCodeWorkspace::showRubyError(const QString& err) {
    if (!mPathLabel) return;
    if (err.isEmpty()) {
        mPathLabel->setStyleSheet("");
        if (!mCurrentPath.isEmpty()) mPathLabel->setText(mCurrentPath);
        return;
    }
    mPathLabel->setStyleSheet("color:#ff6666; font-weight:bold;");
    mPathLabel->setText(QString("RUBY ERROR: %1").arg(err));
}

void QtCodeWorkspace::onFind() {
    if (!mEditor || !mFindEdit) return;
    mLastFind = mFindEdit->text();
    mFindPos = 0;
    onFindNext();
}

void QtCodeWorkspace::onFindNext() {
    if (!mEditor || mLastFind.isEmpty()) {
        if (mFindEdit) mLastFind = mFindEdit->text();
    }
    if (mLastFind.isEmpty()) return;
    QString text = mEditor->toPlainText();
    int pos = text.indexOf(mLastFind, mFindPos, Qt::CaseInsensitive);
    if (pos < 0 && mFindPos > 0) {
        mFindPos = 0;
        pos = text.indexOf(mLastFind, 0, Qt::CaseInsensitive);
    }
    if (pos < 0) {
        emit logMessage(QString("Nicht gefunden: %1").arg(mLastFind));
        return;
    }
    QTextCursor c = mEditor->textCursor();
    c.setPosition(pos);
    c.setPosition(pos + mLastFind.size(), QTextCursor::KeepAnchor);
    mEditor->setTextCursor(c);
    mEditor->setFocus();
    mFindPos = pos + mLastFind.size();
}

void QtCodeWorkspace::onNewRubyScript() {
    if (!mEngine || mLanguage != CodeLanguage::Ruby) return;
    if (mEngine->GetProject().GetProjectPath().empty()) {
        QMessageBox::information(this, "Neues Skript",
            "Bitte zuerst ein Projekt anlegen oder öffnen.");
        return;
    }
    bool ok = false;
    QString name = QInputDialog::getText(this, "Neues Ruby-Skript",
        "Dateiname:", QLineEdit::Normal, "custom_logic.rb", &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    name = name.trimmed();
    if (!name.endsWith(".rb")) name += ".rb";
    auto script = mEngine->GetScriptManager().CreateScript(name.toStdString());
    if (!script) {
        QMessageBox::warning(this, "Neues Skript", "Konnte Skript nicht anlegen.");
        return;
    }
    emit logMessage("Skript angelegt: " + name);
    mCurrentIndex = static_cast<int>(mEngine->GetScriptManager().GetScripts().size()) - 1;
    refresh();
    emit scriptsChanged();
}

void QtCodeWorkspace::onRenameRubyScript() {
    if (!mEngine || mLanguage != CodeLanguage::Ruby || mCurrentIndex < 0) return;
    auto& sm = mEngine->GetScriptManager();
    auto& scripts = sm.GetScripts();
    if (mCurrentIndex >= static_cast<int>(scripts.size())) return;
    auto& s = scripts[static_cast<size_t>(mCurrentIndex)];

    bool ok = false;
    QString name = QInputDialog::getText(this, "Skript umbenennen",
        "Neuer Dateiname:", QLineEdit::Normal, QString::fromStdString(s->name), &ok);
    if (!ok) return;
    name = name.trimmed();
    if (name.isEmpty() || name == QString::fromStdString(s->name)) return;
    if (!name.endsWith(".rb")) name += ".rb";
    if (name.contains(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")))) {
        QMessageBox::warning(this, "Umbenennen",
            QStringLiteral("Der Dateiname enthält ungültige Zeichen (\\ / : * ? \" < > |)."));
        return;
    }
    for (const auto& o : scripts) {
        if (o != s && o->name == name.toStdString()) {
            QMessageBox::warning(this, "Umbenennen",
                QStringLiteral("Ein Skript mit diesem Namen existiert bereits."));
            return;
        }
    }

    // aktuellen Text sicher im Manager ablegen, dann Datei umbenennen
    flushCurrentToManager();
    const QString oldPath = QString::fromStdString(s->path);
    const QString dir = QFileInfo(oldPath).absolutePath();
    const QString newPath = dir + "/" + name;
    if (QFile::exists(oldPath))
        QFile::rename(oldPath, newPath); // scheitert nur, wenn Ziel existiert (oben geprüft)

    s->name = name.toStdString();
    s->path = newPath.toStdString();
    s->modified = true;
    sm.SaveScript(s); // schreibt Inhalt unter neuem Pfad

    refresh();
    emit scriptsChanged();
    emit logMessage(QStringLiteral("Skript umbenannt: %1").arg(name));
}

void QtCodeWorkspace::onDeleteRubyScript() {
    if (!mEngine || mLanguage != CodeLanguage::Ruby || mCurrentIndex < 0) return;
    auto& scripts = mEngine->GetScriptManager().GetScripts();
    if (mCurrentIndex >= static_cast<int>(scripts.size())) return;
    if (scripts[mCurrentIndex]->isCore) {
        QMessageBox::information(this, "Löschen", "Kern-Skripte können nicht gelöscht werden.");
        return;
    }
    const QString name = QString::fromStdString(scripts[mCurrentIndex]->name);
    if (QMessageBox::question(this, "Skript löschen",
            QString("\"%1\" wirklich löschen?").arg(name)) != QMessageBox::Yes) {
        return;
    }
    mEngine->GetScriptManager().DeleteScript(scripts[mCurrentIndex]->name);
    mCurrentIndex = -1;
    mDirty = false;
    emit logMessage("Skript gelöscht: " + name);
    refresh();
    emit scriptsChanged();
}

void QtCodeWorkspace::onReloadFromDisk() {
    if (!mEngine) return;
    if (hasUnsavedChanges()) {
        const auto r = QMessageBox::question(this, "Neu laden",
            "Ungespeicherte Änderungen verwerfen und von der Festplatte laden?",
            QMessageBox::Yes | QMessageBox::No);
        if (r != QMessageBox::Yes) return;
    }
    mEngine->GetScriptManager().ReloadFromDisk();
    mDirty = false;
    mCurrentIndex = -1;
    emit logMessage("Skripte von der Festplatte neu geladen.");
    refresh();
    emit scriptsChanged();
}

void QtCodeWorkspace::onOpenExternal() {
    if (mCurrentPath.isEmpty() || mCurrentPath.startsWith("include/") ||
        mCurrentPath.startsWith("qt_editor/")) {
        // C++ Referenz: versuche Repo-Pfad relativ zum CWD
        if (mLanguage == CodeLanguage::Cpp && mCurrentIndex >= 0 &&
            mCurrentIndex < static_cast<int>(mCppDocs.size())) {
            const QString hint = mCppDocs[static_cast<size_t>(mCurrentIndex)].pathHint;
            if (QFileInfo::exists(hint)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(hint).absoluteFilePath()));
                emit logMessage("Extern geöffnet: " + hint);
                return;
            }
        }
        emit logMessage("Keine Datei zum Öffnen (Referenz ist eingebettet).");
        return;
    }
    if (!QFileInfo::exists(mCurrentPath)) {
        emit logMessage("Datei existiert nicht: " + mCurrentPath);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(mCurrentPath).absoluteFilePath()));
    emit logMessage("Externer Editor: " + mCurrentPath);
}

void QtCodeWorkspace::onInsertSnippet(int index) {
    if (index <= 0) return;
    const int si = index - 1;
    QString text;
    if (mLanguage == CodeLanguage::Ruby) {
        const int n = static_cast<int>(sizeof(kRubySnippets) / sizeof(kRubySnippets[0]));
        if (si < 0 || si >= n) return;
        text = QString::fromUtf8(kRubySnippets[si][1]);
    } else {
        const int n = static_cast<int>(sizeof(kCppSnippets) / sizeof(kCppSnippets[0]));
        if (si < 0 || si >= n) return;
        text = QString::fromUtf8(kCppSnippets[si][1]);
        // C++ ist read-only – Snippet in Zwischenablage? Stattdessen temporaer einfuegbar machen
        mEditor->setReadOnly(false);
        mEditor->insertPlainText(text);
        mEditor->setReadOnly(true);
        mSnippetCombo->setCurrentIndex(0);
        emit logMessage("C++ Snippet eingefuegt (Referenz-Ansicht, nicht speicherbar).");
        return;
    }
    if (mEditor->isReadOnly()) return;
    mEditor->insertPlainText(text);
    mSnippetCombo->setCurrentIndex(0);
}

} // namespace qt_editor
