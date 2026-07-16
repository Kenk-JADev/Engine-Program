#pragma once
// RPG Maker 3D - RmlUi Integration
// Zwei getrennte UI-Kontexte: "editor" und "game".
// Input: SDL (Player) oder Qt-Bruecke (Editor).

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

    // Benoetigt: Fenster + GL-Kontext bereits erstellt (nach Renderer::Initialize).
    bool Initialize(Engine* engine);
    void Shutdown();

    // SDL-Events einspeisen; true wenn die UI das Event konsumiert hat.
    bool ProcessEvent(const SDL_Event& e);

    // ---- Qt-Editor Input-Bruecke (kein SDL-Event noetig) ----
    // modifiers: Bitmaske wie Rml::Input::KeyModifier (Ctrl=1, Shift=2, Alt=4, Meta=8)
    // oder 0; wir akzeptieren auch Qt::KeyboardModifiers-kompatible Werte via Map.
    bool ProcessMouseMove(int x, int y, int modifiers = 0);
    bool ProcessMouseButton(int button /*0=L,1=M,2=R*/, bool down, int modifiers = 0);
    bool ProcessMouseWheel(float deltaY, int modifiers = 0);
    // qtKey: Qt::Key enum Wert (z.B. 0x41 = 'A', 0x01000000 = Escape, ...)
    bool ProcessKeyQt(int qtKey, bool down, int modifiers = 0);
    bool ProcessTextInput(const std::string& utf8);
    void NotifyViewport(int width, int height);

    void Update(float dt);
    void Render();

    bool IsVisible() const;
    void SetVisible(bool visible);
    void ToggleVisible();

private:
    std::unique_ptr<RmlUiSystemImpl> m;
};

} // namespace rpg
