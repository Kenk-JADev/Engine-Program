#pragma once
// RPG Maker 3D - "Alles custom"-Schalter
//
// RPG-Maker-XP-Philosophie: Die Engine liefert fertige Standard-Oberflaechen
// (Titel, HUD, Spielmenue, Kampfmenue), aber ein Spiel darf JEDE davon durch
// eigene Ruby-Szenen / RmlUi-Skins ersetzen. Dazu dient die Datei
// <Projekt>/Game.ini (XP-Nostalgie):
//
//   [RPG Maker 3D]
//   NativeTitle=1        ; 0 = kein eingebauter Titel -> Ruby-Hook
//                          "Game.custom_title" (oder direkter Spielstart)
//   NativeHud=1          ; 0 = HUD/RmlUi beim Start aus (UI.hud_visible= steuert)
//   NativeGameMenu=1     ; 0 = Esc oeffnet NICHT das eingebaute Spielmenue
//   NativeBattleMenu=1   ; 0 = kein eingebautes Kampfmenue (Custom via Battle-API)
//   NativeBattleStatus=1 ; 0 = keine eingebaute Gegner-/Gruppenzeile im Kampf
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

    /// Alles auf Standard (eingebaute Oberflaechen aktiv)
    void Reset();

    /// Liest <Projekt>/Game.ini (fehlende Datei = Defaults, kein Fehler).
    /// Idempotent - darf bei jedem Projekt-/Spielstart erneut laufen.
    void LoadFromProject(const std::string& projectPath);

private:
    CustomConfig() = default;
};

} // namespace rpg
