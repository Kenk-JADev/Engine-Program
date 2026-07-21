#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/AudioManager.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/ResourceManager.h"
#include "rpgmaker3d/Camera.h"
#ifdef RPGMAKER3D_ENABLE_RMLUI
#include "rpgmaker3d/RmlUiSystem.h"
#endif
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Framebuffer.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/CommandHistory.h"
#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/ScriptManager.h"
#include "rpgmaker3d/Raycast.h"
#include "rpgmaker3d/Lighting.h"
#include "rpgmaker3d/ParticleSystem.h"
#include "rpgmaker3d/Config.h"
#include "rpgmaker3d/Platform.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/BattleSystem.h"
#include "rpgmaker3d/UI.h"

#include <SDL.h>

#include <glad/gl.h>
#include <iostream>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>

namespace rpg {

Engine::Engine() = default;
Engine::~Engine() { Shutdown(); }

bool Engine::Initialize(const std::string& title, int width, int height, bool editorMode) {
    return InitializeInternal(title, width, height, editorMode, true);
}

bool Engine::InitializeEmbedded(int width, int height, bool editorMode) {
    // Aufrufer (z.B. Qt QOpenGLWidget) stellt GL-Kontext + glad bereits bereit.
    return InitializeInternal("RPG Maker 3D (Embedded)", width, height, editorMode, false);
}

bool Engine::InitializeInternal(const std::string& title, int width, int height, bool editorMode, bool createOsWindow) {
    mEditorMode = editorMode;

    // Logger
    Logger::Get().SetLogFile("engine.log");
    Logger::Get().SetConsoleOutput(true);
    RPG_LOG_INFO(std::string(EngineConfig::NAME) + " v" + EngineConfig::VERSION + " - Init started [" + RPG_PLATFORM_NAME + "]");

    // Plattform
    // Im Qt-Host setzt Qt die DPI-Awareness (PerMonitorV2). Ein zweiter
    // SetProcessDpiAwarenessContext-Aufruf schlaegt mit "Zugriff verweigert" fehl.
#ifndef RPGMAKER3D_EDITOR_QT
    Platform::SetDPIAware();
#endif
    RPG_LOG_INFO("Working Dir: " + Platform::GetWorkingDirectory());
    RPG_LOG_INFO("Exe Path: " + Platform::GetExecutablePath());
#ifdef _WIN32
    RPG_LOG_INFO("Windows Version: " + Platform::GetWindowsVersion());
#endif

    // Fenster (echtes OS-Fenster via SDL oder eingebetteter Host-Kontext)
    mWindow = std::make_unique<Window>();
    bool windowOk = createOsWindow
        ? mWindow->Create(title, width, height, editorMode)
        : mWindow->CreateForeign(width, height);
    if (!windowOk) {
        RPG_LOG_ERROR("Failed to create window");
        return false;
    }

    // Renderer
    mRenderer = std::make_unique<Renderer>();
    if (!mRenderer->Initialize()) {
        RPG_LOG_ERROR("Failed to initialize renderer");
        return false;
    }

    // Framebuffer für Editor Scene View
    mSceneFramebuffer = std::make_unique<Framebuffer>();
    if (!mSceneFramebuffer->Create(1280, 720)) {
        RPG_LOG_ERROR("Failed to create scene framebuffer");
        return false;
    }

#ifdef RPGMAKER3D_ENABLE_RMLUI
    // RmlUi UI-System (F9 toggelt Sichtbarkeit; Kontexte "editor" / "game")
    mRmlUi = std::make_unique<RmlUiSystem>();
    if (!mRmlUi->Initialize(this)) {
        RPG_LOG_WARN("RmlUi initialization failed - continuing without RmlUi");
        mRmlUi.reset();
    }
    // Im Editor (Qt) ist das In-Game-HUD standardmaessig AUS: Der Editor
    // zeigt FPS/Karte/Status in der eigenen Statuszeile an – das HUD wuerde
    // nur im Weg liegen. Es wird beim Playtest-Start sichtbar (F9 toggelt
    // jederzeit manuell). Im Player startet es sichtbar.
    if (mRmlUi && mEditorMode) mRmlUi->SetVisible(false);
#endif

    // Core Systeme
    mInput = std::make_unique<Input>();
    mCommandHistory = std::make_unique<CommandHistory>();
    mRubyVM = std::make_unique<RubyVM>();
    try {
        if (!mRubyVM->Initialize(this)) {
            // Ruby ist optional: VM-Objekt bleibt erhalten (mMrb == nullptr),
            // ExecuteString/ExecuteFile/Update liefern dann sauber false.
            RPG_LOG_WARN("RubyVM-Initialisierung fehlgeschlagen - Engine laeuft ohne Ruby-Scripting");
        }
    } catch (const std::exception& e) {
        RPG_LOG_WARN(std::string("RubyVM-Initialisierung warf Exception - Engine laeuft ohne Ruby: ") + e.what());
    }

    mScriptManager = std::make_unique<ScriptManager>();
    mScriptManager->SetRubyVM(mRubyVM.get());

    // Event-Befehl Script -> RubyVM (Script-Editor Codes)
    EventSystem_SetScriptRunner([this](const std::string& code) {
        if (mRubyVM) {
            if (!mRubyVM->ExecuteString(code, "<event-script>")) {
                RPG_LOG_ERROR(std::string("[Event Script] ") + mRubyVM->GetLastError());
            }
        }
    });

    // Tasten-Provider fuer "Button Input Processing" (105) und Bedingung "Taste"
    // XP-Codes: 2=unten, 4=links, 6=rechts, 8=oben,
    //           11=A(Shift), 12=B(Esc), 13=C(Enter/E/Space), 15=L(Q), 16=R(Tab)
    EventSystem_SetButtonProvider([this]() -> int {
        if (!mInput) return 0;
        if (mInput->IsKeyPressed(Key::Down))  return 2;
        if (mInput->IsKeyPressed(Key::Left))  return 4;
        if (mInput->IsKeyPressed(Key::Right)) return 6;
        if (mInput->IsKeyPressed(Key::Up))    return 8;
        if (mInput->IsKeyPressed(Key::LShift)) return 11;
        if (mInput->IsKeyPressed(Key::Escape)) return 12;
        if (mInput->IsKeyPressed(Key::Enter) || mInput->IsKeyPressed(Key::E) ||
            mInput->IsKeyPressed(Key::Space)) return 13;
        if (mInput->IsKeyPressed(Key::Q))     return 15;
        if (mInput->IsKeyPressed(Key::Tab))   return 16;
        return 0;
    });

    mAudio = std::make_unique<AudioManager>();
    if (!mAudio->Initialize()) {
        RPG_LOG_WARN("Audio initialization failed - continuing without audio");
    }

    // Event-Audio-Bruecke: Ab sofort spielen Befehle BGM/BGS/ME/SE abspielen,
    // FadeOut BGM/BGS/SE stoppen und Karten-Autoplay wirklich Audio ab -
    // statt wie bisher nur ins Log zu schreiben.
    EventSystem_SetAudioPlayer([this](const std::string& name, int kind, bool loop) {
        PlayEventAudio(name, kind, loop);
    });

    // Map-Wechsel-Bruecke: Transfer-Befehl (201) und Savegame-Laden wechseln
    // jetzt wirklich die Karte (Visual + Events + BGM) - vorher blieb die
    // alte Karte sichtbar und nur die interne ID wechselte.
    EventSystem_SetMapChangeHandler([this](int mapId) {
        if (mapId <= 0) return;
        if (mMap && mProject) {
            const std::string p = mProject->GetMapPath(mapId);
            if (std::filesystem::exists(p)) mMap->Load(p);
        }
        // Setup (ID + Karten-BGM/-BGS) nur wenn die ID wirklich neu ist;
        // Game::Load hat sie bereits gesetzt (vermeidet doppelten BGM-Start).
        if (Game::Get().Map().GetMapId() != mapId)
            Game::Get().Map().Setup(mapId);
        if (mProject)
            EventSystem::Get().LoadMapEvents(mapId, mProject->GetProjectPath());
        RPG_LOG_INFO("Map-Wechsel auf Karte " + std::to_string(mapId));
    });

    // Spielmenue-Callbacks (XP) EINMAL zentral verdrahten - sie gelten fuer
    // Editor-Playtest UND Player gleichermassen (vorher nur im Editor-Zweig
    // von SetPlaying: im Player tat "Spiel beenden" deshalb nichts).
    GameUI::Get().Pause().onResume = []() { GameUI::Get().Pause().Hide(); };
    // "Speichern" oeffnet den XP-Speicherbildschirm (4 Slots)
    GameUI::Get().Pause().onSave = []() { GameUI::Get().ShowSaveScreen(true); };
    // "Zum Titelbildschirm": Editor-Playtest stoppt, Player -> Titel
    GameUI::Get().Pause().onExitToTitle = [this]() {
        if (mEditorMode) this->SetPlaying(false);
        else this->ReturnToTitle();
    };
    // "Spiel verlassen": Editor-Playtest stoppt, Player beendet das Spiel
    GameUI::Get().Pause().onQuitGame = [this]() {
        if (mEditorMode) this->SetPlaying(false);
        else this->RequestQuit();
    };

    // Bild-Pfadaufloeser fuer GameUI (UI.show_picture + Titelgrafik):
    // sucht in den XP-Projektordnern (Graphics/Pictures|Titles) usw.
    GameUI::SetPicturePathResolver([this](const std::string& filename) {
        return ResolvePicturePathFor(filename);
    });

    mScene = std::make_unique<Scene>();
    mProject = std::make_unique<Project>();
    mMap = std::make_unique<Map>();
    mResources = std::make_unique<ResourceManager>();

// ImGui-Editor entfernt. UI-Host ist Qt (RPGMAKER3D_EDITOR_QT) bzw. RmlUi im Player.
    RPG_LOG_INFO("ImGui editor disabled - Qt/RmlUi host only");

    // Datenbank laden / Defaults
    try {
        Database::Get().CreateDefaults();
        RPG_LOG_INFO("Database initialized with defaults");
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("Database init failed: ") + e.what());
    }

