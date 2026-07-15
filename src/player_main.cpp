#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Config.h"
#include "rpgmaker3d/Platform.h"
#include "rpgmaker3d/Game.h"
#include <iostream>

// Separater Player-Executable für fertige Spiele (ohne Editor)
// Wird als RPGMaker3D_Player.exe gebaut

int main(int argc, char* argv[]) {
    rpg::Platform::SetDPIAware();

    std::cout << rpg::EngineConfig::NAME << " Player v" << rpg::EngineConfig::VERSION 
              << " [" << RPG_PLATFORM_NAME << "]" << std::endl;

    std::string projectPath = "./SampleProject";
    if (argc > 1) projectPath = argv[1];

    std::cout << "Projekt: " << projectPath << std::endl;

    rpg::Engine engine;
    std::string title = rpg::Database::Get().System().gameTitle;
    if (title.empty()) title = "RPG Game";

    if (!engine.Initialize(title, rpg::EngineConfig::DEFAULT_WIDTH, rpg::EngineConfig::DEFAULT_HEIGHT, false)) {
        std::cerr << "Engine Init fehlgeschlagen!" << std::endl;
        rpg::Platform::ShowMessageBox("Fehler", "Engine konnte nicht initialisiert werden", true);
        return -1;
    }

    rpg::Game::Get().NewGame();
    engine.SetPlaying(true); // loads demo events + scripts inside SetPlaying

    try {
        engine.Run();
    } catch (const std::exception& e) {
        rpg::Platform::ShowMessageBox("Fatal", e.what(), true);
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
