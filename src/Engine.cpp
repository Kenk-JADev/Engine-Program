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
#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Texture.h"
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
#include "rpgmaker3d/Rui.h" // PAKET 31: eigenes UI-Framework (Window-Schicht)
#include "rpgmaker3d/Custom.h" // Game.ini / "alles custom"-Schalter
#include "rpgmaker3d/RgssUI.h" // RGSS-Fenstersystem (reine Ruby-UI)

#include <SDL.h>

#include <glad/gl.h>
// PAKET 10 Fix: GameUI-ImGui-Overlay bekommt einen echten Frame-Lebenszyklus
// (Kontext + OpenGL3-Backend). Nur das GL-Backend — kein SDL-Backend
// (Eingaben laufen nativ; der Qt-Host hat keine SDL-Event-Schleife).
#ifdef RPGMAKER3D_ENABLE_IMGUI
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#endif
#include <iostream>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <memory>

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

    // PAKET 27: Zufallszahlen-Seed! Bisher wurde nirgends std::srand
    // aufgerufen - rand() lieferte bei JEDEM Start exakt dieselbe Folge
    // (Zufallskaempfe, Trupp-Wahl, NPC-Routen, Kampf-Ziele reproduzierbar).
    std::srand(static_cast<unsigned int>(
        std::chrono::system_clock::now().time_since_epoch().count()));

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

    // PAKET 10: RmlUi ist entfernt — die gesamte Spielanzeige laeuft im
    // GameUI-ImGui-Overlay (Menues/HUD/Messages). HUD-Startwert: Im Editor
    // (Qt) ist das In-Game-HUD standardmaessig AUS (der Editor zeigt
    // FPS/Karte/Status in der eigenen Statuszeile); es wird beim
    // Playtest-Start sichtbar (F9 toggelt jederzeit). Im Player: an.
    GameUI::Get().SetHudVisible(!mEditorMode);

#ifdef RPGMAKER3D_ENABLE_IMGUI
    InitImGui(); // PAKET 10 Fix: Lebenszyklus fehlte komplett (s.u.)
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
        // PAKET 25: fehlende Kartendatei -> Standardkarte (statt alte
        // Karte stehen zu lassen, waehrend ID + Events schon wechseln)
        LoadRuntimeMap(mapId);
        // Setup (ID + Karten-BGM/-BGS) nur wenn die ID wirklich neu ist;
        // Game::Load hat sie bereits gesetzt (vermeidet doppelten BGM-Start).
        if (Game::Get().Map().GetMapId() != mapId)
            Game::Get().Map().Setup(mapId);
        if (mProject)
            EventSystem::Get().LoadMapEvents(mapId, mProject->GetProjectPath());
        RPG_LOG_INFO("Map-Wechsel auf Karte " + std::to_string(mapId));
    });

    // PAKET 14: Transfer-Befehl (201) mit XP-Crossfade — die Engine bekommt
    // den kompletten Wechsel als verzoegerten Swap (Freeze am jetzigen Tick,
    // Positions-/Kartenwechsel erst wenn der Snapshot steht, dann Fade).
    // Wie XP Scene_Map#transfer_player: freeze + transfer + transition(10).
    EventSystem_SetTransferTransitionHandler(
        [this](int x, int y, int z, int mapId) {
            RequestTransition([x, y, z, mapId]() {
                Game::Get().Player().SetPosition(
                    Vec3((float)x, (float)y + 0.05f, (float)z));
                if (mapId > 0) EventSystem_NotifyMapChanged(mapId);
            });
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

    // Game Over (XP): Niederlage ohne "Niederlage moeglich" -> Anzeige +
    // nach Bestaetigung zurueck zum Titel (Editor: Playtest-Stopp).
    // Wird EINMAL zentral injiziert (Callback-Ueberschreibungen der
    // Event-/Encounter-Verdrahtung betreffen onGameOver nicht).
    // XP-Kampfereignis-Seiten (Trupps-Tab): das Gemeinsame Ereignis einer
    // feuernden Seite als blockierenden Interpreter starten; der Kampf
    // pausiert ueber diese beiden Hooks bis die Befehlsliste fertig ist.
    BattleSystem::Get().onRunTroopPage = [](int commonEventId, int runtimeEventId) {
        EventSystem::Get().StartCommonEventById(commonEventId, runtimeEventId, true);
    };
    BattleSystem::Get().onIsTroopPageRunning = [](int runtimeEventId) {
        return EventSystem::Get().IsEventRunning(runtimeEventId);
    };
    BattleSystem::Get().onGameOver = [this]() {
        mGameOverPending = true;
        // XP: Game-Over-Grafik (Graphics/Gameovers/) wenn vorhanden,
        // darueber/hinter dem Text-Fallback
        GameUI::Get().ShowPicture(Database::Get().System().gameoverGraphicName,
                                  std::string("$gameover"), Vec2(0.5f, 0.5f),
                                  1.0f, 1.0f, 0.0f);
        GameUI::Get().AddScreenText("GAME OVER", Vec2(0.5f, 0.42f),
                                    Color(1.0f, 0.25f, 0.25f, 1.0f), 5.0f, true, 2.2f);
        GameUI::Get().ShowMessage("GAME OVER");
        if (mAudio) PlayEventAudio(Database::Get().System().gameoverMe, 2, false);
    };

    // PAKET 9: XP-Kampf-Feedback — fliegende Schadens-/Heilungszahlen ueber
    // dem Ziel + Treffer-Flash auf der Gegner-Grafik (XP: Battler blinkt
    // weiss beim Treffer). Quelle: zentraler Hook aus
    // Battler::ApplyDamage/Recover/NotifyMiss (deckt auch Kampf-Ereignis-
    // Befehle ab, nicht nur Angriff/Skill/Item).
    BattleSystem::Get().onBattlerHit = [this](const Battler& b, BattleHitKind kind, int amount) {
        SpawnBattleFeedbackPopup(b, kind, amount);
        // Treffer-Flash nur bei Gegnern (Akteure haben kein Bild im Feld)
        if (!b.isActor) {
            const std::string pic = "$battler" + std::to_string(b.index);
            switch (kind) {
                case BattleHitKind::Crit:
                    GameUI::Get().FlashPicture(pic, Color(1.0f, 0.55f, 0.2f, 1.0f), 0.30f);
                    break;
                case BattleHitKind::Damage:
                case BattleHitKind::Miss:
                    GameUI::Get().FlashPicture(pic, Color(1.0f, 1.0f, 1.0f, 0.9f), 0.22f);
                    break;
                case BattleHitKind::Heal:
                    GameUI::Get().FlashPicture(pic, Color(0.45f, 1.0f, 0.55f, 0.9f), 0.30f);
                    break;
            }
        }
    };

    // PAKET 12: XP-Kampf-Animationen — Waffen-/Skill-/Item-Animation am
    // Ziel-Battler (BattleSystem::onBattleAnimation). Zielpunkt wie die
    // Battler-Bilder/Popup-Formel (normierte Bildschirmposition -> RGSS-
    // Canvas 640x480, top-origin — dieselbe Konvention wie GameUI-Pictures
    // und RGSS-Canvas). Die Sequenz laeuft ueber das RGSS-Spritesystem
    // (z=9999) und deckt damit Battler-Bilder UND 3D-Karte einheitlich ab.
    BattleSystem::Get().onBattleAnimation = [this](const Battler& b, int animId) {
        if (animId <= 0) return;
        auto& bs = BattleSystem::Get();
        float x;
        if (b.isActor) {
            const int n = (int)bs.Actors().size();
            x = n > 1 ? (0.25f + 0.5f * (float)b.index / (float)(n - 1)) : 0.5f;
        } else {
            const int n = (int)bs.Enemies().size();
            x = n > 1 ? (0.25f + 0.5f * (float)b.index / (float)(n - 1)) : 0.5f;
        }
        // Gegner: Mitte des Battler-Bildes (y=0.30). Akteure: Mitte der
        // XP-Statuszeile unten (~20% hoch -> Zentrum bei y=0.90).
        const float yNorm = b.isActor ? 0.90f : 0.30f;
        Game::Get().StartAnimationAtCanvas(animId,
                                           (int)std::round(x * 640.0f),
                                           (int)std::round(yNorm * 480.0f));
    };

    // PAKET 15: XP-Siegseite — EINMAL zentral (die kampfstart-seitigen
    // Setup-Pfade ueberschreiben onVictory/onMessage regelmaessig; diese
    // beiden Hooks bleiben davon unberuehrt):
    //  a) Sieg-ME aus System.battleEndMe (XP Scene_Battle battle_end)
    //  b) Victory wartet auf die Quittierung der Ergebnis-Nachricht
    BattleSystem::Get().onVictoryMe = [this](const std::string& meName) {
        if (!meName.empty()) PlayEventAudio(meName, 2, false); // 2 = ME
    };
    BattleSystem::Get().isMessageBusy = []() {
        return GameUI::Get().Message().IsBusy();
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

// ImGui-Editor entfernt; UI-Host ist Qt (RPGMAKER3D_EDITOR_QT). Die
// Spielanzeige laeuft im GameUI-ImGui-Overlay (PAKET 10, kein RmlUi mehr).
    RPG_LOG_INFO("UI host: Qt editor / GameUI overlay in game");

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
    // XP-Tileset-Flags aus der Datenbank ans Runtime-Tileset koppeln
    // (Durchgaengigkeit, 4-Richtung, Prioritaet, Busch, Tresen, Terrain-Tag).
    // Standard-Map benutzt Tileset 1, Cache-Fallback: erster Eintrag.
    try {
        const auto& sets = Database::Get().Tilesets();
        const rpg::TilesetData* chosen = nullptr;
        for (const auto& ts : sets) { if (ts.id == 1) { chosen = &ts; break; } }
        if (!chosen && !sets.empty()) chosen = &sets.front();
        if (chosen) tileset->SetTilesetData(*chosen);
    } catch (const std::exception& e) {
        RPG_LOG_ERROR(std::string("Tileset-Flags anwenden fehlgeschlagen: ") + e.what());
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
        RPG_LOG_INFO("Editor mode without Qt host – F5 Playtest. "
                     "Fuer vollen Qt-Editor: Qt6 + -DRPGMAKER3D_EDITOR_QT=ON -DCMAKE_PREFIX_PATH=<Qt>");
#endif
    }

    // PAKET 26: Keine Legacy-Demo-Entitaeten mehr (blauer "Demo Cube" +
    // 20x20-"Floor"-Plane mit Checker-Standardtextur). Sie verdeckten die
    // echte Karte in der Qt-Editor-Ansicht (Z-Fighting auf y=0), liessen
    // sich per Gizmo versehentlich verschieben und wirkten wie ein
    // durcheinander gewuerfeltes Schachbrett. Der Boden kommt jetzt
    // ausschliesslich aus der Karte (LoadRuntimeMap liefert zur Not die
    // PAKET-25-Standardkarte) - das Verhalten von Player und Editor ist
    // damit identisch und vorhersagbar.

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

    // Playtest-/Spielstopp: Executed-Merker zuruecksetzen, damit der
    // naechste Start die Skripte wieder frisch ausfuehrt (Neustart-Verhalten).
    if (!playing && mScriptManager) mScriptManager->InvalidateExecutedScripts();
    // Spielstopp: alle Ruby-Fenster (RGSS-UI) entfernen.
    if (!playing) RgssUI::Get().ClearAll();
    // PAKET 37: Spielstopp raeumt auch die RUI-Fenster (Menue/Message/HUD
    // aus dem Playtest) — sie wuerden sonst im Editor weitergezeichnet.
    if (!playing) rui::Manager::Get().Clear();

    // "Alles custom": Game.ini + Projekt-Skins bei jedem Spielstart neu ziehen
    if (playing) LoadCustomConfigForProject();

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
            GameUI::Get().SetHudVisible(true); // HUD im Playtest zeigen (F9 toggelt)
            GameUI::Get().ShowMessage(std::string("PLAYTEST\nWASD bewegen | E/Enter sprechen | Esc Pause\nGehe zum Dorfältesten (NPC) und drücke E."));

            if (mScriptManager) mScriptManager->ExecuteAllScriptsOnce();
            RPG_LOG_INFO("Playtest spawn at " + std::to_string(spawn.x) + "," +
                         std::to_string(spawn.z) + " | gold=" +
                         std::to_string(Game::Get().Party().GetGold()));
        } else {
            RPG_LOG_INFO("=== PLAYTEST STOP ===");
            GameUI::Get().Message().Hide();
            GameUI::Get().Pause().Hide();
            GameUI::Get().SetHudVisible(!mEditorMode); // Editor: HUD wieder aus
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
            if (mScriptManager) mScriptManager->ExecuteAllScriptsOnce();
        }
    }
}