    // Projekt anlegen / laden
    try {
        if (std::filesystem::exists("./SampleProject/project.json")) {
            mProject->Load("./SampleProject");
            RPG_LOG_INFO("Loaded SampleProject");
            // Try to load real database from project
            try {
                if (Database::Get().Load(mProject->GetProjectPath())) {
                    RPG_LOG_INFO("Database loaded from project: " + mProject->GetProjectPath());
                }
            } catch (const std::exception& e) {
                RPG_LOG_ERROR(std::string("Database::Load fehlgeschlagen (Defaults bleiben aktiv): ") + e.what());
            }
        } else {
            mProject->New("./SampleProject", "Sample RPG 3D");
            RPG_LOG_INFO("Created new SampleProject");
        }
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("Projekt laden/erzeugen fehlgeschlagen: ") + e.what());
    }

    // Load/Create scripts
    if (mScriptManager) {
        try {
            mScriptManager->LoadProjectScripts(mProject->GetProjectPath());
            // If no scripts exist, create defaults
            if (mScriptManager->GetScripts().empty()) {
                mScriptManager->CreateDefaultScripts(mProject->GetProjectPath());
            }
        } catch (const std::exception& e) {
            RPG_LOG_ERROR(std::string("Script-Laden fehlgeschlagen: ") + e.what());
        }
    }

    // Tileset + Map Setup
    mMap->AddLayer("Ground");
    auto tileset = std::make_shared<Tileset>();
    // Versuche mehrere Pfade
    bool tilesetLoaded = false;
    std::vector<std::string> tryPaths = {
        mProject->GetAssetPath("textures/tileset_demo.png"),
        "./SampleProject/assets/textures/tileset_demo.png",
        "assets/textures/tileset_demo.png",
        "./assets/textures/tileset_demo.png"
    };
    for (auto& p : tryPaths) {
        if (Platform::FileExists(p)) {
            tileset->Load(p, 32, 32);
            tilesetLoaded = true;
            RPG_LOG_INFO("Tileset loaded: " + p);
            break;
        }
    }
    if (!tilesetLoaded) {
        tileset->Load("assets/textures/tileset_demo.png", 32, 32); // wird checker fallback
    }
    mMap->SetTileset(tileset);
    // Passability-Flags aus Database auf Tileset anwenden
    try {
        if (!Database::Get().Tilesets().empty()) {
            const auto& flags = Database::Get().Tilesets()[0].flags;
            for (size_t i = 0; i < flags.size(); ++i) {
                if (flags[i]) {
                    rpg::TileInfo ti;
                    if (const auto* old = tileset->GetTileInfo((int)i)) ti = *old;
                    ti.id = (int)i;
                    ti.solid = true;
                    tileset->SetTileInfo((int)i, ti);
                }
            }
        }
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("Tileset-Passability anwenden fehlgeschlagen: ") + e.what());
    }

    // Bind GameMap for collision checks
    Game::Get().Map().BindMap(mMap.get());
    RPG_LOG_INFO("GameMap bound to editor Map for collision");

    // Neues Projekt: Karte LEER lassen, nicht mit Demo-Tiles fuellen
    // Früher wurde hier ((x+z)%8) gesetzt, das führte zu grauem Boden + kaputtem Mapping
    // Jetzt: nur wenn kein Save existiert und Map noch leer ist, KEINE Tiles setzen
    // Damit neues Projekt wirklich leer ist (Nutzer kann selbst bemalen)
    // Demo-Tiles nur noch wenn explizit gewünscht via Script
    // mMap ist bereits mit -1 (empty) initialisiert durch Resize/AddLayer

    // Game System – NICHT hier NewGame aufrufen!
    // Game::NewGame() wird vom Player / Editor Play-Mode explizit gestartet,
    // damit Editor und Player identisch sind.
    EventSystem::Get().Clear();

    // Editor-Host: Qt (extern) – kein ImGui-Editor mehr.
    if (editorMode) {
#ifdef RPGMAKER3D_EDITOR_QT
        RPG_LOG_INFO("Qt editor host active (ImGui Editor removed)");
#else
        RPG_LOG_INFO("Editor mode without Qt host – RmlUi panels (F9) + F5 Playtest. "
                     "Fuer vollen Qt-Editor: Qt6 + -DRPGMAKER3D_EDITOR_QT=ON -DCMAKE_PREFIX_PATH=<Qt>");
#endif
    }

    // Demo Entitäten
    EntityID cube = mScene->CreateEntity("Demo Cube");
    auto* tc = mScene->AddComponent<TransformComponent>(cube);
    tc->transform.position = Vec3(0, 0.5f, 0);
    auto* sc = mScene->AddComponent<ModelRendererComponent>(cube);
    sc->model = std::make_shared<Model>();
    sc->model->AddMesh(MeshFactory::CreateCube(1.0f));
    auto* mat = mScene->AddComponent<MaterialComponent>(cube);
    mat->material.diffuse = Color(0.2f, 0.6f, 1.0f, 1.0f);

    EntityID floor = mScene->CreateEntity("Floor");
    auto* tf = mScene->AddComponent<TransformComponent>(floor);
    tf->transform.position = Vec3(0, 0, 0);
    auto* mf = mScene->AddComponent<ModelRendererComponent>(floor);
    mf->model = std::make_shared<Model>();
    mf->model->AddMesh(MeshFactory::CreatePlane(20.0f));

    // Grid
    mGridMesh = MeshFactory::CreateGrid(40, 1.0f);

    // UI
    GameUI::Get().Title().onNewGame = []() {
        Game::Get().NewGame();
        RPG_LOG_INFO("New Game via Title Screen");
    };
    GameUI::Get().Title().onExit = [this]() {
        this->RequestQuit();
    };

    mRunning = true;
    mInitialized = true;
    RPG_LOG_INFO("Engine initialized successfully");
    return true;
}

unsigned int Engine::GetSceneTextureID() const {
    if (mSceneFramebuffer) return mSceneFramebuffer->GetTextureID();
    return 0;
}

void Engine::SetPlaying(bool playing) {
    if (playing == mPlayMode) return;
    mPlayMode = playing;

    if (mEditorMode) {
        if (playing) {
            RPG_LOG_INFO("=== PLAYTEST START ===");
            // Snapshot current editor scene so Stop restores everything
            if (mProject) {
                SaveScene(mProject->GetProjectPath() + "/__editor_play_backup.json");
                // Savegames des Playtests gehoeren ins Projekt, nicht ins
                // Arbeitsverzeichnis des Editors (gleiche Regel wie Player)
                Game::Get().SetSaveDirectory(mProject->GetProjectPath() + "/saves");
            }

            // Ruby-Startpruefung: alle .rb einmal parsen, Fehler (Datei:Zeile)
            // landen im Editor-Log, bevor der Playtest loslaeuft.
            if (mScriptManager) {
                std::vector<std::string> scriptErrors;
                if (!mScriptManager->ValidateAllScripts(scriptErrors)) {
                    for (const auto& e : scriptErrors)
                        RPG_LOG_ERROR("[Ruby] Playtest-Skriptfehler: " + e);
                    RPG_LOG_ERROR("[Ruby] Bitte im Skript-Tab korrigieren und erneut speichern.");
                }
            }

            // Start game at camera-look ground point if possible, else system start
            Vec3 spawn(static_cast<float>(Database::Get().System().startX),
                       0.0f,
                       static_cast<float>(Database::Get().System().startY));
            {
                Camera& cam = mRenderer->GetCamera();
                Ray ray{ cam.GetPosition(), cam.GetForward() };
                auto hit = Raycast::IntersectPlane(ray, Vec3(0, 1, 0), Vec3(0, 0, 0));
                if (hit.hit) spawn = hit.point + Vec3(0, 0.05f, 0);
            }

            EventSystem::Get().Clear();
            Game::Get().NewGameAt(spawn, Database::Get().System().startMapId);
            EventSystem::Get().BindRuntimeCallbacks();
            EventSystem::Get().LoadMapEvents(Game::Get().Map().GetMapId(),
                mProject ? mProject->GetProjectPath() : ".");
            EventSystem::Get().EnsureDemoEvent();

            // Hide title during playtest; show a short intro message
            GameUI::Get().Title().Hide();
            // Menue-Callbacks (Speichern/Beenden) sind bereits zentral in
            // InitializeInternal verdrahtet (gelten auch fuer den Player).
#ifdef RPGMAKER3D_ENABLE_RMLUI
            if (mRmlUi) mRmlUi->SetVisible(true); // HUD im Playtest zeigen
#endif
            GameUI::Get().ShowMessage(std::string("PLAYTEST\nWASD bewegen | E/Enter sprechen | Esc Pause\nGehe zum Dorfältesten (NPC) und drücke E."));

            if (mScriptManager) mScriptManager->ExecuteAllScripts();
            RPG_LOG_INFO("Playtest spawn at " + std::to_string(spawn.x) + "," +
                         std::to_string(spawn.z) + " | gold=" +
                         std::to_string(Game::Get().Party().GetGold()));
        } else {
            RPG_LOG_INFO("=== PLAYTEST STOP ===");
            GameUI::Get().Message().Hide();
            GameUI::Get().Pause().Hide();
#ifdef RPGMAKER3D_ENABLE_RMLUI
            if (mRmlUi) mRmlUi->SetVisible(false); // Editor: HUD wieder aus
#endif
            if (mProject) {
                std::string backup = mProject->GetProjectPath() + "/__editor_play_backup.json";
                if (std::filesystem::exists(backup)) LoadScene(backup);
            }
            EventSystem::Get().Clear();
            Game::Get().SetGameStarted(false);
            Game::Get().Player().SetLocked(false);
        }
    } else {
        RPG_LOG_INFO(std::string("Play Mode ") + (playing ? "ON (Player)" : "OFF (Player)"));
        if (playing) {
            // Player: Savegame-Ordner absichern (player_main setzt ihn schon;
            // hier als Fallback, falls der Pfad anders zusammengesetzt wurde)
            if (mProject && !mProject->GetProjectPath().empty())
                Game::Get().SetSaveDirectory(mProject->GetProjectPath() + "/saves");
            EventSystem::Get().BindRuntimeCallbacks();
            EventSystem::Get().LoadMapEvents(Game::Get().Map().GetMapId(),
                mProject ? mProject->GetProjectPath() : ".");
            EventSystem::Get().EnsureDemoEvent();
            if (mScriptManager) mScriptManager->ExecuteAllScripts();
        }
    }
}

