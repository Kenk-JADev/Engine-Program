#include "QtCodeWorkspace.h"
#include "QtSyntaxHighlighter.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/ScriptManager.h"
#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/Project.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSplitter>
#include <QToolButton>
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
    {"RGSS-Fenster (XP-Stil)",
     "# Fenster wie im RPG Maker XP (volles RGSS-Fenstersystem aus Ruby):\n"
     "@win = Window.new\n"
     "@win.x = 80; @win.y = 120; @win.width = 480; @win.height = 200\n"
     "@win.windowskin = RPG::Cache.windowskin(\"001-Blue01\")  # XP: Bitmap!\n"
     "@win.contents.font.color.set(255, 255, 0)              # Referenz-Semantik\n"
     "@win.contents.draw_text(4, 4, 440, 32, \"Hallo RGSS!\")\n"
     "@win.z = 100\n"
     "# openness (0..255), active, pause, opacity, back_opacity,\n"
     "# contents_opacity, stretch, cursor_rect, ox/oy (Scrollen)\n"
     "# Aufraeumen: @win.dispose\n"},
    {"RGSS: Bitmap & Sprite",
     "# Grafik laden und als Sprite anzeigen (640x480-Raum):\n"
     "@bmp = RPG::Cache.picture(\"titel_hintergrund\")   # Graphics/Pictures/\n"
     "@spr = Sprite.new\n"
     "@spr.bitmap = @bmp\n"
     "@spr.src_rect = Rect.new(0, 0, @bmp.width, @bmp.height)\n"
     "@spr.x = 40; @spr.y = 60; @spr.z = 10\n"
     "# zoom_x/zoom_y, angle, mirror, opacity, blend_type (0 normal/1 add/2 sub),\n"
     "# bush_depth, color (mischen), tone (Farbton), flash(Color.new(..), dauer)\n"
     "# Bitmap-Pixel: bmp.fill_rect, gradient_fill_rect, blt, stretch_blt,\n"
     "# get_pixel/set_pixel, hue_change, blur/radial_blur, draw_text, text_size\n"},
    {"RGSS: Viewport & Tilemap",
     "# Viewport = Ausschnitt mit eigener Ebene (Clip + Scroll-Offset):\n"
     "@vp = Viewport.new(0, 0, 640, 480)\n"
     "@spr2 = Sprite.new(@vp)\n"
     "# Vollwertige Karte (Editordaten wie im XP: 3 Ebenen + Autotiles):\n"
     "@tm = Tilemap.new(@vp)\n"
     "@tm.tileset = RPG::Cache.tileset(\"001-Grossstadt01\")\n"
     "@tm.map_data = Table.new(20, 15, 3)\n"
     "# Spalten/Zeilen fuellen: @tm.map_data[x, y, ebene] = tile_id\n"
     "# tile_id < 384: Autotile (48er-Muster wie XP), sonst tileset-Kachel.\n"},
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
    // Neuer XP-Look (PAKET 28): links Skriptliste, rechts grosser Editor,
    // oben nur eine schlanke Kopfzeile (Ansicht + Suche), unten zwei Buttons.
    // Alle Dateiaktionen liegen im RECHTSKLICK-Menue der Skriptliste (XP-Stil),
    // nicht in einer ueberladenen Buttonleiste.
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- Kopfzeile: Ansicht + Suche (mehr nicht) -------------------------
    auto* top = new QWidget(this);
    auto* topLay = new QHBoxLayout(top);
    topLay->setContentsMargins(6, 4, 6, 4);
    topLay->setSpacing(6);
    topLay->addWidget(new QLabel(QStringLiteral("Ansicht:"), top));
    mLangCombo = new QComboBox(top);
    mLangCombo->addItem(QStringLiteral("Ruby (Spiellogik)"));
    mLangCombo->addItem(QStringLiteral("C++ (Engine API-Referenz)"));
    connect(mLangCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QtCodeWorkspace::setLanguage);
    topLay->addWidget(mLangCombo);
    topLay->addStretch(1);
    topLay->addWidget(new QLabel(QStringLiteral("Suchen:"), top));
    mFindEdit = new QLineEdit(top);
    mFindEdit->setPlaceholderText(QStringLiteral("Suchbegriff [Enter = finden]"));
    mFindEdit->setMaximumWidth(220);
    connect(mFindEdit, &QLineEdit::returnPressed, this, &QtCodeWorkspace::onFind);
    topLay->addWidget(mFindEdit);
    auto* nextBtn = new QToolButton(top);
    nextBtn->setText(QStringLiteral("Weiter"));
    nextBtn->setToolTip(QStringLiteral("Nächsten Treffer suchen"));
    connect(nextBtn, &QToolButton::clicked, this, &QtCodeWorkspace::onFindNext);
    topLay->addWidget(nextBtn);
    root->addWidget(top);

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

    // ---- Dateiaktionen: Rechtsklick-Kontextmenue + Tasten (statt Buttons) ---
    mNewAction = new QAction(QStringLiteral("Neues Skript …"), this);
    mNewAction->setToolTip(QStringLiteral("Leeres Ruby-Skript anlegen"));
    connect(mNewAction, &QAction::triggered, this, &QtCodeWorkspace::onNewRubyScript);

    auto* renameAction = new QAction(QStringLiteral("Umbenennen"), this);
    renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(renameAction, &QAction::triggered, this, &QtCodeWorkspace::onRenameRubyScript);

    mDeleteAction = new QAction(QStringLiteral("Löschen"), this);
    mDeleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    mDeleteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(mDeleteAction, &QAction::triggered, this, &QtCodeWorkspace::onDeleteRubyScript);

    auto* reloadAction = new QAction(QStringLiteral("Von Datenträger neu laden"), this);
    connect(reloadAction, &QAction::triggered, this, &QtCodeWorkspace::onReloadFromDisk);

    auto* externalAction = new QAction(QStringLiteral("Im externen Editor öffnen"), this);
    connect(externalAction, &QAction::triggered, this, &QtCodeWorkspace::onOpenExternal);

    mSaveAction = new QAction(QStringLiteral("Speichern"), this);
    mSaveAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    mSaveAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(mSaveAction, &QAction::triggered, this, [this]() { saveCurrent(); });

    auto* saveAllAction = new QAction(QStringLiteral("Alle speichern"), this);
    connect(saveAllAction, &QAction::triggered, this, [this]() { saveAll(); });

    // XP-Paritaet: KEIN "Skript ausfuehren" - Skripte laufen im Spiel, nicht
    // einzeln aus dem Editor. Nur Hot-Reload bleibt als Dev-Werkzeug.
    auto* hotReloadAction = new QAction(QStringLiteral("Speichern + Hot-Reload"), this);
    hotReloadAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    hotReloadAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(hotReloadAction, &QAction::triggered, this, &QtCodeWorkspace::onHotReload);

    // Tasten direkt auf der Skriptliste (F2/Entf bleiben listenlokal, damit
    // Entf im Textfeld normal Zeichen loescht)
    mFileList->addAction(renameAction);
    mFileList->addAction(mDeleteAction);
    addAction(mSaveAction);         // Ctrl+S im ganzen Skriptfenster
    addAction(hotReloadAction);     // Ctrl+R im ganzen Skriptfenster

    // Rechtsklick auf Skripte (XP: Einfügen/Umbenennen/Löschen …)
    mFileList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mFileList, &QListWidget::customContextMenuRequested, this,
            [this, renameAction, reloadAction, externalAction,
             saveAllAction, hotReloadAction](const QPoint& pos) {
        const bool ruby = (mLanguage == CodeLanguage::Ruby);
        QMenu menu(mFileList);
        menu.addAction(mNewAction);
        menu.addAction(renameAction);
        menu.addAction(mDeleteAction);
        menu.addSeparator();
        menu.addAction(reloadAction);
        menu.addAction(externalAction);
        menu.addSeparator();
        menu.addAction(mSaveAction);
        menu.addAction(saveAllAction);
        menu.addAction(hotReloadAction);
        mNewAction->setEnabled(ruby);
        renameAction->setEnabled(ruby);
        mDeleteAction->setEnabled(ruby);
        mSaveAction->setEnabled(ruby);
        saveAllAction->setEnabled(ruby);
        hotReloadAction->setEnabled(ruby);
        menu.exec(mFileList->viewport()->mapToGlobal(pos));
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

    // ---- Fusszeile: Snippets + nur zwei Buttons (XP-artig schlank) --------
    auto* bottom = new QWidget(this);
    auto* botLay = new QHBoxLayout(bottom);
    botLay->setContentsMargins(6, 2, 6, 4);
    botLay->setSpacing(6);
    botLay->addWidget(new QLabel(QStringLiteral("Snippet:"), bottom));
    mSnippetCombo = new QComboBox(bottom);
    mSnippetCombo->setMinimumWidth(200);
    connect(mSnippetCombo, QOverload<int>::of(&QComboBox::activated),
            this, &QtCodeWorkspace::onInsertSnippet);
    botLay->addWidget(mSnippetCombo, 1);
    auto* saveBtn = new QPushButton(QStringLiteral("Speichern"), bottom);
    saveBtn->setToolTip(QStringLiteral("Aktuelles Skript speichern [Strg+S]"));
    connect(saveBtn, &QPushButton::clicked, this, [this]() { saveCurrent(); });
    botLay->addWidget(saveBtn);
    auto* saveAllBtn = new QPushButton(QStringLiteral("Alle speichern"), bottom);
    connect(saveAllBtn, &QPushButton::clicked, this, [this]() { saveAll(); });
    botLay->addWidget(saveAllBtn);
    root->addWidget(bottom);

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
