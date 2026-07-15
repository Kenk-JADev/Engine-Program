#pragma once
// RPG Maker 3D - Engine Config
// Windows-zentriert, aber cross-platform kompatibel

#include <string>

// Windows-Header MUSS im globalen Scope inkludiert werden - NIEMALS innerhalb
// eines namespace! Ein #include <windows.h> innerhalb von namespace rpg wuerde
// alle Win32-Deklarationen (HWND, SSIZE_T, DWORD, ...) nach rpg:: verschieben
// und gleichzeitig die Include-Guards (_WINDOWS_, _BASETSD_H_, ...) setzen.
// Ein spaeteres #include <windows.h> im globalen Scope waere dann wirkungslos
// und ::SSIZE_T & Co. waeren unbekannt (hat den MSVC-Build gebrochen).
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace rpg {

struct EngineConfig {
    static constexpr const char* VERSION = "0.2.0";
    static constexpr const char* NAME = "RPG Maker 3D Engine";
    static constexpr int DEFAULT_WIDTH = 1280;
    static constexpr int DEFAULT_HEIGHT = 720;
    static constexpr bool DEFAULT_FULLSCREEN = false;
    static constexpr bool DEFAULT_VSYNC = true;
    static constexpr int OPENGL_MAJOR = 3;
    static constexpr int OPENGL_MINOR = 3;

    // Pfade (plattform-neutral, Windows benutzt / trotzdem)
    static constexpr const char* ASSETS_DIR = "assets";
    static constexpr const char* PROJECT_FILE = "project.json";
    static constexpr const char* MAP_DIR = "maps";
    static constexpr const char* SCRIPT_DIR = "scripts";
    static constexpr const char* PREFAB_DIR = "prefabs";
    static constexpr const char* SHADER_DIR = "assets/shaders";
    static constexpr const char* TEXTURE_DIR = "assets/textures";
    static constexpr const char* AUDIO_DIR = "assets/audio";
    static constexpr const char* MODEL_DIR = "assets/models";
    static constexpr const char* SAVE_DIR = "saves";

    // Limits
    static constexpr int MAX_MAP_SIZE = 256;
    static constexpr int MAX_LAYERS = 8;
    static constexpr int MAX_EVENTS_PER_MAP = 999;
    static constexpr int MAX_SWITCHES = 5000;
    static constexpr int MAX_VARIABLES = 5000;
};

#ifdef _WIN32
#define RPG_PLATFORM_WINDOWS 1
#define RPG_PLATFORM_NAME "Windows"
#else
#define RPG_PLATFORM_WINDOWS 0
#ifdef __linux__
#define RPG_PLATFORM_LINUX 1
#define RPG_PLATFORM_NAME "Linux"
#else
#define RPG_PLATFORM_NAME "Unknown"
#endif
#endif

} // namespace rpg
