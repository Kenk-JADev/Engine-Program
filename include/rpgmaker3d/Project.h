#pragma once

#include <string>
#include <vector>
#include "Types.h"

namespace rpg {

struct ProjectInfo {
    std::string name = "Untitled RPG";
    std::string author = "Unknown";
    std::string version = "0.1.0";
    int startMapId = 1;
    int startX = 0;
    int startY = 0;
    int resolutionWidth = 1280;
    int resolutionHeight = 720;
    bool fullscreen = false;
    bool vsync = true;
};

class Project {
public:
    Project();

    bool New(const std::string& path, const std::string& name);
    bool Load(const std::string& path);
    bool Save() const;

    const std::string& GetProjectPath() const { return mProjectPath; }
    ProjectInfo& GetInfo() { return mInfo; }
    const ProjectInfo& GetInfo() const { return mInfo; }

    std::string GetAssetPath(const std::string& subPath) const;
    std::string GetScriptPath(const std::string& subPath) const;
    std::string GetMapPath(int mapId) const;

    static std::string GetEngineVersion();

private:
    std::string mProjectPath;
    ProjectInfo mInfo;
};

} // namespace rpg
