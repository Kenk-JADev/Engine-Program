# RPG Maker 3D Engine

Eine modulare 3D-Game-Engine im Stil von RPG Maker, aber mit modernem Renderer und Ruby-Scripting.

## Ziele

- **Low-Spec-freundlich**: OpenGL 3.3, instanzierbares Rendering, optionale Low-Poly-Modi
- **Ruby-Scripting**: Eingebettete `mruby`-VM für Spiellogik, Events, Kampfsysteme
- **Qt-Editor**: Native Fenster, Game View, **Code Workspace (Ruby + C++)**
- **Asset-Support**: PNG-Tilesets, OBJ/GLTF-Modelle, OGG/MP3/WAV-Audio, Shader

> **Hinweis:** Der alte Dear-ImGui-Editor ist **entfernt**. Einziger Editor-Host ist Qt
> (`docs/QT-EDITOR.md`). Der Tab **Code** ersetzt den früheren Game-Scene-/Script-Fokus
> und ist für Ruby-Spiellogik sowie C++-Engine-API gedacht.

## Architektur

```
┌─────────────────────────────────────────────┐
│   Qt Editor (Game View + Code Ruby/C++)     │
├─────────────────────────────────────────────┤
│              Scripting (mruby)              │
├─────────────────────────────────────────────┤
│  Scene Graph  │  Renderer  │  Audio  │ ECS  │
├─────────────────────────────────────────────┤
│  SDL2 (Player) / Qt GL (Editor) │ OpenGL 3.3│
└─────────────────────────────────────────────┘
```

## Ordnerstruktur

- `src/` – Engine-Quellcode
- `include/rpgmaker3d/` – Öffentliche Header
- `qt_editor/` – Qt-Editor (Hauptfenster, Game View, Code Workspace)
- `ruby/` – Beispiel-Scripts und Runtime-Scripts
- `assets/` – Shaders, Texturen, Modelle, Audio
- `third_party/` – glad, stb_image, miniaudio, RmlUi, …
- `docs/` – Architektur- und API-Dokumentation

## Build (Windows mit Visual Studio 2022)

1. vcpkg installieren:
   ```cmd
   git clone https://github.com/Microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat
   .\vcpkg\vcpkg install sdl2:x64-windows-static sdl2-image:x64-windows-static glew:x64-windows-static
   ```

2. Projekt generieren:
   ```cmd
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

## Build (Linux)

```bash
sudo apt install build-essential cmake libsdl2-dev libglew-dev libgl1-mesa-dev
mkdir build && cd build
cmake ..
make -j
```

## Build (mit mruby)

Für Ruby-Scripting muss mruby gebaut werden:

```bash
cd third_party
git clone https://github.com/mruby/mruby.git
cd mruby
make
```

Dann in CMake `RPGMAKER3D_ENABLE_RUBY=ON` setzen.

## Schnellstart (Qt-Editor)

```bash
cmake -B build -S . -DRPGMAKER3D_EDITOR_QT=ON
cmake --build build -j
./build/RPGMaker3D   # bzw. build/Release/RPGMaker3D.exe
```

Im Editor:
- Tab **Game View** – 3D-Szene, Selektion, Playtest (F5)
- Tab **Code** – Ruby-Scripts des Projekts + C++ Engine-API-Referenz
- `Datei > Neues Projekt` / `Projekt öffnen`

## Lizenz

MIT – für kommerzielle und nicht-kommerzielle Projekte frei verwendbar.
