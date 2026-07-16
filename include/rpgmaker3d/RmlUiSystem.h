#pragma once
// RPG Maker 3D - RmlUi Integration
// Zwei getrennte UI-Kontexte: "editor" und "game".
// Input: SDL (Player) oder Qt-Bruecke (Editor).
//
// WICHTIG zur Script-Pipeline:
//   Ruby (UI.show_*)  ->  GameUI (Logik)
//                     ->  RmlUi (Darstellung im Game-Fenster)
// RmlUi hat KEIN eigenes Ruby-Binding. Scripts sprechen immer GameUI an;
// RmlUi spiegelt Message/HUD/ScreenTexts daraus.

#include <memory>
#include <string>

union SDL_Event;

namespace rpg {

class Engine;
class RmlUiSystemImpl;

class RmlUiSystem {
public:
    RmlUiSystem();
    ~RmlUiSystem();

    bool Initialize(Engine* engine);
    void Shutdown();

    bool ProcessEvent(const SDL_Event& e);

    bool ProcessMouseMove(int x, int y, int modifiers = 0);
    bool ProcessMouseButton(int button /*0=L,1=M,2=R*/, bool down, int modifiers = 0);
    bool ProcessMouseWheel(float deltaY, int modifiers = 0);
    bool ProcessKeyQt(int qtKey, bool down, int modifiers = 0);
    bool ProcessTextInput(const std::string& utf8);
    void NotifyViewport(int width, int height);

    void Update(float dt);
    void Render();

    bool IsVisible() const;
    void SetVisible(bool visible);
    void ToggleVisible();

    // Von GameUI / Engine: Spiel-Daten in RmlUi-HUD spiegeln
    void SyncFromGameUI();

private:
    std::unique_ptr<RmlUiSystemImpl> m;
};

} // namespace rpg
