#include "rpgmaker3d/Custom.h"
#include "rpgmaker3d/Logger.h"
#include <fstream>
#include <algorithm>
#include <cctype>

namespace rpg {

CustomConfig& CustomConfig::Get() {
    static CustomConfig instance;
    return instance;
}

void CustomConfig::Reset() {
    nativeTitle = true;
    nativeHud = true;
    nativeGameMenu = true;
    nativeBattleMenu = true;
    nativeBattleStatus = true;
    nativeMessage = true; // PAKET 42
    xpSceneMode = false; // XP-Szenen-Framework ist Opt-in
}

void CustomConfig::LoadFromProject(const std::string& projectPath) {
    Reset(); // Projekte koennen sich unterscheiden - immer von Defaults ausgehen
    if (projectPath.empty()) return;

    const std::string path = projectPath + "/Game.ini";
    std::ifstream f(path);
    if (!f) return; // keine Game.ini -> alles eingebaut (XP-Standard)

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    };
    auto trim = [](std::string& s) {
        while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
        while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    };
    auto parseBool = [&](std::string v, bool fallback) {
        trim(v);
        v = lower(v);
        if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
        if (v == "0" || v == "false" || v == "no" || v == "off") return false;
        return fallback;
    };

    std::string line;
    while (std::getline(f, line)) {
        trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[')
            continue; // Kommentar/Leerzeile/Sektion
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = lower(line.substr(0, eq));
        trim(key);
        std::string value = line.substr(eq + 1);
        // Inline-Kommentare abschneiden ("NativeTitle=0 ; eigener Titel")
        const size_t cmt = value.find(';');
        if (cmt != std::string::npos) value = value.substr(0, cmt);

        if (key == "nativetitle")       nativeTitle = parseBool(value, nativeTitle);
        else if (key == "nativehud")    nativeHud = parseBool(value, nativeHud);
        else if (key == "nativegamemenu")    nativeGameMenu = parseBool(value, nativeGameMenu);
        else if (key == "nativebattlemenu")  nativeBattleMenu = parseBool(value, nativeBattleMenu);
        else if (key == "nativebattlestatus") nativeBattleStatus = parseBool(value, nativeBattleStatus);
        else if (key == "nativemessage")     nativeMessage = parseBool(value, nativeMessage); // PAKET 42
        else if (key == "xpscenemode")        xpSceneMode = parseBool(value, xpSceneMode);
    }

    RPG_LOG_INFO("[Custom] Game.ini gelesen: Title=" + std::to_string(nativeTitle) +
                 " Hud=" + std::to_string(nativeHud) +
                 " GameMenu=" + std::to_string(nativeGameMenu) +
                 " BattleMenu=" + std::to_string(nativeBattleMenu) +
                 " BattleStatus=" + std::to_string(nativeBattleStatus) +
                 " Message=" + std::to_string(nativeMessage) +
                 " XpSceneMode=" + std::to_string(xpSceneMode));
}

} // namespace rpg
