#include "rpgmaker3d/Prefab.h"
#include "rpgmaker3d/Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace rpg {

std::string Prefab::GetPrefabDirectory() {
    return "./SampleProject/prefabs";
}

bool Prefab::Save(const std::string& path, const PrefabData& data) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream file(path);
    if (!file.is_open()) {
        RPG_LOG_ERROR("Failed to save prefab: " + path);
        return false;
    }

    file << "[name]\n" << data.name << "\n";
    file << "[transform]\n";
    file << "position=" << data.position.x << "," << data.position.y << "," << data.position.z << "\n";
    file << "rotation=" << data.rotation.x << "," << data.rotation.y << "," << data.rotation.z << "\n";
    file << "scale=" << data.scale.x << "," << data.scale.y << "," << data.scale.z << "\n";
    file << "[model]\n";
    file << "enabled=" << (data.hasModel ? "1" : "0") << "\n";
    file << "type=" << data.modelType << "\n";
    file << "color=" << data.color.r << "," << data.color.g << "," << data.color.b << "," << data.color.a << "\n";
    file << "[light]\n";
    file << "enabled=" << (data.hasLight ? "1" : "0") << "\n";
    file << "color=" << data.lightColor.r << "," << data.lightColor.g << "," << data.lightColor.b << "," << data.lightColor.a << "\n";
    file << "intensity=" << data.lightIntensity << "\n";

    return true;
}

static Vec3 ParseVec3(const std::string& s) {
    Vec3 v{0.0f};
    std::stringstream ss(s);
    char sep;
    ss >> v.x >> sep >> v.y >> sep >> v.z;
    return v;
}

static Vec4 ParseVec4(const std::string& s) {
    Vec4 v{0.0f};
    std::stringstream ss(s);
    char sep;
    ss >> v.x >> sep >> v.y >> sep >> v.z >> sep >> v.w;
    return v;
}

bool Prefab::Load(const std::string& path, PrefabData& data) {
    std::ifstream file(path);
    if (!file.is_open()) {
        RPG_LOG_ERROR("Failed to load prefab: " + path);
        return false;
    }

    std::string line;
    std::string section;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            if (section == "name") data.name = line;
            continue;
        }

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (section == "transform") {
            if (key == "position") data.position = ParseVec3(value);
            else if (key == "rotation") data.rotation = ParseVec3(value);
            else if (key == "scale") data.scale = ParseVec3(value);
        } else if (section == "model") {
            if (key == "enabled") data.hasModel = (value == "1");
            else if (key == "type") data.modelType = value;
            else if (key == "color") data.color = ParseVec4(value);
        } else if (section == "light") {
            if (key == "enabled") data.hasLight = (value == "1");
            else if (key == "color") data.lightColor = ParseVec4(value);
            else if (key == "intensity") data.lightIntensity = std::stof(value);
        }
    }

    return true;
}

} // namespace rpg
