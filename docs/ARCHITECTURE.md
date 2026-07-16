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
- Optional RmlUi für Ingame-UI

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
| `RPGMAKER3D_ENABLE_RMLUI` | ON | RmlUi |
| `RPGMAKER3D_ENABLE_RUBY` | OFF | mruby |
| `RPGMAKER3D_ENABLE_IMGUI` | OFF | Legacy GameUI-Overlay only |

## 6. Nächste Schritte

1. Gizmo-Transform im Game View
2. Asset-Browser
3. Undo-Batch fuer Multi-Tile-Pinsel
4. GameUI Messages komplett auf RmlUi


## 7. UI-Pipeline (Scripts & Game-Fenster)

```
  Script-Editor (Ruby)
        |
        v
  RubyVM  UI.show_message / show_screen_text / show_picture
        |
        v
  GameUI  (Logik: MessageWindow, ScreenTexts, Pictures)
        |
        +--(optional ImGui Draw, wenn RPGMAKER3D_ENABLE_IMGUI)
        |
        v
  RmlUiSystem::SyncFromGameUI()
        |
        v
  RmlUi Game-Kontext  (sichtbares HUD + Dialog-Box im GL-Fenster)
```

**RmlUi hat kein Ruby-Binding.** Scripts nutzen immer das Modul `UI` (C++-Bindings
in `RubyVM::BindUI`). RmlUi ist nur der Renderer fuer das Game-Fenster.

Qt-Editor-Docks sind **Werkzeuge** (Map/Events/DB/Code) und laufen nicht im Player.