// ---------------------------------------------------------------------------
// XP-Titelbildschirm (Player): Neues Spiel / Weiterspielen / Beenden
// ---------------------------------------------------------------------------
void Engine::StartTitleMode() {
    auto& title = GameUI::Get().Title();

    // Titel-BGM + Titelgrafik aus der Datenbank (System-Tab, XP)
    const auto& sys = Database::Get().System();
    if (!sys.titleBgm.empty()) PlayEventAudio(sys.titleBgm, 0, true);
    if (!sys.titleGraphicName.empty()) {
        const int picId = GameUI::Get().ShowPicture(
            sys.titleGraphicName, Vec2(0.5f, 0.5f), 1.0f, 1.0f, 0.0f, "$title");
        GameUI::Get().SetPictureSize(picId, 1.0f, 1.0f); // Vollbild
    }

    title.onNewGame = [this]() {
        EndTitleMode();
        Game::Get().NewGame();
        SetPlaying(true); // Player-Zweig: laedt Events + fuehrt Skripte aus
    };
    title.onContinue = [this]() {
        // Lade-Ansicht des Speicherbildschirms; danach: entweder Spiel
        // weiterfuehren (Slot geladen) oder Abbruch -> zurueck zum Titel.
        GameUI::Get().ShowSaveScreen(false, [this]() {
            if (Game::Get().IsGameStarted()) {
                EndTitleMode();
                // Karte/Events/BGM kamen bereits per Game::Load-
                // Map-Wechsel-Hook; SetPlaying startet Logik + Skripte.
                SetPlaying(true);
            } else {
                GameUI::Get().Title().Show();
            }
        });
    };
    title.onExit = [this]() { RequestQuit(); };
    title.Show();
    RPG_LOG_INFO("Titelbildschirm aktiv (Neues Spiel / Weiterspielen / Beenden)");
}

void Engine::EndTitleMode() {
    GameUI::Get().Title().Hide();
    GameUI::Get().Menu().Hide();
    GameUI::Get().RemovePicture(std::string("$title"));
    if (mAudio) mAudio->FadeOutBGM(0.3f); // Karten-BGM uebernimmt danach
}

void Engine::ReturnToTitle() {
    // Spiel sauber anhalten (analog PLAYTEST STOP), dann Titel zeigen
    GameUI::Get().Message().Hide();
    GameUI::Get().Menu().Hide();
    EventSystem::Get().Clear();
    if (BattleSystem::Get().IsInBattle()) BattleSystem::Get().Abort();
    Game::Get().SetGameStarted(false);
    Game::Get().Player().SetLocked(false);
    SetPlaying(false);
    StartTitleMode();
}

