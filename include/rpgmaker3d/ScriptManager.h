#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace rpg {

class RubyVM;

class ScriptManager {
public:
    struct Script {
        std::string name;
        std::string path;
        std::string content;
        bool modified = false;
        bool isCore = false;
    };

    ScriptManager();
    ~ScriptManager();

    void Initialize();
    void Shutdown();

    void SetRubyVM(RubyVM* vm);
    void LoadProjectScripts(const std::string& projectPath);
    void CreateDefaultScripts(const std::string& projectPath);

    const std::vector<std::shared_ptr<Script>>& GetScripts() const;
    std::shared_ptr<Script> CreateScript(const std::string& name);
    void DeleteScript(const std::string& name);
    bool SaveScript(std::shared_ptr<Script> script);
    void SaveAllScripts();
    void ReloadFromDisk();
    void ExecuteAllScripts();

    std::string GetScriptsDirectory() const;

private:
    std::vector<std::shared_ptr<Script>> mScripts;
    std::string mScriptsDirectory;
    RubyVM* mRubyVM = nullptr;
};

} // namespace rpg