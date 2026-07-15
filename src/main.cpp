#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Config.h"
#include "rpgmaker3d/Platform.h"
#include "rpgmaker3d/Logger.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Game.h"
#include <iostream>
#include <string>

void PrintHelp() {
    std::cout << "RPG Maker 3D Engine v" << rpg::EngineConfig::VERSION << "\n"
              << "Usage:\n"
              << "  RPGMaker3D.exe [options]\n"
              << "Options:\n"
              << "  --editor          Starte im Editor-Modus (Standard)\n"
              << "  --play            Starte im Play-Modus (direkt Spiel testen)\n"
              << "  --project <path>  Projekt-Pfad angeben (Standard: ./SampleProject)\n"
              << "  --width <px>      Fensterbreite\n"
              << "  --height <px>     Fensterhöhe\n"
              << "  --fullscreen      Vollbild starten\n"
              << "  --help            Diese Hilfe anzeigen\n"
              << "\nBeispiele:\n"
              << "  RPGMaker3D.exe --editor\n"
              << "  RPGMaker3D.exe --play --project ./MyGame\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Windows DPI Awareness setzen bevor SDL Fenster erstellt wird
    rpg::Platform::SetDPIAware();

    std::cout << rpg::EngineConfig::NAME << " v" << rpg::EngineConfig::VERSION
              << " [" << RPG_PLATFORM_NAME << "]" << std::endl;

    bool editorMode = true;
    bool playMode = false;
    bool fullscreen = false;
    std::string projectPath = "./SampleProject";
    int width = rpg::EngineConfig::DEFAULT_WIDTH;
    int height = rpg::EngineConfig::DEFAULT_HEIGHT;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            PrintHelp();
            return 0;
        } else if (arg == "--editor") {
            editorMode = true;
            playMode = false;
        } else if (arg == "--play" || arg == "--game") {
            editorMode = false;
            playMode = true;
        } else if (arg == "--project" && i + 1 < argc) {
            projectPath = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (arg == "--fullscreen") {
            fullscreen = true;
        }
    }

    std::cout << "Projekt: " << projectPath << "\n";
    std::cout << "Modus: " << (editorMode ? "Editor" : "Player") << "\n";
    std::cout << "Aufloesung: " << width << "x" << height << (fullscreen ? " Vollbild" : "") << "\n";

    // Datenbank initialisieren
    rpg::Database::Get().Load(projectPath);

    rpg::Engine engine;

    std::string title = std::string(rpg::EngineConfig::NAME) + " v" + rpg::EngineConfig::VERSION;
    if (!editorMode) {
        title = rpg::Database::Get().System().gameTitle + " - " + rpg::EngineConfig::NAME;
    }

    if (!engine.Initialize(title, width, height, editorMode)) {
        std::cerr << "Engine Initialisierung fehlgeschlagen!" << std::endl;
        rpg::Platform::ShowMessageBox("Fehler", "Engine konnte nicht initialisiert werden.\nPrüfe engine.log", true);
        return -1;
    }

    // Falls Player-Modus, direkt neues Spiel starten
    if (playMode) {
        rpg::Game::Get().NewGame();
        engine.SetPlaying(true);
    }

    try {
        engine.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        RPG_LOG_FATAL(std::string("Fatal exception: ") + e.what());
        rpg::Platform::ShowMessageBox("Fataler Fehler", e.what(), true);
        return -2;
    }

    std::cout << "Engine beendet. Bye!" << std::endl;
    return 0;
}

#ifdef _WIN32
// Windows: Unterstützung für WinMain falls als WIN32_EXECUTABLE gebaut
// SDL2main erledigt das normalerweise, aber wir bieten Fallback
#ifdef RPGMAKER3D_ENABLE_WINMAIN
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {
    return main(__argc, __argv);
}
#endif
#endif