// ---------------------------------------------------------------------------
// Bild-Pfadaufloesung (Graphics/Pictures|Titles, XP-Struktur)
// ---------------------------------------------------------------------------
std::string Engine::ResolvePicturePathFor(const std::string& filename) const {
    if (filename.empty()) return {};
    const std::string base = mProject ? mProject->GetProjectPath() : std::string();
    static const char* kDirs[] = {
        "Graphics/Pictures/", "Graphics/Titles/", "Pictures/", "pictures/",
        "assets/pictures/", "assets/textures/", "assets/", ""
    };
    static const char* kExts[] = {"", ".png", ".jpg", ".jpeg", ".bmp", ".tga"};
    std::vector<std::string> roots;
    if (!base.empty())
        for (const char* d : kDirs) roots.push_back(base + "/" + d);
    for (const char* d : kDirs) roots.push_back(d); // Fallback: relativ zum CWD
    for (const auto& root : roots) {
        for (const char* ext : kExts) {
            const std::string p = root + filename + ext;
            if (!p.empty() && std::filesystem::exists(p) &&
                !std::filesystem::is_directory(p))
                return p;
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// Event-Audio (BGM/BGS/ME/SE) mit XP-Pfadaufloesung
// ---------------------------------------------------------------------------

std::string Engine::ResolveAudioPath(const std::string& name, int kind) const {
    static const char* kDirs[4] = {"BGM", "BGS", "ME", "SE"};
    static const char* kDirsLower[4] = {"bgm", "bgs", "me", "se"};
    const int k = (kind >= 0 && kind < 4) ? kind : 3;
    const std::string base = mProject ? mProject->GetProjectPath() : std::string();

    // Suchreihenfolge: XP-Projektstruktur zuerst, dann Engine-Assets,
    // zuletzt der Name selbst (falls der Aufrufer schon einen Pfad gab).
    std::vector<std::string> roots;
    if (!base.empty()) {
        roots.push_back(base + "/Audio/" + kDirs[k] + "/");
        roots.push_back(base + "/Audio/" + kDirsLower[k] + "/");
        roots.push_back(base + "/audio/" + kDirsLower[k] + "/");
        roots.push_back(base + "/assets/audio/" + kDirsLower[k] + "/");
        roots.push_back(base + "/assets/audio/" + std::string(kDirs[k]) + "/");
    }
    roots.push_back("assets/audio/" + std::string(kDirs[k]) + "/");
    roots.push_back("assets/audio/" + std::string(kDirsLower[k]) + "/");
    roots.push_back("");

    static const char* kExts[] = {"", ".ogg", ".mp3", ".wav", ".flac"};
    for (const auto& root : roots) {
        for (const char* ext : kExts) {
            const std::string p = root + name + ext;
            if (!p.empty() && std::filesystem::exists(p) &&
                !std::filesystem::is_directory(p))
                return p;
        }
    }
    return {};
}

void Engine::PlayEventAudio(const std::string& name, int kind, bool loop) {
    if (!mAudio) return;
    if (name.empty()) {
        // Stop-Befehle (FadeOut): nur bei Musik-Hintergrundarten sinnvoll
        if (kind == 0) mAudio->FadeOutBGM(0.5f);
        else if (kind == 1) mAudio->FadeOutBGS(0.5f);
        return;
    }
    const std::string path = ResolveAudioPath(name, kind);
    if (path.empty()) {
        RPG_LOG_WARN("[Audio] Datei nicht gefunden (Audio/" +
                     std::string(kind == 0 ? "BGM" : kind == 1 ? "BGS" :
                                 kind == 2 ? "ME" : "SE") + "): " + name);
        return;
    }
    switch (kind) {
        case 0:  mAudio->PlayBGM(path, loop); break;
        case 1:  mAudio->PlayBGS(path, loop); break;
        case 2:  mAudio->PlayME(path, false); break;
        default: mAudio->PlaySE(path, false); break;
    }
    RPG_LOG_INFO("[Audio] " + std::string(kind == 0 ? "BGM" : kind == 1 ? "BGS" :
                 kind == 2 ? "ME" : "SE") + ": " + name);
}

void Engine::Shutdown() {
    RPG_LOG_INFO("Engine shutdown started");
    mInitialized = false;
// ImGui/Editor shutdown removed
    mGridMesh.Delete();
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (mRmlUi) { mRmlUi->Shutdown(); mRmlUi.reset(); }
#endif
    if (mResources) mResources.reset();
    if (mMap) mMap.reset();
    if (mProject) mProject.reset();
    if (mScene) mScene.reset();
    if (mCommandHistory) mCommandHistory.reset();
    if (mRubyVM) mRubyVM.reset();
    if (mAudio) mAudio.reset();
    if (mInput) mInput.reset();
    if (mRenderer) mRenderer.reset();
    if (mSceneFramebuffer) mSceneFramebuffer.reset();
    if (mWindow) mWindow.reset();
    mRunning = false;
    RPG_LOG_INFO("Engine shutdown complete");
}

void Engine::Run() {
    using Clock = std::chrono::high_resolution_clock;
    auto lastTime = Clock::now();

    while (mRunning) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        // Clamp dt für Stabilität (z.B. bei Debugger Pause)
        if (dt > 0.1f) dt = 0.1f;
        mDeltaTime = dt;
        mTime += dt;

        mFrameCount++;
        mFPSTimer += dt;
        if (mFPSTimer >= 0.5f) {
            mFPS = static_cast<int>(mFrameCount / mFPSTimer);
            mFrameCount = 0;
            mFPSTimer = 0.0f;
        }

        Update(dt);
        if (!mRunning) break;

        Render();
        mWindow->SwapBuffers();
    }
}

static Key MapSDLKey(SDL_Scancode code) {
    switch (code) {
        case SDL_SCANCODE_A: return Key::A;
        case SDL_SCANCODE_B: return Key::B;
        case SDL_SCANCODE_C: return Key::C;
        case SDL_SCANCODE_D: return Key::D;
        case SDL_SCANCODE_E: return Key::E;
        case SDL_SCANCODE_F: return Key::F;
        case SDL_SCANCODE_G: return Key::G;
        case SDL_SCANCODE_H: return Key::H;
        case SDL_SCANCODE_I: return Key::I;
        case SDL_SCANCODE_J: return Key::J;
        case SDL_SCANCODE_K: return Key::K;
        case SDL_SCANCODE_L: return Key::L;
        case SDL_SCANCODE_M: return Key::M;
        case SDL_SCANCODE_N: return Key::N;
        case SDL_SCANCODE_O: return Key::O;
        case SDL_SCANCODE_P: return Key::P;
        case SDL_SCANCODE_Q: return Key::Q;
        case SDL_SCANCODE_R: return Key::R;
        case SDL_SCANCODE_S: return Key::S;
        case SDL_SCANCODE_T: return Key::T;
        case SDL_SCANCODE_U: return Key::U;
        case SDL_SCANCODE_V: return Key::V;
        case SDL_SCANCODE_W: return Key::W;
        case SDL_SCANCODE_X: return Key::X;
        case SDL_SCANCODE_Y: return Key::Y;
        case SDL_SCANCODE_Z: return Key::Z;
        case SDL_SCANCODE_0: return Key::Num0;
        case SDL_SCANCODE_1: return Key::Num1;
        case SDL_SCANCODE_2: return Key::Num2;
        case SDL_SCANCODE_3: return Key::Num3;
        case SDL_SCANCODE_4: return Key::Num4;
        case SDL_SCANCODE_5: return Key::Num5;
        case SDL_SCANCODE_6: return Key::Num6;
        case SDL_SCANCODE_7: return Key::Num7;
        case SDL_SCANCODE_8: return Key::Num8;
        case SDL_SCANCODE_9: return Key::Num9;
        case SDL_SCANCODE_ESCAPE: return Key::Escape;
        case SDL_SCANCODE_SPACE: return Key::Space;
        case SDL_SCANCODE_RETURN: return Key::Enter;
        case SDL_SCANCODE_TAB: return Key::Tab;
        case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
        case SDL_SCANCODE_DELETE: return Key::Delete;
        case SDL_SCANCODE_LEFT: return Key::Left;
        case SDL_SCANCODE_RIGHT: return Key::Right;
        case SDL_SCANCODE_UP: return Key::Up;
        case SDL_SCANCODE_DOWN: return Key::Down;
        case SDL_SCANCODE_LSHIFT: return Key::LShift;
        case SDL_SCANCODE_LCTRL: return Key::LCtrl;
        case SDL_SCANCODE_LALT: return Key::LAlt;
        case SDL_SCANCODE_F1: return Key::F1;
        case SDL_SCANCODE_F2: return Key::F2;
        case SDL_SCANCODE_F3: return Key::F3;
        case SDL_SCANCODE_F4: return Key::F4;
        case SDL_SCANCODE_F5: return Key::F5;
        case SDL_SCANCODE_F6: return Key::F6;
        case SDL_SCANCODE_F7: return Key::F7;
        case SDL_SCANCODE_F8: return Key::F8;
        case SDL_SCANCODE_F9: return Key::F9;
        case SDL_SCANCODE_F10: return Key::F10;
        case SDL_SCANCODE_F11: return Key::F11;
        case SDL_SCANCODE_F12: return Key::F12;
        default: return Key::Unknown;
    }
}

void Engine::Update(float dt) {
    mInput->Update();

#ifdef RPGMAKER3D_EDITOR_QT
    // Im Qt-Editor gibt es kein SDL-Fenster: Input kommt vom Qt-Widget
    // (ruft OnKeyChanged/OnMouseMoved direkt), RmlUi-Input ist hier (noch) nicht angeschlossen.
#else
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
#ifdef RPGMAKER3D_ENABLE_RMLUI
        if (mRmlUi) mRmlUi->ProcessEvent(e);
#endif
        switch (e.type) {
            case SDL_QUIT:
                mRunning = false;
                return;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                mInput->OnKeyChanged(MapSDLKey(e.key.keysym.scancode), e.type == SDL_KEYDOWN);
                break;
            case SDL_MOUSEMOTION:
                mInput->OnMouseMoved(static_cast<float>(e.motion.x), static_cast<float>(e.motion.y));
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT)
                    mInput->OnMouseChanged(MouseButton::Left, e.type == SDL_MOUSEBUTTONDOWN);
                if (e.button.button == SDL_BUTTON_RIGHT)
                    mInput->OnMouseChanged(MouseButton::Right, e.type == SDL_MOUSEBUTTONDOWN);
                if (e.button.button == SDL_BUTTON_MIDDLE)
                    mInput->OnMouseChanged(MouseButton::Middle, e.type == SDL_MOUSEBUTTONDOWN);
                break;
            case SDL_MOUSEWHEEL:
                mInput->OnMouseWheel(static_cast<float>(e.wheel.y));
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
                    mRunning = false;
                    return;
                }
                break;
        }
    }
