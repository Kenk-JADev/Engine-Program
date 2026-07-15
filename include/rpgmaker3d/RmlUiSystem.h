#pragma once
// RPG Maker 3D - RmlUi Integration (PoC)
// Zwei getrennte UI-Kontexte: "editor" (Editor-Oberflaeche) und "game" (Playtest/Player HUD).
// Langfristiges Ziel: ImGui komplett ersetzen. Aktuell laeuft beides parallel (F9 toggelt RmlUi).

#include <memory>

union SDL_Event;

namespace rpg {

class Engine;
class RmlUiSystemImpl;

class RmlUiSystem {
public:
    RmlUiSystem();
    ~RmlUiSystem();

    // Benoetigt: Fenster + GL-Kontext bereits erstellt (nach Renderer::Initialize aufrufen).
    bool Initialize(Engine* engine);
    void Shutdown();

    // SDL-Events einspeisen; Rueckgabe: true, wenn die UI das Event konsumiert hat.
    bool ProcessEvent(const SDL_Event& e);

    // SDL-freie Eingabe-Injection (Qt-Editor / externe Hosts ohne SDL-Event-Loop).
    // Rueckgabe: true, wenn die UI das Event konsumiert hat (Host sollte es dann
    // NICHT mehr in die 3D-Welt geben - wie ImGui WantCaptureMouse).
    // rmlKeyMods: Bitmaske aus Rml::Input::KeyModifier (KM_CTRL/KM_SHIFT/...).
    // rmlButton: Reihenfolge wie RmlUi/SDL: 0=Links, 1=Mitte, 2=Rechts.
    bool InjectMouseMove(int x, int y, int rmlKeyMods = 0);
    bool InjectMouseButton(int rmlButton, bool down, int rmlKeyMods = 0);
    bool InjectMouseWheel(float delta, int rmlKeyMods = 0);
    bool InjectKey(int rmlKeyId, bool down, int rmlKeyMods = 0);
    bool InjectText(const char* utf8);
    // Fenstergroesse nachziehen (View wurde resized; Kontext-Dimensionen+Viewport)
    void SetContextSize(int width, int height);

    void Update(float dt);
    void Render();

    bool IsVisible() const;
    void SetVisible(bool visible);
    void ToggleVisible();

private:
    std::unique_ptr<RmlUiSystemImpl> m;
};

} // namespace rpg
