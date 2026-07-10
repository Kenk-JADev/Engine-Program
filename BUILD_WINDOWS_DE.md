# RPG Maker 3D Engine – Windows Programm Code (Deutsch)

Willkommen zur **RPG Maker 3D Engine** – einer vollständigen 3D RPG Engine im Stil des klassischen RPG Makers, aber als natives **Windows Programm** (C++17, OpenGL 3.3, SDL2, ImGui).

Diese Dokumentation beschreibt den kompletten Windows-Code.

## Was ist das?

- **3D RPG Editor** wie RPG Maker, aber in 3D
- **Map Editor** mit Tilesets, Layern, 3D-Vorschau
- **Entity System**: Cube, Plane, Light, Particle, Sprite
- **Material System**: Diffuse, Emissive, Metallic, Roughness, Transparent, Wireframe
- **Ruby Scripting** via mruby: Events, Spiellogik, Actors
- **Datenbank**: Actors, Items, Waffen, Skills, Enemies, Tilesets
- **Event System**: RPG Maker kompatible Event-Kommandos + 3D Erweiterungen
- **Game Runtime**: Switches, Variables, SelfSwitches, Party, Player, Map
- **Battle System**: Rundenbasiert, erweiterbar
- **Audio**: BGM/SE via miniaudio (OGG, MP3, WAV)
- **Platform Layer**: Windows DPI Awareness, AppData, MessageBox etc.

## Architektur (Windows Fokus)

```
┌─────────────────────────────────────────────────────────┐
│ Editor (Dear ImGui, Docking)                            │
│  - Hierarchie, Inspector, MapEditor, ScriptEditor, ...  │
├─────────────────────────────────────────────────────────┤
│ Scripting (mruby) + EventSystem + Game + Database       │
├─────────────────────────────────────────────────────────┤
│ Core: Scene Graph / ECS / ResourceManager / UI          │
├─────────────────────────────────────────────────────────┤
│ Renderer (OpenGL 3.3, Forward, Instancing)              │
│ Audio (miniaudio, WASAPI auf Windows)                   │
│ Input (SDL2) / Window / Platform (Win32 API)            │
├─────────────────────────────────────────────────────────┤
│ SDL2 + glad + glm + stb_image + ImGui + miniaudio       │
└─────────────────────────────────────────────────────────┘
```

## Windows-spezifische Dateien

Neu hinzugekommen für Windows-Support:

- `include/rpgmaker3d/Config.h` – Version, Pfade, Plattform-Erkennung
- `include/rpgmaker3d/Platform.h` – Windows API Wrapper (DPI, Pfade, MessageBox)
- `src/Platform.cpp` – Implementierung (SHGetFolderPath, ShellExecute etc.)
- `include/rpgmaker3d/Database.h` + `src/Database.cpp` – RPG Datenbank
- `include/rpgmaker3d/EventSystem.h` + `src/EventSystem.cpp` – Event Interpreter
- `include/rpgmaker3d/Game.h` + `src/Game.cpp` – Runtime (Player, Party, Switches)
- `include/rpgmaker3d/BattleSystem.h` + `src/BattleSystem.cpp` – Kampf
- `include/rpgmaker3d/UI.h` + `src/UI.cpp` – Message, Title, Pause
- `resources/resource.rc` + `app.manifest` + `icon.ico` – Windows Ressourcen
- `src/player_main.cpp` – Standalone Player EXE
- `docs/WINDOWS.md` – Deutsche Build Anleitung
- `installer/installer.nsi` – NSIS Installer (geplant im nächsten Schritt)

## CMake für Windows

`CMakeLists.txt` wurde komplett überarbeitet für Windows:

- Findet SDL2 via vcpkg, Config oder manuellem Pfad + Fallback als Subprojekt
- Unterstützt statisches Linken (`x64-windows-static`) für einfache Verteilung
- Kopiert automatisch `assets/`, `SampleProject/`, `ruby/`
- Bindet `resources/resource.rc` ein (Icon, Version, Manifest)
- Option `RPGMAKER3D_ENABLE_CONSOLE` – ob Konsole sichtbar bleibt (Debug)
- CPack für ZIP + NSIS Installer

Build Typen:

```cmake
# Editor (Standard)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static

# Mit Ruby
cmake -B build -S . -DRPGMAKER3D_ENABLE_RUBY=ON ...

# Ohne Konsole (Release, nur Fenster)
cmake -B build -S . -DRPGMAKER3D_ENABLE_CONSOLE=OFF ...

# Nur Player
cmake -B build -S . -DRPGMAKER3D_BUILD_PLAYER=ON -DRPGMAKER3D_BUILD_EDITOR=OFF ...
```

## Ruby API (Windows getestet)

Die Engine erlaubt Ruby Scripts (mruby) für Spiellogik:

```ruby
class Game
  def initialize
    @player = Actor.new("Hero")
    @player.move_to(0,0,0)
    @player.set_model("cube")
    @player.set_color(0.2, 0.6, 1.0)
  end

  def update(dt)
    speed = dt * 5.0
    @player.move(speed,0,0) if Input.key_down?(:d)
    @player.move(-speed,0,0) if Input.key_down?(:a)
    @player.move(0,0,speed) if Input.key_down?(:s)
    @player.move(0,0,-speed) if Input.key_down?(:w)
    
    Engine.log("Player pos: #{@player.position}")
  end
end

$game = Game.new
```

Module:

- `Actor.new(name)` – neue Entität
- `move_to(x,y,z)`, `move(dx,dy,dz)`
- `position`, `rotation`, `scale`
- `set_model("cube"/"plane")`, `set_color(r,g,b)`
- `Input.key_down?(:w)` etc.
- `Map.set_tile(layer,x,z,id)`, `Map.get_tile(...)`, `Map.width`, `Map.height`
- `Audio.bgm_play(path, loop)`, `Audio.se_play(path)`
- `Camera.set_position(x,y,z)` etc.
- `Engine.time`, `Engine.delta_time`, `Engine.log(msg)`

## Projekt-Export für Windows

Ein fertiges Spiel besteht aus:

```
MeinSpiel/
├─ Game.exe (umbenannte RPGMaker3D_Player.exe)
├─ assets/
├─ SampleProject/ -> umbenannt zu GameData/
├─ saves/
└─ ruby/
```

Später: Export-Button im Editor der automatisch kopiert + umbenennt.

## Nächste Schritte (Roadmap Windows)

Geplant:

- [ ] GLTF Model Import (tinygltf)
- [ ] Animation System (Skeletal)
- [x] Database Editor UI
- [ ] Event Editor UI (visuell)
- [ ] Battle Editor + Balancing
- [ ] Quest System
- [ ] Save/Load UI vollständig
- [ ] Multiplayer (optional, via ENet)
- [ ] Steam Workshop / Steamworks SDK
- [ ] DirectX 11 Renderer optional (für bessere Windows Kompatibilität)
- [ ] Installer (NSIS) fertig

## Lizenz

MIT – frei für kommerzielle und private Projekte. Windows-Code ist vollständig enthalten, keine externen Lizenzen außer SDL2 (zlib), ImGui (MIT), glm (MIT), miniaudio (MIT), stb_image (MIT).

Viel Spaß beim Entwickeln!
