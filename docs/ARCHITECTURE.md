# RPG Maker 3D Engine – Architektur

## 1. Design-Prinzipien

1. **Low-Spec**: Zielhardware sind auch ältere iGPUs. OpenGL 3.3 als Baseline, einfache Shader, keine Raytracing-Features.
2. **Modularität**: Core-Engine, Editor und Scripting sind klar getrennt.
3. **Datengetrieben**: Maps, Tilesets, Modelle und Sounds werden aus JSON/Binärdateien geladen.
4. **Erweiterbar**: Ruby-Scripts können Engine-Klassen erweitern und Events steuern.

## 2. Module

### 2.1 Core Engine (`src/`)

| Klasse | Aufgabe |
|--------|---------|
| `Engine` | Hauptloop, Zeitsteuerung, Modul-Initialisierung |
| `Window` | SDL2-Fenster, OpenGL-Kontext, VSync |
| `Renderer` | Rendering-Pipeline, Kamera, Batch/Instancing |
| `Shader` | OpenGL-Shader kompilieren, Uniforms setzen |
| `Texture` | PNG/JPG laden, OpenGL-Textur erstellen |
| `Model` | OBJ/GLTF laden, Meshes verwalten |
| `AudioManager` | Sound/Musik über miniaudio |
| `Input` | Tastatur, Maus, Gamepad |
| `Scene` | Szenegraph, Entitäten, Komponenten |
| `Map` | Gitterbasierter 3D-Map-Editor-Runtime |
| `Tileset` | PNG-Tileset, UV-Koordinaten pro Tile |
| `Project` | Projektstruktur, Pfade, Metadaten |

### 2.2 Scripting (`src/RubyVM`, `ruby/`)

- `mruby` wird in den Core eingebunden.
- C++-Bindings machen Engine-Klassen (Sprite, Actor, Map, Audio) aus Ruby heraus erreichbar.
- User-Scripts werden zur Laufzeit geladen und ausgeführt.
- Beispiel-API:
  ```ruby
  actor = Actor.new("hero")
  actor.move_to(5, 0, 3)
  actor.say("Hallo Welt!")
  Audio.bgm_play("town.ogg")
  ```

### 2.3 Editor (`src/Editor`)

- Basiert auf Dear ImGui, läuft im selben Fenster wie die Engine.
- Panels:
  - Scene Hierarchy
  - Inspector
  - Asset Browser
  - Map Editor (3D-Ansicht + 2D-Layer)
  - Tileset Editor
  - Script Editor (Texteditor mit Syntax-Highlighting für Ruby)
  - Console/Log

### 2.4 Asset-Pipeline

- Texturen: PNG mit stb_image
- Modelle: zuerst OBJ, später GLTF über tinygltf
- Audio: OGG/MP3/WAV über miniaudio
- Maps/Projekte: JSON (Editor) + binär kompiliert (Release)

## 3. Rendering

- Forward-Renderer mit Phong/ Lambert-Beleuchtung
- Tilemap als gebatchetes Mesh mit Instancing
- Kamera: Perspektivisch oder orthographisch (RPG-Style)
- Post-Processing optional: FXAA, einfacher Bloom

## 4. Speicher & Performance

- Resource-Manager mit Referenzzählung
- Shader/Texturen nur einmal laden
- Tilemap-Instancing für viele gleiche Tiles
- Objektpooling für Partikel/Audio-Quellen

## 5. Nächste Schritte

1. Lauffähiges Fenster + ImGui
2. Shader-Pipeline + Kamera
3. Erste Tilemap-Renderer
4. mruby einbinden
5. Editor-Panels
6. Asset-Import
