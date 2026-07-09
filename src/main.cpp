#include "rpgmaker3d/Engine.h"
#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "RPG Maker 3D Engine v0.1.0" << std::endl;

    rpg::Engine engine;
    if (!engine.Initialize("RPG Maker 3D Engine", 1280, 720, true)) {
        std::cerr << "Engine initialization failed." << std::endl;
        return -1;
    }

    engine.Run();
    return 0;
}
