#include "rpgmaker3d/Project.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace rpg {

Project::Project() = default;

bool Project::New(const std::string& path, const std::string& name) {
    mProjectPath = path;
    mInfo.name = name;

    std::filesystem::create_directories(path);
    std::filesystem::create_directories(path + "/assets/textures");
    std::filesystem::create_directories(path + "/assets/models");
    std::filesystem::create_directories(path + "/assets/audio");
    std::filesystem::create_directories(path + "/assets/shaders");
    std::filesystem::create_directories(path + "/maps");
    std::filesystem::create_directories(path + "/scripts");

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
    // Einfache JSON-Parsing würde hier erfolgen; Stub belässt Defaults.
    return true;
}

bool Project::Save() const {
    std::ofstream file(mProjectPath + "/project.json");
    if (!file.is_open()) return false;
    file << "{\n";
    file << "  \"name\": \"" << mInfo.name << "\",\n";
    file << "  \"author\": \"" << mInfo.author << "\",\n";
    file << "  \"version\": \"" << mInfo.version << "\",\n";
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
