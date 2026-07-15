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

    void Update(float dt);
    void Render();

    bool IsVisible() const;
    void SetVisible(bool visible);
    void ToggleVisible();

private:
    std::unique_ptr<RmlUiSystemImpl> m;
};

} // namespace rpg