#endif // !RPGMAKER3D_EDITOR_QT

    // Hinweis: Esc (Spielmenue) wird NUR im PlayMode-Block weiter unten
    // behandelt. Ein zweiter globaler Handler hier toggelte im Player
    // oeffnend+schliessend im selben Frame (Menue kam nie hoch).

    // F5: Playtest-Toggle im Editor-Modus (SDL-Host ohne Qt / RmlUi-Panel)
    if (mEditorMode && mInput->IsKeyPressed(Key::F5)) {
        SetPlaying(!mPlayMode);
    }

    // Kamera Navigation (Editor oder Play)
    // Qt-Editor: Input kommt nur vom Game-View-Widget (StrongFocus) -> immer erlaubt.
    bool allowCamera = true;

    // Im PlayMode: Kamera folgt optional dem GamePlayer
    // (kann im Editor umgeschaltet werden) - NUR wenn Follow Player AKTIV
    if (mPlayMode && mPlayModeFollowPlayer) {
        Camera& cam = mRenderer->GetCamera();
        Vec3 playerPos = Game::Get().Player().GetPosition();
        // Simple Follow-Cam: leicht versetzt hinter/über dem Spieler
        Vec3 targetPos = playerPos + Vec3(0, 3.0f, 5.0f);
        cam.SetPosition(targetPos);
        cam.SetRotation(Vec3(-20.0f, 0.0f, 0.0f));
        allowCamera = false; // keine Free-Fly im PlayMode wenn Follow aktiv
    }
    // Im PlayMode OHNE Follow: Erlaube Editor-Kamera (free-fly) für Testing
    // Das erlaubt im Editor Play-Test: Kamera frei bewegen während Spiel läuft

    if (allowCamera) {
        Camera& cam = mRenderer->GetCamera();
        float speed = (mInput->IsKeyDown(Key::LShift) ? 15.0f : 6.0f) * dt;
        
        // Movement relative to camera direction (WASD)
        if (mInput->IsKeyDown(Key::W)) cam.SetPosition(cam.GetPosition() + cam.GetForward() * speed);
        if (mInput->IsKeyDown(Key::S)) cam.SetPosition(cam.GetPosition() - cam.GetForward() * speed);
        if (mInput->IsKeyDown(Key::A)) cam.SetPosition(cam.GetPosition() - cam.GetRight() * speed);
        if (mInput->IsKeyDown(Key::D)) cam.SetPosition(cam.GetPosition() + cam.GetRight() * speed);
        if (mInput->IsKeyDown(Key::Q)) cam.SetPosition(cam.GetPosition() + Vec3(0, 1, 0) * speed);  // Up
        if (mInput->IsKeyDown(Key::E)) cam.SetPosition(cam.GetPosition() - Vec3(0, 1, 0) * speed);  // Down

        // Orbit / Look around: Right mouse drag
        if (mInput->IsMouseDown(MouseButton::Right)) {
            Vec2 delta = mInput->GetMouseDelta();
            Vec3 rot = cam.GetRotation();
            rot.y -= delta.x * 0.3f;   // Yaw (horizontal)
            rot.x -= delta.y * 0.3f;   // Pitch (vertical)
            rot.x = glm::clamp(rot.x, -89.0f, 89.0f);
            cam.SetRotation(rot);
        }

        // Mouse wheel: Zoom (dolly)
        if (mInput->GetMouseWheel() != 0.0f) {
            cam.SetPosition(cam.GetPosition() + cam.GetForward() * mInput->GetMouseWheel() * 3.0f);
        }
        
        // Middle mouse: Pan (optional)
        if (mInput->IsMouseDown(MouseButton::Middle)) {
            Vec2 delta = mInput->GetMouseDelta();
            Vec3 right = cam.GetRight();
            Vec3 up = cam.GetUp();
            cam.SetPosition(cam.GetPosition() - right * delta.x * 0.01f * speed * 10.0f + up * delta.y * 0.01f * speed * 10.0f);
        }
    }

    // Partikel & Scene
    for (EntityID id : mScene->GetEntities()) {
        auto* emitter = mScene->GetComponent<ParticleEmitterComponent>(id);
        if (emitter && emitter->emitter) {
            emitter->emitTimer += dt;
            if (emitter->autoEmit && emitter->emitTimer >= emitter->emitRate) {
                auto* transform = mScene->GetComponent<TransformComponent>(id);
                Vec3 origin = transform ? transform->transform.position : Vec3(0.0f);
                emitter->emitter->Emit(emitter->emitCount, origin, emitter->emitDirection,
                    emitter->emitSpread, emitter->emitSpeed, emitter->emitLife, emitter->emitColor);
                emitter->emitTimer = 0.0f;
            }
            emitter->emitter->Update(dt);
        }
    }

    // Ensure GameMap always bound to current editor Map (for collision)
    if (mMap) {
        Game::Get().Map().BindMap(mMap.get());
    }

    // Titelbildschirm braucht Menue-Eingabe auch ohne laufendes Spiel.
    if (!mPlayMode && GameUI::Get().Title().IsVisible())
        GameUI::Get().UpdateModalInput(*mInput);

    // Game Logic - läuft im PlayMode (Editor Play-Test UND Player)
    if (mPlayMode) {
        // Modale Eingaben (Choices, Zahleneingabe 103, Namenseingabe 303)
        // haben Vorrang vor allem anderen und konsumieren die Tasten zuerst.
        GameUI::Get().UpdateModalInput(*mInput);

        const bool modalActive = GameUI::Get().IsNumberInputActive() ||
                                 GameUI::Get().IsNameInputActive() ||
                                 GameUI::Get().Message().HasChoices() ||
                                 GameUI::Get().Menu().IsVisible();
        // XP: Esc oeffnet das Spielmenue; Schliessen laeuft ueber
        // MenuWindow::Cancel in UpdateModalInput (Teil von modalActive).
        // Im Kampf ist der Menueaufruf gesperrt (XP-Verhalten).
        if (!modalActive && mInput->IsKeyPressed(Key::Escape) &&
            !GameUI::Get().Message().IsBusy() &&
            !BattleSystem::Get().IsInBattle()) {
            GameUI::Get().Pause().Show();
        }
        // Interact with nearby events (E or Enter) when not in dialog
        if (!modalActive && !GameUI::Get().Message().IsBusy() &&
            !GameUI::Get().Menu().IsVisible() && !BattleSystem::Get().IsInBattle()) {
            if (mInput->IsKeyPressed(Key::E) || mInput->IsKeyPressed(Key::Enter)) {
                EventSystem::Get().TryInteract(Game::Get().Player().GetPosition());
            }
        }
        // Advance/close message with E/Enter/Space (nicht waehrend Choices/Inputs/Menue)
        if (!GameUI::Get().Message().HasChoices() &&
            !GameUI::Get().IsNumberInputActive() && !GameUI::Get().IsNameInputActive() &&
            !GameUI::Get().Menu().IsVisible() &&
            GameUI::Get().Message().IsBusy()) {
            if (mInput->IsKeyPressed(Key::E) || mInput->IsKeyPressed(Key::Enter) || mInput->IsKeyPressed(Key::Space)) {
                GameUI::Get().Message().AdvanceInput();
            }
        }

        Game::Get().Update(dt);
        if (!GameUI::Get().Menu().IsVisible() && !BattleSystem::Get().IsInBattle()) {
            Game::Get().Player().Update(dt, *mInput);
        }
        // XP-Kampfmenue: Sobald ein Akteur eine Aktion waehlen darf, oeffnet
        // sich das Befehlsmenue (Angriff/Fertigkeit/Gegenstand/Verteidigen/
        // Flucht) mit Ziel- und Listen-Untermenues. Solange ein Menue offen
        // ist, wird nicht erneut geoeffnet; die Eingabe laeuft ueber
        // UpdateModalInput (MenuWindow hat oberste Prioritaet).
        if (BattleSystem::Get().NeedsInput() && !GameUI::Get().Menu().IsVisible() &&
            !GameUI::Get().Message().IsBusy()) {
            GameUI::Get().OpenBattleCommands();
        }
        BattleSystem::Get().Update(dt);

        // XP-Kampfstatus: Gegner- und Gruppen-Zeile oben im Bild, solange der
        // Kampf laeuft (wird nach dem Kampfende automatisch entfernt).
        if (BattleSystem::Get().IsInBattle()) {
            auto& bs = BattleSystem::Get();
            if (mBattleStatusEnemiesId < 0) {
                mBattleStatusEnemiesId = GameUI::Get().AddScreenText(
                    "", Vec2(0.5f, 0.03f), Color(1.0f, 0.85f, 0.6f, 1.0f), 0.0f, true, 1.0f);
                mBattleStatusPartyId = GameUI::Get().AddScreenText(
                    "", Vec2(0.5f, 0.10f), Color(0.75f, 1.0f, 0.75f, 1.0f), 0.0f, true, 1.0f);
            }
            mBattleStatusTimer -= dt;
            if (mBattleStatusTimer <= 0.0f) {
                mBattleStatusTimer = 0.25f;
                std::string enemies;
                for (const auto& e : bs.Enemies()) {
                    if (!enemies.empty()) enemies += "     ";
                    enemies += e.isDead
                        ? ("[" + e.name + " besiegt]")
                        : (e.name + "  " + std::to_string(e.hp) + "/" + std::to_string(e.maxHp));
                }
                std::string party;
                for (const auto& a : bs.Actors()) {
                    if (!party.empty()) party += "    |    ";
                    party += a.name + "  " + std::to_string(a.hp) + "/" + std::to_string(a.maxHp) +
                             " HP, " + std::to_string(a.mp) + "/" + std::to_string(a.maxMp) + " MP";
                    if (a.isDead) party += " (K.O.)";
                }
                GameUI::Get().SetScreenText(mBattleStatusEnemiesId, enemies);
                GameUI::Get().SetScreenText(mBattleStatusPartyId, party);
            }
        } else if (mBattleStatusEnemiesId >= 0) {
            GameUI::Get().RemoveScreenText(mBattleStatusEnemiesId);
            GameUI::Get().RemoveScreenText(mBattleStatusPartyId);
            mBattleStatusEnemiesId = -1;
            mBattleStatusPartyId = -1;
            mBattleStatusTimer = 0.0f;
        }
        if (mRubyVM) mRubyVM->Update(dt);
    }

    // UI - GameUI läuft im Player IMMER, im Editor nur im PlayMode
    GameUI::Get().Update(dt);

#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (mRmlUi) mRmlUi->Update(dt);
#endif

    // Push scene LightComponents into the global lighting system (point lights)
    // so floors/objects actually receive per-object light in the editor & play mode.
    {
        auto& lighting = Lighting::Get();
        lighting.ClearPointLights();
        for (EntityID id : mScene->GetEntities()) {
            auto* light = mScene->GetComponent<LightComponent>(id);
            auto* tr = mScene->GetComponent<TransformComponent>(id);
            if (!light || !tr) continue;
            auto& pl = lighting.AddPointLight();
            pl.enabled = true;
            pl.position = tr->transform.position;
            pl.color = light->color;
            pl.intensity = light->intensity;
            pl.range = light->range > 0.0f ? light->range : 10.0f;
        }
    }

    mScene->Update(dt);
}

void Engine::Render() {
    // Direkt in den aktuellen GL-Framebuffer rendern
    // (Qt: QOpenGLWidget FBO; Player: Default-Framebuffer)
    // Qt-Fix: Das Host-FBO merken. Shadow-Passes binden eigene FBOs und duerfen
    // am Ende NICHT hart 0 binden (sonst bleibt der Game View schwarz, weil die
    // Szene in den Fenster-Backbuffer statt ins Widget-FBO laeuft).
    int hostFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &hostFBO);
    if (mWindow) {
        Camera& cam = mRenderer->GetCamera();
        const int w = mWindow->GetWidth();
        const int h = mWindow->GetHeight();
        if (w > 0 && h > 0) {
            cam.SetPerspective(60.0f, (float)w / (float)h, 0.1f, 1000.0f);
            glViewport(0, 0, w, h);
        }
    }
    RenderScene();
    // Sicherheitsnetz: falls ein Pass das FBO verbogen hat, zurueck zum Host
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<unsigned int>(hostFBO));
    if (mWindow && mWindow->GetWidth() > 0 && mWindow->GetHeight() > 0)
        glViewport(0, 0, mWindow->GetWidth(), mWindow->GetHeight());

    // GameUI-Draw ist ohne ImGui No-Op; Logik (Messages) laeuft weiter via Input.
#ifdef RPGMAKER3D_ENABLE_IMGUI
    if (mPlayMode || !mEditorMode) {
        GameUI::Get().Draw();
        if (mPlayMode) GameUI::Get().DrawPlayHud(mEditorMode);
    }
#else
    if (mPlayMode) {
        // Fortschritt nur ueber Engine-Input (AdvanceInput bereits in Update)
        (void)0;
    }
