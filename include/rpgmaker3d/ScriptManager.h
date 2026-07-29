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
    /// Fuehrt alle Skripte aus, falls seit dem letzten Laden/
    /// Invalidate noch nicht geschehen (Custom-Titel: Skripte laufen schon
    /// vor SetPlaying(true), damit Game.custom_title definiert ist).
    void ExecuteAllScriptsOnce();
    /// Setzt den Executed-Merker zurueck (Playtest-Stopp -> naechster Start
    /// fuehrt die Skripte erneut aus).
    void InvalidateExecutedScripts();

    /// Prueft ALLE .rb-Dateien einmal per Ruby-Parser auf Syntaxfehler,
    /// ohne sie auszufuehren (Start-Pruefung vor dem Spielstart).
    /// errors erhaelt pro defektem Skript einen Eintrag
    /// "<datei>:<zeile>: <meldung>". true = alle Skripte ok.
    bool ValidateAllScripts(std::vector<std::string>& errors);

    // ---- SADS Kap. 17: Plugin-Metadaten (Ruby-only Plugins) -------------
    // Konvention im Kommentarkopf einer plugins/*.rb-Datei:
    //   # @name    Mein Plugin
    //   # @version 1.0
    //   # @author  Max
    //   # @desc    Was das Plugin tut (eine Zeile)
    // Fehlende Felder bleiben leer; der Dateiname ist der Fallback-Name.
    struct PluginInfo {
        std::string file;     // "plugins/foo.rb"
        std::string name;     // @name oder Dateiname
        std::string version;
        std::string author;
        std::string desc;
    };
    /// Liste aller geladenen Plugins mit Metadaten (fuer Editor-Liste/Log).
    const std::vector<PluginInfo>& GetPluginInfos() const { return mPluginInfos; }

    std::string GetScriptsDirectory() const;

private:
    std::vector<std::shared_ptr<Script>> mScripts;
    std::vector<PluginInfo> mPluginInfos;
    std::string mScriptsDirectory;
    RubyVM* mRubyVM = nullptr;
    bool mAllScriptsExecuted = false; // ExecuteAllScriptsOnce-Guard
};

} // namespace rpg