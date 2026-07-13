#include "rpgmaker3d/ScriptManager.h"
#include "rpgmaker3d/RubyVM.h"
#include "rpgmaker3d/Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace rpg {

ScriptManager::ScriptManager() {
}

ScriptManager::~ScriptManager() {
    Shutdown();
}

void ScriptManager::Initialize() {
    mScriptsDirectory = std::filesystem::current_path().string() + "/scripts";
    std::filesystem::create_directories(mScriptsDirectory);
    ReloadFromDisk();
}

void ScriptManager::Shutdown() {
    SaveAllScripts();
    mScripts.clear();
    mRubyVM = nullptr;
}

void ScriptManager::SetRubyVM(RubyVM* vm) {
    mRubyVM = vm;
}

void ScriptManager::LoadProjectScripts(const std::string& projectPath) {
    mScriptsDirectory = projectPath + "/scripts";
    std::filesystem::create_directories(mScriptsDirectory);
    ReloadFromDisk();
    RPG_LOG_INFO("Loaded project scripts from: " + mScriptsDirectory);
}

void ScriptManager::CreateDefaultScripts(const std::string& projectPath) {
    mScriptsDirectory = projectPath + "/scripts";
    std::filesystem::create_directories(mScriptsDirectory);

    // Main game script
    {
        auto script = std::make_shared<Script>();
        script->name = "main.rb";
        script->path = mScriptsDirectory + "/main.rb";
        script->isCore = true;
        script->modified = true;
        script->content =
            "# RPG Maker 3D - Main Script\n"
            "# This script is executed when the game starts.\n\n"
            "puts \"RPG Maker 3D game started!\"\n\n"
            "def on_update(dt)\n"
            "  # Called every frame\n"
            "end\n";
        mScripts.push_back(script);
        SaveScript(script);
    }

    // Game helpers
    {
        auto script = std::make_shared<Script>();
        script->name = "game.rb";
        script->path = mScriptsDirectory + "/game.rb";
        script->isCore = true;
        script->modified = true;
        script->content =
            "# RPG Maker 3D - Game Helpers\n\n"
            "module GameHelpers\n"
            "  def self.hello\n"
            "    puts \"Hello from GameHelpers!\"\n"
            "  end\n"
            "end\n";
        mScripts.push_back(script);
        SaveScript(script);
    }

    RPG_LOG_INFO("Created default scripts in: " + mScriptsDirectory);
}

std::vector<std::shared_ptr<ScriptManager::Script>> ScriptManager::GetScripts() const {
    return mScripts;
}

std::shared_ptr<ScriptManager::Script> ScriptManager::CreateScript(const std::string& name) {
    std::string path = mScriptsDirectory + "/" + name;
    
    auto script = std::make_shared<Script>();
    script->name = name;
    script->path = path;
    script->content = "# " + name + "\n\n";
    script->modified = true;
    script->isCore = false;
    
    mScripts.push_back(script);
    RPG_LOG_INFO("Created script: " + name);
    return script;
}

void ScriptManager::DeleteScript(const std::string& name) {
    auto it = std::find_if(mScripts.begin(), mScripts.end(),
        [&name](const auto& s) { return s->name == name; });
    
    if (it != mScripts.end()) {
        if (!(*it)->isCore) {
            std::filesystem::remove((*it)->path);
            mScripts.erase(it);
            RPG_LOG_INFO("Deleted script: " + name);
        } else {
            RPG_LOG_WARN("Cannot delete core script: " + name);
        }
    }
}

bool ScriptManager::SaveScript(std::shared_ptr<Script> script) {
    if (!script) return false;
    
    std::ofstream file(script->path);
    if (!file.is_open()) {
        RPG_LOG_ERROR("Failed to open script for writing: " + script->path);
        return false;
    }
    
    file << script->content;
    file.close();
    
    script->modified = false;
    RPG_LOG_INFO("Saved script: " + script->name);
    return true;
}

void ScriptManager::SaveAllScripts() {
    for (auto& script : mScripts) {
        if (script->modified) {
            SaveScript(script);
        }
    }
}

void ScriptManager::ReloadFromDisk() {
    mScripts.clear();
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(mScriptsDirectory)) {
            if (entry.path().extension() == ".rb") {
                auto script = std::make_shared<Script>();
                script->name = entry.path().stem().string() + ".rb";
                script->path = entry.path().string();
                script->isCore = false;
                
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    script->content = buffer.str();
                    file.close();
                }
                
                mScripts.push_back(script);
                RPG_LOG_INFO("Loaded script: " + script->name);
            }
        }
    } catch (...) {
        RPG_LOG_WARN("Could not load scripts from directory");
    }
}

void ScriptManager::ExecuteAllScripts() {
    for (auto& script : mScripts) {
        RPG_LOG_INFO("Execute script: " + script->name);
        if (mRubyVM) {
            if (!script->path.empty() && std::filesystem::exists(script->path)) {
                mRubyVM->ExecuteFile(script->path);
            } else if (!script->content.empty()) {
                mRubyVM->ExecuteString(script->content);
            }
        }
    }
}

std::string ScriptManager::GetScriptsDirectory() const {
    return mScriptsDirectory;
}

} // namespace rpg