#endif

#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (mRmlUi) mRmlUi->Render();
#endif
}

void Engine::RenderScene() {
    // Bestimme aktive Kamera - vermeide goto und dangling pointer (MSVC mag goto nicht)
    Camera activeCam;
    bool hasActiveCam = false;
    Camera* camera = &mRenderer->GetCamera();

    if (mActiveCameraEntity != INVALID_ENTITY) {
        auto* camComp = mScene->GetComponent<CameraComponent>(mActiveCameraEntity);
        auto* transform = mScene->GetComponent<TransformComponent>(mActiveCameraEntity);
        if (camComp && transform) {
            activeCam.SetPosition(transform->transform.position);
            activeCam.SetRotation(transform->transform.rotation);
            activeCam.SetPerspective(camComp->fov, camComp->aspect, camComp->nearPlane, camComp->farPlane);
            camera = &activeCam;
            hasActiveCam = true;
        }
    }

    // === SHADOW PASS (vor normalem Frame) ===
    // Render depth from directional light perspective into shadow map
    // BUGFIX: Shadow FBO Bind hat vorher Scene Framebuffer überschrieben und nicht wiederhergestellt,
    // deshalb war Scene im Editor schwarz/leer (Player.exe ohne Framebuffer ging).
    // Fix: Nach Shadow-Pass Framebuffer re-binden + Viewport restoren, plus PolygonOffset gegen Light-Leak.
    if (mRenderer->IsShadowsEnabled() && Lighting::Get().GetShadows().enabled && Lighting::Get().GetDirectionalLight().castShadows) {
        // Light space matrix based on current lighting/time of day + map bounds
        float mapW = mMap ? static_cast<float>(mMap->GetWidth()) : 20.0f;
        float mapH = mMap ? static_cast<float>(mMap->GetHeight()) : 20.0f;
        float adaptiveOrtho = std::max(mapW, mapH) * 0.6f + 10.0f;
        float ortho = Lighting::Get().GetShadows().orthoSize;
        // Wenn ortho viel zu groß für kleine Map, adaptive nehmen um Schattenquali zu verbessern (weniger Light-Leak an Ecken)
        if (ortho > adaptiveOrtho * 1.5f) ortho = adaptiveOrtho;

        mRenderer->CalculateLightSpaceMatrix(
            ortho,
            Lighting::Get().GetShadows().nearPlane,
            Lighting::Get().GetShadows().farPlane
        );
        mRenderer->BeginShadowPass();
        // Map depth - nur wenn Map sichtbar (Scene System) und nicht zu flach (Boden empfängt nur, wirft aber auch wenn elevation>0)
        if (mMap && Game::Get().Map().IsVisible()) {
            mMap->RenderDepth(*mRenderer);
        }
        // Entities depth – alle Objekte werfen Schatten (Fix: vorher hat nur ModelRenderer Schatten geworfen,
        // aber Light-Entity Meshes und Particle etc. nicht. Jetzt auch Bounding und alle.)
        for (EntityID id : mScene->GetEntities()) {
            auto* transform = mScene->GetComponent<TransformComponent>(id);
            if (!transform) continue;
            auto* model = mScene->GetComponent<ModelRendererComponent>(id);
            if (model && model->model) {
                Mat4 mat = transform->transform.GetMatrix();
                for (int mi = 0; mi < model->model->GetMeshCount(); ++mi) {
                    mRenderer->DrawMeshDepth(model->model->GetMesh(mi), mat);
                }
            } else {
                // Fallback: Wenn kein Model, aber z.B. nur Transform (Cube erstellt), trotzdem Schatten via kleinem Cube?
                // Wir können hier nichts tun, aber sicherstellen dass Cube Entities Model haben (tun sie).
            }
        }
        mRenderer->EndShadowPass();
        // Haupt-Scene auf den sichtbaren Framebuffer (Screen/Widget) rendern.
        // Der Offscreen-mSceneFramebuffer war nur fuer den entfernten ImGui-
        // Scene-View gedacht; ihn hier zu nutzen liess den Game View im Editor
        // leer erscheinen (Scene landete im Offscreen-Buffer, nie am Bildschirm).
        if (mWindow) {
            glViewport(0, 0, mWindow->GetWidth(), mWindow->GetHeight());
        }

        // === POINT LIGHT CUBEMAP SHADOWS (falls aktiviert) ===
        if (mRenderer->IsPointShadowsEnabled()) {
            mRenderer->RenderPointShadows(*mScene);
            // Nach Cubemap Shadow Pass wieder auf den sichtbaren Framebuffer
            // (Screen/Widget) rendern, nicht in den Offscreen-mSceneFramebuffer.
            if (mWindow) {
                glViewport(0, 0, mWindow->GetWidth(), mWindow->GetHeight());
            }
        }
    }

    if (hasActiveCam) {
        mRenderer->BeginFrame(activeCam);
    } else {
        mRenderer->BeginFrame(*camera);
    }

    // Skybox zuerst zeichnen (hinter allem)
    mRenderer->DrawSkybox(*camera);

    // Grid (nur im Editor, als Linien – nicht als gefüllte Dreiecke)
    if (mEditorMode && mShowGrid) {
        mRenderer->DrawGrid(mGridMesh, Mat4(1.0f), Color(0.35f, 0.35f, 0.40f, 0.55f));
    }

    // Map - nur wenn sichtbar (Scene System nutzt Map Daten über Script)
    // In Scene_Title oder Scene_Battle kann Map ausgeblendet sein, damit macht Scene Switch Sinn
    if (Game::Get().Map().IsVisible()) {
        mMap->Render(*mRenderer);
    }

    // Entitäten
    for (EntityID id : mScene->GetEntities()) {
        auto* transform = mScene->GetComponent<TransformComponent>(id);
        if (!transform) continue;

        auto* model = mScene->GetComponent<ModelRendererComponent>(id);
        auto* material = mScene->GetComponent<MaterialComponent>(id);

        if (model && model->model) {
            Mat4 matrix = transform->transform.GetMatrix();
            if (material) {
                if (model->model->GetMeshCount() > 0)
                    mRenderer->DrawMeshWithMaterial(model->model->GetMesh(0), matrix, material->material);
            } else {
                mRenderer->DrawModel(*model->model, matrix, model->texture.get());
            }
        }

        auto* light = mScene->GetComponent<LightComponent>(id);
        if (light) {
            Mesh lightMesh = MeshFactory::CreateCube(0.2f);
            Mat4 matrix = glm::translate(Mat4(1.0f), transform->transform.position);
            mRenderer->DrawMesh(lightMesh, matrix, nullptr, light->color);
        }

        auto* emitter = mScene->GetComponent<ParticleEmitterComponent>(id);
        if (emitter && emitter->emitter) {
            mRenderer->DrawParticles(emitter->emitter->GetParticles());
        }

        // Sprite als Billboard
        auto* sprite = mScene->GetComponent<SpriteComponent>(id);
        if (sprite) {
            Mat4 matrix;
            if (sprite->billboard) {
                Vec3 pos = transform->transform.position;
                Vec3 camPos = camera->GetPosition();
                Vec3 forward = glm::normalize(camPos - pos);
                Vec3 right = glm::normalize(glm::cross(Vec3(0,1,0), forward));
                Vec3 up = glm::cross(forward, right);
                Mat4 rot(right.x, right.y, right.z, 0,
                         up.x, up.y, up.z, 0,
                         forward.x, forward.y, forward.z, 0,
                         0,0,0,1);
                matrix = glm::translate(Mat4(1.0f), pos) * rot;
                matrix = glm::scale(matrix, Vec3(sprite->size.x * transform->transform.scale.x,
                                                 sprite->size.y * transform->transform.scale.y, 1.0f));
            } else {
                matrix = transform->transform.GetMatrix();
                matrix = glm::scale(matrix, Vec3(sprite->size.x, sprite->size.y, 1.0f));
            }
            Mesh quad = MeshFactory::CreateQuad(1.0f, 1.0f);
            mRenderer->DrawMesh(quad, matrix, sprite->texture.get(), sprite->color);
        }
    }

    // Auswahl-BoundingBox + einfache Translate-Gizmo-Achsen (Editor)
    if (mEditorMode && !mPlayMode && mSelectedEntity >= 0) {
        auto* transform = mScene->GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
        if (transform) {
            Mat4 matrix = transform->transform.GetMatrix();
            mRenderer->DrawBoundingBox(Vec3(-0.5f), Vec3(0.5f), matrix, Color(1.0f, 0.8f, 0.0f, 1.0f));
            // Achsen-Gizmo (kleine Wuerfel an den Enden der Achsen)
            const Vec3 o = transform->transform.position;
            auto drawAxis = [&](const Vec3& end, const Color& col) {
                Mesh tip = MeshFactory::CreateCube(0.12f);
                Mat4 m = glm::translate(Mat4(1.0f), end);
                mRenderer->DrawMesh(tip, m, nullptr, col);
                // Linie als gestreckter duenner Wuerfel
                Vec3 mid = (o + end) * 0.5f;
                Vec3 d = end - o;
                float len = glm::length(d);
                if (len < 1e-4f) return;
                Mat4 line = glm::translate(Mat4(1.0f), mid);
                // rough scale along largest component
                Vec3 sc(0.04f, 0.04f, 0.04f);
                if (std::fabs(d.x) > std::fabs(d.y) && std::fabs(d.x) > std::fabs(d.z)) sc = Vec3(len, 0.04f, 0.04f);
                else if (std::fabs(d.y) > std::fabs(d.z)) sc = Vec3(0.04f, len, 0.04f);
                else sc = Vec3(0.04f, 0.04f, len);
                line = glm::scale(line, sc);
                Mesh bar = MeshFactory::CreateCube(1.0f);
                mRenderer->DrawMesh(bar, line, nullptr, col);
            };
            drawAxis(o + Vec3(1.5f, 0, 0), Color(1.0f, 0.2f, 0.2f, 1.0f)); // X
            drawAxis(o + Vec3(0, 1.5f, 0), Color(0.2f, 1.0f, 0.3f, 1.0f)); // Y
            drawAxis(o + Vec3(0, 0, 1.5f), Color(0.25f, 0.45f, 1.0f, 1.0f)); // Z
        }
    }

    // Player + Event markers in PlayMode
    if (mPlayMode) {
        static Mesh playerMesh;
        static Mesh markerMesh;
        static bool meshesInit = false;
        if (!meshesInit) {
            playerMesh = MeshFactory::CreateCube(0.7f);
            markerMesh = MeshFactory::CreateCube(0.45f);
            meshesInit = true;
        }
        // Player (Transparent-Flag 208 -> halbtransparent)
        Vec3 playerPos = Game::Get().Player().GetPosition();
        Mat4 playerMat = glm::translate(Mat4(1.0f), playerPos + Vec3(0, 0.35f, 0));
        const float playerAlpha = Game::Get().Player().IsTransparent() ? 0.35f : 1.0f;
        mRenderer->DrawMesh(playerMesh, playerMat, nullptr, Color(0.25f, 0.95f, 0.35f, playerAlpha));
        // Facing indicator
        Vec3 dir = Game::Get().Player().GetDirection();
        Mat4 nose = glm::translate(Mat4(1.0f), playerPos + Vec3(0, 0.35f, 0) + dir * 0.45f);
        nose = glm::scale(nose, Vec3(0.2f, 0.2f, 0.2f));
        mRenderer->DrawMesh(markerMesh, nose, nullptr, Color(1.0f, 1.0f, 0.2f, 1.0f));

        // Event NPC markers
        for (const auto& ev : EventSystem::Get().GetEvents()) {
            if (!ev.enabled || ev.erased) continue;
            Vec3 ep = ev.worldPos;
            if (glm::length(ep) < 0.001f) ep = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
            Mat4 em = glm::translate(Mat4(1.0f), ep + Vec3(0, 0.55f, 0));
            // Bob slightly
            float bob = std::sin(mTime * 3.0f + ev.id) * 0.08f;
            em = glm::translate(em, Vec3(0, bob, 0));
            Color col = (ev.id == 1) ? Color(0.95f, 0.75f, 0.2f, 1.0f) : Color(0.4f, 0.7f, 1.0f, 1.0f);
            mRenderer->DrawMesh(markerMesh, em, nullptr, col);
        }
    }

    mRenderer->EndFrame();
}

