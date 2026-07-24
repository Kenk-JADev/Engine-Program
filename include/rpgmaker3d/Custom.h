#pragma once
// RPG Maker 3D - "Alles custom"-Schalter
//
// RPG-Maker-XP-Philosophie: Die Engine liefert fertige Standard-Oberflaechen
// (Titel, HUD, Spielmenue, Kampfmenue — alles GameUI-ImGui-Overlay bzw.
// RGSS-Fenster; RmlUi ist mit PAKET 10 entfernt), aber ein Spiel darf JEDE
// davon durch eigene Ruby-Szenen ersetzen. Dazu dient die Datei
// <Projekt>/Game.ini (XP-Nostalgie):
//
//   [RPG Maker 3D]
//   NativeTitle=1        ; 0 = kein eingebauter Titel -> Ruby-Hook
//                          "Game.custom_title" (oder direkter Spielstart)
//   NativeHud=1          ; 0 = HUD beim Start aus (UI.hud_visible= steuert, F9)
//   NativeGameMenu=1     ; 0 = Esc oeffnet NICHT das eingebaute Spielmenue
//   NativeBattleMenu=1   ; 0 = kein eingebautes Kampfmenue (Custom via Battle-API)
//   NativeBattleStatus=1 ; 0 = keine eingebaute Gegner-/Gruppenzeile im Kampf
//   NativeMessage=1      ; 0 = Standard-Dialoge (101 Text, 102 Auswahl,
//                          103 Zahl, 303 Name) gehen NICHT ans eingebaute
//                          Fenster, sondern an Ruby-Hooks Game.on_ui_*
//                          (PAKET 42: Referenz-Skript 18_System_Message.rb)
//   XpSceneMode=0        ; 1 = XP-Szenen-Framework aktiv: die Engine tickt
//                          pro Frame $scene.__engine_frame (Scene_Base aus dem
//                          RGSS-Prelude) — Opt-in, Default bleibt nativ.
//
// Die gleichen Flags sind zur Laufzeit aus Ruby schreibbar:
//   UI.native_battle_menu = false   usw. (siehe README "Alles custom")

#include <string>

namespace rpg {

class CustomConfig {
public:
    static CustomConfig& Get();

    bool nativeTitle = true;
    bool nativeHud = true;
    bool nativeGameMenu = true;
    bool nativeBattleMenu = true;
    bool nativeBattleStatus = true;
    bool nativeMessage = true;   // PAKET 42: Standard-Dialoge nativ (Script-Hooks Game.on_ui_* wenn false)
    /// XP-Szenen-Framework (Opt-in, PAKET 6/h): pro Frame $scene.__engine_frame
    /// (Scene_Base-Tick: start -> update -> terminate). Default AUS.
    bool xpSceneMode = false;

    /// Alles auf Standard (eingebaute Oberflaechen aktiv)
    void Reset();

    /// Liest <Projekt>/Game.ini (fehlende Datei = Defaults, kein Fehler).
    /// Idempotent - darf bei jedem Projekt-/Spielstart erneut laufen.
    void LoadFromProject(const std::string& projectPath);

private:
    CustomConfig() = default;
};

} // namespace rpg
