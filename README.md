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
- Tabs unten (Browser-Stil): **Spielansicht** (3D), **Landkarte** (2D), **Spiel** (Playtest), **Skript** (Ruby/C++)
- Ribbon oben: **Datei / Werkzeuge / Ansicht / Fenster / Debug**
- `Datei > Neues Projekt` / `Projekt öffnen`

## Landkarte (2D ↔ 3D-Mapsystem)

Die Landkarte malt direkt in `Engine::GetMap()` — dieselbe Karte, aus der das
3D-Mapsystem seine Geometrie baut. Oben sitzen die **XP-Ebenen-Buttons
1 / 2 / 3 / EV**: die ersten drei wählen die Tile-Ebene (Karten wachsen
automatisch auf 3 Ebenen), **EV** schaltet in den **Ereignis-Modus**: Events
werden als Kästchen eingezeichnet, **Doppelklick** auf ein leeres Feld legt ein
neues Event an und öffnet den XP-Event-Dialog, Doppelklick auf ein Event
bearbeitet es, **Entf** löscht das gewählte Event. Das Events-Dock wird dabei
live synchronisiert. Links sitzt die **XP-Tileset-Palette** mit echten
Tile-Bildern (erster Eintrag: Radierer) — sie ist mit dem Map-Dock und dem
3D-Pinsel in beide Richtungen synchronisiert.

## Karteneigenschaften

Ribbon **Werkzeuge → Karteneigenschaften** (oder **Doppelklick** auf eine Karte
in der Map-Liste) öffnet den XP-Dialog: Name, **Tileset**, **Größe** (Resize
erhält den Inhalt links-oben, wie XP), Scroll-Typ, Encounter (Schritte +
Trupp-Liste), **BGM/BGS mit ▶-Anhören** und „Rennen verboten". OK speichert
Karte + Datenbank und aktualisiert 2D/3D sofort.

## Skript-Editor

Der Skript-Tab folgt dem XP-Aufbau: links die Skriptliste, rechts der Editor.
Neu / **Umbenennen (F2 oder Doppelklick auf den Eintrag)** / Löschen /
Suchen (Strg+F) / Snippets / Ausführen / Hot-Reload. Umlaute sind überall
erlaubt.

## Playtest

- **F5** – speichert das Projekt und startet **`RPGMaker3D_Player.exe --project <Pfad>`**
  (exakt wie beim Spieler; das Player-Target ist `RPGMAKER3D_BUILD_PLAYER=ON`, Standard)
- **Umschalt+F5** – schneller eingebetteter Test direkt in der Spielansicht
- Player von Hand: `RPGMaker3D_Player --project SampleProject` (oder Pfad als 1. Argument)

## Events (XP-Befehlssatz)

Der Event-Interpreter unterstützt den **vollständigen RPG-Maker-XP-Befehlssatz**
(Codes 101–355, siehe `docs/EVENTS-XP.md`): Text, Auswahl mit „Wenn“-Zweigen,
Zahlen-/Namenseingabe, Bedingungen (13 Typen), Schleifen, Bewegungsrouten,
Bildschirmeffekte, Kampfverarbeitung mit Sieg/Flucht/Niederlage-Zweigen usw.

Im Editor: Dock **Events** → Doppelklick auf ein Event öffnet den **XP-artigen
Event-Dialog** (Seiten, Bedingungen, Grafik, Autonome Bewegung, Optionen,
Auslöser und die Befehlsliste mit `@>`-Einrückung). Der Befehlsdialog bietet
alle XP-Befehle auf drei Seiten.

## Datenbank (XP-Dialog)

Ribbon **Werkzeuge → Datenbank** (oder der Button im Datenbank-Dock) öffnet den
**XP-artigen Datenbank-Dialog** mit den 13 Tabs **Akteure, Klassen,
Fertigkeiten, Gegenstände, Waffen, Rüstungen, Gegner, Trupps, Status,
Animationen, Tilesets, Gemeinsame Events und System** — links die nummerierte
Liste („001: …") mit **Maximum ändern …**, rechts die Eigenschaften, unten
**OK / Abbrechen / Anwenden**. Der System-Tab enthält wie in XP Anfangsgruppe,
Elemente (mit Maximum), Systemgrafiken, BGM/ME, 12 Soundeffekte und alle
Begriffe (HP, SP, STR … Ausrüsten). Die Anfangsgruppe steuert direkt die
Start-Party im Spiel. Alles wird im Projekt unter `database/*.json`
gespeichert.

## Sound-Test

Ribbon **Werkzeuge → Sound-Test** öffnet das XP-artige **Sound-Test-Fenster**:
Tabs **BGM / BGS / ME / SE**, Dateiliste mit „(Kein)", **Wiedergabe / Stopp**,
**Lautstärke-Regler** (0–100 %) und **Pitch-Regler** (50–150 %), beide greifen
live in die laufende Wiedergabe. Doppelklick auf einen Titel spielt ihn sofort
ab; beim Schließen wird automatisch gestoppt. Gescannt werden die Ordner
`Audio/BGM`, `Audio/BGS`, `Audio/ME`, `Audio/SE` (XP-Konvention) bzw.
`assets/audio/<typ>`; neue Projekte legen die XP-Ordner automatisch an.
Abspielbar: WAV, MP3, OGG, FLAC (MIDI nicht).

## Umlaute (ä ö ü Ä Ö Ü ß)

UI und Spiel sind UTF-8-durchgängig (MSVC baut mit `/utf-8`); Nachrichten,
Namen und Datenbanktexte dürfen Umlaute enthalten. In der Namenseingabe im
Spiel lassen sich Umlaute per **Alt+A / Alt+O / Alt+U** tippen.

## Lizenz

MIT – für kommerzielle und nicht-kommerzielle Projekte frei verwendbar.
