# RPG Maker 3D Engine – Architektur

## 1. Design-Prinzipien

1. **Low-Spec**: OpenGL 3.3, einfache Shader, keine Raytracing-Features.
2. **Modularität**: Core-Engine, Qt-Editor und Scripting sind klar getrennt.
3. **Datengetrieben**: Maps, Tilesets, Modelle und Sounds aus JSON/Binärdateien.
4. **Erweiterbar**: Ruby-Scripts steuern Spiellogik; C++ ist die Engine-API.

## 2. Module

### 2.1 Core Engine (`src/`)

| Klasse | Aufgabe |
|--------|---------|
| `Engine` | Hauptloop / Embedded-Tick, Modul-Initialisierung |
| `Window` | SDL2-Fenster **oder** Foreign-Host (Qt) |
| `Renderer` | Rendering-Pipeline, Kamera |
| `Scene` | Szenegraph, Entitäten, Komponenten |
| `Map` / `Tileset` | Gitterbasierte 3D-Map |
| `ScriptManager` / `RubyVM` | Ruby-Spiellogik |
| `Project` / `Database` | Projektstruktur & RPG-Daten |

### 2.2 Scripting (`src/RubyVM`, `ruby/`, `SampleProject/scripts/`)

- mruby optional (`RPGMAKER3D_ENABLE_RUBY`)
- Scripts: `00_*.rb` … `main.rb` in Load-Order
- C++-Bindings: Actor, Map, Audio, UI, Input, …

### 2.3 Editor (`qt_editor/`) – **einziger Editor-Host**

- **Qt 6** (QMainWindow, Docks, Tabs)
- **Game View**: QOpenGLWidget + Engine Embedded
- **Code Workspace**: Ruby-Editor + C++ API-Referenz  
  (ersetzt den alten ImGui-„Game Scene“-/Script-Schwerpunkt)
- ImGui-Editor ist **entfernt** (nicht im Build)

### 2.4 Player (`src/player_main.cpp`)

- SDL2-Fenster, `editorMode=false`
- Kein Qt, kein ImGui-Editor
- GameUI-ImGui-Overlay für Ingame-UI (PAKET 10; RmlUi vollständig entfernt)

## 3. Schichten

```
┌──────────────────────────────────────────────┐
│     Qt Editor (Docks + Game View + Code)     │
├──────────────────────────────────────────────┤
│           Scripting (mruby / Ruby)           │
├──────────────────────────────────────────────┤
│  Scene │ Renderer │ Audio │ Map │ Database   │
├──────────────────────────────────────────────┤
│  SDL2 (Player) / Qt GL (Editor) │ OpenGL 3.3 │
└──────────────────────────────────────────────┘
```

## 4. Rendering

- Forward-Renderer, Phong/Lambert
- Tilemap-Instancing, Shadows optional
- Editor-Selektion: Bounding-Box Highlight

## 5. Build-Flags

| Flag | Default | Bedeutung |
|------|---------|-----------|
| `RPGMAKER3D_EDITOR_QT` | ON | Qt-Editor bauen |
| `RPGMAKER3D_BUILD_EDITOR` | ON | Editor-Target |
| `RPGMAKER3D_BUILD_PLAYER` | ON | Player-EXE |
| `RPGMAKER3D_ENABLE_RUBY` | OFF | mruby |
| `RPGMAKER3D_ENABLE_IMGUI` | ON | GameUI-Overlay = gesamte Spielanzeige |

## 6. Nächste Schritte

1. Gizmo-Transform im Game View
2. Asset-Browser
3. Undo-Batch fuer Multi-Tile-Pinsel
4. (erledigt, PAKET 10) RmlUi entfernt — gesamte Anzeige im GameUI-ImGui-Overlay


## 7. UI-Pipeline (Scripts & Game-Fenster)

```
  Script-Editor (Ruby)
        |
        v
  RubyVM  UI.show_message / show_screen_text / show_picture
        |
        v
  GameUI  (Logik: MessageWindow, MenuWindow, ScreenTexts, Pictures,
        |   Modal-Input)  +  Draw im ImGui-Overlay (PAKET 10: Messages,
        |   Menues, Zahl-/Namenseingabe, HUD, Pictures, Kampfstatus)
        v
  ImGui GameUI-Overlay (sichtbare Anzeige im GL-Fenster)
```

**ImGui-Frame-Lebenszyklus (PAKET-10-Nachtrag 2026-07-24):** Kontext +
OpenGL3-Backend werden in `Engine::InitImGui` erzeugt (host-neutral:
SDL-Player und Qt-GameView gleich; kein SDL-Backend — Eingaben laufen
nativ, DisplaySize/DeltaTime pro Frame manuell). Render-Reihenfolge:
3D-Szene → GameUI-DrawData (ImGui GL-Backend) → RgssUI-Canvas
(Ruby-Fenster bleiben oberste Schicht). Ohne diesen Lebenszyklus ist
jeder ImGui-Draw ein NULL-Kontext-Zugriff (historischer Bug).

