# Build-Anleitung RPG Maker 3D Engine

## Voraussetzungen

- C++17-Compiler (GCC, Clang, MSVC)
- CMake 3.16+ (optional, alternativ Makefile)
- Python 3 (nur um glad zu generieren)
- Unter Linux: X11 + OpenGL + ALSA/PulseAudio-Entwicklungspakete

## Schnellstart Linux (Makefile)

Die benötigten Abhängigkeiten (SDL2, glad, glm, imgui, stb, miniaudio) liegen bereits im `third_party/`-Ordner. SDL2 wurde statisch kompiliert.

```bash
cd rpgmaker3d
make -j$(nproc)
./rpgmaker3d
```

Falls die X11/OpenGL-Bibliotheken nicht unter den Standardpfaden liegen, passe `Makefile` -> `LDLIBS` an.

## Schnellstart Windows (automatisiert)

Am einfachsten ist das PowerShell-Skript:

```powershell
cd rpgmaker3d
.\scripts\build-windows.ps1
```

Das Skript installiert automatisch vcpkg, SDL2, glm, generiert glad und baut die `.exe`.

Danach findest du die ausführbare Datei unter:

```
build\Release\RPGMaker3D.exe
```

## Schnellstart Windows (manuell mit Visual Studio 2022)

1. Installiere die Dependencies über vcpkg:

   ```cmd
   git clone https://github.com/Microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat
   .\vcpkg\vcpkg install sdl2:x64-windows-static glm:x64-windows-static
   ```

2. Generiere `glad` für OpenGL 3.3 Core:

   ```cmd
   python3 -m pip install glad2
   python3 -m glad --api gl:core=3.3 --out-path third_party/glad_generated c
   ```

3. Baue mit CMake:

   ```cmd
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   build\Release\RPGMaker3D.exe
   ```

## Automatischer Build über GitHub Actions

Wenn du das Projekt auf GitHub hochlädst, wird bei jedem Push automatisch eine Windows-EXE erstellt und als Download bereitgestellt.

Workflow-Datei: `.github/workflows/build-windows.yml`

1. Erstelle ein Repository auf GitHub.
2. Lade den `rpgmaker3d`-Ordner hoch.
3. Gehe im Repository auf **Actions > Build Windows EXE > Run workflow**.
4. Nach ca. 5–10 Minuten findest du unter **Actions > Build Windows EXE > Artifacts** die ZIP-Datei `RPGMaker3D-Windows-x64`.
5. Entpacke die ZIP und starte `RPGMaker3D.exe`.

## Ruby-Scripting aktivieren

1. mruby klonen und bauen:

   ```bash
   cd third_party
   git clone https://github.com/mruby/mruby.git
   cd mruby
   make
   cd ../..
   ```

2. CMake aufrufen mit:

   ```bash
   cmake -B build -S . -DRPGMAKER3D_ENABLE_RUBY=ON
   cmake --build build
   ```

## Bekannte Einschränkungen

- Der Editor verwendet Dear ImGui mit Docking.
- mruby-Bindings sind als Stub vorhanden und müssen für die finale API ausgebaut werden.
- GLTF-Import ist noch nicht implementiert (nur OBJ + primitive Meshes).
