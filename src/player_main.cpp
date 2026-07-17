#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Config.h"
#include "rpgmaker3d/Platform.h"
#include "rpgmaker3d/Game.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/ScriptManager.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Logger.h"
#include <iostream>
#include <string>
#include <filesystem>

// Separater Player-Executable für fertige Spiele (ohne Editor)
// Wird als RPGMaker3D_Player(.exe) gebaut.
//
// Wird auch vom Editor-Playtest gestartet (RPG Maker Klassik: "Playtest"
// startet die Player-exe mit dem Projektordner als Argument):
//   RPGMaker3D_Player.exe --project "C:/MeinPfad/MeinSpiel"
//   RPGMaker3D_Player.exe "C:/MeinPfad/MeinSpiel"          (Kurzform)

namespace {

std::string ParseProjectPath(int argc, char* argv[]) {
    std::string projectPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--project" && i + 1 < argc) {
            projectPath = argv[++i];
        } else if (a.rfind("--project=", 0) == 0) {
            projectPath = a.substr(10);
        } else if (a == "--debug" || a == "--console" || a == "--battletest") {
            // Flags ohne Wert (battletest: zukuenftig)
        } else if (a.rfind("--", 0) != 0 && projectPath.empty()) {
            projectPath = a; // erstes Argument ohne "--" = Projektpfad
        }
    }
    if (projectPath.empty()) projectPath = "./SampleProject";
    // Abschliessende Slashs entfernen
    while (projectPath.size() > 1 &&
           (projectPath.back() == '/' || projectPath.back() == '\\'))
        projectPath.pop_back();
    return projectPath;
}

} // namespace

int main(int argc, char* argv[]) {
    rpg::Platform::SetDPIAware();

    std::cout << rpg::EngineConfig::NAME << " Player v" << rpg::EngineConfig::VERSION
              << " [" << RPG_PLATFORM_NAME << "]" << std::endl;

    const std::string projectPath = ParseProjectPath(argc, argv);
    std::cout << "Projekt: " << projectPath << std::endl;

    const bool validProject =
        std::filesystem::exists(projectPath + "/project.json");

    rpg::Engine engine;
    if (!engine.Initialize("RPG Maker 3D Player",
                           rpg::EngineConfig::DEFAULT_WIDTH,
                           rpg::EngineConfig::DEFAULT_HEIGHT, false)) {
        std::cerr << "Engine-Initialisierung fehlgeschlagen!" << std::endl;
        rpg::Platform::ShowMessageBox("Fehler", "Engine konnte nicht initialisiert werden", true);
        return -1;
    }

    // --- Projekt explizit laden ---
    // Engine::Initialize laedt standardmaessig ./SampleProject; hier wird das
    // uebergebene Projekt geladen (Szene, Datenbank, Scripts, Map).
    if (validProject &&
        projectPath != engine.GetProject().GetProjectPath()) {
        if (engine.GetProject().Load(projectPath)) {
            std::cout << "Projekt geladen: " << engine.GetProject().GetInfo().name << std::endl;
            rpg::Database::Get().Load(projectPath);
            engine.GetScriptManager().LoadProjectScripts(projectPath);
            // Szene (Entities) + Map-Datei der Startkarte laden
            if (!engine.LoadScene(projectPath + "/scene.json")) {
                RPG_LOG_WARN("Keine scene.json im Projekt - leere Szene");
            }
            engine.GetMap().Load(
                engine.GetProject().GetMapPath(rpg::Database::Get().System().startMapId));
        } else {
            std::cerr << "Projekt konnte nicht geladen werden: " << projectPath << std::endl;
            rpg::Platform::ShowMessageBox("Projektfehler",
                "Das Projekt konnte nicht geladen werden:\n" + projectPath, true);
        }
    } else if (!validProject) {
        std::cout << "Hinweis: Kein project.json unter '" << projectPath
                  << "' - Standardprojekt wird verwendet." << std::endl;
    }

    // Fenstertitel aus der Datenbank (Spieltitel)
    std::string title = rpg::Database::Get().System().gameTitle;
    if (title.empty()) title = "RPG Maker 3D Player";
    engine.GetWindow().SetTitle(title);

    // Spiel ab der in der Datenbank definierten Startposition
    rpg::Game::Get().NewGame();
    engine.SetPlaying(true); // laedt Map-Events + fuehrt Scripts aus

    try {
        engine.Run();
    } catch (const std::exception& e) {
        rpg::Platform::ShowMessageBox("Schwerer Fehler", e.what(), true);
        return -2;
    }

    return 0;
}

#ifdef _WIN32
#ifdef RPGMAKER3D_PLAYER_WINMAIN
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
#endif
#endif