// ---------------------------------------------------------------------------
// XP-Titelbildschirm (Player): Neues Spiel / Weiterspielen / Beenden
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// "Alles custom": Game.ini einlesen + Skin/HUD-Startwert anwenden.
// Laeuft bei jedem Spiel-/Titelstart erneut (billig, kleine Datei) - damit
// greifen Aenderungen ohne Engine-Neustart (Playtest aus dem Editor).
// ---------------------------------------------------------------------------
void Engine::LoadCustomConfigForProject() {
    const std::string base = mProject ? mProject->GetProjectPath() : std::string();
    CustomConfig::Get().LoadFromProject(base);
    RgssUI::Get().SetProjectBase(base); // Windowskin-Aufloesung der Ruby-UI
    // HUD-Startwert aus der Projekt-Config (F9 kann es jederzeit umlegen)
    GameUI::Get().SetHudVisible(CustomConfig::Get().nativeHud);
}

void Engine::StartTitleMode() {
    LoadCustomConfigForProject();

    // Custom-Titel: das eingebaute Titelbild ist abgeschaltet -> erst den
    // Ruby-Hook Game.custom_title probieren; ohne Hook direkt ins Spiel
    // (XP-Demo-Start ohne Titel). Die Skripte laufen dabei SCHON JETZT
    // (einmalig), damit der Hook ueberhaupt definiert ist - SetPlaying(true)
    // fuehrt sie dank ExecuteAllScriptsOnce nicht doppelt aus.
    if (!CustomConfig::Get().nativeTitle) {
        if (mScriptManager) mScriptManager->ExecuteAllScriptsOnce();
        bool hooked = false;
        if (mRubyVM) hooked = mRubyVM->CallGameHook("custom_title");
        if (!hooked) {
            RPG_LOG_INFO("[Custom] NativeTitle=0, kein Game.custom_title -> direkter Spielstart");
            Game::Get().NewGame();
            SetPlaying(true);
        }
        return;
    }

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
        // PAKET 14: XP-Crossfade Titel → Karte (Wechsel erst nach Snapshot)
        RequestTransition([this]() {
            EndTitleMode();
            Game::Get().NewGame();
            SetPlaying(true); // Player-Zweig: laedt Events + fuehrt Skripte aus
        });
    };
    title.onContinue = [this]() {
        // Lade-Ansicht des Speicherbildschirms; danach: entweder Spiel
        // weiterfuehren (Slot geladen) oder Abbruch -> zurueck zum Titel.
        GameUI::Get().ShowSaveScreen(false, [this]() {
            if (Game::Get().IsGameStarted()) {
                // PAKET 14: XP-Crossfade Ladebildschirm → Karte
                RequestTransition([this]() {
                    EndTitleMode();
                    // Karte/Events/BGM kamen bereits per Game::Load-
                    // Map-Wechsel-Hook; SetPlaying startet Logik + Skripte.
                    SetPlaying(true);
                });
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
    GameUI::Get().ClearScreenTexts(); // u. a. GAME-OVER-/Kampfstatus-Texte
    GameUI::Get().ClearPictures();    // u. a. $gameover-Grafik
    EventSystem::Get().Clear();
    if (BattleSystem::Get().IsInBattle()) BattleSystem::Get().Abort();
    Game::Get().SetGameStarted(false);
    Game::Get().Player().SetLocked(false);
    SetPlaying(false);
    StartTitleMode();
}

// ---------------------------------------------------------------------------
// PAKET 14: XP-Uebergaenge (Graphics.freeze → Swap → Graphics.transition)
// ---------------------------------------------------------------------------
// XP-Vorlage (Scene_Map#transfer_player / Scene_Base): Graphics.freeze
// haelt das Bild an, der Szenenwechsel passiert darunter, dann fadet
// Graphics.transition(10) weich ueber. Bei uns erstellt der Freeze-Snapshot
// host-sicher am ENDE des naechsten Render (kein Readback nach Swap) — der
// Arbiter wartet deshalb genau einen Tick, bevor er wechselt + fadet.
void Engine::RequestTransition(std::function<void()> swapNow, int durFrames) {
    const bool uiVisible = mPlayMode || !mEditorMode;
    if (!uiVisible || !mWindow) {       // kein Overlay-Kanal sichtbar
        if (swapNow) swapNow();         // (Editor-Scene-View o. Playtest)
        return;
    }
    // bereits anstehende Anfrage sofort abschliessen — kein Verlust,
    // kein Ueberschreiben (z. B. zwei Transfer-Befehle hintereinander)
    if (mTransitionReq.active) {
        auto prev = std::move(mTransitionReq.swap);
        mTransitionReq = TransitionRequest{};
        if (prev) prev();
    }
    RgssGraphicsFreeze();
    mTransitionReq.swap = std::move(swapNow);
    mTransitionReq.durationFrames = durFrames;
    mTransitionReq.framesWaited = 0;
    mTransitionReq.active = true;
}

void Engine::UpdateTransitionRequest() {
    if (!mTransitionReq.active) return;
    // Snapshot kommt am Ende des naechsten Render-Laufs — bis dahin heisst
    // es warten (ein Frame alte Ansicht, wie XP-Freeze).
    if (!RgssGraphicsHasSnapshot()) {
        // Sicherheitsnetz: stockt der Render-Takt des Hosts (z. B. Qt ohne
        // Repaint), darf der Szenenwechsel nicht haengen — nach ~0,5 s
        // ohne Fade sofort wechseln statt ewig zu warten.
        if (++mTransitionReq.framesWaited > 30) {
            auto fnLate = std::move(mTransitionReq.swap);
            mTransitionReq = TransitionRequest{};
            if (fnLate) fnLate();
            RPG_LOG_WARN("[PAKET 14] Uebergangs-Snapshot wartete zu lange - Wechsel ohne Fade");
        }
        return;
    }
    auto fn = std::move(mTransitionReq.swap);
    const int dur = mTransitionReq.durationFrames;
    mTransitionReq = TransitionRequest{};
    if (fn) fn();
    // Crossfade ohne Maskengrafik (XP Graphics.transition(10) ≈ 15 @60fps)
    RgssGraphicsTransition(std::max(1, dur), "", 40.0f);
}

// ---------------------------------------------------------------------------
// Bild-Pfadaufloesung (Graphics/Pictures|Titles, XP-Struktur)
// ---------------------------------------------------------------------------
std::string Engine::ResolvePicturePathFor(const std::string& filename) const {
    if (filename.empty()) return {};
    const std::string base = mProject ? mProject->GetProjectPath() : std::string();
    static const char* kDirs[] = {
        "Graphics/Pictures/", "Graphics/Titles/", "Graphics/Gameovers/",
        "Graphics/Battlers/", // XP-Gegnergrafiken (Kampf)
        "Graphics/Faces/",    // XP-Gesichter (Kampf-Statusfenster, PAKET 9)
        "Graphics/System/",   // XP-Systemgrafiken (Windowskin, PAKET 33)
        "Pictures/", "pictures/",
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

// ---------------------------------------------------------------------------
// PAKET 10 Fix: ImGui-Frame-Lebenszyklus (Kontext + OpenGL3-Backend)
// ---------------------------------------------------------------------------
// Hintergrund: Seit PAKET 10 zeichnet die KOMPLETTE Spielanzeige (Titel,
// Messages, Menues, HUD, Pictures, Kampfstatus, Bildschirmeffekte) ueber
// ImGui — aber es gab nirgendwo ImGui::CreateContext/NewFrame/Render. Mit
// aktivem RPGMAKER3D_ENABLE_IMGUI waere der erste Draw ein NULL-Kontext-
// Zugriff gewesen (Absturz). Hier laeuft alles host-unabhaengig:
// - SDL-Player (Game.exe): Kontext nach Window/GL-Init, Fenster = SDL.
// - Qt-Editor (GameView): Kontext in initializeGL (Qt-Kontext ist current),
//   Fenstergroesse kommt ueber Window::SetForeignSize.
// Loader: das Backend nutzt seinen eingebetteten gl3w-Loader
// (IMGUI_IMPL_OPENGL_LOADER_IMGL3W) — kein Eingriff in glad noetig.
#ifdef RPGMAKER3D_ENABLE_IMGUI

void Engine::InitImGui() {
    if (mImGuiReady) return;
    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext()) {
        RPG_LOG_ERROR("ImGui::CreateContext fehlgeschlagen - GameUI-Overlay aus");
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // keine imgui.ini-ins-Projekt schreiben
    io.LogFilename = nullptr;   // kein imgui_log.txt
    ImGui::StyleColorsDark();
    // GLSL 130 = GL 3.0+ (Engine-Kontext ist 3.3, Abwaertskompatibilitaet ok)
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        RPG_LOG_ERROR("ImGui GL3-Backend-Init fehlgeschlagen - GameUI-Overlay aus");
        ImGui::DestroyContext();
        return;
    }
    mImGuiReady = true;
    mImGuiLastTime = std::chrono::steady_clock::now();
    RPG_LOG_INFO("ImGui-Overlay bereit (GameUI: Titel/Messages/Menues/HUD)");
}

void Engine::ImGuiBeginFrame() {
    if (!mImGuiReady || mImGuiFrameOpen) return;
    ImGuiIO& io = ImGui::GetIO();
    int w = mWindow ? mWindow->GetWidth() : 0;
    int h = mWindow ? mWindow->GetHeight() : 0;
    // DisplaySize ist LESE-relevant fuer das gesamte GameUI-Layout
    // (Messages/Menues positionieren relativ zur Fenstergroesse).
    io.DisplaySize = ImVec2((float)std::max(1, w), (float)std::max(1, h));
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - mImGuiLastTime).count();
    mImGuiLastTime = now;
    if (dt <= 0.0f || dt > 0.5f) dt = 1.0f / 60.0f; // Pausen/Stopps abfangen
    io.DeltaTime = dt;
    // Keine Maus/Tastatur-Fuetterung: GameUI liest Eingaben nativ
    // (Input::IsKeyPressed / UpdateModalInput), Mausposition bleibt "keine".
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    mImGuiFrameOpen = true;
}

void Engine::ImGuiEndFrame() {
    if (!mImGuiReady || !mImGuiFrameOpen) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    mImGuiFrameOpen = false;
}

void Engine::ShutdownImGui() {
    if (!mImGuiReady) return;
    if (mImGuiFrameOpen) {       // defensiv: offenen Frame sauber schliessen
        ImGui::EndFrame();
        mImGuiFrameOpen = false;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    mImGuiReady = false;
}

#endif // RPGMAKER3D_ENABLE_IMGUI

void Engine::Shutdown() {
    RPG_LOG_INFO("Engine shutdown started");
    // PAKET 9: Kampf-Feedback-Hook loesen (haelt this)
    BattleSystem::Get().onBattlerHit = nullptr;
    BattleSystem::Get().onBattleAnimation = nullptr; // PAKET 12 (haelt this)
    BattleSystem::Get().onVictoryMe = nullptr;       // PAKET 15 (haelt this)
    BattleSystem::Get().isMessageBusy = nullptr;     // PAKET 15
#ifdef RPGMAKER3D_ENABLE_IMGUI
    ShutdownImGui(); // vor Window/GL-Teardown (Backend loescht GL-Ressourcen)
#endif
    mInitialized = false;
// ImGui/Editor shutdown removed
    mGridMesh.Delete();
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

// ---------------------------------------------------------------------------
// PAKET 9: XP-Kampf-Feedback — fliegende Schadens-/Heilungszahlen (+Crit/Miss)
// ---------------------------------------------------------------------------
// Quelle: BattleSystem::onBattlerHit (Angriff, Fertigkeit, Item UND
// Kampf-Ereignis-Befehle). Die Zahl schwebt per MoveScreenText-Tween
// (easeOutQuad) nach oben und fadet ueber ihre Lebensdauer aus. Positionen:
// Gegner an derselben Verteilungsformel wie ihre Battler-Bilder (Bild
// y=0.30 -> Popup knapp darueber), Akteure an der Party-Statuszeile
// (y=0.10 -> Popup darunter).
void Engine::SpawnBattleFeedbackPopup(const Battler& b, BattleHitKind kind, int amount) {
    const bool heal = kind == BattleHitKind::Heal;
    if (amount == 0 && kind != BattleHitKind::Miss) return;
    const int v = heal ? -amount : amount;
    auto& bs = BattleSystem::Get();

    float x, y;
    if (b.isActor) {
        const int n = (int)bs.Actors().size();
        x = n > 1 ? (0.25f + 0.5f * (float)b.index / (float)(n - 1)) : 0.5f;
        y = 0.155f;
    } else {
        const int n = (int)bs.Enemies().size();
        x = n > 1 ? (0.25f + 0.5f * (float)b.index / (float)(n - 1)) : 0.5f;
        y = 0.235f;
    }

    std::string txt;
    Color col;
    float scale = 1.5f;
    switch (kind) {
        case BattleHitKind::Miss:
            txt = "Ausgewichen!";
            col = b.isActor ? Color(0.95f, 0.95f, 1.0f, 1.0f)
                            : Color(0.85f, 0.85f, 0.95f, 1.0f);
            scale = 1.25f;
            break;
        case BattleHitKind::Crit:
            txt = "KRITISCH! -" + std::to_string(v);
            col = Color(1.0f, 0.55f, 0.15f, 1.0f);
            scale = 1.85f;
            break;
        case BattleHitKind::Heal:
            txt = "+" + std::to_string(v);
            col = Color(0.45f, 1.0f, 0.55f, 1.0f);
            break;
        case BattleHitKind::Damage:
        default:
            txt = "-" + std::to_string(v);
            col = b.isActor ? Color(1.0f, 0.35f, 0.30f, 1.0f)
                            : Color(1.0f, 1.0f, 0.88f, 1.0f);
            break;
    }
    const int id = GameUI::Get().AddScreenText(txt, Vec2(x, y), col, 0.95f, true, scale);
    GameUI::Get().MoveScreenText(id, Vec2(x, y - 0.055f), 0.9f, 2 /* easeOutQuad */);
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

    // PAKET 14: anstehende XP-Uebergaenge vollfuehren, sobald der
    // Freeze-Snapshot steht (Swap + Crossfade, siehe RequestTransition).
    UpdateTransitionRequest();

    // XP-Verhalten: Alt+Enter schaltet Vollbild um. Window::ToggleFullscreen
    // ist im Foreign-Modus (Qt-Host) ein No-op, bei SDL fehlschlagsfest.
    if (mWindow &&
        mInput->IsKeyDown(Key::LAlt) && mInput->IsKeyPressed(Key::Enter)) {
        mWindow->ToggleFullscreen();
    }

#ifdef RPGMAKER3D_EDITOR_QT
    // Im Qt-Editor gibt es kein SDL-Fenster: Input kommt vom Qt-Widget
    // (ruft OnKeyChanged/OnMouseMoved direkt).
#else
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
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

    // XP-Debug-Inspektor (Paket 4): F10 oeffnet das Schalter/Variablen-Fenster
    if (mPlayMode && !mDbgVisible && mInput->IsKeyPressed(Key::F10)) {
        ToggleDebugWindow();
    }
    // Fenster live halten / bei Spielstopp automatisch schliessen
    UpdateDebugWindow(dt);

    // Kamera Navigation (Editor oder Play)
    // Qt-Editor: Input kommt nur vom Game-View-Widget (StrongFocus) -> immer erlaubt.
    bool allowCamera = true;

    // Im PlayMode: Kamera folgt optional dem GamePlayer
    // (kann im Editor umgeschaltet werden) - NUR wenn Follow Player AKTIV
    if (mPlayMode && mPlayModeFollowPlayer) {
        Camera& cam = mRenderer->GetCamera();
        Vec3 playerPos = Game::Get().Player().GetPosition();
        // Follow-Cam: leicht versetzt hinter/über dem Spieler
        const Vec3 targetPos = playerPos + Vec3(0, 3.0f, 5.0f);
        // PAKET 29: weiche Nachfuehrung (exp. Daempfung, framerate-fest).
        // Grosser Sprung (Map-Transfer/Respawn) -> sofort snappen.
        {
            const Vec3 gap = targetPos - mFollowCamPos;
            if (glm::length(gap) > 6.0f)
                mFollowCamPos = targetPos;
            else {
                const float k = 1.0f - std::exp(-8.0f * dt); // ~0.12s Zeitkonstante
                mFollowCamPos += gap * k;
            }
        }
        Vec3 camPos = mFollowCamPos;
        // PAKET 11: Bildschirm-Erschuetterung (Befehl 225) als Kamera-Jitter —
        // Amplitude klingt mit dem Shake-Timer ab, Achsen x/y (Bildebene).
        // (Nach der Daempfung addiert: Shake soll NICHT geglaettet werden.)
        {
            const auto& fx = GetScreenEffects();
            if (fx.shakeTimer > 0.0f && fx.shakeDuration > 0.0f) {
                const float k = fx.shakeTimer / fx.shakeDuration; // 1 -> 0
                const float amp = (float)fx.shakePower * 0.10f * k;
                camPos.x += (((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f) * amp;
                camPos.y += (((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f) * amp;
            }
        }
        cam.SetPosition(camPos);
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

    // PAKET 31/32: RUI-Framework (eigene Window-Schicht) — einmal pro Frame
    // takten: Animation + NATIVES Maus-Routing + Fokus-Navigation fuer
    // Script-Listen. Eingabe kommt ausschliesslich aus rpg::Input (Tasten
    // UND Maus) - ImGui erhaelt weiterhin nichts.
    if (mInput) {
        const Vec2 mpos = mInput->GetMousePosition();
        const int navV = mInput->IsKeyPressed(Key::Up) ? -1 :
                         (mInput->IsKeyPressed(Key::Down) ? 1 : 0);
        const bool navOk = mInput->IsKeyPressed(Key::Enter) ||
                           mInput->IsKeyPressed(Key::E);
        const bool navCancel = mInput->IsKeyPressed(Key::Escape);
        rui::Manager::Get().Update(dt, mpos.x, mpos.y,
                                   mInput->IsMousePressed(MouseButton::Left),
                                   navV, navOk, navCancel);
    }

    // Game Logic - läuft im PlayMode (Editor Play-Test UND Player)
    if (mPlayMode) {
        // Modale Eingaben (Choices, Zahleneingabe 103, Namenseingabe 303)
        // haben Vorrang vor allem anderen und konsumieren die Tasten zuerst.
        GameUI::Get().UpdateModalInput(*mInput);

        // XP-Debug-Inspektor (F10) blockt die Spieleingabe, solange er offen ist.
        // PAKET 32: Auch ein fokussiertes Script-Menue (RUI SetFocusList)
        // zaehlt als modal - Interagieren/Menueaufruf werden gesperrt.
        const bool modalActive = mDbgVisible ||
                                 GameUI::Get().IsNumberInputActive() ||
                                 GameUI::Get().IsNameInputActive() ||
                                 GameUI::Get().Message().HasChoices() ||
                                 GameUI::Get().Menu().IsVisible() ||
                                 rui::Manager::Get().HasFocus();
        // XP: Esc oeffnet das Spielmenue; Schliessen laeuft ueber
        // MenuWindow::Cancel in UpdateModalInput (Teil von modalActive).
        // Im Kampf ist der Menueaufruf gesperrt (XP-Verhalten).
        // NativeGameMenu=0 (Game.ini/UI.native_game_menu=): abgeschaltet -
        // das Spiel baut sein eigenes Menue (z. B. via Input.key_pressed?).
        if (!modalActive && mInput->IsKeyPressed(Key::Escape) &&
            !GameUI::Get().Message().IsBusy() &&
            !BattleSystem::Get().IsInBattle() &&
            CustomConfig::Get().nativeGameMenu) {
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

        // XP-Animations-SE (Paket 5): einmalig mit der Audio-Pipeline verdrahten,
        // damit Animations-Frames ihre Soundeffekte abspielen koennen.
        if (mAudio && !Game::Get().playSeHook) {
            Game::Get().playSeHook = [this](const std::string& seName, int vol, int pitch) {
                std::string p = ResolveAudioPath(seName, 3);
                if (p.empty() && seName.find('.') == std::string::npos)
                    p = ResolveAudioPath(seName + ".wav", 3);
                if (!p.empty())
                    mAudio->PlaySE(p, false, vol / 100.0f, pitch / 100.0f);
            };
        }

        // XP-Animations-Zielprojektion (Paket 6): Weltposition -> RGSS-
        // Canvas (640x480). Der RGSS-Renderer streckt den Canvas direkt auf
        // den Framebuffer, deshalb gilt: canvasUV == NDC-UV aus der
        // Laufzeitkamera. View/Proj kommen von derselben Kamera, mit der
        // gerendert wird (mRenderer->GetCamera(), Aspect wird in Render()
        // jedes Frame gesetzt). false bei Ziel hinter der Kamera.
        // PAKET 33: Windowskin automatisch aus dem Projekt übernehmen
        // (Graphics/System/windowskin.*), aber NUR solange kein Script ein
        // eigenes Skin gesetzt hat (Rui.windowskin = bleibt Chef).
        if (mProject && rui::Manager::Get().GetSkinSource().empty()) {
            const std::string skinP = ResolvePicturePathFor("windowskin");
            if (!skinP.empty()) rui::Manager::Get().SetSkinSource(skinP);
        }

        if (!Game::Get().worldToScreenHook) {
            Game::Get().worldToScreenHook = [this](const Vec3& wp, float& outX, float& outY) {
                if (!mWindow || !mRenderer) return false;
                const Camera& cam = mRenderer->GetCamera();
                const Vec4 clip = cam.GetProjectionMatrix() * cam.GetViewMatrix() *
                                  Vec4(wp, 1.0f);
                if (clip.w <= 0.0001f) return false;
                const float ndcX = clip.x / clip.w;
                const float ndcY = clip.y / clip.w;
                outX = (ndcX * 0.5f + 0.5f) * 640.0f;
                outY = (0.5f - ndcY * 0.5f) * 480.0f;
                return true;
            };
        }

        // Prioritaet (Paket 6): Paket-1-Tables als RGSS-Fallback verdrahten,
        // damit Tilemap-Drawables ohne eigene priorities-Table die
        // TilesetData-Prioritaeten der aktiven Karte nutzen (XP liest sie aus
        // $data_tilesets). RGSS-Id-Raum: 0..383 = Autotile-Slots (ohne
        // Paket-1-Prio), 384+ -> visueller Index (id-384).
        RgssSetTilePriorityHooks(
            [](int rgssId) -> int {
                const Map* bound = Game::Get().Map().GetBoundMap();
                auto ts = bound ? bound->GetTileset() : nullptr;
                if (!ts || !ts->HasTilesetData()) return 0;
                if (rgssId < 384) return 0; // Autotile-Slot: keine Paket-1-Daten
                const int visual = rgssId - 384;
                // Kodierung: Bits 0..6 = Prioritaet (0..5), Bit 7 = Busch-Flag
                return ts->GetPriority(visual) + (ts->GetBush(visual) ? 128 : 0);
            },
            []() -> int {
                const Map* bound = Game::Get().Map().GetBoundMap();
                auto ts = bound ? bound->GetTileset() : nullptr;
                return (ts && ts->HasTilesetData()) ? ts->GetMaxPriority() : 0;
            });

        // Schritt-SE nach Terrain-Tag (Paket 6): stepFor liest den Tag des
        // Tiles unter dem Spieler, play spielt footsteps/<name> aus dem
        // Audio/SE-Baum (Fallback: <name> direkt). Tag 0/lautlos: nichts.
        RgssSetFootstepHooks(
            [](int rgssId) -> std::string {
                if (rgssId < 384) return ""; // Autotile: vorerst kein Tag
                const Map* bound = Game::Get().Map().GetBoundMap();
                auto ts = bound ? bound->GetTileset() : nullptr;
                if (!ts || !ts->HasTilesetData()) return "";
                // Paket-6-Belegung (XP: Tags sind frei verwendbar — wir
                // belegen sie mit Defaults, Datei Audio/SE/footsteps/<name>):
                // 0 = lautlos, 1 Gras, 2 Stein, 3 Wasser, 4 hohes Gras
                // (klingt wie Gras; Encounter-Rate verdoppelt, s. Game.cpp),
                // 5 Sand, 6 Holz/Bruecke, 7 Eis.
                switch (ts->GetTerrainTag(rgssId - 384)) {
                case 1: return "grass";
                case 2: return "stone";
                case 3: return "water";
                case 4: return "grass"; // hohes Gras
                case 5: return "sand";
                case 6: return "wood";
                case 7: return "ice";
                default: return "";
                }
            },
            [this](const std::string& name) {
                if (!mAudio) return;
                // Nur bei Bewegung: Stillstand soll still sein (XP-Feeling).
                static Vec3 s_lastPos(0.0f);
                static bool s_hasLast = false;
                const Vec3 now = Game::Get().Player().GetPosition();
                const bool moved = !s_hasLast || glm::length(now - s_lastPos) > 0.25f;
                s_lastPos = now; s_hasLast = true;
                if (!moved) return;
                std::string p = ResolveAudioPath("footsteps/" + name, 3);
                if (p.empty()) p = ResolveAudioPath("footsteps/" + name + ".wav", 3);
                if (p.empty()) p = ResolveAudioPath(name, 3);
                if (p.empty()) p = ResolveAudioPath(name + ".wav", 3);
                if (!p.empty()) mAudio->PlaySE(p, false, 0.6f, 1.0f);
            });

        Game::Get().Update(dt);
        if (!GameUI::Get().Menu().IsVisible() && !BattleSystem::Get().IsInBattle()) {
            Game::Get().Player().Update(dt, *mInput);
        }
        // XP-Kampfmenue: Sobald ein Akteur eine Aktion waehlen darf, oeffnet
        // sich das Befehlsmenue (Angriff/Fertigkeit/Gegenstand/Verteidigen/
        // Flucht) mit Ziel- und Listen-Untermenues. Solange ein Menue offen
        // ist, wird nicht erneut geoeffnet; die Eingabe laeuft ueber
        // UpdateModalInput (MenuWindow hat oberste Prioritaet).
        // NativeBattleMenu=0: ausgeschaltet - eine Ruby-Szene uebernimmt die
        // Aktionswahl komplett (Battle.needs_input? / Battle.set_action).
        if (CustomConfig::Get().nativeBattleMenu &&
            BattleSystem::Get().NeedsInput() && !GameUI::Get().Menu().IsVisible() &&
            !GameUI::Get().Message().IsBusy()) {
            GameUI::Get().OpenBattleCommands();
        }
        BattleSystem::Get().Update(dt);

        // XP-Kampfstatus: Gegner- und Gruppen-Zeile oben im Bild, solange der
        // Kampf laeuft (wird nach dem Kampfende automatisch entfernt).
        if (CustomConfig::Get().nativeBattleStatus && BattleSystem::Get().IsInBattle()) {
            auto& bs = BattleSystem::Get();
            // PAKET 9: Ziel-Blinken — im Gegner-Zielmenue flackert die Grafik
            // des markierten Gegners (XP: Ziel blinkt beim Waehlen). Cursor-
            // Index == Gegner-Index (Menue listet alle, tote nur deaktiviert).
            {
                std::string wantTag;
                if (GameUI::Get().Menu().IsVisible() &&
                    GameUI::Get().Menu().GetTitle() == "Welchen Gegner?") {
                    const int cur = GameUI::Get().Menu().GetCursor();
                    if (cur >= 0 && cur < (int)bs.Enemies().size())
                        wantTag = "$battler" + std::to_string(cur);
                }
                if (wantTag != mBattleBlinkTag) {
                    if (!mBattleBlinkTag.empty())
                        GameUI::Get().SetPictureBlinking(mBattleBlinkTag, false);
                    if (!wantTag.empty())
                        GameUI::Get().SetPictureBlinking(wantTag, true);
                    mBattleBlinkTag = wantTag;
                }
            }
            if (mBattleStatusEnemiesId < 0) {
                mBattleStatusEnemiesId = GameUI::Get().AddScreenText(
                    "", Vec2(0.5f, 0.03f), Color(1.0f, 0.85f, 0.6f, 1.0f), 0.0f, true, 1.0f);
            }
            mBattleStatusTimer -= dt;
            if (mBattleStatusTimer <= 0.0f) {
                mBattleStatusTimer = 0.25f;
                std::string enemies;
                for (const auto& e : bs.Enemies()) {
                    if (!enemies.empty()) enemies += "     ";
                    if (e.isDead) {
                        enemies += "[" + e.name + " besiegt]";
                    } else {
                        const std::string sev = e.MostSevereStateName(); // PAKET 17
                        enemies += e.name +
                                   (sev.empty() ? "" : " [" + sev + "]") + "  " +
                                   std::to_string(e.hp) + "/" + std::to_string(e.maxHp);
                    }
                }
                GameUI::Get().SetScreenText(mBattleStatusEnemiesId, enemies);

                // PAKET 9: Party-Status als XP-Statusfenster unten (Gesicht,
                // Name, HP-/MP-Balken, K.O.) — ersetzt die fruehere Textzeile
                std::vector<GameUI::BattleStatusEntry> statusEntries;
                statusEntries.reserve(bs.Actors().size());
                for (const auto& a : bs.Actors()) {
                    GameUI::BattleStatusEntry se;
                    se.name = a.name;
                    se.hp = a.hp; se.maxHp = a.maxHp;
                    se.mp = a.mp; se.maxMp = a.maxMp;
                    se.dead = a.isDead;
                    se.stateName = a.isDead ? std::string() : a.MostSevereStateName(); // PAKET 17
                    if (const auto* ad = Database::Get().GetActor(a.id)) {
                        se.faceName = ad->faceName;
                        se.faceIndex = ad->faceIndex;
                    }
                    statusEntries.push_back(std::move(se));
                }
                GameUI::Get().SetBattleStatusEntries(std::move(statusEntries));

                // --- Gegner-Grafiken (Graphics/Battlers/<battlerName>, XP) ---
                // Max. 4 Stueck; Bilder entstehen einmal und bleiben (kein
                // Flackern/Reload), tote Gegner faden weich aus (XP-Collapse).
                const int enemyCount = (int)bs.Enemies().size();
                auto removeBattlerPic = [this](const std::string& tag) {
                    auto it2 = std::find(mBattlerPicNames.begin(), mBattlerPicNames.end(), tag);
                    if (it2 != mBattlerPicNames.end()) {
                        GameUI::Get().RemovePicture(*it2);
                        mBattlerPicNames.erase(it2);
                    }
                    mBattlerPicIds.erase(tag);
                };
                auto isDying = [this](const std::string& tag) {
                    for (const auto& d : mBattlerDying) if (d.tag == tag) return true;
                    return false;
                };
                for (int i = 0; i < enemyCount && i < 4; ++i) {
                    const auto& e = bs.Enemies()[(size_t)i];
                    const std::string tag = "$battler" + std::to_string(i);
                    const bool exists = std::find(mBattlerPicNames.begin(),
                        mBattlerPicNames.end(), tag) != mBattlerPicNames.end();
                    if (!e.isDead) {
                        if (!exists) {
                            std::string gfx;
                            int hue = 0; // PAKET 9: XP-Farbton des Battler-Bildes
                            if (const auto* ed = Database::Get().GetEnemy(e.id)) {
                                gfx = ed->battlerName;
                                hue = ed->battlerHue;
                            }
                            if (gfx.empty()) gfx = "slime";
                            const float x = enemyCount > 1
                                ? (0.25f + 0.5f * (float)i / (float)(enemyCount - 1))
                                : 0.5f;
                            const int picId = GameUI::Get().ShowPicture(gfx, tag, Vec2(x, 0.30f),
                                                      1.5f, 1.0f, 0.0f, hue);
                            mBattlerPicNames.push_back(tag);
                            mBattlerPicIds[tag] = picId;
                        }
                    } else if (exists && !isDying(tag)) {
                        // PAKET 15: XP-Collapse — das Bild fadet ueber
                        // ~0,45 s aus (XP: Battler loest sich beim Sieg auf).
                        const int picId = mBattlerPicIds.count(tag)
                                            ? mBattlerPicIds[tag] : 0;
                        if (picId > 0)
                            GameUI::Get().TweenPictureOpacity(picId, 0.0f, 0.45f, 2);
                        mBattlerDying.push_back({picId, tag, 0.5f});
                    }
                    // Schon im Fade: Nichts mehr tun — der decay unten
                    // entfernt das Picture nach Ablauf endgueltig.
                }
                for (int i = enemyCount; i < 4; ++i)
                    removeBattlerPic("$battler" + std::to_string(i));
                // PAKET 15: abgelaufene Todes-Fades endgueltig entfernen
                for (size_t di = 0; di < mBattlerDying.size();) {
                    mBattlerDying[di].t -= dt;
                    if (mBattlerDying[di].t <= 0.0f) {
                        removeBattlerPic(mBattlerDying[di].tag);
                        mBattlerDying.erase(mBattlerDying.begin() + (ptrdiff_t)di);
                    } else {
                        ++di;
                    }
                }
            }
        } else if (mBattleStatusEnemiesId >= 0) {
            GameUI::Get().RemoveScreenText(mBattleStatusEnemiesId);
            mBattleStatusEnemiesId = -1;
            mBattleStatusTimer = 0.0f;
            GameUI::Get().ClearBattleStatus(); // PAKET 9: XP-Statusfenster aus
            if (!mBattleBlinkTag.empty()) {   // Ziel-Blinken aus
                GameUI::Get().SetPictureBlinking(mBattleBlinkTag, false);
                mBattleBlinkTag.clear();
            }
            for (const auto& tag : mBattlerPicNames) GameUI::Get().RemovePicture(tag);
            mBattlerPicNames.clear(); // Gegner-Grafiken weg
            mBattlerPicIds.clear();
            mBattlerDying.clear(); // PAKET 15: laufende Todes-Fades weg
        }
        if (mRubyVM) mRubyVM->Update(dt);
    }

    // Game Over (XP): Anzeige bestaetigt (oder Playtest manuell gestoppt) ->
    // zurueck zum Titel (Player) bzw. Playtest-Stopp (Editor). Laeuft
    // absichtlich ausserhalb des PlayMode-Blocks, damit kein Zustand haengt.
    if (mGameOverPending) {
        if (!mPlayMode) {
            mGameOverPending = false; // Playtest wurde manuell gestoppt
        } else if (!GameUI::Get().Message().IsBusy()) {
            mGameOverPending = false;
            if (mEditorMode) SetPlaying(false);
            else ReturnToTitle();
        }
    }

    // Kampfstatus-Anzeige aufraeumen, wenn kein Kampf (mehr) laeuft -
    // auch nach Titelwechsel/Playtest-Stopp (laeuft sonst als Geist weiter)
    if (mBattleStatusEnemiesId >= 0 && !BattleSystem::Get().IsInBattle()) {
        GameUI::Get().RemoveScreenText(mBattleStatusEnemiesId);
        mBattleStatusEnemiesId = -1;
        mBattleStatusTimer = 0.0f;
        GameUI::Get().ClearBattleStatus(); // PAKET 9: XP-Statusfenster aus
        if (!mBattleBlinkTag.empty()) {       // Ziel-Blinken aus
            GameUI::Get().SetPictureBlinking(mBattleBlinkTag, false);
            mBattleBlinkTag.clear();
        }
        for (const auto& tag : mBattlerPicNames) GameUI::Get().RemovePicture(tag);
        mBattlerPicNames.clear(); // Gegner-Grafiken weg
        mBattlerPicIds.clear();
        mBattlerDying.clear(); // PAKET 15: laufende Todes-Fades weg
    }

    // UI - GameUI läuft im Player IMMER, im Editor nur im PlayMode
    GameUI::Get().Update(dt);

    // PAKET 10: F9 = Spiel-HUD ein/aus (ersetzt den RmlUi-HUD-Toggle).
    // Direkt im Update abgefragt, damit es im Player UND im eingebetteten
    // Qt-Playtest funktioniert (wie der F10-Debug-Inspektor).
    if (mInput && (mPlayMode || !mEditorMode) && mInput->IsKeyPressed(Key::F9))
        GameUI::Get().ToggleHud();

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

    // PAKET 37: Die gesamte Spielanzeige (Messages, Menues, HUD, Pictures,
    // ScreenTexts, Kampfstatus, Farbton, Wetter) laeuft ueber das eigene
    // RUI-Framework. ImGui (falls gebaut) ist nur noch ein optionaler
    // Zeichen-Adapter fuer RUI; die Fensterlogik laeuft immer. DrawPlayHud
    // wird bewusst IMMER aufgerufen (verwaltet RemoveWindow selbst).
#ifdef RPGMAKER3D_ENABLE_IMGUI
    ImGuiBeginFrame();
#endif
    {
        const float uiW = mWindow ? (float)mWindow->GetWidth() : 1280.0f;
        const float uiH = mWindow ? (float)mWindow->GetHeight() : 720.0f;
        GameUI::Get().SetDisplaySize(uiW, uiH);
        if (mPlayMode || !mEditorMode) {
            GameUI::Get().Draw();
            GameUI::Get().DrawPlayHud(mEditorMode);
        }
        // ImGui-Builds: RUI-Fenster zeichnen als DrawList im selben Frame.
        // Ohne ImGui: braucht einen echten DrawTarget (PAKET 39: GL-Adapter).
#ifdef RPGMAKER3D_ENABLE_IMGUI
        if (mImGuiReady) rui::Manager::Get().Draw();
#else
        rui::Manager::Get().Draw();
#endif
    }
#ifdef RPGMAKER3D_ENABLE_IMGUI
    ImGuiEndFrame();
#endif

    // RGSS-Fenster (reine Ruby-UI) liegen auf der obersten Schicht -
    // nach den GameUI-Overlays zeichnen, damit Ruby-UIs alles ueberdecken.
    if (mWindow) RgssUI::Get().Render(mWindow->GetWidth(), mWindow->GetHeight());
}

// ===========================================================================
// PAKET 8: XP-Charakter-Sprites in der Spielszene
// ---------------------------------------------------------------------------
// Loest die farbigen Quader-Marker ab, sobald das Projekt unter
// Graphics/Characters/<Name>(.png/.jpg/.jpeg/.bmp) ein XP-Spritesheet
// liefert. XP-Layout: 4 Richtungen x 4 Laufphasen (Zeile 0..3 =
// down/left/right/up, Spalte 0..3 = Laufphase, 0 = Standbild). Events mit
// graphicIndex > 0 nutzen ein 8er-Sheet (4x2 Unterbloecke a 4x4 Frames,
// VX-Ace-Stil). Busch-Tiles (GameMap::IsBushAt) zeichnen die untere Haelfte
// mit 45% Alpha ("im Gras stehen", wie zuvor die Quader-Fassung).
// ===========================================================================
namespace {

struct CharacterSheetEntry {
    bool tried = false;                    // Ladeversuch unternommen?
    std::shared_ptr<Texture> tex;          // nullptr = Datei nicht gefunden
};

/// Laedt (einmalig) Graphics/Characters/<name> und cachet auch den
/// Negativfall, damit fehlende Dateien keinen wiederholten Lade-Spam erzeugen.
std::shared_ptr<Texture> LoadCharacterSheet(const std::string& graphicName) {
    static std::unordered_map<std::string, CharacterSheetEntry> sCache;
    if (graphicName.empty()) return nullptr;
    auto it = sCache.find(graphicName);
    if (it != sCache.end()) return it->second.tex;

    CharacterSheetEntry entry;
    entry.tried = true;
    const std::string path = RgssResolveGraphic("Graphics/Characters/" + graphicName);
    if (!path.empty()) {
        auto t = std::make_shared<Texture>();
        if (t->LoadFromFile(path)) {
            if (t->GetWidth() >= 4 && t->GetHeight() >= 4) {
                entry.tex = std::move(t);
            } else {
                RPG_LOG_WARN("Charakter-Sheet zu klein (erwarte 4x4 Frames): " + path);
            }
        } else {
            RPG_LOG_WARN("Charakter-Sheet nicht lesbar: " + path);
        }
    }
    return sCache.emplace(graphicName, std::move(entry)).first->second.tex;
}

/// Baut (und cachet) das Frame-Quad: Ursprung an den Fuessen (x zentriert),
/// Weltgroesse = Framepixel / 32 (XP: 1 Tile = 32 px = 1 Welteinheit).
/// Textur ist vertikal geflippt (stb) -> v=0 entspricht PNG-UNTEN.
/// half: 0 = ganzer Frame, 1 = untere Haelfte, 2 = obere Haelfte (Busch).
const Mesh& GetCharacterFrameQuad(unsigned int texId, int texW, int texH,
                                  int graphicIndex, int row, int col, int half) {
    const uint64_t key = (uint64_t)texId << 32
                       | (uint64_t)((unsigned)graphicIndex & 0xFFu) << 16
                       | (uint64_t)(row & 7) << 8
                       | (uint64_t)(col & 3) << 4
                       | (uint64_t)(half & 3);
    static std::unordered_map<uint64_t, Mesh> sQuads;
    auto it = sQuads.find(key);
    if (it != sQuads.end()) return it->second;

    // Frame-Geometrie im Sheet
    int fw, fh, oxF = 0, oyF = 0;          // Framegroesse + Frame-Offset (in Frames)
    if (graphicIndex > 0) {                // 8er-Sheet: 4x2 Sub-Bloecke a 4x4
        fw = texW / 16; fh = texH / 8;
        oxF = ((graphicIndex - 1) % 4) * 4;
        oyF = ((graphicIndex - 1) / 4) * 4;
    } else {                               // klassisches XP-Sheet: 4x4 Frames
        fw = texW / 4; fh = texH / 4;
    }
    if (fw <= 0) fw = 1;
    if (fh <= 0) fh = 1;

    const float w = (float)fw * (1.0f / 32.0f);
    const float h = (float)fh * (1.0f / 32.0f);
    float y0 = 0.0f, y1 = h;
    if (half == 1)      y1 = h * 0.5f;
    else if (half == 2) y0 = h * 0.5f;

    // PNG-Zeile py0..py0+fh (von OBEN gezaehlt) -> UV (geflippt geladen)
    const float u0 = (float)((oxF + col) * fw) / (float)texW;
    const float u1 = (float)((oxF + col + 1) * fw) / (float)texW;
    const float vB = 1.0f - (float)((oyF + row + 1) * fh) / (float)texH;
    const float vT = 1.0f - (float)((oyF + row) * fh) / (float)texH;
    // Halbierte Quads fuehren den UV-Ausschnitt proportional mit
    const float f0 = (float)(y0 / h);      // 0.0 bzw. 0.5
    const float f1 = (float)(y1 / h);      // 1.0 bzw. 0.5
    const float vLo = vB + (vT - vB) * f0;
    const float vHi = vB + (vT - vB) * f1;

    Mesh mesh;
    mesh.vertices = {
        {{-w * 0.5f, y0, 0.0f}, {0, 0, 1}, {u0, vLo}},
        {{ w * 0.5f, y0, 0.0f}, {0, 0, 1}, {u1, vLo}},
        {{ w * 0.5f, y1, 0.0f}, {0, 0, 1}, {u1, vHi}},
        {{-w * 0.5f, y1, 0.0f}, {0, 0, 1}, {u0, vHi}}
    };
    mesh.indices = {0, 1, 2, 2, 3, 0};     // Winding wie MeshFactory::CreateQuad
    mesh.BuildGPU();
    return sQuads.emplace(key, std::move(mesh)).first->second;
}

/// XP-Sheet-Zeile aus der Laufzeit-Richtung (2/4/6/8 -> 0..3)
int CharacterRowFromDir(int dir2d) {
    switch (dir2d) {
        case 4:  return 1;   // left
        case 6:  return 2;   // right
        case 8:  return 3;   // up
        case 2:
        default: return 0;   // down
    }
}

/// Dominante 2D-Richtung aus dem Bewegungsvektor (Engine-Konvention:
/// z- = hoch/8, z+ = runter/2, x- = links/4, x+ = rechts/6)
int CharacterDir2DFromVec(const Vec3& d) {
    if (std::fabs(d.x) >= std::fabs(d.z)) return d.x > 0.0f ? 6 : 4;
    return d.z > 0.0f ? 2 : 8;
}

/// Animationszustand pro Charakter (XP: Phase 0..3, Takt ~0,13 s; 0 = Stand)
struct CharacterAnimState {
    int pattern = 0;
    float t = 0.0f;
    Vec3 lastPos{1.0e30f, 0.0f, 0.0f};
};

/// Phase weiterzaehlen. movingKnown: hat der Aufrufer die Bewegung bereits
/// bestimmt (Player: IsMoving); sonst Positionsdelta als Bewegungsindikator.
/// walkAnime=false -> XP: immer Standbild. stepAnime -> auch im Stand animieren.
void CharacterAnimAdvance(CharacterAnimState& st, const Vec3& pos, bool movingKnown,
                          bool walkAnime, bool stepAnime, float dt) {
    bool moving = movingKnown;
    if (!moving) {
        const float dx = pos.x - st.lastPos.x;
        const float dz = pos.z - st.lastPos.z;
        moving = (dx * dx + dz * dz) > 1.0e-8f;
    }
    st.lastPos = pos;
    if (!walkAnime) { st.pattern = 0; st.t = 0.0f; return; }
    if (moving || stepAnime) {
        st.t += dt;
        if (st.t >= 0.13f) {               // ~5 Frames im XP-40fps-Takt
            st.t = 0.0f;
            st.pattern = (st.pattern + 1) & 3;
        }
    } else {
        st.pattern = 0;
        st.t = 0.0f;
    }
}

/// Zeichnet einen Charakter als kamerazugewandtes, texturiertes Quad.
/// Rueckgabe false = kein gueltiges Sheet (Aufrufer nimmt den Quader-Rueckfall).
bool DrawCharacterSprite(Renderer& renderer, const Camera& camera,
                         const std::shared_ptr<Texture>& tex,
                         const Vec3& basePos, int dir2d, int pattern,
                         int graphicIndex, float alpha, bool bush) {
    if (!tex) return false;

    // Billboard-Orientierung (Muster wie SpriteComponent; robust bei
    // senkrechter Draufsicht: right faellt auf (1,0,0) zurueck)
    Vec3 forward = camera.GetPosition() - basePos;
    forward = glm::length(forward) > 1.0e-5f ? glm::normalize(forward) : Vec3(0, 0, 1);
    Vec3 right = glm::cross(Vec3(0, 1, 0), forward);
    right = glm::length(right) > 1.0e-5f ? glm::normalize(right) : Vec3(1, 0, 0);
    Vec3 up = glm::cross(forward, right);
    Mat4 rot(right.x, right.y, right.z, 0,
             up.x, up.y, up.z, 0,
             forward.x, forward.y, forward.z, 0,
             0, 0, 0, 1);
    Mat4 base = glm::translate(Mat4(1.0f), basePos) * rot;

    const int row = CharacterRowFromDir(dir2d);
    const int col = pattern & 3;
    const unsigned int texId = tex->GetID();
    const int tw = tex->GetWidth();
    const int th = tex->GetHeight();

    Color full(1.0f, 1.0f, 1.0f, alpha);
    if (!bush) {
        renderer.DrawMesh(GetCharacterFrameQuad(texId, tw, th, graphicIndex, row, col, 0),
                          base, tex.get(), full);
        return true;
    }
    Color lower = full; lower.a *= 0.45f;
    renderer.DrawMesh(GetCharacterFrameQuad(texId, tw, th, graphicIndex, row, col, 1),
                      base, tex.get(), lower);
    renderer.DrawMesh(GetCharacterFrameQuad(texId, tw, th, graphicIndex, row, col, 2),
                      base, tex.get(), full);
    return true;
}

} // namespace

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

    // Player + Event-Charaktere im PlayMode (PAKET 8: echte XP-Spritesheets
    // aus Graphics/Characters; fehlt eine Datei, greift der Quader-Rueckfall)
    if (mPlayMode) {
        static Mesh playerMesh;
        static Mesh markerMesh;
        static bool meshesInit = false;
        if (!meshesInit) {
            playerMesh = MeshFactory::CreateCube(0.7f);
            markerMesh = MeshFactory::CreateCube(0.45f);
            meshesInit = true;
        }
        // Charakter-Wuerfel mit XP-Busch-Effekt (Paket 6 Folge): steht der
        // Charakter auf einem Busch-geflaggten Tile (GameMap::IsBushAt),
        // wird die UNTERE Haelfte mit 45% Alpha gezeichnet — die XP-Optik
        // „im Gras stehen" (XP loest das als bush_depth-Maske des 2D-
        // Sprites; wir spiegeln es als Halbschnitt des 3D-Markers, die
        // Tile-Ebene macht seit dem Vortag das untere Quad4 halbtransparent).
        auto drawCharCube = [&](const Mesh& mesh, float meshSize, const Vec3& basePos,
                                float yCenter, const Color& col, bool bush) {
            if (!bush) {
                Mat4 m = glm::translate(Mat4(1.0f), basePos + Vec3(0, yCenter, 0));
                mRenderer->DrawMesh(mesh, m, nullptr, col);
                return;
            }
            Color lower = col; lower.a *= 0.45f;
            Mat4 lo = glm::translate(Mat4(1.0f), basePos + Vec3(0, yCenter - meshSize * 0.25f, 0));
            lo = glm::scale(lo, Vec3(1.0f, 0.5f, 1.0f));
            mRenderer->DrawMesh(mesh, lo, nullptr, lower);
            Mat4 hi = glm::translate(Mat4(1.0f), basePos + Vec3(0, yCenter + meshSize * 0.25f, 0));
            hi = glm::scale(hi, Vec3(1.0f, 0.5f, 1.0f));
            mRenderer->DrawMesh(mesh, hi, nullptr, col);
        };

        static CharacterAnimState sPlayerAnim;
        static std::unordered_map<int, CharacterAnimState> sEventAnims;

        // Player: Grafik = Party-Leader (XP: actors[0].character_name);
        // laeuft -> Phase taktet, sonst Standbild. Rueckfall: Quader + Nase.
        Vec3 playerPos = Game::Get().Player().GetPosition();
        const float playerAlpha = Game::Get().Player().IsTransparent() ? 0.35f : 1.0f;
        const bool playerBush = Game::Get().Map().IsBushAt(playerPos);
        CharacterAnimAdvance(sPlayerAnim, playerPos,
                             Game::Get().Player().IsMoving(), true, false, mDeltaTime);
        std::string playerGraphic;
        if (!Game::Get().Party().Members().empty())
            playerGraphic = Game::Get().Party().Members().front().graphicName;
        const int playerDir = CharacterDir2DFromVec(Game::Get().Player().GetDirection());
        if (!DrawCharacterSprite(*mRenderer, *camera, LoadCharacterSheet(playerGraphic),
                                 playerPos, playerDir, sPlayerAnim.pattern, 0,
                                 playerAlpha, playerBush)) {
            drawCharCube(playerMesh, 0.7f, playerPos, 0.35f,
                         Color(0.25f, 0.95f, 0.35f, playerAlpha), playerBush);
            // Facing indicator (nur im Quader-Rueckfall)
            Vec3 dir = Game::Get().Player().GetDirection();
            Mat4 nose = glm::translate(Mat4(1.0f), playerPos + Vec3(0, 0.35f, 0) + dir * 0.45f);
            nose = glm::scale(nose, Vec3(0.2f, 0.2f, 0.2f));
            mRenderer->DrawMesh(markerMesh, nose, nullptr, Color(1.0f, 1.0f, 0.2f, 1.0f));
        }

        // Events: Grafik + Animationsflags aus der AKTIVEN Seite
        // (graphicName/graphicIndex/walkAnime/stepAnime), Blickrichtung =
        // Laufzeit-direction (2/4/6/8). Bewegung per Positionsdelta erkannt.
        for (const auto& ev : EventSystem::Get().GetEvents()) {
            if (!ev.enabled || ev.erased) continue;
            Vec3 ep = ev.worldPos;
            if (glm::length(ep) < 0.001f) ep = Vec3((float)ev.x, (float)ev.y, (float)ev.z);
            const EventPage* page = ev.GetCurrentPage();
            // PAKET 16: Move-Route-Overrides schlagen die Seitenwerte —
            // XP 39 (Grafik wechseln) und XP 37/38 (Transparent an/aus).
            const std::string gname = !ev.routeGraphic.empty()
                                        ? ev.routeGraphic
                                        : (page ? page->graphicName : std::string());
            const int gindex = !ev.routeGraphic.empty()
                                 ? ev.routeGraphicIndex
                                 : (page ? page->graphicIndex : 0);
            const float evAlpha = ev.transparent ? 0.0f : 1.0f;
            const bool walkAnime = page ? page->walkAnime : true;
            const bool stepAnime = page ? page->stepAnime : false;
            auto sheet = LoadCharacterSheet(gname);
            auto& anim = sEventAnims[ev.id];
            if (sheet) CharacterAnimAdvance(anim, ep, false, walkAnime, stepAnime, mDeltaTime);
            if (!DrawCharacterSprite(*mRenderer, *camera, sheet, ep,
                                     ev.direction, anim.pattern, gindex, evAlpha,
                                     Game::Get().Map().IsBushAt(ep))) {
                float bob = std::sin(mTime * 3.0f + ev.id) * 0.08f;
                Color col = (ev.id == 1) ? Color(0.95f, 0.75f, 0.2f, evAlpha)
                                         : Color(0.4f, 0.7f, 1.0f, evAlpha);
                drawCharCube(markerMesh, 0.45f, ep, 0.55f + bob, col,
                             Game::Get().Map().IsBushAt(ep));
            }
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

// ---------------------------------------------------------------------------
// PAKET 25: Runtime-Karte laden, fehlende Datei -> spielbare Standardkarte
// (statt leerer 0-Layer-Welt, in der weder Boden noch Kollision existiert)
// ---------------------------------------------------------------------------
bool Engine::LoadRuntimeMap(int mapId) {
    if (!mMap) return false;
    if (mProject) {
        const std::string p = mProject->GetMapPath(mapId);
        if (std::filesystem::exists(p) && mMap->Load(p)) return true;
    }
    // Groesse aus den Datenbank-MapInfos (Karteneigenschaften), sonst 20x15
    int w = 20, h = 15;
    for (const auto& mi : Database::Get().MapInfos()) {
        if (mi.id == mapId) { w = mi.width; h = mi.height; break; }
    }
    mMap->CreateFallback(w, h);
    RPG_LOG_WARN("Karte " + std::to_string(mapId) +
                 " hat keine maps/map-Datei - Standardkarte generiert (" +
                 std::to_string(w) + "x" + std::to_string(h) + ")");
    return false;
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
        // Kein "map"-Abschnitt in der scene.json: Kartendatei laden,
        // bei Bedarf PAKET-25-Standardkarte (nie wieder leere Welt).
        LoadRuntimeMap(Database::Get().System().startMapId);
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

// ===========================================================================
// XP-Debug-Inspektor (Paket 4, TODO_XP_PARITY.md) - F10 im Playtest
// Fenster mit Schaltern (Live-Toggle per Enter) und Variablen (Enterprise =
// Zahleneingabe). Rendert ueber die RGSS-Fensterschicht (RgssUI), damit es
// im Player und im eingebetteten Qt-Playtest gleich auf dem Bildschirm
// landet. Angelehnt an das XP-F9-Debugfenster; unsere F9-Taste bleibt HUD.
// ===========================================================================

void Engine::ToggleDebugWindow() {
    if (!mPlayMode) return; // XP: Debug-Inspektor gibt es nur im (Test-)Spiel
    mDbgVisible = !mDbgVisible;
    if (!mDbgVisible) {
        DestroyDebugWindow();
        return;
    }
    // Fenster (neu) erzeugen
    auto& ui = RgssUI::Get();
    mDbgWindowId = ui.MakeWindow(8, 8, 624, 464);
    if (auto* w = ui.GetWindow(mDbgWindowId)) {
        w->z = 9000;            // immer oben
        w->opacity = 235;       // Spiel leicht durchscheinen lassen
        w->backOpacity = 235;
        w->openness = 255.0f;
        w->active = true;
        w->pause = false;
        w->stretch = true;
    }
    mDbgContentsId = RgssBmpCreate(624 - 32, 464 - 32);
    if (auto* w = ui.GetWindow(mDbgWindowId))
        w->contentsBmpId = mDbgContentsId;
    mDbgSel = 0;
    mDbgScroll = 0;
    mDbgEditing = false;
    mDbgEditValue = 0;
    mDbgNeedsRedraw = true;
    mDbgRefresh = 0.0f;
    RPG_LOG_INFO("Debug-Inspektor geoeffnet (F10)");
}

void Engine::DestroyDebugWindow() {
    auto& ui = RgssUI::Get();
    if (mDbgWindowId > 0) {
        ui.DisposeWindow(mDbgWindowId);
        mDbgWindowId = 0;
    }
    if (mDbgContentsId > 0) {
        RgssBmpDispose(mDbgContentsId);
        mDbgContentsId = 0;
    }
}

void Engine::UpdateDebugWindow(float dt) {
    if (!mDbgVisible) return;
    // Spiel beendet -> Fenster schliessen
    if (!mPlayMode) {
        mDbgVisible = false;
        DestroyDebugWindow();
        return;
    }
    // Fenster wurde von aussen disposed (z. B. ClearAll) -> neu aufsetzen
    auto& ui = RgssUI::Get();
    if (!ui.GetWindow(mDbgWindowId)) {
        mDbgWindowId = 0;
        mDbgContentsId = 0;
        mDbgVisible = false;
        ToggleDebugWindow(); // sauber neu anlegen
        return;
    }

    const auto& swNames = Database::Get().System().switches;
    const auto& varNames = Database::Get().System().variables;
    const int swCount = std::max(1, (int)swNames.size());
    const int varCount = std::max(1, (int)varNames.size());
    const int rowCount = swCount + varCount;

    auto& inp = *mInput;
    bool wantRedraw = mDbgNeedsRedraw;

    if (mDbgEditing) {
        // Zahleneingabe: Ziffern anhaengen, M = Negativ, Backspace, Enter, Esc
        for (int k = 0; k <= 9; ++k) {
            if (inp.IsKeyPressed((Key)((int)Key::Num0 + k))) {
                long v = (long)std::llabs((long)mDbgEditValue) * 10 + k;
                if (v > 99999999) v = 99999999;
                mDbgEditValue = (int)((mDbgEditValue < 0) ? -v : v);
                wantRedraw = true;
            }
        }
        if (inp.IsKeyPressed(Key::M)) {
            mDbgEditValue = -mDbgEditValue;
            wantRedraw = true;
        }
        if (inp.IsKeyPressed(Key::Backspace)) {
            mDbgEditValue /= 10;
            wantRedraw = true;
        }
        if (inp.IsKeyPressed(Key::Enter)) {
            const int varId = (mDbgSel - swCount) + 1; // 1-basiert
            Game::Get().Variables().Set(varId, mDbgEditValue);
            mDbgEditing = false;
            wantRedraw = true;
        }
        if (inp.IsKeyPressed(Key::Escape)) {
            mDbgEditing = false;
            wantRedraw = true;
        }
    } else {
        if (inp.IsKeyPressed(Key::F10)) {
            mDbgVisible = false;
            DestroyDebugWindow();
            return;
        }
        if (inp.IsKeyPressed(Key::Escape)) {
            mDbgVisible = false;
            DestroyDebugWindow();
            return;
        }
        if (rowCount > 0) {
            if (inp.IsKeyPressed(Key::Up)) {
                mDbgSel = (mDbgSel - 1 + rowCount) % rowCount;
                wantRedraw = true;
            }
            if (inp.IsKeyPressed(Key::Down)) {
                mDbgSel = (mDbgSel + 1) % rowCount;
                wantRedraw = true;
            }
            if (inp.IsKeyPressed(Key::Left)) {
                mDbgSel = (mDbgSel - 10 + rowCount) % rowCount;
                wantRedraw = true;
            }
            if (inp.IsKeyPressed(Key::Right)) {
                mDbgSel = (mDbgSel + 10) % rowCount;
                wantRedraw = true;
            }
        }
        if (inp.IsKeyPressed(Key::Enter)) {
            if (mDbgSel < swCount) {
                // Schalter togglen (live)
                const int id = mDbgSel + 1; // 1-basiert
                bool now = Game::Get().Switches().Get(id);
                Game::Get().Switches().Set(id, !now);
            } else {
                // Variable editieren
                mDbgEditing = true;
                mDbgEditValue = Game::Get().Variables().Get((mDbgSel - swCount) + 1);
            }
            wantRedraw = true;
        }
    }

    // Scrollfenster der Auswahl folgen lassen
    {
        const int visibleRows = 16; // passt in das 464px-Fenster
        if (mDbgSel < mDbgScroll) mDbgScroll = mDbgSel;
        if (mDbgSel >= mDbgScroll + visibleRows)
            mDbgScroll = mDbgSel - visibleRows + 1;
        (void)visibleRows;
    }

    // Live-Refresh: Werte koennen sich von allein aendern (Events laufen)
    mDbgRefresh -= dt;
    if (mDbgRefresh <= 0.0f) {
        mDbgRefresh = 0.25f;
        wantRedraw = true;
    }
    if (!wantRedraw) return;
    mDbgNeedsRedraw = false;

    RedrawDebugWindowContent();
}

void Engine::RedrawDebugWindowContent() {
    const int id = mDbgContentsId;
    if (id <= 0) return;
    auto* bmp = RgssBmpGet(id);
    if (!bmp || bmp->disposed) return;

    RgssBmpClear(id);

    const auto& swNames = Database::Get().System().switches;
    const auto& varNames = Database::Get().System().variables;
    const int swCount = std::max(1, (int)swNames.size());
    const int varCount = std::max(1, (int)varNames.size());

    // Kopfzeile
    RgssBmpDrawText(id, 0, 0, 592, 22,
                    "DEBUG-INSPEKTOR (F10)  -  Schalter & Variablen", 0);

    const int visibleRows = 16;
    const float rowH = 20.0f;
    for (int r = 0; r < visibleRows; ++r) {
        const int row = mDbgScroll + r;
        if (row >= swCount + varCount) break;
        const float y = 26.0f + r * rowH;
        const bool selRow = (row == mDbgSel);

        std::string line;
        if (row < swCount) {
            const int sid = row + 1;
            const bool on = Game::Get().Switches().Get(sid);
            std::string nm = (row < (int)swNames.size() && !swNames[row].empty())
                                 ? swNames[row] : ("Schalter " + std::to_string(sid));
            line = "S" + std::to_string(sid) + "\t" + nm + "\t[" +
                   (on ? "AN" : "AUS") + "]";
        } else {
            const int vrow = row - swCount;
            const int vid = vrow + 1;
            int val = Game::Get().Variables().Get(vid);
            bool editingRow = mDbgEditing && selRow;
            if (editingRow) val = mDbgEditValue;
            std::string nm = (vrow < (int)varNames.size() && !varNames[vrow].empty())
                                 ? varNames[vrow] : ("Variable " + std::to_string(vid));
            line = "V" + std::to_string(vid) + "\t" + nm + "\t= " +
                   std::to_string(val) + (editingRow ? "_" : "");
        }

        if (selRow) {
            RgssBmpFillRect(id, 0, y, 592, rowH, 255, 255, 255, 48.0f);
        }
        RgssBmpDrawText(id, 4, y, 588, rowH, line, 0);
    }

    // Fusszeile (Hilfen)
    RgssBmpDrawText(id, 0, 416 - 22, 592, 22,
        "F10: zu  Pfeile: waehlen  Enter: umschalten/editieren  "
        "(im Edit: Ziffern, M=Negativ, Esc=Abbruch)", 0);
}

} // namespace rpg
