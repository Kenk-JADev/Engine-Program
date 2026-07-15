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

    auto create = [&](const std::string& filename, const std::string& content, bool core=true) {
        auto script = std::make_shared<Script>();
        script->name = filename;
        script->path = mScriptsDirectory + "/" + filename;
        script->isCore = core;
        script->modified = true;
        script->content = content;
        // Avoid duplicate if already exists
        bool exists = std::any_of(mScripts.begin(), mScripts.end(), [&](auto& s){ return s->name == filename; });
        if (!exists) {
            mScripts.push_back(script);
            SaveScript(script);
        }
    };

    // 00 - Config
    create("00_Config.rb", R"(# RPG Maker 3D - Config
# Vorinstallierte Default Scripts - werden im Game genutzt um Systeme in Scenes zu nutzen
module RPGMaker3D
  VERSION = "0.3.0"
  ENGINE = "RPG Maker 3D"
  module Config
    SCREEN_WIDTH = 1280
    SCREEN_HEIGHT = 720
    START_GOLD = 500
    START_MAP_ID = 1
  end
end
)");

    // 01 - Game Temp
    create("01_Game_Temp.rb", R"(# Game_Temp - Temporäre Daten
class Game_Temp
  attr_accessor :common_event_id, :fade_type
  def initialize
    @common_event_id = 0
    @fade_type = 0
  end
end
$game_temp = Game_Temp.new
)");

    // 02 - Game System
    create("02_Game_System.rb", R"(# Game_System
class Game_System
  attr_accessor :playtime, :save_count, :bgm, :bgs
  def initialize
    @playtime = 0
    @save_count = 0
  end
  def update(delta)
    @playtime += delta
  end
  def playtime_text
    secs = @playtime.to_i
    "%02d:%02d:%02d" % [secs / 3600, (secs % 3600) / 60, secs % 60]
  end
end
$game_system = Game_System.new
)");

    // 03 - Scene Base
    create("03_Scene_Base.rb", R"(# Scene_Base - Basis für alle Scenes
class Scene_Base
  def initialize
    @running = false
  end
  def main
    start
    @running = true
    while @running
      update
      break if scene_changing?
    end
    terminate
  end
  def start
    Engine.log("Scene #{self.class.name} start")
  end
  def update
  end
  def terminate
    Engine.log("Scene #{self.class.name} terminate")
  end
  def scene_changing?
    false
  end
  def goto_scene(scene_class)
    SceneManager.goto(scene_class)
  end
end
)");

    // 04 - SceneManager
    create("04_SceneManager.rb", R"(# SceneManager - Verwaltet Scene Stack
module SceneManager
  @stack = []
  @next_scene = nil
  def self.goto(scene_class)
    @next_scene = scene_class
  end
  def self.push(scene_class)
    @stack.push(@current_scene) if @current_scene
    @next_scene = scene_class
  end
  def self.pop
    @next_scene = @stack.pop
  end
  def self.current_scene
    @current_scene
  end
  def self.update
    if @next_scene
      @current_scene.terminate if @current_scene
      @current_scene = @next_scene.new
      @current_scene.start
      @next_scene = nil
    end
    @current_scene.update if @current_scene
  end
  def self.run(initial_scene)
    @current_scene = initial_scene.new
    @current_scene.start
  end
end
)");

    // 05 - Scene_Title
    create("05_Scene_Title.rb", R"(# Scene_Title
class Scene_Title < Scene_Base
  def start
    super
    UI.show_screen_text("RPG Maker 3D", 0.5, 0.3, 1.0, 0.8, 0.2, 0.0)
    UI.show_screen_text("Press Enter - New Game", 0.5, 0.5, 1.0, 1.0, 1.0, 0.0)
  end
  def update
    if Input.key_down?(:return)
      UI.clear_texts
      SceneManager.goto(Scene_Map)
    end
  end
  def terminate
    UI.clear_texts
    super
  end
end
)");

    // 06 - Scene_Map
    create("06_Scene_Map.rb", R"(# Scene_Map - Hauptspiel
class Scene_Map < Scene_Base
  def start
    super
    UI.show_screen_text("Gold: #{UI.gold}", 0.85, 0.05, 1.0, 0.9, 0.2, 0.0)
    UI.show_screen_text("WASD bewegen, E sprechen", 0.15, 0.95, 0.6, 0.8, 1.0, 0.0)
  end
  def update
  end
  def terminate
    UI.clear_texts
    super
  end
end
)");

    // 07 - Scene_Battle
    create("07_Scene_Battle.rb", R"(# Scene_Battle
class Scene_Battle < Scene_Base
  def start
    super
    UI.show_screen_text("Kampf gestartet!", 0.5, 0.3, 1.0, 0.3, 0.3, 3.0)
  end
  def update
  end
end
)");

    // 08 - Sprite Picture System (Screen Sprites)
    create("08_Sprite_Picture.rb", R"(# Sprite / Picture System - Screen Sprites wie im Original
class Game_Picture
  attr_accessor :name, :x, :y, :scale, :opacity, :visible
  def initialize(id)
    @id = id
    @name = ""
    @x = 0.5
    @y = 0.5
    @scale = 1.0
    @opacity = 1.0
    @visible = false
  end
  def show(filename, x=0.5, y=0.5, scale=1.0, opacity=0.9)
    @name = filename
    @x = x
    @y = y
    @scale = scale
    @opacity = opacity
    @visible = true
    UI.show_picture(filename, filename, x, y, scale, opacity)
  end
  def move(x, y, duration=1.0)
    @x = x
    @y = y
    UI.move_picture(@name, x, y)
  end
  def erase
    UI.remove_picture(@name)
    @visible = false
  end
end

class Game_Pictures
  def initialize
    @pictures = {}
  end
  def [](id)
    @pictures[id] ||= Game_Picture.new(id)
  end
  def clear
    UI.clear_pictures
    @pictures.clear
  end
end

$game_pictures = Game_Pictures.new

class ScreenSprite
  def self.show(filename, x=0.5, y=0.5, scale=1.0)
    $game_pictures[filename].show(filename, x, y, scale)
  end
  def self.hide(filename)
    $game_pictures[filename].erase
  end
end
)");

    // 09 - Window Base
    create("09_Window_Base.rb", R"(# Window_Base
class Window_Base
  attr_accessor :x, :y, :width, :height, :visible, :text
  def initialize(x, y, w, h)
    @x = x
    @y = y
    @width = w
    @height = h
    @visible = true
    @text = ""
  end
  def show_text(t, x=0.5, y=0.1, duration=3.0)
    UI.show_screen_text(t, x, y, 1.0, 1.0, 1.0, duration)
  end
end
)");

    // 10 - UI HUD
    create("10_UI_HUD.rb", R"(# UI HUD - Vorinstalliertes Default Script
module UI_HUD
  def self.show_gold_popup(amount)
    UI.show_world_text("Gold +#{amount}", 0, 1.5, 0, 1.0, 0.9, 0.2, 2.5)
  end
  def self.show_damage(x, y, z, damage, critical=false)
    r = critical ? 1.0 : 1.0
    g = critical ? 0.8 : 0.2
    b = critical ? 0.0 : 0.2
    UI.show_world_text(critical ? "CRITICAL #{damage}!" : "#{damage}", x, y + 1.0, z, r, g, b, 1.5)
  end
  def self.show_quest(text)
    UI.show_screen_text(text, 0.5, 0.15, 1.0, 0.9, 0.3, 4.0)
  end
  def self.show_tutorial(text)
    UI.show_screen_text("[T] #{text}", 0.5, 0.85, 0.4, 0.8, 1.0, 5.0)
  end
  def self.clear
    UI.clear_texts
    UI.clear_pictures
  end
end
)");

    // 11 - Custom Systems
    create("11_Custom_Systems.rb", R"(# Custom Systeme - Beispiel wie im Original Custom möglich
class QuestSystem
  def initialize
    @quests = {}
  end
  def add_quest(id, name, desc)
    @quests[id] = {name: name, desc: desc, completed: false}
    UI_HUD.show_quest("Neue Quest: #{name}")
  end
  def complete_quest(id)
    if @quests[id]
      @quests[id][:completed] = true
      UI_HUD.show_quest("Quest abgeschlossen: #{@quests[id][:name]}")
      UI.show_world_text("Quest Complete!", 0, 2, 0, 0.2, 1.0, 0.2, 3.0)
    end
  end
end

$quest_system = QuestSystem.new

class AchievementSystem
  def initialize
    @achievements = {}
  end
  def unlock(name)
    return if @achievements[name]
    @achievements[name] = true
    UI.show_screen_text("Achievement: #{name}!", 0.5, 0.2, 1.0, 0.8, 0.2, 4.0)
  end
end

$achievements = AchievementSystem.new
)");

    // 12 - Main
    create("main.rb", R"(# RPG Maker 3D - Hauptspielskript
# Nutzt vorinstallierte Default Scripts (Scenes, Sprites, etc.)

Engine.log("RPG Maker 3D v#{RPGMaker3D::VERSION} gestartet")

class Game
  def initialize
    @player = Actor.new("Hero")
    @player.move_to(0, 0, 0)
    @time = 0.0
    SceneManager.run(Scene_Title)
  end

  def update(delta_time)
    @time += delta_time
    SceneManager.update
    $game_system.update(delta_time)

    if Input.key_down?(:h)
      UI_HUD.show_tutorial("WASD bewegen, E sprechen, Q/W/E/R Gizmo, F Fokus")
    end
  end

  def on_gold_gained(amount)
    UI_HUD.show_gold_popup(amount)
  end
end

$game = Game.new

def show_tutorial(text)
  UI_HUD.show_tutorial(text)
end

def show_quest(text)
  UI_HUD.show_quest(text)
end

def show_floating_text(text, x, y, z, r=1.0, g=1.0, b=0.0)
  UI.show_world_text(text, x, y, z, r, g, b, 2.5)
end

def show_picture(filename, x=0.5, y=0.5, scale=1.0)
  ScreenSprite.show(filename, x, y, scale)
end
)", false);

    RPG_LOG_INFO("Created default scripts in: " + mScriptsDirectory);
}

const std::vector<std::shared_ptr<ScriptManager::Script>>& ScriptManager::GetScripts() const {
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
                script->name = entry.path().filename().string();
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
        // Sort by name to ensure load order 00_, 01_, etc.
        std::sort(mScripts.begin(), mScripts.end(), [](auto& a, auto& b){ return a->name < b->name; });
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