// Helpers für JSON Escaping
namespace {
std::string EscapeJSON(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}
std::string Vec3ToJSON(const Vec3& v) {
    return "[" + std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "]";
}
std::string Vec4ToJSON(const Vec4& v) {
    return "[" + std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "," + std::to_string(v.w) + "]";
}
}

void Engine::SaveScene(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        RPG_LOG_ERROR("Failed to save scene: " + path);
        return;
    }

    file << "{\n";
    file << "  \"version\": 2,\n";
    file << "  \"map\": {\n";
    file << "    \"width\": " << mMap->GetWidth() << ",\n";
    file << "    \"height\": " << mMap->GetHeight() << ",\n";
    file << "    \"layers\": [\n";
    const auto& layers = mMap->GetLayers();
    for (size_t li = 0; li < layers.size(); ++li) {
        const auto& layer = layers[li];
        file << "      {\n";
        file << "        \"name\": \"" << EscapeJSON(layer.name) << "\",\n";
        file << "        \"elevation\": " << layer.elevation << ",\n";
        file << "        \"visible\": " << (layer.visible ? "true" : "false") << ",\n";
        file << "        \"tiles\": [";
        for (size_t ti = 0; ti < layer.tiles.size(); ++ti) {
            if (ti) file << ",";
            file << layer.tiles[ti];
        }
        file << "]\n";
        file << "      }";
        if (li + 1 < layers.size()) file << ",";
        file << "\n";
    }
    file << "    ]\n";
    file << "  },\n";

    // Lighting snapshot
    auto& lighting = Lighting::Get();
    auto& dir = lighting.GetDirectionalLight();
    auto& amb = lighting.GetAmbient();
    file << "  \"lighting\": {\n";
    file << "    \"dir\": " << Vec3ToJSON(dir.direction) << ",\n";
    file << "    \"dirColor\": " << Vec4ToJSON(dir.color) << ",\n";
    file << "    \"dirIntensity\": " << dir.intensity << ",\n";
    file << "    \"ambientColor\": " << Vec4ToJSON(amb.color) << ",\n";
    file << "    \"ambientIntensity\": " << amb.intensity << "\n";
    file << "  },\n";

    file << "  \"entities\": [\n";
    const auto& entities = mScene->GetEntities();
    for (size_t i = 0; i < entities.size(); ++i) {
        EntityID id = entities[i];
        file << "    {\n";
        file << "      \"id\": " << id << ",\n";
        file << "      \"name\": \"" << EscapeJSON(mScene->GetEntityName(id)) << "\"";

        if (auto* transform = mScene->GetComponent<TransformComponent>(id)) {
            file << ",\n      \"transform\": {\n";
            file << "        \"position\": " << Vec3ToJSON(transform->transform.position) << ",\n";
            file << "        \"rotation\": " << Vec3ToJSON(transform->transform.rotation) << ",\n";
            file << "        \"scale\": " << Vec3ToJSON(transform->transform.scale) << "\n";
            file << "      }";
        }
        if (auto* model = mScene->GetComponent<ModelRendererComponent>(id)) {
            std::string meshType = "cube";
            // heuristic: plane meshes typically have fewer verts - store explicit tag when possible
            if (model->model && model->model->GetMeshCount() > 0) {
                // keep cube default; plane created via CreatePlane has scale y small sometimes
            }
            file << ",\n      \"model\": {\"mesh\": \"" << meshType << "\"}";
        }
        if (auto* material = mScene->GetComponent<MaterialComponent>(id)) {
            file << ",\n      \"material\": {\"diffuse\": " << Vec4ToJSON(material->material.diffuse)
                 << ", \"emissive\": " << Vec4ToJSON(material->material.emissive)
                 << ", \"metallic\": " << material->material.metallic
                 << ", \"roughness\": " << material->material.roughness
                 << ", \"alpha\": " << material->material.alpha
                 << ", \"transparent\": " << (material->material.transparent ? "true" : "false")
                 << "}";
        }
        if (auto* light = mScene->GetComponent<LightComponent>(id)) {
            file << ",\n      \"light\": {\"color\": " << Vec4ToJSON(light->color)
                 << ", \"intensity\": " << light->intensity
                 << ", \"range\": " << light->range << "}";
        }
        if (auto* emitter = mScene->GetComponent<ParticleEmitterComponent>(id)) {
            file << ",\n      \"particleEmitter\": {\"autoEmit\": " << (emitter->autoEmit ? "true" : "false")
                 << ", \"emitCount\": " << emitter->emitCount
                 << ", \"emitRate\": " << emitter->emitRate
                 << ", \"direction\": " << Vec3ToJSON(emitter->emitDirection)
                 << ", \"spread\": " << emitter->emitSpread
                 << ", \"speed\": " << emitter->emitSpeed
                 << ", \"life\": " << emitter->emitLife
                 << ", \"color\": " << Vec4ToJSON(emitter->emitColor) << "}";
        }
        if (auto* cam = mScene->GetComponent<CameraComponent>(id)) {
            file << ",\n      \"camera\": {\"fov\": " << cam->fov
                 << ", \"near\": " << cam->nearPlane
                 << ", \"far\": " << cam->farPlane
                 << ", \"main\": " << (cam->isMain ? "true" : "false") << "}";
        }

        file << "\n    }";
        if (i + 1 < entities.size()) file << ",";
        file << "\n";
    }
    file << "  ]\n}\n";

    // Also save binary map for runtime map path
    if (mProject) {
        mMap->Save(mProject->GetMapPath(1));
    }
    RPG_LOG_INFO("Scene saved to: " + path);
}

