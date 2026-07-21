#pragma once

#include <string>

struct mrb_state;

typedef unsigned int GLuint;

namespace rpg {

class Engine;

class RubyVM {
public:
    RubyVM();
    ~RubyVM();

    bool Initialize(Engine* engine);
    void Shutdown();

    // sourceName: z.B. "06_Scene_Map.rb" fuer Fehlermeldungen
    bool ExecuteString(const std::string& code, const std::string& sourceName = "<string>");
    bool ExecuteFile(const std::string& path);
    bool Update(float deltaTime);

    // Prueft Ruby-Code NUR auf Syntaxfehler (Parser), OHNE ihn auszufuehren.
    // Dient der Start-Pruefung aller .rb-Dateien (Player/Playtest), damit
    // Tippfehler sofort mit Datei + Zeile gemeldet werden, statt dass das
    // Spiel mitten im Lauf crasht. true = Syntax ok; bei false steht der
    // Fehler inkl. Zeile in errorOut.
    bool CheckSyntax(const std::string& code, const std::string& sourceName,
                     std::string& errorOut);

    // Erzwingt einen vollstaendigen Garbage-Collection-Durchlauf. Nuetzlich
    // vor wiederholtem Script-Reload (Playtest), damit tote Ruby-Objekte
    // (z.B. neu zugewiesene $game/$game_pictures aus vorigen Durchlaeufen)
    // eingesammelt werden und der kleine mruby-Heap nicht ueberlaeuft
    // (NoMemoryError bei vielen Playtest-Durchlaeufen).
    void CollectGarbage();

    // ---------- Custom-Hooks ("alles custom") ----------
    /// Ruft die (Modul-)Methode Game.<name> auf, falls das Spiel sie definiert
    /// hat (z. B. "custom_title" fuer einen eigenen Titelbildschirm).
    /// Rueckgabe: true wenn die Methode existiert und aufgerufen wurde.
    bool CallGameHook(const std::string& name);
    /// Interne Bruecke fuer UI.open_list_menu: ruft den per Block
    /// uebergebenen Ruby-Callback mit dem gewaehlten Index (-1 = Abbruch).
    void CallListMenuBlock(int index);

    mrb_state* GetState() { return mMrb; }

    // Letzter Ruby-Fehler (leer wenn ok)
    const std::string& GetLastError() const { return mLastError; }
    bool HasError() const { return !mLastError.empty(); }
    void ClearError() { mLastError.clear(); }

private:
    void BindEngine();
    void BindInput();
    void BindAudio();
    void BindMap();
    void BindActor();
    void BindCamera();
    void BindGame();
    void BindUI();

    // Schreibt Exception-Text nach mLastError und loggt
    bool CaptureException(const std::string& context);

    mrb_state* mMrb = nullptr;
    Engine* mEngine = nullptr;
    std::string mLastError;
};

} // namespace rpg
