#include "rpgmaker3d/Project.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <cctype>

namespace rpg {

Project::Project() = default;

namespace {

std::string EscapeJSON(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
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

std::string Trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::string ParseJSONString(const std::string& line, size_t valueStart) {
    std::string value = line.substr(valueStart);
    value = Trim(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

int ParseJSONInt(const std::string& line, size_t valueStart) {
    std::string value = Trim(line.substr(valueStart));
    // Remove trailing comma
    if (!value.empty() && value.back() == ',') value.pop_back();
    try { return std::stoi(value); } catch (...) { return 0; }
}

bool ParseJSONBool(const std::string& line, size_t valueStart) {
    std::string value = Trim(line.substr(valueStart));
    if (!value.empty() && value.back() == ',') value.pop_back();
    return value == "true";
}

} // anonymous namespace

bool Project::New(const std::string& path, const std::string& name) {
    mProjectPath = path;
    mInfo.name = name;

    std::filesystem::create_directories(path);
    std::filesystem::create_directories(path + "/assets/textures");
    std::filesystem::create_directories(path + "/assets/models");
    std::filesystem::create_directories(path + "/assets/audio");
    std::filesystem::create_directories(path + "/Audio/BGM");
    std::filesystem::create_directories(path + "/Audio/BGS");
    std::filesystem::create_directories(path + "/Audio/ME");
    std::filesystem::create_directories(path + "/Audio/SE");
    std::filesystem::create_directories(path + "/assets/shaders");
    std::filesystem::create_directories(path + "/maps");
    std::filesystem::create_directories(path + "/scripts");
    std::filesystem::create_directories(path + "/prefabs");
    // XP-Ordnerstruktur (Material-Kategorien des Importdialogs /
    // RPG::Cache-Suchpfade): neue Projekte legen sie gleich mit an,
    // damit der Asset-Browser-Import sie vorfindet.
    static const char* kGfxDirs[] = {
        "Tilesets", "Autotiles", "Characters", "Animations", "Battlers",
        "Battlebacks", "Panoramas", "Fogs", "Pictures", "Titles",
        "Gameovers", "Icons", "Transitions", "System", "Windowskins", "Faces"
    };
    for (const char* d : kGfxDirs)
        std::filesystem::create_directories(path + "/Graphics/" + d);

    Save();
    return true;
}

bool Project::Load(const std::string& path) {
    mProjectPath = path;
    std::ifstream file(path + "/project.json");
    if (!file.is_open()) {
        std::cerr << "Project not found: " << path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line.front() == '{' || line.front() == '}') continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = Trim(line.substr(0, colon));
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') {
            key = key.substr(1, key.size() - 2);
        }
        size_t valueStart = colon + 1;

        if (key == "name") mInfo.name = ParseJSONString(line, valueStart);
        else if (key == "author") mInfo.author = ParseJSONString(line, valueStart);
        else if (key == "version") mInfo.version = ParseJSONString(line, valueStart);
        else if (key == "startMapId") mInfo.startMapId = ParseJSONInt(line, valueStart);
        else if (key == "startX") mInfo.startX = ParseJSONInt(line, valueStart);
        else if (key == "startY") mInfo.startY = ParseJSONInt(line, valueStart);
        else if (key == "resolutionWidth") mInfo.resolutionWidth = ParseJSONInt(line, valueStart);
        else if (key == "resolutionHeight") mInfo.resolutionHeight = ParseJSONInt(line, valueStart);
        else if (key == "fullscreen") mInfo.fullscreen = ParseJSONBool(line, valueStart);
        else if (key == "vsync") mInfo.vsync = ParseJSONBool(line, valueStart);
    }

    return true;
}

bool Project::Save() const {
    std::ofstream file(mProjectPath + "/project.json");
    if (!file.is_open()) return false;
    file << "{\n";
    file << "  \"name\": \"" << EscapeJSON(mInfo.name) << "\",\n";
    file << "  \"author\": \"" << EscapeJSON(mInfo.author) << "\",\n";
    file << "  \"version\": \"" << EscapeJSON(mInfo.version) << "\",\n";
    file << "  \"startMapId\": " << mInfo.startMapId << ",\n";
    file << "  \"startX\": " << mInfo.startX << ",\n";
    file << "  \"startY\": " << mInfo.startY << ",\n";
    file << "  \"resolutionWidth\": " << mInfo.resolutionWidth << ",\n";
    file << "  \"resolutionHeight\": " << mInfo.resolutionHeight << ",\n";
    file << "  \"fullscreen\": " << (mInfo.fullscreen ? "true" : "false") << ",\n";
    file << "  \"vsync\": " << (mInfo.vsync ? "true" : "false") << "\n";
    file << "}\n";
    return true;
}

bool Project::SaveAs(const std::string& path) {
    if (path.empty()) return false;

    // Create destination structure and update project path
    mProjectPath = path;
    std::filesystem::create_directories(path);
    std::filesystem::create_directories(path + "/assets/textures");
    std::filesystem::create_directories(path + "/assets/models");
    std::filesystem::create_directories(path + "/assets/audio");
    std::filesystem::create_directories(path + "/Audio/BGM");
    std::filesystem::create_directories(path + "/Audio/BGS");
    std::filesystem::create_directories(path + "/Audio/ME");
    std::filesystem::create_directories(path + "/Audio/SE");
    std::filesystem::create_directories(path + "/assets/shaders");
    std::filesystem::create_directories(path + "/maps");
    std::filesystem::create_directories(path + "/scripts");
    std::filesystem::create_directories(path + "/prefabs");

    return Save();
}

std::string Project::GetAssetPath(const std::string& subPath) const {
    return mProjectPath + "/assets/" + subPath;
}

std::string Project::GetScriptPath(const std::string& subPath) const {
    return mProjectPath + "/scripts/" + subPath;
}

std::string Project::GetMapPath(int mapId) const {
    return mProjectPath + "/maps/map" + std::to_string(mapId) + ".map";
}

std::string Project::GetEngineVersion() {
    return "0.1.0";
}

} // namespace rpg