bool Engine::LoadScene(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        RPG_LOG_ERROR("Failed to load scene: " + path);
        return false;
    }
    mScene->Clear();
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto findKey = [&](const std::string& text, const std::string& key, size_t from) -> size_t {
        return text.find(std::string("\"") + key + "\"", from);
    };
    auto parseVec3 = [&](const std::string& text, size_t from, Vec3& out) {
        size_t b = text.find('[', from);
        if (b == std::string::npos) return;
        size_t e = text.find(']', b);
        if (e == std::string::npos) return;
        std::stringstream ss(text.substr(b + 1, e - b - 1));
        char sep;
        ss >> out.x >> sep >> out.y >> sep >> out.z;
    };
    auto parseVec4 = [&](const std::string& text, size_t from, Vec4& out) {
        size_t b = text.find('[', from);
        if (b == std::string::npos) return;
        size_t e = text.find(']', b);
        if (e == std::string::npos) return;
        std::stringstream ss(text.substr(b + 1, e - b - 1));
        char sep;
        ss >> out.x >> sep >> out.y >> sep >> out.z >> sep >> out.w;
    };
    auto parseNumber = [&](const std::string& text, const std::string& key, size_t from, float defVal) -> float {
        size_t p = findKey(text, key, from);
        if (p == std::string::npos) return defVal;
        size_t c = text.find(':', p);
        if (c == std::string::npos) return defVal;
        try { return std::stof(text.substr(c + 1)); } catch (...) { return defVal; }
    };
    auto parseBool = [&](const std::string& text, const std::string& key, size_t from, bool defVal) -> bool {
        size_t p = findKey(text, key, from);
        if (p == std::string::npos) return defVal;
        size_t c = text.find(':', p);
        if (c == std::string::npos) return defVal;
        std::string v = text.substr(c + 1, 12);
        if (v.find("true") != std::string::npos) return true;
        if (v.find("false") != std::string::npos) return false;
        return defVal;
    };
    auto extractObject = [&](const std::string& text, size_t objStart) -> std::string {
        int depth = 0;
        for (size_t i = objStart; i < text.size(); ++i) {
            if (text[i] == '{') depth++;
            else if (text[i] == '}') {
                depth--;
                if (depth == 0) return text.substr(objStart, i - objStart + 1);
            }
        }
        return {};
    };

    // Map section
    size_t mapPos = findKey(content, "map", 0);
    if (mapPos != std::string::npos) {
        int width = static_cast<int>(parseNumber(content, "width", mapPos, 20));
        int height = static_cast<int>(parseNumber(content, "height", mapPos, 20));
        mMap->Resize(width, height);
        size_t layersPos = findKey(content, "layers", mapPos);
        if (layersPos != std::string::npos) {
            mMap->GetLayers().clear();
            size_t arr = content.find('[', layersPos);
            size_t search = arr + 1;
            while (search < content.size()) {
                size_t objStart = content.find('{', search);
                if (objStart == std::string::npos) break;
                // stop if we left the layers array roughly
                size_t entitiesKey = findKey(content, "entities", 0);
                if (entitiesKey != std::string::npos && objStart > entitiesKey) break;
                std::string obj = extractObject(content, objStart);
                if (obj.empty()) break;

                std::string name = "Layer";
                size_t np = findKey(obj, "name", 0);
                if (np != std::string::npos) {
                    size_t c = obj.find(':', np);
                    size_t q1 = obj.find('"', c);
                    size_t q2 = obj.find('"', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos)
                        name = obj.substr(q1 + 1, q2 - q1 - 1);
                }
                mMap->AddLayer(name);
                int layerIndex = static_cast<int>(mMap->GetLayers().size()) - 1;
                mMap->GetLayers().back().elevation = parseNumber(obj, "elevation", 0, 0.0f);
                mMap->GetLayers().back().visible = parseBool(obj, "visible", 0, true);

                size_t tilesPos = findKey(obj, "tiles", 0);
                if (tilesPos != std::string::npos) {
                    size_t b = obj.find('[', tilesPos);
                    size_t e = obj.find(']', b);
                    if (b != std::string::npos && e != std::string::npos) {
                        std::stringstream ss(obj.substr(b + 1, e - b - 1));
                        std::string item;
                        int idx = 0;
                        while (std::getline(ss, item, ',')) {
                            if (item.empty()) continue;
                            try {
                                int tile = std::stoi(item);
                                int x = idx % width;
                                int z = idx / width;
                                if (z < height) mMap->SetTile(layerIndex, x, z, tile);
                            } catch (...) {}
                            idx++;
                        }
                    }
                }
                search = objStart + obj.size();
            }
            if (mMap->GetLayers().empty()) mMap->AddLayer("Ground");
        }
    } else if (mProject) {
        mMap->Load(mProject->GetMapPath(1));
    }

    // Lighting
    size_t lightRoot = findKey(content, "lighting", 0);
    if (lightRoot != std::string::npos) {
        auto& dir = Lighting::Get().GetDirectionalLight();
        auto& amb = Lighting::Get().GetAmbient();
        parseVec3(content, findKey(content, "dir", lightRoot), dir.direction);
        parseVec4(content, findKey(content, "dirColor", lightRoot), dir.color);
        dir.intensity = parseNumber(content, "dirIntensity", lightRoot, dir.intensity);
        parseVec4(content, findKey(content, "ambientColor", lightRoot), amb.color);
        amb.intensity = parseNumber(content, "ambientIntensity", lightRoot, amb.intensity);
    }

    // Entities
    size_t entityStart = findKey(content, "entities", 0);
    if (entityStart == std::string::npos) {
        RPG_LOG_INFO("Scene loaded (map only) from: " + path);
        return true;
    }
    size_t arrayStart = content.find('[', entityStart);
    if (arrayStart == std::string::npos) return false;

    size_t pos = arrayStart + 1;
    while (pos < content.size()) {
        size_t objStart = content.find('{', pos);
        if (objStart == std::string::npos) break;
        std::string obj = extractObject(content, objStart);
        if (obj.empty()) break;

        std::string name = "Entity";
        size_t namePos = findKey(obj, "name", 0);
        if (namePos != std::string::npos) {
            size_t c = obj.find(':', namePos);
            size_t q1 = obj.find('"', c);
            size_t q2 = obj.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos)
                name = obj.substr(q1 + 1, q2 - q1 - 1);
        }

        EntityID id = mScene->CreateEntity(name);

        size_t transformPos = findKey(obj, "transform", 0);
        if (transformPos != std::string::npos) {
            auto* tr = mScene->AddComponent<TransformComponent>(id);
            parseVec3(obj, findKey(obj, "position", transformPos), tr->transform.position);
            parseVec3(obj, findKey(obj, "rotation", transformPos), tr->transform.rotation);
            parseVec3(obj, findKey(obj, "scale", transformPos), tr->transform.scale);
        }

        if (findKey(obj, "model", 0) != std::string::npos) {
            auto* model = mScene->AddComponent<ModelRendererComponent>(id);
            model->model = std::make_shared<Model>();
            std::string mesh = "cube";
            size_t meshPos = findKey(obj, "mesh", 0);
            if (meshPos != std::string::npos) {
                size_t c = obj.find(':', meshPos);
                size_t q1 = obj.find('"', c);
                size_t q2 = obj.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos)
                    mesh = obj.substr(q1 + 1, q2 - q1 - 1);
            }
            if (mesh == "plane") model->model->AddMesh(MeshFactory::CreatePlane(2.0f));
            else model->model->AddMesh(MeshFactory::CreateCube(1.0f));
        }

        size_t matPos = findKey(obj, "material", 0);
        if (matPos != std::string::npos) {
            auto* mat = mScene->AddComponent<MaterialComponent>(id);
            parseVec4(obj, findKey(obj, "diffuse", matPos), mat->material.diffuse);
            parseVec4(obj, findKey(obj, "emissive", matPos), mat->material.emissive);
            mat->material.metallic = parseNumber(obj, "metallic", matPos, 0.0f);
            mat->material.roughness = parseNumber(obj, "roughness", matPos, 0.5f);
            mat->material.alpha = parseNumber(obj, "alpha", matPos, 1.0f);
            mat->material.transparent = parseBool(obj, "transparent", matPos, false);
        }

        size_t lightPos = findKey(obj, "light", 0);
        if (lightPos != std::string::npos) {
            auto* light = mScene->AddComponent<LightComponent>(id);
            parseVec4(obj, findKey(obj, "color", lightPos), light->color);
            light->intensity = parseNumber(obj, "intensity", lightPos, 1.0f);
            light->range = parseNumber(obj, "range", lightPos, 10.0f);
        }

        size_t pePos = findKey(obj, "particleEmitter", 0);
        if (pePos != std::string::npos) {
            auto* pe = mScene->AddComponent<ParticleEmitterComponent>(id);
            pe->emitter = std::make_unique<ParticleEmitter>();
            pe->autoEmit = parseBool(obj, "autoEmit", pePos, false);
            pe->emitCount = static_cast<int>(parseNumber(obj, "emitCount", pePos, 5));
            pe->emitRate = parseNumber(obj, "emitRate", pePos, 0.1f);
            parseVec3(obj, findKey(obj, "direction", pePos), pe->emitDirection);
            pe->emitSpread = parseNumber(obj, "spread", pePos, 0.5f);
            pe->emitSpeed = parseNumber(obj, "speed", pePos, 2.0f);
            pe->emitLife = parseNumber(obj, "life", pePos, 1.0f);
            parseVec4(obj, findKey(obj, "color", pePos), pe->emitColor);
        }

        size_t camPos = findKey(obj, "camera", 0);
        if (camPos != std::string::npos) {
            auto* cam = mScene->AddComponent<CameraComponent>(id);
            cam->fov = parseNumber(obj, "fov", camPos, 60.0f);
            cam->nearPlane = parseNumber(obj, "near", camPos, 0.1f);
            cam->farPlane = parseNumber(obj, "far", camPos, 1000.0f);
            cam->isMain = parseBool(obj, "main", camPos, true);
            if (cam->isMain) mActiveCameraEntity = id;
        }

        pos = objStart + obj.size();
    }

    RPG_LOG_INFO("Scene loaded from: " + path);
    return true;
}

} // namespace rpg