**Die Anzeige hat kein Ruby-Binding.** Scripts nutzen immer das Modul `UI`
(C++-Bindings in `RubyVM::BindUI`). Das ImGui-Overlay ist nur der Renderer
fuer das Game-Fenster; `UI.hud_visible=` steuert das HUD, F9 toggelt es.

**Ruby-Runtime pro Frame (Playtest/Player):**
1. `SceneManager.update` (Title/Map/Battle aus Script-Editor)
2. `$game.update(dt)` (optional, main.rb)
3. C++ Player/Events/BattleSystem

Qt-Editor-Docks sind **Werkzeuge** (Map/Events/DB/Code/Assets) und laufen nicht im Player.


## 8. RPG-Maker-Kern (Runtime)

| Feature | Wie |
|---------|-----|
| Scenes | Ruby `SceneManager` + `Scene_Title`/`Map`/`Battle` |
| Dialoge | Event `ShowText` / Ruby `UI.show_message` → GameUI → ImGui-Overlay |
| Switches/Variables | `Game.switch` / `Game.set_switch` / `variable` |
| Save/Load | `Game.save(slot)` / `Game.load` → `saves/saveN.json` |
| Battle | Event `BattleProcessing` / `Game.start_battle(troopId)` / Random Encounter / `Battle.*` Ruby-Modul (Custom-Szenen) |
| Shop | Event `ShopProcessing` → `GameUI::ShowShopGoods` (Items + `w<ID>` Waffen + `a<ID>` Ruestungen) |
| Custom-UIs | `Game.ini` (NativeTitle/Hud/GameMenu/BattleMenu/BattleStatus) + `UI.native_*=` + `CallGameHook("custom_title")` + `UI.open_list_menu` |
| Pictures | ImGui-Overlay (DrawPictures: Skalierung/Rotation, PAKET 9 Flash/Blink, battlerHue-Farbton); Kampf: bis 4 Gegnerbilder `$battlerN` aus `Graphics/Battlers/` (EnemyData.battlerName/battlerHue) |
| Script-Befehl | Event `Script` → RubyVM (Script-Editor-Code) |
| Troops/States | `database/Troops.json`, `States.json` |

Hotkeys im Default-Script: **F1** Save, **F2** Load, **F3** Testkampf.

### Battle-Input (Playtest/Player) - XP-Kampfmenue
Sobald ein Akteur an der Reihe ist (`BattleSystem::NeedsInput()`), oeffnet
die Engine `GameUI::OpenBattleCommands()` – ein MenuWindow mit
**Angriff / Fertigkeit / Gegenstand / Verteidigen / Flucht**
(Flucht deaktiviert bei „Kann nicht fliehen“). Fertigkeit/Gegenstand
oeffnen Listen (MP-Kosten, Anzahl); danach folgt die Zielwahl
(Gegner- oder Verbuendeten-Liste mit HP). Esc geht einen Schritt zurueck.
Verteidigen halbiert Schaden bis zur naechsten eigenen Aktion.
HP/MP werden am Kampfende zurueck in die Party synchronisiert; Sieg
schreibt EXP via `GameActor::AddExp` gut (Level-Ups mit Meldung,
Klassen-EXP-Kurve wie VX Ace). Niederlage ohne „Niederlage moeglich“
loest `BattleSystem::onGameOver` aus (Engine zeigt GAME OVER +
Gameover-ME/Grafik und kehrt zum Titel zurueck; Editor: Playtest-Stopp).

### Map-Kollision
Tile-Flag **solid** (Map-Dock: Button „Solid“) → `TilesetData.flags` → `GameMap::IsPassable`.

### Charakterbewegung (PAKET 29)
XP-Kernmechanik neu als zeitgesteuerte 3D-Bewegung: `CharacterMotion`
(interpolierte Kachel­schritte, echte Sprung-Parabel auf Y), Tempo-/Häufigkeits-
Tabellen aus `xp::`, Diagonal-Routen 5–8 + Tokens `DL DR UL UR`, Breitensuche
für „Annähern“, weiche Follow-Kamera. Details: `docs/XP-3D-CORE.md`.

### Common Events
`maps/CommonEvents.json`, Trigger Autorun wenn `switchId` an (oder 0=immer bei Autorun).
Button **Common+** im Event-Dock.


## 9. Prioritaet 3+4 (Menue, Kampf, MoveRoute, Qualitaet)

| Feature | Umsetzung |
|---------|-----------|
| Esc-Party-Menue | `15_Party_Menu.rb` (Items/Status/Save) |
| Message Name/Pos | `MessageWindow` Speaker + Position → ImGui-Overlay |
| Self-Switch | Event-Befehl + Seiten-Condition im Event-Dock |
| Move Route | `UDLR W T A X` Text-Format, Runtime-Update |
| Battle-UI | XP-Kampfmenue ueber `GameUI::OpenBattleCommands()` (MenuWindow) |
| Undo-Batch | `BatchTileCommand` fuer Pinsel/Rechteck |
| Gizmo-Undo | `MoveEntityCommand` bei Loslassen |
| Plugins | `scripts/plugins/*.rb` |
| Deploy | `scripts/package-player.sh` / `.ps1` |
