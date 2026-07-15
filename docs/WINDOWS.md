# RPG Maker 3D – Windows Build Anleitung (Deutsch)

Diese Anleitung beschreibt wie du die Engine unter **Windows 10 / 11** als natives Programm baust.

## Voraussetzungen

- **Windows 10/11 64-bit**
- **Visual Studio 2022** mit C++ Desktop Entwicklung (MSVC v143)
- **Git** für Windows
- **Python 3.10+** (für glad-Generator)
- **CMake 3.16+** (kommt mit Visual Studio oder separat installieren)

Optional für Ruby-Scripting:
- **Ruby 3.2+** (für mruby Build, via ruby-installer)

## Schnellstart (Empfohlen)

Am einfachsten mit dem PowerShell Skript (liegt bereits im Repo, wird nicht verändert):

```powershell
cd Engine-Program
.\scripts\build-windows.ps1
```

Das Skript macht automatisch:
1. vcpkg klonen nach `C:\dev\vcpkg` (falls nicht vorhanden)
2. SDL2 + glm installieren (x64-windows-static)
3. glad OpenGL Loader generieren
4. CMake Projekt konfigurieren + bauen
5. Assets kopieren

Am Ende liegt die EXE unter:

```
build\Release\RPGMaker3D.exe
```

Doppelklick zum Starten.

## Manueller Build mit vcpkg

Falls du es manuell machen willst:

### 1. vcpkg vorbereiten

```cmd
git clone https://github.com/Microsoft/vcpkg.git C:\dev\vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat
C:\dev\vcpkg\vcpkg.exe install sdl2:x64-windows-static glm:x64-windows-static
```

### 2. glad generieren

```cmd
pip install glad2
python -m glad --api gl:core=3.3 --out-path third_party\glad_generated c
```

### 3. mruby (optional, für Ruby-Scripting)

Öffne für diesen Schritt die **x64 Native Tools Command Prompt for Visual Studio 2022**. Die Engine verwendet den offiziellen mruby-MSVC-Build (nicht den GCC-Toolchain-Default):

```cmd
git clone --branch 4.0.0 --depth 1 https://github.com/mruby/mruby.git third_party\mruby
cd third_party\mruby
set MRUBY_CONFIG=ci/msvc
ruby minirake
cd ..\..
```

Danach müssen diese Dateien vorhanden sein:

```text
third_party\mruby\build\host\lib\libmruby.lib
third_party\mruby\build\host\lib\libmruby_core.lib
```

### 4. CMake Build

```cmd
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DRPGMAKER3D_ENABLE_RUBY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

### 5. Fertig

Die fertige Version liegt in `build\Release\`:

```
build\Release\RPGMaker3D.exe
build\Release\assets\
build\Release\SampleProject\
```

## Start-Parameter

Die EXE unterstützt Kommandozeilen-Argumente:

```
RPGMaker3D.exe --editor                     # Editor Modus (Standard)
RPGMaker3D.exe --play                       # Direkt Spiel testen
RPGMaker3D.exe --project ./MyGame           # Eigenes Projekt laden
RPGMaker3D.exe --width 1920 --height 1080   # Auflösung
RPGMaker3D.exe --fullscreen                 # Vollbild
```

## Player (ohne Editor)

Für fertige Spiele gibt es einen zweiten Build:

```cmd
cmake -B build -S . -DRPGMAKER3D_BUILD_PLAYER=ON ...
```

Erzeugt `RPGMaker3D_Player.exe` – nur Spiel, kein Editor.

## Projekt-Struktur für Windows

Nach dem Build:

```
RPGMaker3D/
├─ RPGMaker3D.exe          # Editor + Runtime
├─ RPGMaker3D_Player.exe   # Nur Runtime (optional)
├─ assets/                 # Engine Assets (Shader, Icons)
│  ├─ shaders/
│  ├─ textures/
│  └─ icon.bmp
├─ SampleProject/          # Beispiel Projekt
│  ├─ project.json
│  ├─ assets/
│  ├─ maps/
│  ├─ scripts/
│  ├─ prefabs/
│  └─ database/
├─ saves/                  # Spielstände
├─ engine.log              # Log Datei
└─ ruby/                   # Beispiel Ruby Scripts
```

## Neues Spiel erstellen

1. Engine starten
2. Menü `File > New Project`
3. Projekt Name eingeben (z.B. `MeinAbenteuer`)
4. Maps erstellen, Tileset laden
5. Ruby Scripts editieren
6. `File > Save Project`
7. Mit `F5` testen, mit `Build > Export Game` exportieren (geplant)

## Deployment / Installer

Ein NSIS Installer-Script liegt unter `installer/installer.nsi`:

```cmd
makensis installer\installer.nsi
```

Erzeugt `RPGMaker3D-Setup.exe`.

## Troubleshooting Windows

**Fehler: SDL2 not found**
- Stelle sicher dass `VCPKG_ROOT` korrekt ist, oder setze `SDL2_ROOT` manuell

**Fehler: glad/gl.h not found**
- Führe `python -m glad --api gl:core=3.3 --out-path third_party/glad_generated c` aus

**Fehler: „Keine mruby lib gefunden“**
- Baue in der *x64 Native Tools Command Prompt for Visual Studio 2022* und setze vor `ruby minirake` zwingend `MRUBY_CONFIG=ci/msvc`.
- Setze dabei **nicht** `CC=cl` oder `CXX=cl`: Das würde bei mruby fälschlich den GCC-Toolchain auswählen. Ein erfolgreicher MSVC-Build erzeugt `libmruby.lib` und `libmruby_core.lib` unter `third_party\\mruby\\build\\host\\lib`.

**Schwarzes Fenster / OpenGL Fehler**
- Grafikkartentreiber aktualisieren
- OpenGL 3.3 wird benötigt (fast alle GPUs seit 2010 unterstützen das)

**Kein Audio**
- miniaudio nutzt WASAPI unter Windows – sollte out-of-the-box funktionieren
- Prüfe `engine.log`

**Antivirus blockiert EXE**
- Manche AVs blockieren frisch gebaute EXEs – Ausnahmeregel hinzufügen oder signieren

## GitHub Actions (Automatischer Windows Build)

Bei jedem Push nach `main` / `master` baut GitHub automatisch eine Windows EXE:

1. Repo auf GitHub pushen
2. Unter Actions -> Build Windows EXE -> Artifacts -> `RPGMaker3D-Windows-x64.zip` herunterladen
3. Entpacken und `RPGMaker3D.exe` starten

Siehe `.github/workflows/Main.yml`.

## Systemanforderungen

- **OS**: Windows 10 64-bit oder neuer (Windows 7/8 geht evtl. mit älterer SDL2)
- **CPU**: Dual Core 2GHz+
- **RAM**: 2GB+
- **GPU**: OpenGL 3.3 kompatibel (Intel HD 4000+ / NVIDIA / AMD)
- **Festplatte**: 200MB

## Kontakt

Bei Problemen Issue auf GitHub erstellen oder im Log `engine.log` nachschauen.
